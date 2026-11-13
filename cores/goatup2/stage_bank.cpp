#include "stage_bank.h"
#include "editor_tables.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace goatup2 {
namespace {
struct FormatError : std::runtime_error {
    using runtime_error::runtime_error;
};
bool space(char c) {
    return c == ' ' || c == '\r' || c == '\n' || c == '\t';
}
std::string_view trim(std::string_view s) {
    while (!s.empty() && space(s.front()))
        s.remove_prefix(1);
    while (!s.empty() && space(s.back()))
        s.remove_suffix(1);
    return s;
}
std::string_view take(std::string_view &text, char delimiter) {
    const auto at = text.find(delimiter);
    if (at == text.npos)
        throw FormatError("missing delimiter");
    const auto result = text.substr(0, at);
    text.remove_prefix(at + 1);
    return result;
}
template <class T> T number(std::string_view s, int base = 10) {
    s = trim(s);
    if (s.empty())
        throw FormatError("missing integer");
    T result{};
    const auto parsed = std::from_chars(s.data(), s.data() + s.size(), result, base);
    if (parsed.ec != std::errc{} || parsed.ptr != s.data() + s.size())
        throw FormatError("invalid integer");
    return result;
}
int tag(std::string_view text, std::string_view name) {
    const auto at = text.find(name);
    if (at == text.npos)
        throw FormatError("missing " + std::string(name));
    text.remove_prefix(at + name.size());
    return number<int>(text.substr(0, text.find('(')));
}
double coordinate(std::string_view s) {
    // 0xab00 performs float multiply/add/divide per decimal digit. Using
    // strtof/strtod changes the bits of many original object coordinates.
    s = trim(s);
    float sign = 1.f;
    if (!s.empty() && s.front() == '-') {
        sign = -1.f;
        s.remove_prefix(1);
    }
    const auto dot = s.find('.');
    if (dot == s.npos)
        throw FormatError("coordinate requires decimal point");
    const auto integer = s.substr(0, dot);
    const auto whole = integer.empty() ? 0 : number<int>(integer);
    if (whole < 0)
        throw FormatError("invalid coordinate sign");
    float value = float(whole), factor = .1f;
    const auto fraction = s.substr(dot + 1);
    if (fraction.empty())
        throw FormatError("missing fraction");
    for (char digit : fraction) {
        if (digit < '0' || digit > '9')
            throw FormatError("invalid coordinate digit");
        const float term = float(digit - '0') * factor;
        value = value + term;
        factor = factor / 10.f;
    }
    return double(sign * value);
}
Stage parseStage(std::string_view text) {
    // 0x48cbc bounds the working fragment before invoking the two readers.
    const auto end = text.find("(XEND)");
    if (end == text.npos || end > StageCapacity)
        throw FormatError("invalid working stage length");
    Stage stage;
    const auto screenAt = text.find("(SCR)");
    if (screenAt == text.npos)
        throw FormatError("missing (SCR)");
    const auto header = text.substr(0, screenAt);
    stage.declaredColumns = tag(header, "(XCL)");
    stage.declaredRows = tag(header, "(YCL)");
    stage.displayWidth = tag(header, "(DWI)");
    stage.displayHeight = tag(header, "(DHI)");
    stage.seed = tag(header, "(NSE)");
    if (stage.seed != 0) {
        stage.highScore = tag(header, "(LHS)");
        stage.bestTime = tag(header, "(LBT)");
        if (stage.bestTime == 0)
            stage.bestTime = 108000;
    }
    const auto screenEnd = text.find("(XSCR)", screenAt + 5);
    if (screenEnd == text.npos)
        throw FormatError("missing (XSCR)");
    auto screen = trim(text.substr(screenAt + 5, screenEnd - screenAt - 5));
    while (!screen.empty()) {
        const auto value = number<uint32_t>(take(screen, ':'), 16);
        const auto comma = screen.find(',');
        const auto count = number<unsigned>(screen.substr(0, comma));
        if (count > 2 * GridCells - stage.expandedCells)
            throw FormatError("screen exceeds working and backup grids");
        for (unsigned i = 0; i < count; ++i) {
            const auto at = stage.expandedCells++;
            if (at < GridCells)
                stage.cells[at] = value;
            else
                stage.backupPrefix.push_back(value);
        }
        if (comma == screen.npos)
            screen = {};
        else {
            screen.remove_prefix(comma + 1);
            screen = trim(screen);
        }
    }
    const auto objectAt = text.find("(OBJ)", screenEnd + 6);
    if (objectAt == text.npos)
        throw FormatError("missing (OBJ)");
    const auto objectEnd = text.find("(XEND)", objectAt + 5);
    if (objectEnd == text.npos)
        throw FormatError("missing (XEND)");
    auto objects = trim(text.substr(objectAt + 5, objectEnd - objectAt - 5));
    const auto lineEnd = objects.find('\n');
    const auto count = number<unsigned>(objects.substr(0, lineEnd));
    if (count > BankCapacity / 8)
        throw FormatError("excessive object count");
    if (lineEnd == objects.npos)
        objects = {};
    else
        objects.remove_prefix(lineEnd + 1);
    for (unsigned i = 0; i < count; ++i) {
        objects = trim(objects);
        if (objects.empty())
            throw FormatError("missing object record");
        const auto next = objects.find('\n');
        auto line = trim(objects.substr(0, next));
        ObjectRecord o;
        o.type = number<int>(take(line, ','));
        o.variant = number<int>(take(line, ','));
        if (o.type < 0 || o.type >= int(PlacementLimits.size()))
            throw FormatError("unsupported object type");
        o.x = coordinate(take(line, ','));
        o.y = coordinate(line);
        stage.objects.push_back(o);
        if (next == objects.npos)
            objects = {};
        else
            objects.remove_prefix(next + 1);
    }
    if (!trim(objects).empty())
        throw FormatError("extra object records");
    if (stage.seed == 0) {
        std::fill(stage.cells.begin(), stage.cells.end(), 0);
        stage.backupPrefix.clear();
        stage.expandedCells = 0;
    }
    return stage;
}
} // namespace
std::vector<uint8_t> transformBank(const std::vector<uint8_t> &input, uint32_t *finalRandomState) {
    auto output = input;
    int64_t state = 0xf105513bLL;
    for (std::size_t at = 0; at + 4 <= output.size(); at += 4) {
        if (state == 0)
            state = 123459876;
        state = 16807 * (state % 127773) - 2836 * (state / 127773);
        if (state < 0)
            state += 2147483647;
        uint32_t word = 0;
        for (int k = 0; k < 4; ++k)
            word |= uint32_t(output[at + k]) << (8 * k);
        word ^= uint32_t(state);
        for (int k = 0; k < 4; ++k)
            output[at + k] = uint8_t(word >> (8 * k));
    }
    if (finalRandomState)
        *finalRandomState = uint32_t(state);
    return output;
}
bool readStage(std::string_view text, Stage &output, std::string &error) {
    try {
        auto stage = parseStage(text);
        output = std::move(stage);
        error.clear();
        return true;
    } catch (const FormatError &e) {
        error = e.what();
        return false;
    }
}
bool readBank(const std::vector<uint8_t> &encoded, Bank &output, std::string &error) {
    try {
        if (encoded.empty() || encoded.size() > BankCapacity)
            throw FormatError("invalid bank length");
        uint32_t decoderState = 0;
        const auto decoded = transformBank(encoded, &decoderState);
        const std::string_view text(reinterpret_cast<const char *>(decoded.data()), decoded.size());
        const auto end = text.find("(END)");
        if (end == text.npos)
            throw FormatError("missing (END)");
        const auto content = text.substr(0, end);
        Bank bank;
        bank.decoderRandomState = decoderState;
        std::array<bool, 20> seen{};
        bool headerSeen = false;
        std::size_t cursor = 0;
        while (cursor < content.size()) {
            if (content.substr(cursor, 2) != "(L")
                throw FormatError("expected level label");
            const auto labelEnd = content.find(')', cursor + 2);
            if (labelEnd == content.npos)
                throw FormatError("unterminated label");
            const int level = number<int>(content.substr(cursor + 2, labelEnd - cursor - 2));
            const auto sectionEnd = content.find("(XEND)", labelEnd + 1);
            if (sectionEnd == content.npos)
                throw FormatError("missing section end");
            const auto section = content.substr(labelEnd + 1, sectionEnd + 6 - labelEnd - 1);
            if (level == 123) {
                if (headerSeen || section.substr(0, 5) != "(HDR)")
                    throw FormatError("invalid bank header");
                bank.gid = tag(section, "(GID)");
                bank.pur = tag(section, "(PUR)");
                bank.tim = tag(section, "(TIM)");
                bank.fur = tag(section, "(FUR)");
                headerSeen = true;
            } else {
                if (level < 0 || level >= 20 || seen[level])
                    throw FormatError("invalid or duplicate level index");
                try {
                    bank.stages[level] = parseStage(section);
                } catch (const FormatError &e) {
                    throw FormatError("level " + std::to_string(level) + ": " + e.what());
                }
                seen[level] = true;
            }
            cursor = sectionEnd + 6;
            while (cursor < content.size() && space(content[cursor]))
                ++cursor;
        }
        if (!headerSeen || std::find(seen.begin(), seen.end(), false) != seen.end())
            throw FormatError("incomplete bank");
        output = std::move(bank);
        error.clear();
        return true;
    } catch (const FormatError &e) {
        error = e.what();
        return false;
    }
}
bool validateRuntimeBank(const Bank &bank, std::string &error) {
    for (size_t slot = 0; slot < bank.stages.size(); ++slot) {
        const auto &s = bank.stages[slot];
        const auto reject = [&](const char *reason) {
            error = "level " + std::to_string(slot + 1) + ": " + reason;
            return false;
        };
        if (s.declaredColumns < 1 || s.declaredColumns > 100 || s.declaredRows < 1 ||
            s.declaredRows > 100 || s.displayWidth < 1 || s.displayHeight < 1 ||
            s.cells.size() != GridCells)
            return reject("unsupported grid dimensions");
        for (const auto word : s.cells)
            if (((word & 0x3fffU) >> 8) >= 32U)
                return reject("unsupported tile palette index");
        for (const auto &p : s.objects) {
            if (p.type < 0 || p.type >= 10)
                return reject("unsupported object type");
            // Goat/kid use any nonzero variant as inverted; exit ignores flags.
            if (p.type >= 2 && p.type <= 8 &&
                (uint32_t(p.variant) & 255U) >=
                    uint32_t(editor_data::Objects[size_t(p.type)].variants))
                return reject("unsupported object variant");
            if (!std::isfinite(p.x) || !std::isfinite(p.y))
                return reject("nonfinite object coordinate");
        }
    }
    error.clear();
    return true;
}
void PlacementTable::clear() {
    for (auto &p : entries_)
        p = Placement{};
    serial_ = 0;
}
bool PlacementTable::place(const ObjectRecord &object, uint32_t extraFlags) {
    if (object.type < 0 || object.type >= int(PlacementLimits.size()))
        return false;
    int count = 0;
    for (const auto &p : entries_)
        if (p.type == object.type)
            ++count;
    auto selected = entries_.end();
    if (count < PlacementLimits[object.type])
        selected = std::find_if(entries_.begin(), entries_.end(),
                                [](const Placement &p) { return p.type < 0; });
    else
        for (auto i = entries_.begin(); i != entries_.end(); ++i)
            if (i->type == object.type &&
                (selected == entries_.end() || i->serial < selected->serial))
                selected = i;
    if (selected == entries_.end() || serial_ == std::numeric_limits<int>::max())
        return false;
    *selected = {object.type, object.x, object.y, uint32_t(object.variant) | (extraFlags << 8),
                 serial_++,   0};
    return true;
}
void PlacementTable::load(const Stage &stage) {
    clear();
    for (const auto &object : stage.objects)
        place(object);
}
} // namespace goatup2
