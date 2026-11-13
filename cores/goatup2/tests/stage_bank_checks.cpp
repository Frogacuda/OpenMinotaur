#include "stage_bank.h"
#include "bank_storage.h"
#include "editor_document.h"
#include "editor_tables.h"
#include <chrono>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace goatup2;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(std::string(#x) + ":" + std::to_string(__LINE__));            \
    } while (false)
namespace {
struct Hash {
    uint64_t value = 14695981039346656037ULL;
    void word(uint64_t v, int bytes) {
        for (int i = 0; i < bytes; ++i) {
            value ^= uint8_t(v >> (8 * i));
            value *= 1099511628211ULL;
        }
    }
    void real(float f) {
        uint32_t bits;
        std::memcpy(&bits, &f, 4);
        word(bits, 4);
    }
    void real(double d) {
        uint64_t bits;
        std::memcpy(&bits, &d, 8);
        word(bits, 8);
    }
};
std::vector<uint8_t> bytes(const std::filesystem::path &p) {
    std::ifstream in(p, std::ios::binary);
    CHECK(in.good());
    return {std::istreambuf_iterator<char>(in), {}};
}
std::string fragment(const std::string &screen = "0:10000,",
                     const std::string &objects = "1\n1,3,0.109023,-0.109023\n") {
    return "(XCL)100\n(YCL)100\n(DWI)21\n(DHI)40\n(NSE)123\n(LHS)0\n(LBT)0\n(SCR)" + screen +
           "(XSCR)(OBJ)" + objects + "(XEND)";
}
std::vector<uint8_t> encode(const std::string &text) {
    return transformBank({text.begin(), text.end()});
}
void unit() {
    for (int count = 0; count < 25; ++count) {
        std::vector<uint8_t> input(count);
        for (int i = 0; i < count; ++i)
            input[i] = uint8_t(i * 17);
        const auto encoded = transformBank(input);
        CHECK(transformBank(encoded) == input && transformBank(input) == encoded);
        for (int i = count - count % 4; i < count; ++i)
            CHECK(input[i] == encoded[i]);
    }
    Stage stage;
    std::string error;
    CHECK(readStage(fragment(), stage, error));
    uint32_t bits;
    const float x = float(stage.objects[0].x);
    std::memcpy(&bits, &x, 4);
    CHECK(bits == 0x3ddf4773);
    CHECK(stage.objects[0].y == -stage.objects[0].x && stage.bestTime == 108000);
    CHECK(readStage(fragment("5:10000,abc:1,"), stage, error));
    CHECK(stage.expandedCells == 10001 && stage.cells.back() == 5 &&
          stage.backupPrefix == std::vector<uint32_t>{0xabc});
    CHECK(readStage(fragment("ff:2,"), stage, error));
    CHECK(stage.cells[0] == 255 && stage.cells[1] == 255 && stage.cells[2] == 0 &&
          stage.expandedCells == 2);
    for (const auto &invalid :
         {fragment("0:20001,"), fragment("zz:1,"), fragment(std::string(9000, ' ') + "0:1,"),
          fragment("0:-1,"), fragment("0:10000,", "1\n10,0,0.0,0.0\n"),
          fragment("0:10000,", "1\n1,0,nan,0.0\n"), fragment("0:10000,", "2\n1,0,0.0,0.0\n"),
          fragment("0:10000,", "0\n1,0,0.0,0.0\n")}) {
        CHECK(!readStage(invalid, stage, error) && !error.empty());
        CHECK(stage.expandedCells == 2 && stage.cells[0] == 255);
    }
    auto empty = fragment();
    empty.replace(empty.find("(NSE)123"), 8, "(NSE)0");
    CHECK(readStage(empty, stage, error));
    CHECK(stage.seed == 0 && stage.expandedCells == 0 && stage.cells[0] == 0);
    Bank bank;
    bank.gid = 7;
    CHECK(!readBank({}, bank, error) && bank.gid == 7);
    std::string text = "(L123)(HDR)(GID)1(PUR)2(TIM)3(FUR)4(XEND)";
    for (int i = 0; i < 20; ++i)
        text += "(L" + std::to_string(i) + ")" + fragment();
    CHECK(readBank(encode(text + "(END)"), bank, error) && bank.gid == 1 && bank.fur == 4);
    CHECK(validateRuntimeBank(bank, error));
    auto unsafe = bank;
    unsafe.stages[3].declaredRows = 101;
    CHECK(!validateRuntimeBank(unsafe, error));
    unsafe = bank;
    unsafe.stages[3].objects[0].type = 6;
    unsafe.stages[3].objects[0].variant = 10;
    CHECK(!validateRuntimeBank(unsafe, error));
    unsafe = bank;
    unsafe.stages[3].cells[0] = 0x2001;
    CHECK(!validateRuntimeBank(unsafe, error));
    const auto dup = text + "(L0)" + fragment() + "(END)";
    CHECK(!readBank(encode(dup), bank, error) && bank.gid == 1);
    CHECK(!readBank(encode("(L123)(HDR)(GID)1(PUR)2(TIM)3(FUR)4(XEND)(END)"), bank, error));
    PlacementTable table;
    CHECK(table.entries()[0].type == -1);
    CHECK(table.place({0, 0, 1, 2}));
    CHECK(table.place({0, 5, 3, 4}, 2));
    CHECK(table.nextSerial() == 2 && table.entries()[0].x == 3 && table.entries()[0].flags == 517);
    CHECK(table.entries()[1].type == -1);
    table.clear();
    for (int i = 0; i < 12; ++i)
        CHECK(table.place({1, i, double(i), 0}));
    CHECK(table.entries()[0].x == 10 && table.entries()[1].x == 11 && table.entries()[2].x == 2 &&
          table.entries()[10].type == -1);
    table.clear();
    for (int i = 0; i < 99; ++i)
        CHECK(table.place({2, 0, 0, 0}));
    for (int i = 0; i < 99; ++i)
        CHECK(table.place({3, 0, 0, 0}));
    CHECK(table.place({4, 0, 0, 0}) && table.place({4, 0, 0, 0}));
    CHECK(!table.place({5, 0, 0, 0}) && table.nextSerial() == 200);
    CHECK(table.place({2, 0, 5, 0}) && table.entries()[0].x == 5 && table.nextSerial() == 201);
    CHECK(!table.place({10, 0, 0, 0}));
    std::cout << "Codec tails/reset, original decimal bits, bounded/atomic readers, backup "
                 "overflow and placement limits/replacement passed\n";
}
void editorMechanisms() {
    Stage s;
    s.seed = 123;
    s.declaredColumns = s.declaredRows = 100;
    s.displayWidth = 21;
    s.displayHeight = 40;
    s.bestTime = 108000;
    s.objects = {{0, 0, 0, 0}, {9, 0, .2, .2}};
    EditorDocument e;
    e.open(s);
    neon::DarwinRandom random(1);
    std::string error;
    e.brush = 2;
    e.style = 0;
    e.ink = 1;
    CHECK(e.paint(5, 5));
    CHECK(e.stage().cells[505] == 0x3010b);
    CHECK(e.commit(random, error));
    CHECK(e.canUndo());
    CHECK(e.undo() && e.stage().cells[505] == 0);
    e.sample(0, 0);
    e.paint(5, 5);
    CHECK(e.stage().cells[505] == 0); // sampled empty is the eraser
    e.sampled = true;
    e.sampledTile = 43;
    e.paint(50, 20);
    for (int x = 0; x < 100; ++x)
        CHECK((e.stage().cells[2000 + x] & 255) == 43);
    e.sampledTile = 0;
    e.paint(50, 20);
    for (int x = 0; x < 100; ++x)
        CHECK(e.stage().cells[2000 + x] == 0);
    e.object = 0;
    e.variant = 1;
    CHECK(e.place({.3, .4}));
    int goats = 0;
    for (const auto &p : e.placements.entries())
        goats += p.type == 0;
    CHECK(goats == 1);
    CHECK(e.selectNearest({.3, .4}) >= 0);
    CHECK(e.moveSelected({.1, .1}, {1, 1}));
    CHECK(e.selectNearest({.45, .55}, true) >= 0);
    CHECK(e.removeSelected(1));
    e.object = 0;
    e.variant = 0;
    CHECK(e.place({0, 0}));
    CHECK(e.commit(random, error));
    for (int i = 0; i < 10; ++i) {
        e.settings().cells[505] = uint32_t(i + 1);
        CHECK(e.commit(random, error));
    }
    CHECK(e.undo() && e.stage().cells[505] == 9);
    e.settings().cells.assign(10000, 0);
    e.settings().cells[1234] = 7;
    e.shift(0, {1, 1}, {.1, .1});
    CHECK(e.stage().cells[1233] == 7);
    e.shift(1, {1, 1}, {.1, .1});
    CHECK(e.stage().cells[1234] == 7);
    std::string text;
    CHECK(serializeEditedStage(e.stage(), e.placements, text, error));
    Stage read;
    CHECK(readStage(text, read, error));
    CHECK(read.cells == e.stage().cells);
    for (int i = 0; i < 10000; ++i)
        e.settings().cells[size_t(i)] = uint32_t(i % 97);
    const auto previous = e.serialized();
    CHECK(!e.commit(random, error) &&
          e.serialized() == previous); // capacity failure retains last history
    // Sparse campaign ordinals are mapped to physical slots by the source reader.
    std::cout << "Editor brush/water/eraser, placement/group/removal, history, shifts, "
                 "serialization and capacity checks passed\n";
}
void storageRoundtrip(const std::string &assetDirectory) {
    const auto root = std::filesystem::current_path() / "build-gu2-research" /
                      ("save-check-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    BankStorage store(root);
    std::string error;
    for (int slot = 0; slot < 4; ++slot) {
        const auto original =
            bytes(std::filesystem::u8path(assetDirectory) / ("DEFAULT_" + std::to_string(slot)));
        Bank baseline;
        CHECK(readBank(original, baseline, error));
        store.source(slot, original, baseline);
        auto changed = baseline;
        changed.pur = 9000 + slot;
        changed.tim = 12000 + slot;
        changed.fur = 19;
        for (int i = 0; i < 20; ++i) {
            changed.stages[i].highScore = 100 + i;
            changed.stages[i].bestTime = 1000 + i;
        }
        std::vector<uint8_t> encoded;
        CHECK(BankStorage::encodeRecords(original, changed, encoded, error));
        Bank decoded;
        CHECK(readBank(encoded, decoded, error));
        for (int i = 0; i < 20; ++i) {
            const auto &a = baseline.stages[i];
            const auto &b = decoded.stages[i];
            CHECK(a.cells == b.cells && a.backupPrefix == b.backupPrefix && a.seed == b.seed);
            CHECK(a.objects.size() == b.objects.size());
            for (size_t j = 0; j < a.objects.size(); ++j) {
                CHECK(a.objects[j].x == b.objects[j].x && a.objects[j].y == b.objects[j].y);
                CHECK(a.objects[j].type == b.objects[j].type &&
                      a.objects[j].variant == b.objects[j].variant);
            }
            CHECK(b.highScore == 100 + i && b.bestTime == 1000 + i);
        }
        CHECK(store.save(slot, changed, error));
        const auto file = root / ("goatup2-bank-" + std::to_string(slot) + ".dat");
        const auto modified = std::filesystem::last_write_time(file);
        CHECK(store.save(slot, changed, error) &&
              std::filesystem::last_write_time(file) == modified);
        BankStorage reopened(root);
        reopened.source(slot, original, baseline);
        auto restored = baseline;
        CHECK(reopened.load(slot, restored, error));
        CHECK(restored.pur == 9000 + slot && restored.fur == 19 &&
              restored.stages[19].highScore == 119);
        auto mismatch = baseline;
        ++mismatch.stages[0].seed;
        CHECK(!reopened.load(slot, mismatch, error) && mismatch.pur == baseline.pur);
        // Replacement path must also work on Windows where rename won't overwrite.
        changed.pur += 100;
        CHECK(store.save(slot, changed, error));
        CHECK(reopened.load(slot, restored, error) && restored.pur == changed.pur);
        auto backup = file;
        backup += ".bak";
        std::filesystem::copy_file(file, backup, std::filesystem::copy_options::overwrite_existing);
        {
            std::ofstream bad(file, std::ios::binary | std::ios::trunc);
            bad << "truncated";
        }
        const auto before = restored.pur;
        CHECK(!reopened.load(slot, restored, error) && restored.pur == before);
        std::filesystem::remove(file);
        CHECK(reopened.load(slot, restored, error)); // interrupted replace recovery
        CHECK(bytes(std::filesystem::u8path(assetDirectory) /
                    ("DEFAULT_" + std::to_string(slot))) == original);
    }
    CHECK(store.saveTouchMode(1, error));
    int hand = 0;
    BankStorage preferences(root);
    CHECK(preferences.loadTouchMode(hand, error) && hand == 1);
    CHECK(preferences.saveTouchMode(0, error, true));
    bool tilt = false;
    BankStorage newPreferences(root);
    CHECK(newPreferences.loadTouchMode(hand, error, &tilt) && hand == 0 && tilt);
    {
        std::ofstream legacy(root / "goatup2-controls.txt");
        legacy << "GU2-CONTROLS 1 1";
    }
    CHECK(newPreferences.loadTouchMode(hand, error, &tilt) && hand == 1 && !tilt);
    {
        std::ofstream bad(root / "goatup2-controls.txt");
        bad << "GU2-CONTROLS 1 9";
    }
    hand = 0;
    CHECK(!preferences.loadTouchMode(hand, error) && hand == 0);
    // An I/O failure must leave the dirty record retryable.
    const auto blocked = root / "blocked";
    {
        std::ofstream file(blocked);
        file << "block";
    }
    BankStorage retry(blocked);
    const auto original = bytes(std::filesystem::u8path(assetDirectory) / "DEFAULT_0");
    Bank b;
    CHECK(readBank(original, b, error));
    retry.source(0, original, b);
    b.pur = 42;
    CHECK(!retry.save(0, b, error));
    std::filesystem::remove(blocked);
    CHECK(retry.save(0, b, error));
    std::cout << "Encoded bank saves: all 80 geometries/coordinates preserved, records reload, "
                 "replacement/recovery/rejection/retry and touch preference passed\n";
}
void originals(const char *assetDirectory, const char *fixturePath) {
    std::array<Bank, 4> banks;
    std::string error;
    const uint32_t finalStates[4] = {1398078657U, 1129744019U, 193147076U, 1555783949U};
    const int headers[4][4] = {{1298753211, 3155422, 125406, 19},
                               {1049997439, 393123, 2160000, 12},
                               {60171756, 16123, 2160000, 6},
                               {1262655684, 206901, 2160000, 19}};
    for (int i = 0; i < 4; ++i) {
        const auto encoded =
            bytes(std::filesystem::u8path(assetDirectory) / ("DEFAULT_" + std::to_string(i)));
        if (!readBank(encoded, banks[i], error))
            throw std::runtime_error(error);
        CHECK(transformBank(transformBank(encoded)) == encoded);
        CHECK(banks[i].decoderRandomState == finalStates[i]);
        CHECK(banks[i].gid == headers[i][0] && banks[i].pur == headers[i][1] &&
              banks[i].tim == headers[i][2] && banks[i].fur == headers[i][3]);
        CHECK(validateRuntimeBank(banks[i], error));
    }
    std::ifstream fixture(fixturePath);
    CHECK(fixture.good());
    int bank, index, count = 0, total = 0, overflows = 0;
    std::array<std::array<bool, 20>, 4> seen{};
    while (fixture >> bank >> index) {
        CHECK(bank >= 0 && bank < 4 && index >= 0 && index < 20 && !seen[bank][index]);
        seen[bank][index] = true;
        int columns, rows, width, height, seed, score, time;
        std::size_t expanded, objects, active;
        uint64_t cellsHash, objectsHash, placementsHash;
        CHECK(bool(fixture >> columns >> rows >> width >> height >> seed >> score >> time >>
                   expanded >> objects >> active >> cellsHash >> objectsHash >> placementsHash));
        const auto &s = banks[bank].stages[index];
        CHECK(s.declaredColumns == columns && s.declaredRows == rows && s.displayWidth == width &&
              s.displayHeight == height && s.seed == seed && s.highScore == score &&
              s.bestTime == time);
        CHECK(s.expandedCells == expanded && s.objects.size() == objects);
        Hash cells;
        for (auto v : s.cells)
            cells.word(v, 4);
        for (auto v : s.backupPrefix)
            cells.word(v, 4);
        CHECK(cells.value == cellsHash);
        Hash object;
        for (auto &o : s.objects) {
            object.word(uint32_t(o.type), 4);
            object.word(uint32_t(o.variant), 4);
            object.real(float(o.x));
            object.real(float(o.y));
        }
        CHECK(object.value == objectsHash);
        PlacementTable table;
        table.load(s);
        Hash placed;
        std::size_t used = 0;
        for (const auto &p : table.entries()) {
            placed.word(uint32_t(p.type), 4);
            placed.real(p.x);
            placed.real(p.y);
            placed.word(p.flags, 4);
            placed.word(uint32_t(p.serial), 4);
            placed.word(uint32_t(p.state), 4);
            used += p.type >= 0;
        }
        CHECK(used == active && placed.value == placementsHash);
        ++count;
        total += int(objects);
        overflows += !s.backupPrefix.empty();
        std::string edited;
        CHECK(serializeEditedStage(s, table, edited, error));
        Stage reopened;
        CHECK(readStage(edited, reopened, error));
        CHECK(reopened.cells == s.cells && reopened.objects.size() == s.objects.size());
        // The editor deliberately writes six decimals, unlike the source
        // decoder's original higher-precision coordinates. Bound that rounding.
        for (size_t n = 0; n < s.objects.size(); ++n) {
            CHECK(std::abs(reopened.objects[n].x - s.objects[n].x) < .000001 &&
                  std::abs(reopened.objects[n].y - s.objects[n].y) < .000001);
        }
    }
    CHECK(fixture.eof() && count == 80 && total == 5300 && overflows == 3);
    std::cout << "All 80 original levels / 5300 serialized objects: headers, every grid word, "
                 "decimal coordinate bits and all 200 placement slots match independent "
                 "source-derived fixtures; 3 backup-prefix writes retained\n";
}
} // namespace
int main(int argc, char **argv) {
    try {
        unit();
        editorMechanisms();
        CHECK(argc == 1 || argc == 3);
        if (argc == 3) {
            originals(argv[1], argv[2]);
            storageRoundtrip(argv[1]);
        }
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
