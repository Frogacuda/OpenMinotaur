// cavern_launch.h - theGame::launch (0x5ab00 of Minos_armv7): the map-character entity
// factory. stm::translate calls it the first time a cell holding an entity character is
// generated (cell flag low byte 0); launch creates the entity, adds it to one of two
// theGame lists and sets the cell's launched bit, so the regenerated line does not launch
// it again.
//
// Per handled character the original: `new`s the class, runs entity::entity, writes the
// fields below (byte +0x181 = 1, position, map cell, class presets), calls Class::init(game)
// when the class has one, applies post-init overrides, addEntity(list), marks the cell.
// Launch describes each allocation as a SpawnRequest; the host constructs and
// initializes the concrete entity before adding it to the selected list.
//
// Offsets are the original entity offsets (entity::entity base). The Minos entity is the
// Minotron entity plus a byte at +0x0a (constructor: 1) and a 4-byte member in +0xec..+0x104
// (constructor zeroes +0xfc); fields from +0x104 on sit 4 bytes later than in Minotron
// (matching constructor layouts). Names marked (Minotron) are the
// Minotron port's names for the same member; offset-named fields are class-specific.
#pragma once
#include "cavern_data.h"
#include "cavern_map.h"
#include "neon/source_random.h"
#include <cstdint>
#include <functional>
#include <optional>

namespace minos::cavern {

enum class EntityClass : uint8_t {
    ScrambleRocket,
    Faller,
    Turret,
    Rocket,
    CavernGoal,
    WavingMinotaur,
    OneUp,
    MotherSheep,
    Goat,
    Saucer,
};
// theGame entity lists (reset bits 0x20 / 0x40; both run by runUnpausedEntities).
enum class EntityList : uint8_t { List2e4, List2e8 }; // theGame+0x2e4, theGame+0x2e8

struct SpawnRequest {
    char character = 0;
    EntityClass cls{};
    EntityList list{};
    bool callsInit = false; // Class::init(game) runs between the two write groups

    // ---- written before init (or as the whole setup for classes without init) ----
    Vec2 position;           // +0x8c/+0x94: launch position, y lowered per class
    int column = 0, row = 0; // +0x178/+0x17c map cell
    std::optional<int32_t>
        spriteIndex; // +0x1c (Minotron spriteIndex: sprite number; cavernGoal queries frame +2)
    std::optional<float> rot; // +0x84 (Minotron rot: sprite angle, radians)

    // ---- written after init (rockets, motherSheep) or inline (cavernGoal/oneUp/minotaur) ----
    std::optional<int32_t> spriteIndexAfterInit; // +0x1c
    bool promoteSprite5To6 = false;              // 'X': if init left spriteIndex 5, make it 6
    bool colourWhite = false;                    // colour::white(+0x28, 1.0)
    std::optional<float> size;                   // +0x134 (Minotron +0x130 size: collision radius)
    std::optional<uint8_t> hidden; // +0x180 centity::draw: non-zero = not drawn (entity.h)
    std::optional<uint8_t>
        burstOnClear; // +0x181 final value (launch first sets 1): centity::run explo on clear
    std::optional<int32_t> drawDelay;     // +0x20 (Minotron drawDelay)
    bool game = false;                    // +0x168 = theGame* (Minotron +0x164 game)
    std::optional<double> scaleX, scaleY; // +0xdc/+0xe4 (Minotron scaleX/scaleY)
    std::optional<int> sizeQueryFrame;    // cavernGoal: sprite::getSize of this frame
    std::optional<float> phase184;        // +0x184 wavingMinotaur: float(rand(0, 7))
    std::optional<int32_t> word188;       // +0x188
    std::optional<Vec2> home1b0;          // +0x1b0 wavingMinotaur: copy of position
};

struct LaunchContext {
    const CavernDefinition *cavern = nullptr; // caverns[theGame+0x450]
    neon::DarwinRandom *rand = nullptr;       // the global libc rand stream (oxRNG::rand)
    // sprite::getSize for a sprite-map frame (cavernGoal aspect); returns {w, h}.
    std::function<Vec2(int frame)> spriteSize;
};

// theGame::launch. Returns the entity to create (nullopt: none) and marks the cell for
// every handled character - including 's' when the cavern has no saucers.
std::optional<SpawnRequest> launch(StripMap &map, const LaunchContext &ctx, char ch, Vec2 position,
                                   int column, int row);

} // namespace minos::cavern
