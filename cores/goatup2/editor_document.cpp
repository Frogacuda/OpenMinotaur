#include "editor_document.h"
#include "editor_tables.h"
#include "name_generator.h"
#include <sstream>
#include <iomanip>
#include <locale>
#include <algorithm>
namespace goatup2 {
bool serializeEditedStage(const Stage &s, const PlacementTable &p, std::string &output,
                          std::string &error, const std::vector<uint32_t> *adjacentBackup) {
    if (s.cells.size() != GridCells) {
        error = "invalid editor grid";
        return false;
    }
    std::ostringstream text;
    text.imbue(std::locale::classic());
    text << "(XCL)" << s.declaredColumns << "\n(YCL)" << s.declaredRows << "\n(DWI)"
         << s.displayWidth << "\n(DHI)" << s.displayHeight << "\n(NSE)" << s.seed << "\n(LHS)"
         << s.highScore << "\n(LBT)" << s.bestTime << "\n(SCR)\n";
    // 0x49510's final single word can compare equal to adjacent backup[0],
    // producing one extra decoded backup-prefix word. Model the lookahead safely.
    bool running = true;
    size_t at = 0;
    const auto cell = [&](size_t i) {
        return i < GridCells ? s.cells[i]
                             : (adjacentBackup ? adjacentBackup->at(i - GridCells)
                                               : s.cells.at(i - GridCells));
    };
    while (running) {
        const auto value = cell(at);
        size_t next = at + 1, count = 1;
        if (value == cell(next)) {
            size_t offset = 0;
            do {
                if (at + offset + 1 > 9998)
                    running = false;
                if (value != cell(at + offset + 2) || !running)
                    break;
                ++offset;
            } while (true);
            at += offset + 1;
            count = offset + 2;
            next = at + 1;
        }
        text << std::hex << value << std::dec << ":" << count << ",\n";
        if (at > 9998)
            running = false;
        if (running)
            at = next;
    }
    text << "(XSCR)\n";
    int used = 0;
    for (const auto &a : p.entries())
        if (a.type >= 0)
            ++used;
    text << "(OBJ)" << used << "\n" << std::fixed << std::setprecision(6);
    for (const auto &a : p.entries())
        if (a.type >= 0)
            text << a.type << "," << int32_t(a.flags) << "," << a.x << "," << a.y << "\n";
    text << "(XEND)";
    auto result = text.str();
    if (result.size() > StageCapacity) {
        error = "original 8192-byte level capacity exceeded";
        return false;
    }
    Stage checked;
    if (!readStage(result, checked, error))
        return false;
    output = std::move(result);
    error.clear();
    return true;
}
bool replaceEditedStage(const std::vector<uint8_t> &encoded, int level, const std::string &stage,
                        std::vector<uint8_t> &result, std::string &error) {
    if (level < 0 || level >= 20) {
        error = "invalid level slot";
        return false;
    }
    Stage checked;
    if (!readStage(stage, checked, error))
        return false;
    const auto decoded = transformBank(encoded);
    std::string text(decoded.begin(), decoded.end());
    const auto label = "(L" + std::to_string(level) + ")";
    const auto start = text.find(label), end = text.find("(XEND)", start);
    if (start == std::string::npos || end == std::string::npos) {
        error = "level section missing";
        return false;
    }
    text.replace(start + label.size(), end + 6 - start - label.size(), stage);
    const auto finish = text.find("(END)");
    if (finish == std::string::npos || finish + 5 > BankCapacity) {
        error = "original bank capacity exceeded";
        return false;
    }
    text.resize(finish + 5);
    auto bytes = transformBank({text.begin(), text.end()});
    Bank bank;
    if (!readBank(bytes, bank, error))
        return false;
    result = std::move(bytes);
    error.clear();
    return true;
}
bool replaceEditedBankHeader(const std::vector<uint8_t> &encoded, const Bank &bank,
                             std::vector<uint8_t> &result, std::string &error) {
    const auto raw = transformBank(encoded);
    std::string text(raw.begin(), raw.end());
    const auto at = text.find("(L123)"), end = text.find("(XEND)", at);
    if (at == std::string::npos || end == std::string::npos) {
        error = "bank header missing";
        return false;
    }
    const auto header = "(L123)(HDR)\n(GID)" + std::to_string(bank.gid) + "\n(PUR)" +
                        std::to_string(bank.pur) + "\n(TIM)" + std::to_string(bank.tim) +
                        "\n(FUR)" + std::to_string(bank.fur) + "\n(XEND)";
    text.replace(at, end + 6 - at, header);
    if (text.size() > BankCapacity) {
        error = "bank capacity exceeded";
        return false;
    }
    auto data = transformBank({text.begin(), text.end()});
    Bank check;
    if (!readBank(data, check, error))
        return false;
    result = std::move(data);
    error.clear();
    return true;
}
void EditorDocument::open(const Stage &s) {
    generatedName.clear();
    stage_ = s;
    backup_ = s.cells;
    placements.load(s);
    valid_.fill(false);
    cursor_ = 0;
    std::string error;
    if (serializeEditedStage(stage_, placements, history_[0], error))
        valid_[0] = true;
}
bool EditorDocument::writeCell(int x, int y, uint32_t value) {
    // The source accepts the declared boundary; a safe physical-grid bound also
    // prevents its negative/wrapped neighbor probes escaping owned storage.
    if (x < 0 || y < 0 || x > stage_.declaredColumns || y > stage_.declaredRows)
        return false;
    const auto at = size_t(y) * 100 + size_t(x);
    if (at >= 2 * GridCells)
        return false;
    auto &destination = at < GridCells ? stage_.cells[at] : backup_[at - GridCells];
    const auto previous = destination;
    uint32_t old = 0;
    if (!(value & 255U) && (previous & 255U) >= 43 && (previous & 255U) <= 45)
        old = previous & 255U;
    bool changed = previous != value;
    destination = value;
    if (old || ((value & 255U) >= 43 && (value & 255U) <= 45)) {
        for (int direction : {-1, 1})
            for (int column = x + direction;
                 column >= 0 && column < stage_.declaredColumns && column < 100;
                 column += direction) {
                const auto n = size_t(y) * 100 + size_t(column);
                auto &tile = n < GridCells ? stage_.cells[n] : backup_[n - GridCells];
                if ((tile & 255U) != old)
                    break;
                changed |= tile != value;
                tile = value;
            }
    }
    return changed;
}
std::array<uint32_t, 3> EditorDocument::previewCells() const {
    auto cells = sampled ? std::array<uint32_t, 3>{0, sampledTile, 0}
                         : std::array<uint32_t, 3>{editor_data::Brushes[brush].cells[style][0],
                                                   editor_data::Brushes[brush].cells[style][1],
                                                   editor_data::Brushes[brush].cells[style][2]};
    for (auto &value : cells)
        if (value || sampled) {
            if (!sampled && (value & 255U) == 51)
                value = uint32_t(ink + 51) | 0x1200U;
            else if (!sampled && editor_data::Brushes[brush].colours > 0)
                value = (value & 0xffff00ffU) | (uint32_t(ink & 31) << 8);
            if (std::find(editor_data::TranslucentIds.begin(), editor_data::TranslucentIds.end(),
                          value & 255U) != editor_data::TranslucentIds.end())
                value |= 0x4000U;
        }
    return cells;
}
bool EditorDocument::paint(int x, int y) {
    const auto cells = previewCells();
    if (sampled)
        return writeCell(x, y, cells[1]);
    bool changed = false;
    for (int i = 0; i < 3; ++i)
        if (cells[size_t(i)])
            changed |= writeCell(x, y + i - 1, cells[size_t(i)]);
    return changed;
}
void EditorDocument::sample(int x, int y) {
    sampled = true;
    sampledTile =
        x >= 0 && x < 100 && y >= 0 && y < 100 ? stage_.cells[size_t(y) * 100 + size_t(x)] : 0;
}
bool EditorDocument::place(neon::vec2d p) {
    return placements.place({object, variant, p.x, p.y}, uint32_t(transform));
}
void EditorDocument::clearSelection() {
    for (auto &a : placements.editableEntries())
        a.state = 0;
}
int EditorDocument::selectNearest(neon::vec2d p, bool deletion) {
    clearSelection();
    int found = -1;
    float distance = 1000;
    auto &entries = placements.editableEntries();
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].type >= 0) {
            const float d = float((neon::vec2d{entries[i].x, entries[i].y} - p).length());
            if (d < distance) {
                distance = d;
                found = int(i);
            }
        }
    if (found >= 0 && double(distance) <= (deletion ? .04 : .06))
        entries[size_t(found)].state = deletion ? 1 : 2;
    else
        found = -1;
    return found;
}
bool EditorDocument::moveSelected(neon::vec2d delta, TerrainBounds bounds) {
    bool changed = false;
    delta = delta * 1.5;
    for (auto &a : placements.editableEntries())
        if (a.type >= 0 && a.state == 2) {
            const double x =
                std::clamp(a.x + delta.x, -double(bounds.halfWidth), double(bounds.halfWidth));
            const double y =
                std::clamp(a.y + delta.y, -double(bounds.halfHeight), double(bounds.halfHeight));
            changed |= x != a.x || y != a.y;
            a.x = x;
            a.y = y;
        }
    return changed;
}
bool EditorDocument::removeSelected(int state) {
    bool changed = false;
    for (auto &a : placements.editableEntries())
        if (a.type >= 0 && a.state == state) {
            a.type = -1;
            changed = true;
        }
    return changed;
}
void EditorDocument::clear() {
    std::fill(stage_.cells.begin(), stage_.cells.end(), 0);
    placements.clear();
    stage_.seed = 0;
    stage_.highScore = 0;
    stage_.bestTime = 108000;
}
void EditorDocument::shift(int direction, TerrainBounds bounds, neon::vec2d size) {
    const auto before = stage_.cells;
    for (int y = 0; y < 100; ++y)
        for (int x = 0; x < 100; ++x) {
            const int sx = (x + (direction == 0 ? 1 : direction == 1 ? -1 : 0) + 100) % 100;
            const int sy = (y + (direction == 2 ? 1 : direction == 3 ? -1 : 0) + 100) % 100;
            stage_.cells[size_t(y) * 100 + size_t(x)] = before[size_t(sy) * 100 + size_t(sx)];
        }
    const auto delta = neon::vec2d{direction == 0   ? -size.x
                                   : direction == 1 ? size.x
                                                    : 0,
                                   direction == 2   ? -size.y
                                   : direction == 3 ? size.y
                                                    : 0};
    for (auto &a : placements.editableEntries())
        if (a.type >= 0) {
            a.x += delta.x;
            a.y += delta.y;
            if (a.x < -double(bounds.halfWidth))
                a.x += double(bounds.halfWidth + bounds.halfWidth);
            if (a.x > double(bounds.halfWidth))
                a.x -= double(bounds.halfWidth + bounds.halfWidth);
            if (a.y < -double(bounds.halfHeight))
                a.y += double(bounds.halfHeight + bounds.halfHeight);
            if (a.y > double(bounds.halfHeight))
                a.y -= double(bounds.halfHeight + bounds.halfHeight);
        }
}
void EditorDocument::category(int direction, bool objects) {
    if (objects) {
        variants_[object] = variant;
        transforms_[object] = transform;
        object = (object + direction + 10) % 10;
        variant = variants_[object];
        transform = transforms_[object];
    } else {
        styles_[brush] = style;
        inks_[brush] = ink;
        brush = (brush + direction + 12) % 12;
        style = styles_[brush];
        ink = inks_[brush];
        const auto &b = editor_data::Brushes[brush];
        if (b.colours > 0 && !ink)
            ink = int((b.cells[style][1] & 65535) >> 8);
        sampled = false;
    }
}
void EditorDocument::variation(int direction, bool objects, bool secondary) {
    if (objects) {
        const auto &d = editor_data::Objects[object];
        auto &n = secondary ? transform : variant;
        const int count = secondary ? d.transforms + 1 : d.variants;
        n = (n + direction + count) % count;
    } else {
        const auto &b = editor_data::Brushes[brush];
        auto &n = secondary ? ink : style;
        const int count = secondary ? b.colours + 1 : b.count;
        n = (n + direction + count) % count;
        sampled = false;
        if (!secondary && inks_[brush] == 0)
            ink = int((b.cells[style][1] & 65535) >> 8);
    }
}
bool EditorDocument::commit(neon::DarwinRandom &random, std::string &error, bool assignName) {
    auto next = stage_;
    next.highScore = 0;
    next.bestTime = 108000;
    if (!next.seed && assignName) {
        next.seed = int(random.next());
        const auto unused = random.next();
        generatedName = generateOriginalName(random, uint32_t(next.seed),
                                             unused == 1 ? NameMode::Bank : NameMode::Level);
    }
    std::string text;
    if (!serializeEditedStage(next, placements, text, error, &backup_))
        return false;
    stage_ = std::move(next);
    backup_ = stage_.cells;
    cursor_ = (cursor_ + 1) & 7U;
    history_[cursor_] = std::move(text);
    valid_[cursor_] = true;
    return true;
}
bool EditorDocument::canUndo() const {
    return valid_[(cursor_ + 7) & 7U];
}
bool EditorDocument::undo() {
    if (!canUndo())
        return false;
    cursor_ = (cursor_ + 7) & 7U;
    std::string error;
    Stage s;
    if (!readStage(history_[cursor_], s, error))
        return false;
    stage_ = std::move(s);
    backup_ = stage_.cells;
    placements.load(stage_);
    return true;
}
} // namespace goatup2
