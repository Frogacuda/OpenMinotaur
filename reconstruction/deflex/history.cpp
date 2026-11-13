#include "history.h"
#include "neon/entity_list.h"
#include <cmath>
#include <algorithm>
namespace deflex {
namespace {
#include "history_scripts.inc"
#include "character_data.inc"
} // namespace
History::History(Progress &progress, const std::vector<neon::vec2d> &sizes, uint32_t seed)
    : world(0, progress, sizes, seed, WorldMode::Scenery),
      console(world.level.grid, world.gridView, world.random) {
    console.setMode(Console::Pet);
    startScript(0);
    phase = 1;
}
void History::startScript(int n) {
    console.start(HistoryScripts[n]);
}
void History::clearActors() {
    world.context.balls = nullptr;
    ball_.reset();
    targets_.clear();
    world.particles.clear();
    world.input.pending.reset();
}
void History::spawnBall(int mode) {
    ball_ = std::make_unique<CharacterBall>(*this, mode);
    // PET/VIC use the overlay list; only C64 belongs to the main ball list.
    world.context.balls = mode == 2 ? ball_.get() : nullptr;
}
History::CharacterBall::CharacterBall(History &h, int kind) : mode(kind) {
    const auto &c = h.console;
    left = c.left;
    right = c.right;
    top = c.top;
    bottom = c.bottom;
    if (kind != 0) {
        ++left;
        --right;
        ++top;
        --bottom;
    }
    const CellCoords cell = kind == 0 ? CellCoords{10, 10} : CellCoords{19, 12};
    previous = cell;
    logical = pos = h.world.level.grid.getPos(cell.x, cell.y);
    vel = {.005, 0};
    frame = kind == 2 ? 12 : 11;
    size = kind == 0 ? 0.023809524f : kind == 2 ? .025f : .0125f;
    colour.unpackRGBA(kind == 1 ? 0xffffffff : kind == 0 ? 0x0000ffff : 0x00ffffff);
}
bool History::lineNotes(int x, int y, int count, bool horizontal, bool blend) {
    bool sounded = false;
    auto &grid = world.level.grid;
    if (horizontal && (y < 0 || y >= grid.rows))
        return false;
    if (!horizontal && (x < 0 || x >= grid.cols))
        return false;
    for (int i = 0; i < count; ++i) {
        int a = x + (horizontal ? i : 0), b = y + (horizontal ? 0 : i);
        if (a < 0 || a >= (grid.cols + (horizontal ? 0 : 1)) || b < 0 || b > grid.rows)
            continue;
        const int glyph = grid.peek(a, b);
        if (blend)
            grid.cell(b, a).blendBCC(.01f, 0xffc00080, .2f);
        if (glyph > 100) {
            world.note(a + b);
            sounded = true;
        }
    }
    return sounded;
}
void History::spray(int count, neon::vec2d p, float speed, uint32_t rgba) {
    for (int i = 0; i < count; ++i) {
        auto &a = world.particles.add(ParticleLayer::World);
        a.pos = p;
        a.vel = {world.random.range(double(-speed), double(speed)),
                 world.random.range(double(-speed), double(speed))};
        a.from.unpackRGBA(rgba);
        a.to.unpackRGBA(0x10101010);
        a.endScale = 2;
        a.fade = .01f;
        a.frame = 0;
    }
}
void History::CharacterBall::run(History &h) {
    auto &w = h.world;
    auto &grid = w.level.grid;
    auto proposed = logical + vel;
    auto edge = grid.getCellCoords(proposed);
    ++age;
    // Touch sweeps illuminate their row/column and use the same palette notes.
    for (size_t i = 0; i < touchCells.size(); ++i) {
        const auto &t = w.input.touches[i];
        if (!t.down) {
            touchCells[i] = {-1, -1};
            continue;
        }
        if (t.position.y >= .3)
            continue;
        const auto cell = grid.getCellCoords(t.position);
        if (cell.x == touchCells[i].x && cell.y == touchCells[i].y)
            continue;
        if (cell.x == touchCells[i].x)
            h.lineNotes(cell.x, cell.y, right - left, true, true);
        else
            h.lineNotes(cell.x, cell.y, bottom - top, false, true);
        touchCells[i] = cell;
        if (cell.x >= 0 && cell.x <= grid.cols && cell.y >= 0 && cell.y <= grid.rows)
            grid.cell(cell.y, cell.x).setBSequence(.26666668f, TrailOut);
    }
    if ((vel.x < 0 && edge.x < left) || (vel.x >= 0 && edge.x > right)) {
        const double old = vel.x;
        vel.x = -old;
        proposed.x -= old;
        turned = false;
        h.lineNotes(old < 0 ? left : right, edge.y, right - left, true);
    }
    if ((vel.y < 0 && edge.y < top) || (vel.y >= 0 && edge.y > bottom)) {
        const double old = vel.y;
        vel.y = -old;
        proposed.y -= old;
        turned = false;
        h.lineNotes(edge.x, old < 0 ? top : bottom, bottom - top, false);
    }
    logical = proposed;
    const auto cell = grid.getCellCoords(logical);
    const auto offset = grid.getPosInCell(logical);
    auto future = cell;
    if (vel.x < 0 && offset.x < 0 && cell.x > left)
        --future.x;
    else if (vel.x > 0 && offset.x > 0 && cell.x < right)
        ++future.x;
    if (vel.y < 0 && offset.y < 0 && cell.y > top)
        --future.y;
    else if (vel.y > 0 && offset.y > 0 && cell.y < bottom)
        ++future.y;
    // Preserve the original signed poke and literal positive guard. Current
    // glyph dispatch below explicitly narrows to unsigned char in the original.
    if (w.input.pending && grid.peek(future.x, future.y) != 128 &&
        grid.peek(future.x, future.y) != 130) {
        h.console.poke(future.x, future.y, *w.input.pending ? 130 : 128, colour.packRGBA());
        w.input.pending.reset();
    }
    const bool vertical = vel.x == 0;
    const double off = vertical ? offset.y : offset.x;
    const double speed = std::fabs(vertical ? vel.y : vel.x);
    if (!turned && std::fabs(off) < speed) {
        auto &tile = grid.cell(cell.y, cell.x);
        tile.setBSequence(.13333334f, TrailOut);
        const unsigned glyph = unsigned(grid.peek(cell.x, cell.y)) & 255;
        if (glyph == 128 || glyph == 130) {
            const double old = vertical ? vel.y : vel.x;
            double redirected = double(.005f);
            if ((glyph == 128 && old >= 0) || (glyph == 130 && old < 0))
                redirected = -redirected;
            if (vertical)
                vel = {redirected, 0};
            else
                vel = {0, redirected};
            turned = true;
            if (mode != 1)
                h.console.poke(cell.x, cell.y, glyph == 128 ? 130 : 128, colour.packRGBA());
            const bool sounded = h.lineNotes(cell.x, cell.y, 10, glyph == 128);
            if (!sounded) {
                w.note(cell.x);
                w.note(cell.y);
            }
        } else if (glyph == 132) {
            h.spray(20, logical, .002f, colour.packRGBA());
            h.console.poke(cell.x, cell.y, 32, colour.packRGBA());
            w.sample(int(w.random.next() % 7) + 6,
                     int(float(w.context.volume) * w.context.volumeScale));
            h.unusedPickupBonus_ = std::min(99.f, h.unusedPickupBonus_ + 5);
        }
    }
    if (cell.x != previous.x || cell.y != previous.y) {
        previous = cell;
        turned = false;
    }
    if (frame == 11)
        pos = grid.getPos(cell.x, cell.y);
    else {
        pos = logical;
        colour = h.console.palette[C64Pulse[(uint32_t(age) >> 1) & 7]];
        for (auto &target : h.targets_)
            if (!target->dead && (pos - target->pos).length() < double(size + target->size)) {
                h.spray(120, target->pos, .004f, target->colour.packRGBA());
                w.sample(12, w.context.volume);
                h.unusedPickupBonus_ = std::min(99.f, h.unusedPickupBonus_ + 20);
                target->dead = true;
            }
    }
}
void History::spawnTargets() {
    for (int i = 0; i < 16; ++i) {
        auto target = std::make_unique<Entity>();
        auto &t = *target;
        t.pos = {world.random.range(-.3, .3), world.random.range(-.3, .3)};
        t.mass = -1000000;
        t.frame = int(world.random.next() & 3) + 13;
        t.colour.unpackRGBA(TargetColours[t.frame - 13]);
        t.vel = {world.random.range(-.005, .005), world.random.range(-.005, .005)};
        t.drag = {.98, .98};
        t.size = .03f;
        t.scale.y = 1.2;
        targets_.push_front(std::move(target));
    }
}
void History::runTargets() {
    for (auto i = targets_.begin(); i != targets_.end();) {
        auto &t = **i;
        t.vel += attract(t, world.context.balls, 0);
        if (t.pos.length() < .3)
            t.vel = t.vel * t.drag;
        t.pos += t.vel;
        if (t.pos.x < -.45 || t.pos.x > .45) {
            const double v = t.vel.x;
            t.vel.x = -v;
            t.pos.x -= v;
        }
        if (t.pos.y < -.45 || t.pos.y > .45) {
            const double v = t.vel.y;
            t.vel.y = -v;
            t.pos.y -= v;
        }
        if (t.dead)
            i = targets_.erase(i);
        else
            ++i;
    }
}
void History::tick(bool held, bool pressed, bool ok, bool paused) {
    if (paused)
        return;
    ++world.context.frameCount;
    switch (phase) {
    case 1:
        if (!console.busy()) {
            startScript(1);
            phase = 2;
        }
        break;
    case 2:
        if (!console.busy() && pressed) {
            startScript(2);
            phase = 3;
        }
        break;
    case 3:
        if (!console.busy()) {
            spawnBall(1);
            startScript(3);
            phase = 4;
        }
        break;
    case 4:
        if (!console.busy()) {
            world.deflector(DeflectorButton(1));
            world.deflector(DeflectorButton(0));
            phase = 5;
            world.context.frameCount = 0;
        }
        break;
    case 5:
    case 8:
    case 11: {
        if (world.context.frameCount < 1000 && !(world.context.frameCount & 31)) {
            int x = int(world.random.range(console.left + 2, console.right - 2)),
                y = int(world.random.range(console.top + 2, console.bottom - 2));
            uint32_t col = phase == 5 ? 0xffffffff : phase == 8 ? 0xff0000ff : 0xffff00ff;
            console.poke(x, y, 132, col);
            world.level.grid.cell(y, x).setFSequence(.5f, phase == 5   ? DiamondPulse
                                                          : phase == 8 ? VicDiamondPulse
                                                                       : C64DiamondPulse);
        }
        if (ok) {
            int old = phase;
            clearActors();
            if (old == 11) {
                console.smash(world.random);
                phase = 12;
                world.context.frameCount = 0;
            } else {
                startScript(old == 5 ? 4 : 6);
                phase = old == 5 ? 6 : 9;
            }
        }
        break;
    }
    case 6:
    case 9:
        if (!console.busy() && pressed) {
            startScript(phase == 6 ? 5 : 7);
            phase = phase == 6 ? 7 : 10;
        }
        break;
    case 7:
    case 10:
        if (!console.busy()) {
            spawnBall(phase == 7 ? 0 : 2);
            world.deflector(DeflectorButton(1));
            world.deflector(DeflectorButton(0));
            if (phase == 10)
                spawnTargets();
            phase = phase == 7 ? 8 : 11;
            world.context.frameCount = 0;
        }
        break;
    }
    // doPetDemo may reset the shared clock. The palette and note gate use that
    // resulting phase, not a separate continuously increasing tutorial clock.
    world.beginTick(false, false);
    // doPetDemo places PET and VIC in +1c4, C64 in +1b8. Overlay
    // controls are newer than those balls, so run before them.
    if (ball_ && ball_->mode == 2)
        ball_->run(*this);
    world.particles.run(ParticleLayer::World, world.level.grid, world.noteGate, world);
    runTargets();
    world.input.palette = world.context.palette;
    world.particles.run(ParticleLayer::Overlay, world.level.grid, world.noteGate, world,
                        &world.input, &world);
    if (ball_ && ball_->mode != 2)
        ball_->run(*this);
    console.run(held);
}
void History::drawGrid(GridRenderer &q, ActorRenderer &r, neon::vec2d size, bool paused) {
    world.gridView.cursorX = console.cursor ? console.x : -1;
    world.gridView.cursorY = console.y;
    world.gridView.cursorColour = console.palette[size_t(console.foreground)].packRGBA();
    world.gridView.draw(world.level.grid, console.palette.data(), world.random, paused, q, r, size);
}
void History::drawActors(ActorRenderer &out, neon::vec2d size, bool) {
    world.particles.draw(ParticleLayer::World, out, size);
    for (auto &p : targets_)
        p->drawEntity(out, size);
    if (trial()) {
        ActorSprite ok;
        ok.frame = 3;
        ok.position = {0, .43 * size.y};
        ok.size = {.168 * size.x, .168 * size.y};
        ok.colour = world.context.palette[4];
        out.emit(ok);
    }
    world.particles.draw(ParticleLayer::Overlay, out, size);
    // Draw order differs from update order: showJoy draws +1b8 after +1c4.
    // The older PET/VIC ball is likewise after its newer overlay controls.
    if (ball_)
        ball_->drawEntity(out, size);
}
} // namespace deflex
