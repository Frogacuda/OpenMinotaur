// Minos map entity families; state and interfaces are in enemies.h.
#include "neon/score_notices.h"
#include "enemies.h"
#include <cmath>
#include <cstring>

namespace minos {
namespace {
constexpr double kPi = 3.141592654; // oxRNG(-pi, pi) bound from the original binary64 literal pair
inline double bits(uint64_t u) {
    double d;
    std::memcpy(&d, &u, 8);
    return d;
}
inline float bits32(uint32_t u) {
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}
inline int32_t toInt(double v) {
    if (std::isnan(v))
        return 0;
    if (v >= 2147483647.0)
        return INT32_MAX;
    if (v <= -2147483648.0)
        return INT32_MIN;
    return int32_t(v);
}
inline Vec2 rotated(Vec2 v, double a) {
    neon::vec2d r{v.x, v.y};
    r.rotateBy(a);
    return {r.x, r.y};
}

// The visible tile under a position as shot / _enemyBull / faller / thrustParticle index it:
// column from float(x), line from float(y), bounds 0..columns / 0..rows; -1 outside.
int32_t visibleTileAt(const cavern::StripMap &m, Vec2 p) {
    const int col = toInt((double(float(p.x)) + 0.5) * double(float(m.columns)) - m.fraction.x);
    if (col < 0 || col > m.columns)
        return -1;
    const int row = toInt((double(float(p.y)) + 0.5) * double(float(m.rows)) - m.fraction.y);
    if (row < 0 || row > m.rows)
        return -1;
    const long i = long(row) * (m.columns + 1) + col;
    return i >= 0 && i < long(m.tiles.size()) ? m.tiles[size_t(i)] : 0;
}

// The +0x1037fc word: the per-cell rand() table newLevel fills (map row + 2048).
uint32_t randomCellWord(const cavern::StripMap &m, int row, int col) {
    const long i = long(row + 0x800) * cavern::StripMap::kMapColumns + col;
    return i >= 0 && i < long(m.cells.size()) ? m.cells[size_t(i)] : 0;
}

} // namespace
void addExplo(PlayState &g, Vec2 pos, float from, float to, float rate) {
    auto *e = new Explo;
    e->from = from;
    e->to = to;
    e->t = 0;
    e->rate = rate;
    e->spriteIndex = 4;
    e->pos = pos;
    e->rot = float(g.rnd(-kPi, kPi));
    e->baseScale = {1.0, 1.0};
    e->g = &g;
    g.addEntity(g.debris, e);
}

// bspark as xentity::hitBy / _enemyBull::hitBy create them (vel y, vel x, age rate, rot).
void addSpark(PlayState &g, Vec2 pos, double ylo, double yhi, double xlo, double xhi, double rlo,
              double rhi) {
    auto *b = new Bspark;
    b->vel.y = g.rnd(ylo, yhi);
    b->vel.x = g.rnd(xlo, xhi);
    b->ageRate = float(g.rnd(rlo, rhi));
    b->exploChance = 0;
    b->pos = pos;
    b->rot = float(g.rnd(-kPi, kPi));
    b->age = 0;
    b->spriteIndex = 3;
    b->colourIndex = 1;
    b->maxScale = 1.0f;
    b->g = &g;
    b->size = 0.02f;
    g.addEntity(g.debris, b);
}

// Map cell indexing as goat/motherSheep/wavingMinotaur write it: column from float(x), line
// from float(y'), where the caller forms y' in binary64; -1 when outside 0..columns/0..rows.
int cellColumn(const cavern::StripMap &m, double x) {
    const int c = toInt((double(float(x)) + 0.5) * double(float(m.columns)) - m.fraction.x);
    return c < 0 || c > m.columns ? -1 : c;
}
int cellRow(const cavern::StripMap &m, double y) {
    const int r = toInt((double(float(y)) + 0.5) * double(float(m.rows)) - m.fraction.y);
    return r < 0 || r > m.rows ? -1 : r;
}
int tileByte(const cavern::StripMap &m, int row, int col) { // ldrb of the tile word
    const long i = long(row) * (m.columns + 1) + col;
    return i >= 0 && i < long(m.tiles.size()) ? (m.tiles[size_t(i)] & 0xff) : 0;
}
// The ship's (+0x2d4) position minus this one.
Vec2 toShip(const PlayState &g, Vec2 p) {
    return {g.shipList->pos.x - p.x, g.shipList->pos.y - p.y};
}
namespace {} // namespace
// rcols (data 0x231ba0): word 0 = 11, then the colours goat::init picks from.
const uint32_t kRcols[12] = {0xb,         0xff0000ffu, 0xff8000ffu, 0xffff00ffu,
                             0x80ff00ffu, 0x00ff00ffu, 0x00ff80ffu, 0x00ffffffu,
                             0x0080ffffu, 0x0000ffffu, 0x000080ffu, 0x008040ffu};
namespace {} // namespace

// ------------------------------------------------------------------ shared
void Enemy::mapRun() {
    MapEntityWorld w;
    w.scrollDelta = g->scrollDelta;
    w.clearAll = g->clearAll;
    w.map = g->map;
    w.rand = g->rand;
    if (auto x = centityRun(*this, w))
        g->addMapExplosion(*x);
}

void PlayState::addMapExplosion(const ExplosionBirth &birth) {
    auto *e = new Explo;
    e->from = birth.fromScale;
    e->to = birth.toScale;
    e->t = birth.progress;
    e->rate = birth.scaleRate;
    e->spriteIndex = birth.spriteIndex;
    e->pos = birth.pos;
    e->rot = birth.rot;
    e->baseScale = birth.baseScale;
    e->g = this;
    addEntity(debris, e);
}

int32_t Enemy::visibleTile() const {
    return visibleTileAt(*g->map, pos);
}

void Enemy::hitBy(Entity *a) { // xentity::hitBy 0x6e9e8
    PlayState &G = *g;
    if (a->type == 2) {
        auto *ship = static_cast<Ship *>(a);
        G.shield = G.shield - float(damage) * ship->hitDamage;
        if (G.shield < 0.0f)
            G.shield = 0;
        G.cleanRun = false;
        G.sample(0x20);
    }
    if (a->dead)
        return;
    a->dead = true;
    int id = sound;
    if (id < 0) {
        static const int32_t kBooms[7] = {14, 16, 17, 18, 19, 20, 21}; // _booms (data)
        id = kBooms[(G.libcRand() & 0x7fff) % 7];
    }
    G.sample(id);
    addExplo(G, pos, 2.0f, 8.0f, 0.016f); // 0x40000000, 0x41000000, 0x3c83126f
    G.score(score, pos, {1.0, 1.0});
    for (int i = 0; i < 10; i++)
        addSpark(G, pos, -0.02, -0.005, -0.01, 0.01, 0.001, 0.006);
    dead = true;
}

void PlayState::score(int points, Vec2 p, Vec2 sc) { // theGame::_score 0x5a318
    // mla at 0x5a3ac: points * (1 + minotaurs saved +0x48c + shipKind +0x9e4); the popup
    // shows the multiplied value (the decompilation drops this).
    points = points * (minosSaved + shipKind) + points;
    scoreTotal += points;
    auto *f = new FloatingScore;
    f->pos = p;
    f->vel.x = p.x / -70.0;
    f->vel.y = p.y < 0.0 ? 0.003 : -0.003; // 0x3f689374bc6a7efa
    f->spriteBase = 0x12;
    f->first = 15;
    f->alpha = 1.0f;
    f->g = this;
    f->fade = -0.01f; // 0xbc23d70a
    for (int v = points; v > 0; v /= 10)
        f->digits[f->first--] = v % 10;
    if (f->first == 15)
        f->first = 14;
    f->scale = sc;
    addEntity(debris, f);
}

// ------------------------------------------------------------------ turret, bullet
void Turret::init() { // 0x55888
    spriteIndex = 0x31;
    size = 0.04f;
    aim = rotated({0.0, bits(0xbf70624dcccccccdull)}, double(rot));
    pos = {pos.x + aim.x * 4.0, pos.y + aim.y * 4.0};
    score = 20;
    damage = 200;
    sound = 0x1f;
    hidden = false;
    if ((randomCellWord(*g->map, row, column) & 0x20) && !g->flag203808) {
        hidden = true;
        drawDelay = 1;
    }
}

void Turret::run() { // 0x6a024
    if (hidden) {
        mapRun();
        drawDelay = 1;
        return;
    }
    mapRun();
    if (g->flash && g->flash->watched[2]) {
        g->flash->seen[2] = 1;
        auto *b = new EnemyBull;
        b->pos = pos;
        b->noDrift = true;
        b->g = g;
        b->spriteIndex = 3;
        b->size = 0.02f;
        b->vel = aim;
        b->fall = 0;
        g->addEntity(g->bounceList, b);
    }
}

void EnemyBull::run() { // 0x7035c
    vel.y = vel.y + double(fall);
    if (g->clearAll != 0)
        dead = true; // +0x9fc (instruction 0x703c0)
    pos = {pos.x + vel.x, pos.y + vel.y};
    if (!noDrift)
        pos.x = pos.x + double(g->shotDrift) * 0.03;
    pos = {pos.x - g->scrollDelta.x, pos.y - g->scrollDelta.y};
    if (pos.y > 0.6 || pos.y < -0.7)
        dead = true;
    col = g->colours[4];
    if (visibleTileAt(*g->map, pos) > 0) {
        addExplo(*g, pos, 0.5f, 2.0f, 0.06f);
        dead = true;
    }
}

void EnemyBull::hitBy(Entity *a) { // 0x5fca8
    PlayState &G = *g;
    if (a->type == 2) {
        auto *ship = static_cast<Ship *>(a);
        G.shield = G.shield - ship->hitDamage * 400.0f; // vmls.f32 (binary32)
        if (G.shield < 0.0f)
            G.shield = 0;
        G.cleanRun = false;
        G.sample(0x20);
    }
    if (a->dead)
        return;
    a->dead = true;
    G.sample(0x34);
    for (int i = 0; i < 3; i++)
        addSpark(G, pos, -0.002, 0.0, -0.002, 0.002, 0.001, 0.006);
    dead = true;
}

// ------------------------------------------------------------------ goal, 1-up
void CavernGoal::run() { // 0x69f1c
    mapRun();
    const int32_t f = g->frameCount;
    rot = float(double(std::sin(float(double(float(f & 31) * 0.03125f) * 6.2831853072))) *
                0.3); // vmul.f32, then binary64 2pi
    const float k = float(int32_t(uint32_t(f) & 0x7fff) % 49);
    size = float(double(std::sin(float(double(k / 49.0f) * 6.2831853072))) * 0.01 + 0.05);
}

void CavernGoal::hitBy(Entity *a) { // 0x758d4
    PlayState &G = *g;
    G.setMessage(1, 300, 0xe, 2, kGoMsg[2]); // "Now bring" / cavern name / "back out!"
    G.setMessage(2, 0x136, 0x10, 4, G.cavern->name);
    G.setMessage(3, 0x140, 0x12, 2, kGoMsg[3]);
    G.flag203808 = true;
    G.byte31e = true;
    G.score(1000, pos, {1.0, 1.0});
    G.sample(G.shipKind == 2 ? 0x4b : 0x29);
    auto *ship = static_cast<Ship *>(a);
    G.respawnPos = ship->mapPos;
    G.respawnRot = 0;
    G.respawnLanded = false;
    dead = true;
    // cavernGoal::hitBy 0x758d4: fifth cavern goal, clean run.
    if (G.cavernIndex == 4 && G.cleanRun && !G.muted && G.awardEligibilityGate < 2 &&
        !(G.goalBits & 4))
        G.achieve(4, 6, "'TONE 40'");
}

void OneUp::run() { // 0x69e1c
    mapRun();
    const int32_t f = g->frameCount;
    const double a = double(float(f & 63) / 63.0f) * 6.2831853072;
    rot = std::sin(float(a)) * 0.5f;
    const float k = float(int32_t(uint32_t(f) & 0x7fff) % 49);
    size = float(double(std::sin(float(double(k / 49.0f) * 6.2831853072 * 5.0))) * 0.005 + 0.04);
}

void OneUp::hitBy(Entity *) { // 0x5e2a0
    PlayState &G = *g;
    G.setMessage(6, 0x96, 8, 4, kGoMsg[8]); // "EXTRA DUDE!"
    G.score(500, pos, {1.0, 1.0});
    G.sample(0x29);
    G.lives++;
    dead = true;
}

// ------------------------------------------------------------------ faller
void Faller::init() { // 0x56b94
    spriteIndex = 0x1e;
    size = float(g->rnd(0.02, 0.04));
    col.white(g->rnd(0.3, 0.8));
    accel = 0;
    v184 = {0, 0};
    rot = float(g->rnd(-kPi, kPi));
    spin = 0;
    spinWhenFalling = float(g->rnd(-0.2, 0.2));
    score = 20;
    damage = 50;
    sound = 0x34;
    hidden = false;
    falling = false;
    fading = false;
}

void Faller::run() { // 0x6a8e8
    mapRun();
    double y = pos.y;
    const int32_t tile = visibleTileAt(*g->map, pos);
    if (fading) {
        col.a = col.a - 0.01;
        if (col.a < 0.02)
            dead = true;
    }
    if (tile < 0 || (tile & 0xff) == 0x17)
        return;
    if ((tile & 0xff) != 0) {
        fading = true;
        accel = 0;
        vel.y = 0;
        return;
    }
    double a;
    if (falling || y <= -0.49 || 0.49 <= y) {
        a = accel;
    } else {
        g->sample(0x3b);
        y = pos.y;
        if (float(g->shipList->pos.y - y) <= 0.0f) {
            a = accel - 0.0001;
            accel = a;
        } else {
            a = 0.0001;
            accel = a;
        }
        spin = spinWhenFalling;
        falling = true;
    }
    vel.y = vel.y + a;
    rot = rot + spin;
    pos = {pos.x + vel.x, pos.y + vel.y};
}

void Faller::hitBy(Entity *a) { // 0x5957c
    if (a->type == 2) {
        auto *ship = static_cast<Ship *>(a);
        PlayState &G = *ship->g;
        G.shield =
            G.shield - ship->hitDamage * 800.0f; // Shield subtraction uses binary32 multiplication
                                                 // and subtraction (vmls.f32).
        if (G.shield < 0.0f)
            G.shield = 0;
        G.cleanRun = false;
    }
    a->dead = true;
}

// ------------------------------------------------------------------ saucer
void Saucer::init() { // 0x54828
    spriteIndex = 0x39;
    size = 0.04f;
    scale.x = 1.4;
    drag = {0.98, 0.98};
    awake = false;
    hidden = false;
    sound = 0x3f;
    score = 10;
    damage = 200;
    pos.y = pos.y - double(0.02f);
    phase = float(g->rnd(-kPi, kPi));
    phaseStep = float(g->rnd(0.04, 0.08));
    triggerDistance = float(g->rnd(0.2, 0.7));
    maxVy = float(g->rnd(0.005, 0.014));
    xSpeed = float(g->rnd(0.002, 0.004));
    if ((randomCellWord(*g->map, row, column) & 0x20) && !g->flag203808) {
        hidden = true;
        drawDelay = 1;
    }
}

void Saucer::run() { // 0x69458
    mapRun();
    if (hidden) {
        drawDelay = 2;
        return;
    }
    const Entity *ship = g->shipList;
    const double x = pos.x;
    const double dy = ship->pos.y - pos.y;
    const float fdy = float(dy);
    if (!awake) {
        if (std::fabs(fdy) < triggerDistance) {
            awake = true;
            g->sample(0x41);
        }
        return;
    }
    phase = phase + phaseStep;
    double vx = double(std::sin(phase)) * 0.009;
    vel.x = float(ship->pos.x - x) <= 0.0f ? vx - double(xSpeed) : vx + double(xSpeed);
    double vy = fdy <= 0.0f ? vel.y - 0.0003 : vel.y + 0.0003;
    vy = vy * drag.y;
    vel.y = vy;
    if (double(maxVy) < vy) {
        vel.y = double(maxVy);
        vy = vel.y;
    }
    if (vy < double(-maxVy))
        vel.y = double(-maxVy);
    pos = {pos.x + vel.x, pos.y + vel.y};
}

// ------------------------------------------------------------------ scrambleRocket
void ScrambleRocket::init() { // 0x570f4
    static const int32_t kKinds[2][3] = {{5, 0x22, 0x37}, {5, 0x37, 0x37}};
    static const int32_t kThresholds[2][3] = {{10, 40, 256}, {30, 49, 256}};
    const int set = g->flag203808 ? 1 : 0;
    const int r = int(g->libcRand() & 0xff);
    size = 0.03f;
    if (spriteIndex == 0) {
        int k = 0;
        if (kThresholds[set][0] <= r)
            k = r < kThresholds[set][1] ? 1 : (r < kThresholds[set][2] ? 2 : -1);
        if (k >= 0)
            spriteIndex = kKinds[set][k];
    }
    scale = g->getScale(spriteIndex);
    dir = rotated({0.0, double(-size) * 0.001}, double(rot));
    pos = {pos.x + dir.x * 400.0, pos.y + dir.y * 400.0};
    score = 20;
    damage = 800;
    sound = -1;
    if (spriteIndex == 0x37) {
        launched = false;
        trigger = float(g->rnd(0.2, 0.5));
        scale.x = bits(0x3fe3333333333333ull); // 0.6
    } else if (spriteIndex == 0x22 || spriteIndex == 5) {
        size = 0.04f;
        scale.y = scale.y * 1.4;
    }
    hidden = false;
    if ((randomCellWord(*g->map, row, column) & 0x20) && !g->flag203808) {
        hidden = true;
        drawDelay = 1;
    }
}

void ScrambleRocket::run() { // 0x6a1ec
    if (hidden) {
        mapRun();
        drawDelay = 1;
        return;
    }
    mapRun();
    const Entity *ship = g->shipList;
    if (spriteIndex == 0x22) {
        const float dy = float(ship->pos.y - pos.y);
        if (0.0f < rot)
            scale.x = dy <= 0.0f ? 1.0 : -1.0;
        if (rot < 0.0f)
            scale.x = dy <= 0.0f ? -1.0 : 1.0;
        if (g->flash && g->flash->watched[0]) {
            g->flash->seen[0] = 1;
            const double x = pos.x, sx = ship->pos.x;
            auto *b = new EnemyBull;
            b->pos = pos;
            b->g = g;
            b->spriteIndex = 3;
            b->size = 0.02f;
            b->noDrift = false;
            const double s = g->rnd(0.002, 0.004);
            b->vel = {s, -s};
            const float r = rot;
            b->vel = rotated(b->vel, double(r) + g->rnd(-0.2, 0.2));
            if (scale.x < 0.0)
                b->vel.y = -b->vel.y;
            b->fall = 0;
            b->aimX = double(float(sx - x)) * -0.008;
            b->pos = {b->pos.x + b->vel.x * 3.0, b->pos.y + b->vel.y * 3.0};
            g->addEntity(g->bounceList, b);
        }
    } else if (spriteIndex == 0x37) {
        if (!launched) {
            if (float(std::fabs(ship->pos.y - pos.y)) < trigger) {
                launched = true;
                row = -1;
            }
            return;
        }
        pos = {pos.x + vel.x, pos.y + vel.y};
        vel = {vel.x + dir.x * 15.0, vel.y + dir.y * 15.0};
        if ((g->cavern->flags & 0x80) && (g->frameCount & 7) == 0) {
            neon::vec2d d{ship->pos.x - pos.x, ship->pos.y - pos.y};
            d = d.normalize();
            auto *b = new EnemyBull;
            b->pos = pos;
            b->g = g;
            b->spriteIndex = 3;
            b->size = 0.02f;
            b->noDrift = false;
            const double s = g->rnd(0.002, 0.004);
            b->vel = {d.x * s, d.y * s};
            b->fall = 0;
            g->addEntity(g->bounceList, b);
        }
        if (pos.x < -0.6 || 0.6 < pos.x)
            dead = true;
    }
}

void ScrambleRocket::hitBy(Entity *a) { // 0x73ca0
    PlayState &G = *g;
    if (a->type == 2) {
        auto *ship = static_cast<Ship *>(a);
        G.shield =
            G.shield - ship->hitDamage * 800.0f; // Shield subtraction uses binary32 multiplication
                                                 // and subtraction (vmls.f32).
        if (G.shield < 0.0f)
            G.shield = 0;
        G.cleanRun = false;
    }
    a->dead = true;
    const bool pod = spriteIndex == 5;
    int points, sparks;
    if (pod) {
        G.fuel = float(double(G.fuel) + 0.1);
        if (1.0f < G.fuel)
            G.fuel = 1.0f;
        G.sample(0xf);
        points = 30;
        sparks = spriteIndex; // the count register holds the sprite index (5)
    } else {
        static const int32_t kBooms[7] = {14, 16, 17, 18, 19, 20, 21};
        const uint32_t r = G.libcRand();
        G.sample(kBooms[(r & 0x7fff) % 7]);
        points = 50;
        sparks = 10;
    }
    G.score(points, pos, {1.0, 1.0});
    addExplo(G, pos, 2.0f, pod ? 20.0f : 5.0f, pod ? bits32(0x3d8f5c29u) : 0.04f);
    for (int i = 0; i < sparks; i++) {
        auto *b = new Bspark;
        if (pod) {
            b->vel.x = G.rnd(-0.002, 0.002);
            b->vel.y = G.rnd(-0.005, 0.005);
            b->ageRate = float(G.rnd(0.003, 0.012));
            b->exploChance = 0.3f;
        } else {
            b->vel.x = G.rnd(-0.005, 0.005);
            b->vel.y = G.rnd(-0.01, 0.01);
            b->ageRate = float(G.rnd(0.001, 0.006));
            b->exploChance = 0;
        }
        b->pos = pos;
        b->rot = float(G.rnd(-kPi, kPi));
        b->age = 0;
        b->spriteIndex = 3;
        b->colourIndex = 1;
        b->maxScale = 1.0f;
        b->g = &G;
        b->size = 0.02f;
        G.addEntity(G.debris, b);
    }
    dead = true;
}

// ------------------------------------------------------------------ goat
void Goat::init() { // 0x55b78
    spriteIndex = 0x34;
    scale.x = bits(0x3ff999999999999aull); // 1.6 (facing)
    size = float(g->rnd(0.03, 0.05));
    if (double(size) < 0.04) {
        const int i = toInt(g->rnd(0.0, 10.0));
        col.unpackRGBA(kRcols[i + 1]);
    }
    pos.y = pos.y + double(size * -0.5f);
    const double vx = g->rnd(0.001, 0.005);
    vel.y = 0;
    sound = 0x3c;
    score = 0x1e;
    damage = 500;
    hidden = false;
    state = 0;
    vel.x = vx;
    if ((randomCellWord(*g->map, row, column) & 0x20) && !g->flag203808) {
        hidden = true;
        drawDelay = 1;
    }
}

void Goat::run() { // 0x7078c
    const cavern::StripMap &m = *g->map;
    constexpr double kFeet = 0.03333333507180214; // (float)(1/30) as binary64
    if (hidden) {
        mapRun();
        drawDelay = 2;
        return;
    }
    if (state == 1) { // jump: no centity::run in this state
        vel.y = vel.y + 0.0003;
        if (vel.y > 0.02)
            vel.y = bits(0x3f947ae147ae147bull);
        const Vec2 np{pos.x + vel.x, pos.y + vel.y};
        double ny = np.y;
        const int c = cellColumn(m, np.x);
        if (c >= 0) {
            const float half = size * 0.5f;
            const int r = cellRow(m, double(half) + ny + kFeet);
            if (r >= 0 && tileByte(m, r, c) == 0x18 && vel.y > 0.0) {
                vel.y = 0;
                vel.x = g->rnd(0.001, 0.005);
                const int r2 = cellRow(m, double(half) + ny + kFeet);
                const float rows = float(m.rows);
                ny = double((float(m.fraction.y + double(r2)) / rows - 0.5f) - size);
                state = 0;
            }
        }
        pos = {np.x, ny};
        rot = float(vel.y * (vel.x < 0.0 ? 40.0 : -40.0));
        spriteIndex = 0x34;
        if (0.6 < pos.y)
            pos.y = pos.y - 1.2;
        if (pos.y < -0.6)
            pos.y = pos.y + 1.2;
        double vx = vel.x;
        if (0.0 < vx && 0.46 < pos.x) {
            vx = -vx;
            vel.x = vx;
        }
        if (vx < 0.0 && pos.x < -0.46)
            vel.x = -vx;
    } else if (state == 2) {
        pos = {pos.x + vel.x, pos.y + vel.y};
        vel.y = vel.y + 0.0012;
        mapRun();
        rot = rot + spin;
        lift = float(double(lift) + 0.001);
        if (lift > 1.0f)
            lift = 1.0f;
        const double r = g->rnd(0.0, 1.0);
        if (r < double(1.0f - lift) * 0.7) {
            const float f = float(double(lift * 3.0f) + 0.8);
            addExplo(*g, pos, f, f + f, float(double(f) * 0.06));
        }
    } else if (state == 0) {
        mapRun();
        if (pos.y < 0.45 && pos.y > -40.5) {
            const Vec2 np{pos.x + vel.x, pos.y + vel.y};
            bool walk = false;
            const int c = cellColumn(m, np.x);
            if (c >= 0) {
                const int r = cellRow(m, double(size * 0.5f) + np.y + kFeet);
                walk = r >= 0 && tileByte(m, r, c) == 0x18;
            }
            if (walk)
                pos = np;
            else
                vel.x = -vel.x;
        }
        spriteIndex = (toInt(pos.x * 50.0) & 1) + 0x34;
        rot = 0;
        const Vec2 d = toShip(*g, pos);
        if (std::sqrt(d.y * d.y + d.x * d.x) < 0.4) {
            vel.y = g->rnd(-0.02, -0.01);
            vel.x = g->rnd(0.005, 0.015);
            if (d.x < 0.0)
                vel.x = -vel.x;
            state = 1;
        }
    }
    scale.x = vel.x < 0.0 ? bits(0x3ff999999999999aull) : bits(0xbff999999999999aull);
}

// ------------------------------------------------------------------ goals, mheart, wavingMinotaur
void PlayState::achieve(uint32_t bit, uint8_t state, const char *name) {
    goalBits |= bit;
    goalState = state; // OSReport "Goal %d achieved"
    if (!muted)
        playSample(0xd, volume << 14);
    setMessage(7, 0x78, 7, 3, "WELL DONE!");
    setMessage(8, 0x78, 9, 5, name);
}

void MHeart::init(PlayState &G) { // 0x55728
    g = &G;
    const double d = G.rnd(0.95, 0.98);
    age = 0;
    drag = {d, d};
    ageRate = float(G.rnd(0.01, 0.02));
    vel = {0, G.rnd(0.001, 0.01)};
    vel = rotated(vel, G.rnd(-kPi, kPi));
    spriteIndex = 0x11;
    const double sz = G.rnd(0.005, 0.02);
    spin = 0;
    accel = {0, 0};
    alphaScale = 1.0f;
    sizeRate = 0;
    size = float(sz);
}

void MHeart::run() { // 0x55094
    vel = {vel.x + accel.x, vel.y + accel.y};
    rot = rot + spin;
    pos = {pos.x + vel.x, pos.y + vel.y};
    vel = {vel.x * drag.x, vel.y * drag.y}; // vec2d * vec2d (componentwise)
    age = age + ageRate;
    size = size + sizeRate;
    if (1.0f <= age) {
        age = 1.0f;
        dead = true;
    }
    col.a = double((1.0f - age) * alphaScale);
}

namespace {
const int32_t kMinoFrames[8] = {0xc,  0xd, 0xe, 0xf,
                                0x10, 0xf, 0xe, 0xd}; // _aFrames.79052 (0x1e6c94)
const char *const kThankMessages[8] = {"THANK YOU!",   "CHEERS MATE!", "OOH TA!",
                                       "CHEERS DEER!", "HOW KIND!",    "THANKS!",
                                       "DIOLCH!",      "*GRUNT*"}; // thankMessages (0x23b0ac)
} // namespace

void WavingMinotaur::hitBy(Entity *) { // 0x7521c
    if (g->landed)
        jumpIntoShip();
}

void WavingMinotaur::jumpIntoShip() { // 0x750d8
    if (state == 1)
        return;
    PlayState &G = *g;
    hop = 0;
    hopVel = bits32(0xbc23d70au); // -0.01
    hopGravity = float(G.rnd(0.0002, 0.0003));
    t = 0;
    const double s = G.rnd(-5.0, 5.0);
    from = pos;
    spinRate = float(s);
    state = 1;
    G.awardLanding();
    G.rescueBonus +=
        G.landingBonus == 5 ? 2000 : (G.landingBonus + 1) * 200;       // "Mino bonus added %d"
    const long i = long(row) * cavern::StripMap::kMapColumns + column; // theGame+0x37fc cell
    if (i >= 0 && i < long(G.map->cells.size()))
        G.map->cells[size_t(i)] = 0xffffffffu;
}

void WavingMinotaur::run() { // 0x76b68
    PlayState &G = *g;
    phase = float(double(phase) + 0.2);
    if (phase >= 8.0f)
        phase = phase - 8.0f;
    spriteIndex = kMinoFrames[int(phase)];
    if (state == 1) {
        if (t < 1.0f)
            t = float(double(t) + 0.004);
        Entity *ship = G.shipList;
        if (ship->type != 2) {
            state = 2;
            vel.y = double(hopGravity);
            return;
        }
        const Vec2 d{ship->pos.x - from.x, ship->pos.y - from.y};
        const double tt = double(t);
        const Vec2 p{d.x * tt + from.x, d.y * tt + from.y};
        if (std::sqrt(d.y * d.y + d.x * d.x) < 0.01)
            t = 1.0f;
        from = p;
        const Vec2 e{from.x - ship->pos.x, from.y - ship->pos.y};
        if (std::sqrt(e.y * e.y + e.x * e.x) < 0.01)
            t = 1.0f;
        rot = hop * spinRate;
        if (hop <= 0.0f) {
            hopVel = hopVel + hopGravity;
            hop = hop + hopVel;
        }
        if (1.0f <= t && 0.0f < hop) { // in the ship: rescued
            dead = true;
            const char *thanks =
                kThankMessages[G.libcRand() & 7]; // drawn whether or not a HUD is attached
            G.setMessage(0, 0x78, 0x14, 4, thanks);
            if (G.shipKind == 2)
                G.sample(0x4a);
            else {
                static const int32_t kVoices[6] = {0, 1, 2, 3, 4, 4};
                const uint32_t r1 = G.libcRand();
                const uint32_t r2 = G.libcRand();
                G.playSample(kVoices[(r2 & 1) + (r1 & 3)], G.volume << 12); // no mute test here
                G.byte398 = true;
            }
            G.minosSaved++;
            if (0x18 < G.minosSaved && !G.muted && G.awardEligibilityGate < 2 &&
                !(G.goalBits & 0x40))
                G.achieve(0x40, 10, "'MINO MASTER'");
            G.minosSavedTotal++;
            if (100 < G.minosSavedTotal && !G.muted && G.awardEligibilityGate < 2 &&
                !(G.goalBits & 0x80))
                G.achieve(0x80, 11, "'MINOTAUR LOVER'");
            if (200 < G.minosSavedTotal && !G.muted && G.awardEligibilityGate < 2 &&
                !(G.goalBits & 0x100))
                G.achieve(0x100, 12, "'MINOTAUR LORD'");
            G.score(100, pos, {1.0, 1.0});
            G.fuel = float(double(G.fuel) + 0.2);
            G.shield = float(double(G.shield) + 0.1);
            if (1.0f < G.fuel)
                G.fuel = 1.0f;
            if (1.0f < G.shield)
                G.shield = 1.0f;
            for (int k = 0; k < 50; k++) {
                auto *h = new MHeart;
                h->pos = pos;
                h->init(G);
                G.addEntity(G.debris, h);
            }
        }
        pos = {p.x, p.y + double(hop)};
        return;
    }
    if (state == 2) {
        mapRun();
        pos.y = pos.y + vel.y;
        vel.y = vel.y + 0.005;
        if (0.6 < pos.y)
            dead = true;
        return;
    }
    if (state != 0)
        return;
    // Waving: jump when the ship has landed on the pad level with this cell, reachable
    // along the ground (tiles 15/16 step up a line), within 3 lines.
    auto *ship = static_cast<Ship *>(G.shipList);
    if (G.landed && 0.001 <= double(G.shield) && !ship->dying && !G.reset310) {
        const cavern::StripMap &m = *G.map;
        const int cols = m.columns, rows = m.rows;
        const int c = toInt((pos.x + 0.5) * double(float(cols)) - m.fraction.x);
        const int r = toInt((pos.y + 0.5 + 0.025) * double(float(rows)) - m.fraction.y);
        int line = r + 1;
        bool jump = false, near = false;
        if (G.padCell[1] == line && G.padCell[0] == c)
            jump = true;
        else {
            const int dx = G.padCell[0] - c;
            int step, dist;
            if (dx < 1) {
                dist = -dx;
                step = -1;
            } else {
                dist = dx;
                step = 1;
            }
            if (dx == 0)
                near = true;
            else {
                int particleColour = c + step;
                if (particleColour >= 0 && line >= 0 && particleColour <= cols && line <= rows) {
                    int tile = tileByte(m, line, particleColour);
                    if (tile != 0) {
                        int k = 0;
                        for (;;) {
                            particleColour += step;
                            k++;
                            if (unsigned(tile - 0xf) < 2)
                                line++;
                            if (dist <= k) {
                                near = true;
                                break;
                            }
                            if (!(particleColour >= 0 && particleColour <= cols && line <= rows))
                                break;
                            tile = tileByte(m, line, particleColour);
                            if (tile == 0)
                                break;
                        }
                    }
                }
            }
            if (near) {
                int dy = line - G.padCell[1];
                if (dy < 0)
                    dy = -dy;
                jump = dy <= 3;
            }
        }
        if (jump)
            jumpIntoShip();
    }
    mapRun();
}

// ------------------------------------------------------------------ motherSheep
namespace {
// sinf(float(double(float(n) / k) * 6.2831853072)) as motherSheep forms its phases.
float phaseSin(int n, float k) {
    return std::sin(float(double(float(n) / k) * 6.2831853072));
}
} // namespace

void MotherSheep::init() { // 0x5674c
    spriteIndex = 0x1d;
    size = bits32(0x3da3d70au); // 0.08
    pulse = 0;
    hum = 0;
    burstOnClear = 0;
    scale = {2.0, 2.0};
    wobble = {0, 0};
    rest = pos;
    wobble.x = double(phaseSin(g->frameCount & 0x1ff, 511.0f)) * 0.3;
    rot = float(double(phaseSin(g->frameCount & 0x7f, 127.0f)) * 0.3);
    pos = {pos.x + wobble.x, pos.y + wobble.y};
    col.white(0.5);
    state = 0;
    heartAngle = float(g->rnd(-kPi, kPi));
    heartStep = float(g->rnd(0.1, 0.5));
}

void MotherSheep::run() { // 0x69620
    PlayState &G = *g;
    if (G.drawMode == 5)
        return;
    const float f = phaseSin(G.frameCount & 0x7f, 127.0f);
    pulse--;
    rot = float(double(f) * 0.3);
    G.bossPresent = true;
    if (pulse < 0)
        pulse = 0x16;
    if (state == 0) {
        if (!G.flag203808) {
            if (!G.gameOver) { // newLevel's go message again
                G.setMessage(1, 0x32, 0x12, 2, kGoMsg[0]);
                G.setMessage(2, 0x3c, 0x14, 4, G.cavern->name);
                G.setMessage(3, 0x46, 0x16, 2, kGoMsg[1]);
            }
            if ((G.frameCount & 0x7f) == 0 && !G.muted)
                G.playSample(0x27, G.volume * hum * 8);
        }
        const Vec2 d = toShip(G, pos);
        const float len = float(std::sqrt(d.y * d.y + d.x * d.x));
        if (len < 0.5f && pulse == 0x16) {
            const int v = int((0.5f - len) * 2000.0f);
            hum = v;
            if (!G.muted)
                G.playSample(0x26, G.volume * v);
        }
        pos = rest;
        mapRun();
        rest = pos;
        wobble.x = double(phaseSin(G.frameCount & 0x1ff, 511.0f)) * 0.3;
        pos = {pos.x + wobble.x, pos.y + wobble.y};
    } else if (state == 1) {
        double sd = G.scrollDelta.y; // +0x348 eases to 0.0005
        if (sd < 0.0005) {
            sd = sd + 5e-05;
            G.scrollDelta.y = sd;
        }
        if (0.0005 < sd)
            G.scrollDelta.y = sd - 5e-05;
        if (G.approachTimer < 0x1f5) {
            if (hum < 6000)
                hum += 5;
            if (col.a < 1.0)
                col.a = col.a + 2e-05;
        } else {
            if (0 < hum && (hum -= 10) < 0) {
                dead = true;
                hum = 0;
            }
            col.a = col.a * 0.991;
        }
        if (G.runState == 5) {
            dead = true;
            return;
        }
        if (pulse == 0x16 && !G.muted)
            G.playSample(0x26, G.volume * hum);
        if ((G.frameCount & 0x7f) == 0 && !G.muted)
            G.playSample(0x28, G.volume * hum * 4);
        pos = {pos.x * bits(0x3fefc6a7ef9db22dull), pos.y * bits(0x3fefc6a7ef9db22dull)}; // 0.993
        heartAngle = heartAngle + heartStep;
        auto *h = new MHeart;
        h->pos = pos;
        h->init(G);
        h->vel = rotated({0, bits(0x3f847ae147ae147bull)}, double(heartAngle)); // (0, 0.01)
        h->pos = {h->pos.x + h->vel.x * 20.0, h->pos.y + h->vel.y * 20.0};
        h->spin = float(G.rnd(-0.4, 0.4) * double(G.bgBright));
        h->alphaScale = float(col.a);
        G.addEntity(G.debris, h);
        G.bgColour[0] = float(double(G.bgBright) * G.colours[4].r);
        G.bgColour[1] = float(double(G.bgBright) * G.colours[4].g);
        G.bgColour[2] = float(double(G.bgBright) * G.colours[4].b);
        if (double(G.bgBright) < 0.96)
            G.bgBright = float(double(G.bgBright) + 0.002);
        G.bgFade[0] = G.bgFade[1] = bits32(0x3f7d70a4u); // 0.99
    }
}

void MotherSheep::hitBy(Entity *a) { // 0x75234
    PlayState &G = *g;
    if (!G.flag203808)
        return;
    if (a->type == 2) { // the ship docks: it becomes an explo
        auto *ship = static_cast<Ship *>(a);
        a->type = 3;
        ship->dying = true; // +0x1b5
        auto *e = new Explo;
        e->from = 1.0f;
        e->to = 20.0f;
        e->rate = 0.01f;
        e->t = 0;
        e->spriteIndex = a->spriteIndex;
        e->col = a->col; // memcpy 0x50
        e->pos = pos;
        e->rot = float(G.rnd(-kPi, kPi));
        e->baseScale = {1.0, 1.0};
        e->g = &G;
        e->pos = a->pos;
        e->rot = a->rot;
        e->baseScale = a->scale; // +0xdc..+0xe8 -> +0xec..+0xf8
        G.addEntity(G.debris, e);
        const uint32_t r = G.libcRand();
        if (!G.muted)
            G.playSample(int((r & 0x3ff) % 6) + 6, G.volume << 14);
        if (0 < G.cavernIndex && G.cleanRun && !G.muted && G.awardEligibilityGate < 2 &&
            !(G.goalBits & 2))
            G.achieve(2, 5, "'CLEAR'");
        if (G.cavernIndex == 0x14 && !G.muted && G.awardEligibilityGate < 2 &&
            !(G.goalBits & 0x200))
            G.achieve(0x200, 13, "'AWESOMETASTIC'");
    }
    if (state < 1) {
        state = 1;
        G.clearMessages(7);                       // slots 0..6: cll(row) where a text is set
        G.setMessage(1, 500, 0x16, 3, kGoMsg[6]); // "Thank you for" / "bringing me" / cavern name
        G.setMessage(2, 0x1fe, 0x18, 3, kGoMsg[7]);
        G.setMessage(3, 0x208, 0x1a, 4, G.cavern->name);
        G.approachTimer = 1;
        G.buttonLatch = true;
    }
}

// ------------------------------------------------------------------ scRocket (doLauncher)
void PlayState::doLauncher(int arg) { // theGame::doLauncher 0x5d138
    if (paused || clearAll != 0)
        return;
    if (--launcherTimer >= 1)
        return;
    launcherTimer = 10;
    if (arg != 0x100)
        return;
    auto *e = new ScRocket;
    e->burstOnClear = 1; // +0x181
    e->g = this;
    e->pos.x = rnd(-0.45, 0.45);
    e->pos.y = bits(0x3fe3333333333333ull); // 0.6
    const double m = shipKind == 1 ? 2.0 : 1.0;
    const double vy = rnd(-0.007, -0.003);
    e->vel = {0, vy * m};
    e->spriteIndex = 2;
    e->col.unpackRGBA(0xd1884fffu);
    e->size = bits32(0x3cf5c28fu); // 0.03
    e->row = -1;                   // +0x17c 0xffffffff: not from the map
    addEntity(bounceList, e);      // +0x2e8
}

void ScRocket::run() {                            // 0x6ab4c
    col = g->colours[4];                          // memcpy 0x50 from theGame+0x618
    pos = {pos.x + vel.x, pos.y + vel.y};         // vec2d::operator+
    pos.x = pos.x + double(g->shotDrift) * 0.009; // vmla.f64, +0xa08
    mapRun();
    if (visibleTile() > 0) { // signed > 0 (outside: -1)
        addExplo(*g, pos, 0.5f, 2.0f, bits32(0x3d75c28fu));
        dead = true;
    }
}

void ScRocket::hitBy(Entity *a) { // 0x742ec
    PlayState &G = *g;
    if (a->type == 2) {
        auto *ship = static_cast<Ship *>(a);
        G.shield =
            G.shield - ship->hitDamage * 800.0f; // Shield subtraction uses binary32 multiplication
                                                 // and subtraction (vmls.f32).
        if (G.shield < 0.0f)
            G.shield = 0;
        G.cleanRun = false;
    }
    a->dead = true;
    addExplo(G, pos, 2.0f, 4.0f, 0.016f); // 0x40000000, 0x40800000, 0x3c83126f
    G.score(50, pos, {1.0, 1.0});
    for (int i = 0; i < 10; i++)
        addSpark(G, pos, -0.02, -0.005, -0.01, 0.01, 0.001, 0.006);
    dead = true;
    static const int32_t kBooms[7] = {14, 16, 17, 18, 19, 20, 21};
    const uint32_t r = G.libcRand(); // drawn before the mute test
    G.sample(kBooms[(r & 0x7fff) % 7]);
}

// ------------------------------------------------------------------ rocket
namespace {
EnemyBull *newBull(PlayState &g, Vec2 pos) {
    auto *b = new EnemyBull;
    b->pos = pos;
    b->g = &g;
    b->spriteIndex = 3;
    b->size = 0.02f;
    b->noDrift = false;
    b->fall = 0;
    return b;
}
} // namespace

void Rocket::init() { // 0x56930
    static const int32_t kKinds[5] = {2, 5, 6, 0x21, 0x22};
    vel.y = 0;
    size = 0.03f;
    scale = {1.0, 1.0};
    timer = 10000;
    const uint32_t w = randomCellWord(*g->map, row, column);
    hidden = false;
    spriteIndex = kKinds[(w & 0xff) % 5];
    if ((w & 0x20) && !g->flag203808 && spriteIndex != 5) {
        hidden = true;
        drawDelay = 1;
    }
    col.white(1.0);
    if (spriteIndex == 2) {
        col.unpackRGBA(0xd1884fff);
        timer = int32_t((g->libcRand() + 0x32u) & 0x7f);
    }
    if (spriteIndex == 0x22) {
        if (g->cavern->flags & 8)
            spriteIndex = 0x40;
    } else {
        if (spriteIndex == 0x21) {
            if (!(g->cavern->flags & 0x10))
                return;
            spriteIndex = 0x41;
            size = 0.05f;
            mineRange = float(g->rnd(0.2, 0.4));
        }
        if (spriteIndex == 6 && (g->cavern->flags & 0x20)) {
            spriteIndex = 0x23;
            size = 0.04f;
        }
    }
}

void Rocket::foom() { // 0x71ae4
    PlayState &G = *g;
    dead = true;
    const uint32_t r = G.libcRand();
    Vec2 dir{0.0, bits(0xbf70624dd2f1a9fcull)}; // -0.004
    const float step = float(r & 7) * bits32(0x3d80adfdu);
    G.sample(0x47);
    const double k = double(float(G.shipList->pos.y - pos.y)) * 0.00015;
    float phase = 0.0f;
    for (int i = 0; i < 100; i++) {
        auto *b = newBull(G, pos);
        const double m = (double(std::sin(phase)) * 0.5 + 0.5) * 0.7 + 1.0;
        b->vel = {dir.x * m, dir.y * m};
        b->fall = k;
        G.addEntity(G.bounceList, b);
        dir = rotated(dir, 0.06283185631036758);
        phase = phase + step;
    }
    addExplo(G, pos, 2.0f, 15.0f, 0.01f);
}

void Rocket::run() { // 0x725ec
    PlayState &G = *g;
    if (hidden) {
        mapRun();
        return;
    }
    int32_t tile = 0;
    const int kind = spriteIndex;
    switch (kind) {
    case 2: {
        col = G.colours[4];
        const int32_t t = visibleTileAt(*G.map, pos);
        tile = t < 0 ? 0 : t;
        break;
    }
    case 5:
        mapRun();
        return;
    case 6: {
        col = G.colours[1];
        if ((G.rocketFlags & 1) && 0 < timer &&
            float(std::fabs(pos.y - G.shipList->pos.y)) < 0.4f) {
            G.sample(0x38);
            timer = 0;
            burstAfter = int32_t((G.libcRand() & 0x3f) + 0x3c);
        }
        if (timer >= 0)
            break;
        const int32_t t = visibleTileAt(*G.map, pos);
        tile = t < 0 ? 0 : t;
        bool burst = 0 < tile;
        vel.y = vel.y - 4e-05;
        if (timer < -burstAfter)
            burst = true;
        if (burst) {
            addExplo(G, pos, 0.5f, 12.0f, bits32(0x3d8f5c29u));
            dead = true;
            G.sample(0x37);
            if (G.rocketFlags & 2) {
                const float dy = float(G.shipList->pos.y - pos.y);
                for (int i = 0; i < G.cavernIndex + 2; i++) {
                    auto *b = newBull(G, pos);
                    b->vel.x = G.rnd(0.005, 0.01);
                    b->vel.y = G.rnd(-0.002, 0.002);
                    if (G.libcRand() & 1)
                        b->vel.x = -b->vel.x;
                    b->fall = dy <= 0.0f ? 0.0 : double(dy) * 0.0004;
                    b->pos = {b->pos.x + b->vel.x * 3.0, b->pos.y + b->vel.y * 3.0};
                    G.addEntity(G.bounceList, b);
                }
            }
        }
        break;
    }
    case 0x21:
        if (G.flash && G.flash->watched[2]) {
            G.flash->seen[2] = 1;
            auto *b = newBull(G, pos);
            b->pos.y = b->pos.y - double(size);
            b->vel = {0.0, bits(0xbf789374bc6a7efaull)}; // -0.006
            G.addEntity(G.bounceList, b);
        }
        break;
    case 0x22: {
        scale.x = G.shipList->pos.x < pos.x ? -1.0 : 1.0;
        mapRun();
        if (G.flash && G.flash->watched[0]) {
            G.flash->seen[0] = 1;
            const float dy = float(G.shipList->pos.y - pos.y);
            auto *b = newBull(G, pos);
            b->vel = {0.0 < scale.x ? 0.003 : -0.003, -0.003};
            b->fall = dy <= 0.0f ? 0.0 : double(dy) * 0.0003;
            b->pos = {b->pos.x + b->vel.x * 3.0, b->pos.y + b->vel.y * 3.0};
            G.addEntity(G.bounceList, b);
        }
        return;
    }
    case 0x23: {
        if (0 < timer) {
            timer--;
            if (float(std::fabs(pos.y - G.shipList->pos.y)) < 0.6f) {
                G.sample(0x38);
                timer = 0;
                burstAfter = int32_t((G.libcRand() & 0x3f) + 0x3c);
            }
        }
        if (timer < 0) {
            neon::vec2d d{toShip(G, pos).x, toShip(G, pos).y};
            d = d.normalize();
            const double k = bits(0x3f23a92a30553261ull); // 0.00015
            vel = {vel.x + d.x * k, vel.y + d.y * k};
            pos = {pos.x + vel.x, pos.y + vel.y};
            mapRun();
            if ((G.frameCount & 7) == 0) {
                auto *b = newBull(G, pos);
                b->vel = {d.x * 0.002, d.y * 0.002};
                G.addEntity(G.bounceList, b);
            }
            return;
        }
        break;
    }
    case 0x40: {
        col = G.colours[0];
        if (G.frameCount & 1)
            break;
        const Vec2 d = toShip(G, pos);
        const double len = std::sqrt(d.y * d.y + d.x * d.x);
        const double r = G.rnd(0.0, 1.0);
        if (!(r > double(float(len) * 4.0f)))
            break;
        auto *b = newBull(G, pos);
        b->pos.y = b->pos.y - double(size);
        G.sample(0x48);
        const float k = float(double(1.0f - float(len)) * 0.002 + 0.002);
        b->vel.y = G.rnd(double(-k), double(k));
        b->vel.x = G.rnd(double(-k), double(k));
        G.addEntity(G.bounceList, b);
        break;
    }
    case 0x41: {
        col = G.colours[1];
        const Vec2 d = toShip(G, pos);
        if (float(std::sqrt(d.y * d.y + d.x * d.x)) < mineRange)
            foom();
        break;
    }
    default:
        break;
    }
    // joined: flying (timer < 1) or waiting
    if (timer < 1) {
        if (G.frameCount & 1) {
            auto *p = new ThrustParticle;
            p->pos = pos;
            p->vel.x = G.rnd(-0.0005, 0.0005);
            p->vel.y = bits(0x3f60624dd2f1a9fcull); // 0.002
            p->size = 0.02f;
            p->growth = float(G.rnd(1.01, 1.04));
            p->fade = float(G.rnd(0.03, 0.04));
            p->collides = false;
            p->init(G);
            G.addEntity(G.debris, p);
        }
        timer--;
        if (1 < G.shipKind)
            vel.x = vel.x + double(-float(G.shipList->pos.x - pos.x)) * vel.y * 0.02;
        pos = {pos.x - G.scrollDelta.x, pos.y - G.scrollDelta.y};
        pos.x = pos.x + double(G.shotDrift) * 0.009;
        vel.y = vel.y + double(float(G.shipKind + 1)) * -2e-05;
        pos = {pos.x + vel.x, pos.y + vel.y};
        if (pos.y < -0.6 || 0.6 < pos.y) {
            dead = true;
            const long ci = long(row) * cavern::StripMap::kMapColumns + column;
            if (ci >= 0 && ci < long(G.map->cells.size()))
                G.map->cells[size_t(ci)] |= 0xff;
        }
    } else {
        timer--;
        mapRun();
    }
    if (0 < tile) {
        addExplo(G, pos, 0.5f, 2.0f, 0.06f);
        dead = true;
    }
}

void Rocket::hitBy(Entity *a) { // 0x71ebc
    PlayState &G = *g;
    if (a->type == 2) {
        auto *ship = static_cast<Ship *>(a);
        G.shield =
            G.shield - ship->hitDamage * 800.0f; // Shield subtraction uses binary32 multiplication
                                                 // and subtraction (vmls.f32).
        if (G.shield < 0.0f)
            G.shield = 0;
        G.cleanRun = false;
    }
    if (spriteIndex == 0x41)
        foom();
    a->dead = true;
    const int kind = spriteIndex;
    int points, sparks;
    float to = 5.0f, rate = 0.01f;
    if (kind == 5) {
        to = 20.0f;
        rate = 0.04f;
        G.fuel = float(double(G.fuel) + 0.1);
        if (1.0f < G.fuel)
            G.fuel = 1.0f;
        G.sample(0xf);
        points = 30;
        sparks = kind; // count register holds the sprite index
    } else {
        static const int32_t kBooms[7] = {14, 16, 17, 18, 19, 20, 21};
        const uint32_t r = G.libcRand();
        G.sample(kBooms[(r & 0x7fff) % 7]);
        points = kind == 6 ? 100 : 50;
        sparks = kind == 6 ? 15 : 10;
    }
    G.score(points, pos, {1.0, 1.0});
    addExplo(G, pos, 2.0f, to, rate);
    for (int i = 0; i < sparks; i++) {
        auto *b = new Bspark;
        if (kind == 5) {
            b->vel.y = G.rnd(-0.002, -0.01);
            b->vel.x = G.rnd(-0.005, 0.005);
            b->ageRate = float(G.rnd(0.003, 0.012));
            b->exploChance = 0.3f;
        } else if (kind == 6) {
            b->vel.y = G.rnd(-0.002, -0.01);
            b->vel.x = G.rnd(-0.035, 0.035);
            b->ageRate = float(G.rnd(bits(0x3f689374bc6a7efaull), bits(0x3f889374bc6a7efaull)));
            b->exploChance = 0;
        } else {
            b->vel.y = G.rnd(-0.005, -0.02);
            b->vel.x = G.rnd(-0.01, 0.01);
            b->ageRate = float(G.rnd(bits(0x3f50624dd2f1a9fcull), bits(0x3f789374bc6a7efaull)));
            b->exploChance = 0;
        }
        b->pos = pos;
        b->rot = float(G.rnd(-kPi, kPi));
        b->age = 0;
        b->spriteIndex = 3;
        b->colourIndex = 1;
        b->maxScale = 1.0f;
        b->g = &G;
        b->size = 0.02f;
        G.addEntity(G.debris, b);
    }
    dead = true;
}

// ------------------------------------------------------------------ floating score
void FloatingScore::run() { // 0x54278
    if (g->paused)
        return;
    neon::notices::advanceFloating(pos, vel, alpha, fade, dead, [&](double opacity) {
        col = g->colours[3];
        col.a = opacity;
    });
}

int FloatingScore::draw(EntityRenderer &r, Vec2 screen) { // 0x55478
    auto *sr = dynamic_cast<SpriteRenderer *>(&r);
    if (!sr)
        return 0;
    // Unlike entity::draw, glyph dimensions are size*scale (not twice size).
    // 0x55618 holds binary64 1.2. Advance both axes before the half-height
    // origin adjustment; preserve the source's operation order for centring.
    const Vec2 dimensions{scale.x * double(size), scale.y * double(size)};
    const Vec2 advance{dimensions.x * 1.2, dimensions.y * 1.2};
    const int n = 15 - first;
    const auto origin = neon::notices::centredOrigin(pos, advance, n);
    double x = origin.x;
    const double y = origin.y;
    SpriteFields f;
    f.orFlags = 3;
    f.col = col;
    f.angle = double(rot);
    f.size = {dimensions.x * screen.x, dimensions.y * screen.y};
    for (int i = first + 1; i <= 15; i++) {
        f.frame = spriteBase + digits[i] + 2;
        f.pos = {x * screen.x, y * screen.y};
        sr->drawSprite(f);
        x += advance.x;
    }
    return n * 6;
}

Enemy *makeEnemy(cavern::EntityClass cls) {
    using C = cavern::EntityClass;
    switch (cls) {
    case C::Turret:
        return new Turret;
    case C::CavernGoal:
        return new CavernGoal;
    case C::OneUp:
        return new OneUp;
    case C::Faller:
        return new Faller;
    case C::Saucer:
        return new Saucer;
    case C::ScrambleRocket:
        return new ScrambleRocket;
    case C::Rocket:
        return new Rocket;
    case C::Goat:
        return new Goat;
    case C::WavingMinotaur:
        return new WavingMinotaur;
    case C::MotherSheep:
        return new MotherSheep;
    default:
        return nullptr;
    }
}

void PlayState::launch(const cavern::SpawnRequest &r) { // theGame::launch, entity part
    Enemy *e = makeEnemy(r.cls);
    if (!e)
        return;
    e->pos = r.position;
    e->column = r.column;
    e->row = r.row;
    if (r.spriteIndex)
        e->spriteIndex = *r.spriteIndex;
    if (r.rot)
        e->rot = *r.rot;
    e->burstOnClear = 1;
    e->g = this;
    if (r.callsInit)
        e->init();
    if (r.spriteIndexAfterInit)
        e->spriteIndex = *r.spriteIndexAfterInit;
    if (r.promoteSprite5To6 && e->spriteIndex == 5)
        e->spriteIndex = 6;
    if (r.colourWhite)
        e->col.white(1.0);
    if (r.size)
        e->size = *r.size;
    if (r.hidden)
        e->hidden = *r.hidden != 0;
    if (r.burstOnClear)
        e->burstOnClear = int8_t(*r.burstOnClear);
    if (r.drawDelay)
        e->drawDelay = *r.drawDelay;
    if (r.scaleX)
        e->scale.x = *r.scaleX;
    if (r.scaleY)
        e->scale.y = *r.scaleY;
    if (auto *w = dynamic_cast<WavingMinotaur *>(e)) { // launch's inline 'M' writes
        if (r.phase184)
            w->phase = *r.phase184;
        if (r.word188)
            w->state = *r.word188;
        if (r.home1b0)
            w->home = *r.home1b0;
    }
    addEntity(r.list == cavern::EntityList::List2e4 ? touchList : bounceList, e);
}

} // namespace minos
