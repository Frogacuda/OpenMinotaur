// level_data_test.cpp - round-trip check: every reconstructed level record must rebuild the
// original 0xe9c bytes (FNV-1a 64 per record in fixtures/level-records-ios.txt, data
// extraction from the iOS executable), and named fields must agree with the bytes.
#include "level_data.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
using namespace deflex::level;

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s level-records-ios.txt\n", argv[0]);
        return 2;
    }
    std::ifstream in(argv[1]);
    std::vector<uint64_t> expected;
    std::string text;
    while (std::getline(in, text)) {
        if (text.empty() || text[0] == '#')
            continue;
        std::istringstream s(text);
        int i;
        std::string name, h;
        s >> i >> name >> h;
        expected.push_back(std::stoull(h, nullptr, 16));
    }
    int failed = 0;
    if (expected.size() != size_t(kLevelCount)) {
        std::fprintf(stderr, "expected %d rows\n", kLevelCount);
        return 1;
    }
    for (int i = 0; i < kLevelCount; i++) {
        const auto bytes = recordBytes(kLevelRecords[i]);
        uint64_t h = 0xcbf29ce484222325ull;
        for (uint8_t b : bytes) {
            h ^= b;
            h *= 0x100000001b3ull;
        }
        const LevelRecordData &l = kLevelRecords[i];
        const bool sane =
            l.columns > 0 && l.columns < kMapStride && l.rows > 0 && l.rows < kMapRows;
        if (h != expected[size_t(i)] || !sane) {
            failed++;
            std::fprintf(stderr, "level %d (%s) mismatch\n", i, l.name());
        }
    }
    std::printf("level records: %d checked, %d mismatched\n", kLevelCount, failed);
    return failed ? 1 : 0;
}
