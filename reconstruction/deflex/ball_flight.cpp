#include "ball_flight.h"
#include <cmath>

namespace deflex {
void BallFlight::init(const LevelSetup &level, bool demo) {
    auto cell = demo ? CellCoords{level.grid.cols / 2, level.grid.rows / 2} : level.ballCell;
    position = logical = level.grid.getPos(cell.x, cell.y);
    previousCell = cell;
    velocity = {level.definition.real(0x5ec), level.definition.real(0x5f4)};
    turnSpeed = float(std::fabs(velocity.x == 0 ? velocity.y : velocity.x));
    left = top = 0;
    right = level.grid.cols;
    bottom = level.grid.rows;
    turnedInCell = false;
    radius = 0.03125f;
}
BallFlight::Prediction BallFlight::predict(const CellGrid &grid, bool placementQueued) const {
    auto cell = grid.getCellCoords(logical);
    auto delta = velocity;
    // 64028 loads the binary64 literal 0.1 at 6436c. It is not an inferred gain.
    if (grid.peek(cell.x, cell.y) == 3 && !turnedInCell)
        delta = delta * 0.1;
    auto next = logical + delta;
    return {next, grid.getCellCoords2(next), placementQueued};
}
void BallFlight::advance(const Prediction &prediction, BallFlightContext &c) {
    auto &grid = c.level.grid;
    auto &events = c.events;
    auto next = prediction.next;
    const auto edge = prediction.boundary;
    // Touch/particle prepass has already prepared the ball's depth and colour.
    events.trail(1, *this);
    const auto rowNotes = [&] {
        if (edge.y >= 0 && edge.y < grid.rows)
            for (int x = 0; x < grid.cols; ++x)
                if (grid.peek(x, edge.y) > 100)
                    events.note(x + edge.y);
    };
    const auto columnNotes = [&] {
        if (edge.x >= 0 && edge.x < grid.cols)
            for (int y = 0; y < grid.rows; ++y)
                if (grid.peek(edge.x, y) > 100)
                    events.note(y + edge.x);
    };
    if ((velocity.x < 0 && edge.x <= left) || (!(velocity.x < 0) && right < edge.x)) {
        const double old = velocity.x;
        velocity.x = -old;
        turnedInCell = false;
        next.x = next.x - old;
        rowNotes();
    }
    if ((velocity.y < 0 && edge.y <= top) || (!(velocity.y < 0) && bottom < edge.y)) {
        const double old = velocity.y;
        velocity.y = -old;
        turnedInCell = false;
        next.y = next.y - old;
        columnNotes();
    }
    logical = next;
    const auto cell = grid.getCellCoords(logical);
    static constexpr uint32_t randomColours[7] = {0xff0000ff, 0xff8000ff, 0xffff00ff, 0x80ff00ff,
                                                  0x00ff00ff, 0x00ff80ff, 0x00ffffff};
    const auto randomColour = [&] { return randomColours[c.random.next() % 7]; };
    if (cell.x != previousCell.x || cell.y != previousCell.y) {
        grid.blendRadius(0.5f, logical, 0.016666668f, c.level.definition.word(0x5dc), 0.3f);
        if (c.secondaryTouch) {
            events.note(cell.x == previousCell.x ? cell.y : cell.x);
            grid.blendRadius(0.4f, logical, 0.008333334f, randomColour(), 0.9f);
        }
        if (c.cuedNote) {
            events.note(c.cuedNote->x);
            events.note(c.cuedNote->y);
            c.cuedNote.reset();
        }
        previousCell = cell;
        turnedInCell = false;
    }
    const auto offset = grid.getPosInCell(logical);
    bool placed = false;
    if (prediction.queuedAtEntry && cell.x >= 0 && cell.x < grid.cols && cell.y >= 0 &&
        cell.y < grid.rows) {
        if (grid.peek(cell.x, cell.y) < 101) {
            turnedInCell = false;
            // The owner preserves the original capacity-one queue through the prepass.
            const int orientation = c.input.pending.value();
            c.input.pending.reset();
            ++c.level.deflectorsCreated;
            placeDeflector(grid, cell.x, cell.y, orientation != 0, (c.level.flags & 0x10) != 0);
            placed = true;
        } else
            c.input.pending.reset();
    }
    if (velocity.x == 0) {
        if (offset.x < 0)
            logical.x += 0.003;
        if (offset.x != 0 && offset.x >= 0)
            logical.x -= 0.003;
    }
    if (velocity.y == 0) {
        if (offset.y < 0)
            logical.y += 0.003;
        if (offset.y != 0 && offset.y >= 0)
            logical.y -= 0.003;
    }
    const unsigned glyph = unsigned(grid.peek(cell.x, cell.y)) & 255;
    const bool vertical = velocity.x == 0;
    const bool crossesCentre = vertical ? std::fabs(offset.y) <= std::fabs(velocity.y)
                                        : std::fabs(offset.x) <= std::fabs(velocity.x);
    const auto blast = [&] {
        for (int dx = -1; dx < 2; ++dx)
            for (int dy = -1; dy < 2; ++dy)
                events.blow(cell.x + dx, cell.y + dy);
    };
    const auto notes = [&] {
        events.note(cell.x);
        events.note(cell.y);
    };
    if (!turnedInCell && (placed || crossesCentre)) {
        if (glyph == 101 || glyph == 102) {
            auto &tile = grid.cell(cell.y, cell.x);
            const double old = vertical ? velocity.y : velocity.x;
            float speed = turnSpeed;
            if ((glyph == 101 && old >= 0) || (glyph == 102 && old < 0))
                speed = -speed;
            if (vertical) {
                velocity.y = 0;
                velocity.x = double(speed);
            } else {
                velocity.x = 0;
                velocity.y = double(speed);
            }
            turnedInCell = true;
            if (!c.level.flippingDeflectors)
                tile.dingDeflector();
            else {
                grid.poke(cell.x, cell.y, glyph == 101 ? 'f' : 'e');
                if (c.demo) {
                    tile.scale = float(double(tile.scale) - 0.2);
                    if (tile.scale < 0) {
                        tile.ch = 0x20;
                        tile.fgOn = 0;
                        c.demoReactionFrames = 60;
                    }
                }
            }
            grid.blendRadius(0.2f, position, 0.008333334f, randomColour(), 0.7f);
            events.spark(40, *this);
            notes();
        }
        if (glyph == 104 || glyph == 105) {
            if (vertical)
                velocity.y = -velocity.y;
            else
                velocity.x = -velocity.x;
            notes();
            if (glyph == 104)
                blast();
            else
                grid.cell(cell.y, cell.x).setBCC(0.02f, 0x40008010);
        } else if (glyph == 106) {
            const double gain = double(c.level.definition.scalar(0xe1c)) + 1.03;
            turnSpeed = float(double(turnSpeed) * gain);
            velocity = velocity * gain;
            events.spark(140, *this);
            notes();
            blast();
        } else if (glyph == 3) {
            c.pendingParticleLoss += 40;
            const auto speak = [&] {
                if (!c.voicePlayed) {
                    static constexpr int voice[6] = {0, 1, 2, 3, 4, 4};
                    const auto first = c.random.next(), second = c.random.next();
                    events.sample(voice[(second & 1) + (first & 3)]);
                    c.voicePlayed = true;
                }
            };
            // The original vertical and horizontal branches have different
            // sound/spark order. Effects can draw RNG, so keep that distinction.
            if (vertical) {
                speak();
                events.spark(140, *this);
            } else {
                events.spark(140, *this);
                speak();
            }
            turnedInCell = true;
        } else if (glyph == 0 && (c.level.flags & 0x10)) {
            auto &tile = grid.cell(cell.y, cell.x);
            tile.smash(c.random);
            tile.ch = 3;
            turnedInCell = true;
        }
    }
    position = logical;
    const float saved = radius;
    radius = float(double(radius) * 1.6);
    events.contactTargets(*this);
    events.contactHazards(*this);
    radius = saved;
}
} // namespace deflex
