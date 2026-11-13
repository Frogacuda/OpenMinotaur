// Compare playfield coordinate conversions with original-execution fixtures;
// include explicit float rounding, wrapping and out-of-grid sentinels.
#include "cell_grid_addressing.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
using namespace deflex;

namespace {
struct Rng {
    uint64_t s;
    uint64_t next() {
        s += 0x9e3779b97f4a7c15ull;
        uint64_t z = s;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    }
};
struct Fnv {
    uint64_t h = 0xcbf29ce484222325ull;
    void bytes(const void *p, size_t n) {
        const auto *b = static_cast<const uint8_t *>(p);
        for (size_t i = 0; i < n; i++) {
            h ^= b[i];
            h *= 0x100000001b3ull;
        }
    }
    void i32(int32_t v) {
        uint8_t b[4];
        for (int i = 0; i < 4; i++)
            b[i] = uint8_t(uint32_t(v) >> (8 * i));
        bytes(b, 4);
    }
    void f64(double d) {
        uint64_t u;
        std::memcpy(&u, &d, 8);
        uint8_t b[8];
        for (int i = 0; i < 8; i++)
            b[i] = uint8_t(u >> (8 * i));
        bytes(b, 8);
    }
};

struct Result {
    uint64_t pos, coords, peek, poke;
    int points;
};

Result run(int cols, int rows, int co, int ro) {
    CellGrid g;
    g.setSize(cols, rows);
    g.colOffset = co;
    g.rowOffset = ro;
    Rng rng{uint64_t(cols) * 1000003u + uint64_t(rows) * 7919u + uint64_t(co) * 131u +
            uint64_t(ro)};
    for (int y = 0; y <= rows; y++)
        for (int x = 0; x <= cols; x++) {
            const uint64_t z = rng.next();
            static const int special[3] = {0x20, 9999, -1};
            g.cell(y, x).ch = z % 5 == 0 ? special[z % 3] : int((z >> 33) % 0x100) - 0x80;
        }
    Result r{};
    Fnv h;
    auto putPos = [&](int x, int y) {
        vec2d p = g.getPos(x, y);
        h.f64(p.x);
        h.f64(p.y);
    };
    for (int x = 0; x <= cols; x++)
        putPos(x, x % (rows + 1));
    for (int y = 0; y <= rows; y++)
        putPos(y % (cols + 1), y);
    r.pos = h.h;

    h = Fnv{};
    int n = 0;
    auto query = [&](double px, double py) {
        const vec2d p{px, py};
        CellCoords c = g.getCellCoords(p);
        h.i32(c.x);
        h.i32(c.y);
        c = g.getCellCoords2(p);
        h.i32(c.x);
        h.i32(c.y);
        const vec2d q = g.getPosInCell(p);
        h.f64(q.x);
        h.f64(q.y);
        n++;
    };
    const int kmax = (cols > rows ? cols : rows) + 2;
    const double eps = 1.0 / 1099511627776.0; // 2^-40
    for (int k = 0; k < kmax; k++) {
        const double bx = double(k) / double(cols) - 0.5, by = double(k) / double(rows) - 0.5;
        const double d[3] = {0.0, eps, -eps};
        for (double e : d)
            query(bx + e, by - e);
    }
    for (int i = 0; i < 200; i++) {
        const uint64_t z1 = rng.next(), z2 = rng.next();
        query(double(z1 >> 11) * (1.0 / 9007199254740992.0) * 1.4 - 0.7,
              double(z2 >> 11) * (1.0 / 9007199254740992.0) * 1.4 - 0.7);
    }
    r.coords = h.h;
    r.points = n;

    h = Fnv{};
    const int sx = cols / 24 > 1 ? cols / 24 : 1, sy = rows / 24 > 1 ? rows / 24 : 1;
    auto peekRow = [&](int y) {
        for (int x = -2; x < cols + 3; x += sx)
            h.i32(g.peek(x, y));
        h.i32(g.peek(cols, y));
        h.i32(g.peek(cols + 1, y));
    };
    for (int y = -2; y < rows + 3; y += sy)
        peekRow(y);
    peekRow(rows);
    peekRow(rows + 1);
    r.peek = h.h;

    for (int i = 0; i < 64; i++) {
        if (i % 16 == 0) {
            const uint64_t z = rng.next();
            g.labelMode = ((i / 16) & 1) != 0;
            g.curLabel = int(z % 1000);
        }
        const uint64_t z = rng.next();
        const int x = int(z % uint64_t(cols + 1)), y = int((z >> 20) % uint64_t(rows + 1));
        int c = int(z >> 56);
        if (((z >> 40) & 7) == 0)
            c = 0x20;
        g.poke(x, y, char(static_cast<signed char>(uint8_t(c))));
    }
    h = Fnv{};
    for (int y = 0; y <= rows; y++)
        for (int x = 0; x <= cols; x++) {
            const GridCell &c = g.cell(y, x);
            h.i32(c.ch);
            h.i32(c.fgOn);
            h.i32(c.label);
        }
    r.poke = h.h;
    return r;
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s cell-addressing-ios.txt\n", argv[0]);
        return 2;
    }
    std::ifstream in(argv[1]);
    if (!in) {
        std::fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }
    std::string line;
    int checked = 0, failed = 0;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream s(line);
        int cols, rows, co, ro, pts;
        std::string a, b, c, d;
        s >> cols >> rows >> co >> ro >> a >> b >> c >> d >> pts;
        const Result r = run(cols, rows, co, ro);
        const uint64_t want[4] = {std::stoull(a, nullptr, 16), std::stoull(b, nullptr, 16),
                                  std::stoull(c, nullptr, 16), std::stoull(d, nullptr, 16)};
        const uint64_t got[4] = {r.pos, r.coords, r.peek, r.poke};
        static const char *names[4] = {"getPos", "coords", "peek", "poke"};
        checked++;
        bool ok = r.points == pts;
        for (int i = 0; i < 4; i++)
            if (got[i] != want[i]) {
                ok = false;
                std::fprintf(stderr, "%dx%d off %d,%d: %s mismatch\n", cols, rows, co, ro,
                             names[i]);
            }
        if (!ok)
            failed++;
    }
    std::printf("cell addressing: %d configurations checked, %d mismatched\n", checked, failed);
    return failed || checked == 0 ? 1 : 0;
}
