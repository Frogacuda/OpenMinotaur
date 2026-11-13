#include "platform.h"
#include "original_tables.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace goatup {
void Platform::reset() {
    active = visible = smashable = false;
    y = -12345.f;
    special = -1;
    offscreenTicks = 0;
    // Reset retains tiles, belts, centres and the upper ten eligibility entries.
    // Summit layout preserves eligibility, including its effect on RNG draws.
    std::fill_n(spawnable.begin(), 10, true);
}
void Platform::earlyLayout(bool returning) {
    // Zones 0/1 share these original four patterns, with a reversed surface
    // and different primary tile in zone 1 (generate 0x5c6f0).
    if (level < 1 || ((level >> 6) % 9) > 2)
        throw std::logic_error("Early layout outside its source zones");
    theme = (level >> 6) % 9;
    smashable = false;
    special = -55555;
    const int floor = theme == 0 ? 4 : theme == 1 ? 14 : 16;
    displacement = 0;
    thicknessScale = 1;
    belt.fill(0);
    const int phase = (level >> 4) & 3;
    const auto put = [&](int at, int value) { tiles.at(size_t(at % 40)) = value; };
    switch (phase) {
    case 0:
        tiles.fill(floor);
        if (level > 1) {
            if (!(level & 2))
                for (int n = 0; n < 5; ++n) {
                    const int at = (level * 72 + n) % 40;
                    put(at, 0);
                    if (theme >= 2)
                        put(39 - at, 0);
                }
            else {
                int start = (level & 1) ? 34 : 5;
                for (int n = 0; n < 5 + 2 * int(returning); ++n) {
                    put(start + n, 0);
                    put(start + n + 20, 0);
                }
            }
        }
        break;
    case 1:
        tiles.fill(floor);
        for (int n = 0; n < 7 + int(returning); ++n) {
            put(n + level * 16, 0);
            put(n + level * 2, 0);
        }
        break;
    case 2:
        tiles.fill(0);
        for (int n = 0; n < 12; ++n) {
            put(level * 8 + n, floor);
            put(level * 4 + n, floor);
        }
        break;
    case 3:
        tiles.fill(0);
        for (int n = 0; n < 14; ++n)
            put(level * 16 + n, floor);
        break;
    }
    if (theme & 1)
        std::reverse(tiles.begin(), tiles.end());
    if (theme > 1 && (level & 1))
        std::reverse(tiles.begin(), tiles.end());
    // In-place single-cell correction, not a cyclic/morphological operation:
    // endpoints 0/39 are deliberately not reconsidered by this source loop.
    for (int left = 0; left < 38; ++left) {
        if (tiles[size_t(left + 1)] == 0) {
            if (tiles[size_t(left)] != 0 && tiles[size_t(left + 2)] != 0)
                tiles[size_t(left + 1)] = floor;
        } else if (tiles[size_t(left)] == 0 && tiles[size_t(left + 2)] == 0)
            tiles[size_t(left + 1)] = 0;
    }
    for (int n = 0; n < 20; ++n)
        spawnable[size_t(n)] = tiles[size_t(n * 2)] == floor && tiles[size_t(n * 2 + 1)] == floor;
}
void Platform::layout(bool returning, neon::DarwinRandom *random,
                      const std::function<void(int, float, int)> &endCap) {
    if (level < 1)
        throw std::logic_error("GoatUp platform level must be positive");
    theme = (level >> 6) % 9;
    if (theme < 3) {
        earlyLayout(returning);
        return;
    }
    constexpr int floors[] = {4, 14, 16, 98, 177, 182, 188, 197, 204};
    const int floor = floors[theme], phase = (level >> 4) & 3, ret = int(returning);
    smashable = false;
    special = -55555;
    displacement = 0;
    thicknessScale = 1;
    belt.fill(0);
    const auto put = [&](int at, int tile) { tiles.at(size_t(at % 40)) = tile; };
    const auto pairs = [&] {
        // Later-zone source loops advance one tile but write n>>1, replacing
        // each even-pair result with the following odd-pair result. The final
        // read aliases the low word of retained centre[0].x, not tile zero.
        // Model that original overrun explicitly without out-of-bounds C++.
        uint64_t centreBits = 0;
        std::memcpy(&centreBits, &centres[0].x, sizeof centreBits);
        for (int n = 0; n < 20; ++n) {
            const bool next = n < 19 ? tiles[size_t(n * 2 + 2)] == floor
                                     : uint32_t(centreBits) == uint32_t(floor);
            spawnable[size_t(n)] = tiles[size_t(n * 2 + 1)] == floor && next;
        }
    };
    if (theme == 3) {
        if (!random)
            throw std::logic_error("GoatUp bridge terrain needs source RNG");
        for (auto &tile : tiles)
            tile = original::GroundBits[size_t(random->next() & 3)];
        const int count = ret + (level & 7), start = (17 - count) & ~1;
        tiles[size_t(start)] = tiles[size_t(start + 1)] = 101;
        for (int n = 0; n < count + 1; ++n)
            tiles[size_t(start + 2 + n * 2)] = tiles[size_t(start + 3 + n * 2)] = 100;
        const int end = start + 2 + (count + 1) * 2;
        tiles[size_t(end)] = tiles[size_t(end + 1)] = 105;
        spawnable.fill(true);
        for (int n = 0; n < 20; ++n)
            if (tiles[size_t(n * 2)] == 100 && tiles[size_t(n * 2 + 1)] == 100)
                spawnable[size_t(n)] = false;
    } else if (theme == 4 || theme == 6 || theme == 7) {
        tiles.fill(floor);
        int start = theme == 4   ? (phase * 10 + 20) % 40
                    : theme == 6 ? (level * 3 + 10 + phase * 5) % 40
                                 : (level * 17 + 32 + phase * 16) % 40;
        if (level & 1)
            start = 39 - start;
        const int count = (phase + ret) * 2 + (theme == 6 ? 3 : 5);
        for (int n = 0; n < count; ++n) {
            put(start + n, 0);
            put(start + n + 20, 0);
        }
        pairs();
        if (theme == 4 && (phase & 1) && (level & 1))
            std::reverse(tiles.begin(), tiles.end());
        // City reversal is phase-odd AND level-odd, after eligibility scanning.
        if (theme == 6) {
            displacement = .03f;
            smashable = true;
        }
        if (theme == 7)
            special = 1;
    } else if (theme == 5) {
        tiles.fill(0);
        const int gap = ret + 2 + phase;
        float speed = float(double(float(phase)) * .0005 + .003);
        if (returning)
            speed = float(double(speed) * .45);
        if (!(level & 1)) {
            for (int n = gap; n < 20 - gap; ++n) {
                if (phase < 2) {
                    tiles[size_t(n)] = tiles[size_t(39 - n)] = floor;
                    belt[size_t(n)] = (level & 2) ? speed : -speed;
                    belt[size_t(39 - n)] = -belt[size_t(n)];
                } else if (level & 2) {
                    tiles[size_t(n)] = floor;
                    belt[size_t(n)] = speed;
                } else {
                    tiles[size_t(39 - n)] = floor;
                    belt[size_t(39 - n)] = speed;
                }
            }
        } else
            for (int n = gap * 2; n < 39 - gap * 2; ++n) {
                tiles[size_t(n)] = floor;
                belt[size_t(n)] = (level & 2) ? -speed : speed;
            }
        if (!endCap)
            throw std::logic_error("GoatUp conveyor end caps not connected");
        for (int n = 1; n < 40; ++n) {
            if (tiles[size_t(n - 1)] == 0 && tiles[size_t(n)] == floor)
                endCap(183, float(double(float(float(n + 1) / 40.f - .5f)) - .02), n);
            if (tiles[size_t(n - 1)] == floor && tiles[size_t(n)] == 0)
                endCap(184, float(double(float(float(n - 1) / 40.f - .5f)) + .02), n - 1);
        }
        pairs();
    } else {
        tiles.fill(returning || (level & 7) ? 0 : floor);
    }
}
int Platform::selectBirthSlot(const std::function<double(double, double)> &random) const {
    const int count = int(std::count(spawnable.begin(), spawnable.end(), true));
    if (count < 1)
        return -1; // No random draw on the empty path.
    int ordinal = int(random(1., double(float(count))));
    for (int n = 0; n < 20; ++n)
        if (spawnable[size_t(n)] && --ordinal < 1)
            return n * 2;
    return -1;
}
TowerSurface::TowerSurface() {
    reset();
}
void TowerSurface::reset(float initialY) {
    quads.clear();
    scroll = -.2f;
    angle = 0;
    tileStep = .05f;
    fade = 0;
    smashableVisible = false;
    for (auto &p : platforms) {
        p.reset();
        p.y = initialY;
    }
}
Platform *TowerSurface::find(int level) {
    for (auto &p : platforms)
        if (p.active && p.level == level)
            return &p;
    return nullptr;
}
double TowerSurface::brightness() const {
    float light = fade > -.1 ? .2f : 1.f;
    if (fade < .1)
        light = float((.1 - double(fade)) * 8. + .2);
    return double(light);
}
void TowerSurface::column(int faces, Vec2 pos, Vec2 size) {
    // platformColumn 0x55fc4: binary32 angle boundaries, binary64 rotation.
    if (faces < 1)
        throw std::logic_error("Invalid column face count");
    Vec2 edge{-size.x, 0};
    edge.rotateBy(double(float(pos.x * 3.141592654)));
    const double low = pos.y, high = pos.y + size.y;
    const double step = double(float(3.141592654 / double(float(faces))));
    double priorX = edge.x, priorDepth = edge.y;
    edge.rotateBy(step);
    for (int n = 0; n < faces * 2; ++n) {
        const double nextX = edge.x;
        if (edge.y > 0 && float(priorDepth) > 0) {
            SurfaceQuad q;
            q.frame = 5;
            q.alpha = (edge.y * 1.6 + .2) * brightness();
            q.corners = {Vec2{priorX, low}, Vec2{priorX, high}, Vec2{nextX, high},
                         Vec2{nextX, low}};
            quads.push_back(q);
        }
        priorDepth = edge.y;
        priorX = nextX;
        edge.rotateBy(step);
    }
}
void TowerSurface::line(Platform &p) {
    // platformLine 0x56204. Source sprite custom corners are TL, BL, BR, TR.
    const float half = float(float(p.thickness * .5f) * p.thicknessScale);
    const float bottom = float(float(p.y + half) + p.displacement);
    const float top = float(float(p.y - half) + p.displacement);
    Vec2 edge{-.5, 0};
    edge.rotateBy(double(float(-double(angle) * 3.141592654)));
    double left = edge.x;
    edge.rotateBy(.07853981852531433);
    double right = edge.x;
    bool priorSolid = p.tiles[0] > 0;
    const double light = dim ? double(.4f) : brightness();
    int frame = 4;
    for (int n = 0; n < 40; ++n) {
        p.centres[size_t(n)] = {edge.x, double(p.y)};
        double farRight = right;
        if (edge.y <= 0) {
            edge.rotateBy(.07853981852531433);
            farRight = edge.x;
            left = right;
        } else {
            const int tile = p.tiles[size_t(n)];
            if (tile == 0) {
                if (priorSolid && left < left * .6) {
                    SurfaceQuad q;
                    q.frame = frame;
                    q.alpha = (edge.y * .2 + .1) * light;
                    q.corners = {Vec2{left, top}, Vec2{left, bottom}, Vec2{left * .6, bottom},
                                 Vec2{left * .6, top}};
                    quads.push_back(q);
                }
                priorSolid = false;
                edge.rotateBy(.07853981852531433);
                farRight = edge.x;
                left = right;
            } else {
                // Wide bridge sprites occupy two cells. Their odd half advances
                // rotation without drawing; other tiles retain the single span.
                const bool wide =
                    tile == 100 || tile == 101 || tile == 105 || tile == 106 || tile == 107;
                if (wide && (n & 1)) {
                    edge.rotateBy(.07853981852531433);
                    priorSolid = true;
                    farRight = right;
                } else {
                    if (wide) {
                        auto expanded = edge;
                        expanded.rotateBy(.07853981852531433);
                        right = expanded.x;
                    }
                    frame = tile + 2;
                    if (!priorSolid && right * .6 < right) {
                        SurfaceQuad side;
                        side.frame = tile + 2;
                        side.alpha = (edge.y * .2 + .1) * light;
                        side.corners = {Vec2{right * .6, top}, Vec2{right * .6, bottom},
                                        Vec2{right, bottom}, Vec2{right, top}};
                        quads.push_back(side);
                    }
                    if (priorSolid) {
                        SurfaceQuad q;
                        q.frame = frame;
                        q.alpha = (edge.y * 1.8 + .05) * light;
                        q.corners = {Vec2{left, top}, Vec2{left, bottom}, Vec2{right, bottom},
                                     Vec2{right, top}};
                        quads.push_back(q);
                    }
                    edge.rotateBy(.07853981852531433);
                    priorSolid = true;
                    farRight = edge.x;
                    left = right;
                }
            }
        }
        right = farRight;
    }
}
void TowerSurface::prepare() {
    quads.clear();
    // fmod/floor would change negative-scroll behavior: the original truncates.
    // Column rows are .2 high, matching the platforms' .05 rows at 10 per scroll unit.
    const float columnUnits = scroll * 2.5f;
    float cy = float(double(float((columnUnits - float(int(columnUnits))) * .2f)) + .6);
    while (double(cy) > -.9) {
        column(10, {-double(angle), double(cy)}, {.3, double(.2f)});
        cy = float(cy - .2f);
    }
    for (auto &p : platforms)
        p.visible = false;
    const float units = float(scroll * 10.f);
    int cell = int(units), previousLevel = cell >> 2;
    float y = float(double(float((units - float(cell)) * tileStep)) + .7);
    smashableVisible = false;
    while (double(y) > -.6) {
        y = float(y - tileStep);
        if (double(y) <= -.6)
            break;
        ++cell;
        const int level = cell >> 2;
        if (level == previousLevel || level <= 0)
            continue;
        Platform *p = find(level);
        if (!p) {
            for (auto &candidate : platforms)
                if (!candidate.active) {
                    p = &candidate;
                    break;
                }
            if (!p)
                throw std::logic_error("Original platform pool exhausted");
            if (!generate)
                throw std::logic_error("Platform generation is not connected");
            p->active = p->visible = true;
            p->level = level;
            p->y = 0;
            generate(*p);
        }
        p->visible = true;
        p->offscreenTicks = 0;
        p->y = y;
        p->thickness = .05f;
        smashableVisible |= p->smashable;
        line(*p);
        previousLevel = level;
    }
    for (auto &p : platforms)
        if (!p.visible) {
            ++p.offscreenTicks;
            if (p.offscreenTicks == 1 && firstOffscreen)
                firstOffscreen(p);
            if (p.offscreenTicks > 320) {
                p.active = false;
                p.y = -.49f;
            }
        }
}
} // namespace goatup
