// entity.h - the Minos entity base (Neon_Internals::entity) and the map-entity base
// behaviour (centity::run 0x691b4, centity::draw 0x54100) of Minos_armv7.
//
// Layout: the Minotron entity plus a byte at +0x0a and one 4-byte member inserted in
// +0xec..+0x104 (matching constructor layouts); names follow the Minotron
// port for the same members. Defaults are the values entity::entity (0x413fc) writes, read from
// the original constructor image (fixtures/cavern-launch-ios.txt `ref`). Fields the original
// constructor leaves unwritten are marked "host default": the original leaves heap contents,
// so a reader must not rely on them; 0 is a host safety value, not recovered behaviour.
//
// Only the fields used by reconstructed code are modelled; others follow with their readers.
#pragma once
#include "cavern_map.h"
#include "neon/source_random.h"
#include "neon/math.h"
#include <cstdint>
#include <optional>

namespace minos {

using cavern::Vec2;
struct Entity;
struct ExplosionBirth;

// theGame as the entities see it (the original passes theGame*, stored at +0x168).
class GameServices {
  public:
    virtual ~GameServices() = default;
    virtual Vec2 scrollDelta() const = 0;   // theGame+0x340
    virtual int32_t clearAll() const = 0;   // theGame+0x9fc
    virtual cavern::StripMap &map() = 0;    // theGame+0xa18
    virtual neon::DarwinRandom &rand() = 0; // libc rand stream
    virtual void addDebris(Entity *e) = 0;  // addEntity(theGame+0x2e0); takes ownership
    virtual void
    addMapExplosion(const ExplosionBirth &birth) = 0; // bind a real explo to this game's state
};

// entity::draw's host side: emits the entity's sprite (sprite fields from the entity).
class EntityRenderer {
  public:
    virtual ~EntityRenderer() = default;
    virtual int drawEntity(Entity &e, Vec2 screen) = 0; // entity::draw
};

// Sprite-level drawing for the entities below that draw more than entity::draw (osj) and for
// theGame::_draw's HUD bars (the core implements it on its sprite).
struct SpriteFields {
    Vec2 pos, size;        // +0x04 / +0x0c, +0x28 / +0x30 (already scaled to the screen)
    double angle = 0;      // +0x20
    int frame = 0;         // +0x58
    uint32_t flags = 0;    // +0x5c
    neon::colour col;      // +0x78
    double alphaMul = 1.0; // +0x90 (osj)
    uint32_t orFlags =
        0; // non-zero: +0x5c |= orFlags (flags kept from the last draw) instead of = flags
    bool preserveFlags = false; // caller does not write +0x5c (osj / openfeintButton)
};

// Renderer extension for SpriteFields drawing (osj, HUD bars).
struct SpriteRenderer : EntityRenderer {
    virtual int drawSprite(const SpriteFields &f) = 0; // sprite fields + setupChecking
};

struct Entity {
    virtual ~Entity() = default;
    virtual void run(); // entity::run: neon::entities::integrate
    virtual int draw(EntityRenderer &r, Vec2 screen) {
        return r.drawEntity(*this, screen);
    }
    // entity::hitBy 0x40d34 (empty); entity::wasShot is only reached for attackers with
    // flags & 3, and no Minos game class sets +0x80 (instructions searched): not modelled.
    virtual void hitBy(Entity *attacker) {
        (void)attacker;
    }
    void wasShot(Entity *) {}
    Entity *prev = nullptr; // +0x158 list links (controlBlock::addEntity = push-front)
    Entity *next = nullptr; // +0x15c

    // ---- written by entity::entity ----
    bool dead = false;            // +0x04
    bool active = true;           // +0x05
    bool noWrap = false;          // +0x07
    bool touching = false;        // +0x08
    bool hit = false;             // +0x09
    bool collisionEnabled = true; // +0x0a constructor 1; collide rejects this target when false
    int32_t fc = 0, age = 0, type = 0; // +0x0c, +0x10, +0x14
    float zOffset = 0;                 // +0x18
    int32_t spriteIndex = 0x8c;        // +0x1c sprite number
    int32_t drawDelay = 0;             // +0x20 frames before drawn
    neon::colour col;                  // +0x28 (0x50 bytes; entity::entity: r, g, b, a = 1.0)
    int32_t group = 0;                 // +0x78
    uint32_t flags = 0;                // +0x80
    float rot = 0;                     // +0x84 radians
    float elasticity = 1.0f;           // +0x88
    Vec2 pos;                          // +0x8c
    Vec2 vel;                          // +0x9c
    Vec2 drag{0.98, 0.98};      // +0xac (original 0.98; the Minotron port's member default is 0.99)
    Vec2 wrap{1.0, 1.0};        // +0xcc wrap width/height
    Vec2 scale{1.0, 1.0};       // +0xdc
    int32_t word0fc = 0;        // +0xfc constructor 0 (the inserted Minos member is in this area)
    int32_t health = 1, hp = 1; // +0x104, +0x108
    int32_t flashFrames = 0;    // +0x10c
    float mass = 1.0f;          // +0x12c
    float size = 0.02f;         // +0x134 collision radius
    int32_t word160 = 0;        // +0x160
    // ---- not written by entity::entity (host defaults) ----
    bool wrapped = false;         // +0x06
    Vec2 baseScale;               // +0xec (set by the classes that use it)
    GameServices *game = nullptr; // +0x168 theGame* (set by launch/init)
};

// Map entities (centity and subclasses: everything theGame::launch creates).
bool centityDrawVisible(struct MapEntity &e);
struct MapEntity : Entity {
    void run() override;                                // centity::run (centityRun)
    int draw(EntityRenderer &r, Vec2 screen) override { // centity::draw
        return centityDrawVisible(*this) ? r.drawEntity(*this, screen) : 0;
    }
    // xentity (+0x168 game, +0x16c..+0x174): what xentity::hitBy uses
    int32_t score = 0;  // +0x16c points (theGame::_score)
    int32_t damage = 0; // +0x170 shield damage to the ship (* ship +0x180)
    int32_t sound = 0;  // +0x174 sample on destruction (< 0: random from _booms); host default 0
    int32_t column = 0, row = 0; // +0x178/+0x17c launch cell (row < 0: not from the map)
    bool hidden = false;         // +0x180 centity::draw skips it and holds drawDelay at 1
    int8_t burstOnClear = 0;     // +0x181 1: becomes an explo while the game's clear flag is set
};

// centity::run's birth writes. This is a construction record, not a runnable
// entity; GameServices binds it to the existing Explo implementation.
struct ExplosionBirth {
    int32_t spriteIndex = 4;
    Vec2 pos;
    float rot = 0;
    Vec2 baseScale{1, 1};
    GameServices *game = nullptr;
    float fromScale = 0, toScale = 0, scaleRate = 0; // +0x16c, +0x170, +0x178
    float progress = 0;                              // +0x174
};

// The theGame state centity::run reads.
struct MapEntityWorld {
    Vec2 scrollDelta;     // theGame+0x340: this frame's map scroll, subtracted from positions
    int32_t clearAll = 0; // theGame+0x9fc: non-zero turns burstOnClear entities into explos
    cavern::StripMap *map = nullptr;
    neon::DarwinRandom *rand = nullptr;
    GameServices *game = nullptr; // stored into the explo
};

// centity::run. Moves with the map, dies when y < -0.7 or y > 0.7 (clearing its cell's launched
// byte so it can be launched again), and while clearAll is set replaces itself by an
// ExplosionBirth (returned; the caller adds it to theGame's debris list +0x2e0).
std::optional<ExplosionBirth> centityRun(MapEntity &e, MapEntityWorld &world);

// centity::draw: hidden entities are not drawn and get drawDelay = 1; others draw as
// entity::draw (the core renderer). Returns whether it is to be called.
} // namespace minos
