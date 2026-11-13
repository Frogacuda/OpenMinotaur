// Production-world integration, not an original-execution witness or a solver.
#include "world.h"
#include <fstream>
#include <regex>
#include <cstdio>
#include <cmath>
using namespace deflex;
struct Draws : ActorRenderer, GridRenderer {
    int sprites = 0, quads = 0;
    bool finite = true, alphaText = false, additiveTiles = false;
    void emit(const ActorSprite &s) override {
        ++sprites;
        finite &=
            std::isfinite(s.position.x) && std::isfinite(s.position.y) && std::isfinite(s.depth);
    }
    void quad(const std::array<GridVertex, 6> &v, bool additive, int) override {
        ++quads;
        alphaText |= !additive;
        additiveTiles |= additive;
        for (auto &p : v)
            finite &= std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) &&
                      std::isfinite(p.u) && std::isfinite(p.v);
    }
};
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    std::ifstream file(argv[1]);
    const std::string xml((std::istreambuf_iterator<char>(file)), {});
    const std::regex entry(
        R"(<Spentry[^>]*>\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),)");
    std::vector<neon::vec2d> sizes;
    for (auto i = std::sregex_iterator(xml.begin(), xml.end(), entry); i != std::sregex_iterator();
         ++i)
        sizes.push_back({double(std::stoi((*i)[4])), double(std::stoi((*i)[5]))});
    // The original 113 entries retain their indices; two appended host
    // pause labels, without changing gameplay sprites or original fixtures.
    if (sizes.size() != 115 || sizes[113].x != 112 || sizes[114].x != 64) {
        std::fprintf(stderr, "unexpected sprite catalog/host pause labels\n");
        return 1;
    }
    int failures = 0;
    auto check = [&](bool ok, const char *text) {
        if (!ok) {
            ++failures;
            std::fprintf(stderr, "%s\n", text);
        }
    };
    Progress progress;
    {
        World w(0, progress, sizes, 1, WorldMode::Scenery);
        TargetBirth birth;
        birth.frame = 15;
        birth.colour = 0xffffffff;
        birth.size = .02f;
        birth.spriteScale = {1, 1};
        w.target(birth);
        w.hazard({.2, .2}, 0);
        struct Packets : ActorRenderer {
            std::vector<ActorSprite> values;
            void emit(const ActorSprite &s) override {
                values.push_back(s);
            }
        } normal, paused;
        w.drawActors(normal, {512, 512}, false, false);
        w.drawActors(paused, {512, 512}, false, true);
        check(normal.values.size() == 2 && normal.values[0].frame == 17 &&
                  normal.values[1].frame == 53,
              "showJoy submits target list before hazard list independently of simulation order");
        check(paused.values.size() == 2 && paused.values[0].colour.a == double(.3f) &&
                  paused.values[1].colour.a == double(.3f),
              "owner applies source pause dimming to both actor lists");
    }
    for (int stage : {8, 18}) {
        // Original CAMEL MAZE / DESPERATION contain five messages, including
        // entries beyond the first three descriptors.
        World w(stage, progress, sizes);
        w.ball().mesh.startupFrames = 10000;
        Draws out;
        bool seen[5] = {};
        for (int tick = 0; tick < 2600; ++tick) {
            w.tick(false);
            w.drawGrid(out, out, {512, 512}, false);
            for (int slot = 0; slot < 5; ++slot) {
                const auto &m = level::kLevelRecords[stage].messages[slot];
                seen[slot] |= w.gridView.lines[size_t(m.row + 1)].text == m.text;
            }
        }
        bool all = true;
        for (bool value : seen)
            all &= value;
        check(all && level::kLevelRecords[stage].messages[5].delayFrames < 0,
              "owner schedules all five original instructions and reaches their real sentinel");
    }
    {
        // Actual iOS achieveGoal observations: 20 layouts, ten goal names,
        // ten samples per accepted award, and one shared RNG draw (seed 17).
        static constexpr const char *names[] = {"MINIMALIST",
                                                "ZEN",
                                                "ON SOURCE",
                                                "GOLDEN AGE OF DEFLEX",
                                                "NICE MULTIPLIER!!",
                                                "GREAT MULTIPLIER!!",
                                                "THREE BAT BULLS GET!",
                                                "DISAPPOINTED MINOTAUR!",
                                                "UNLOCKY!",
                                                "RAVISHED BY THE MINOTAUR!"};
        constexpr int chord[] = {13, 26, 30, 33, 38, 42, 45, 50, 54, 57};
        for (int goal = 0; goal < 10; ++goal)
            for (bool occupied : {false, true}) {
                Progress saved;
                saved.achievements[size_t(goal)] = true;
                World w(0, saved, sizes);
                w.random.seed(17);
                if (occupied)
                    w.level.grid.messageLines[11].count = 1;
                w.achievement(goal);
                const int row = occupied ? 12 : 11;
                const auto &notice = w.gridView.lines[size_t(row)];
                bool audio = w.sounds.size() == 10;
                for (size_t i = 0; i < w.sounds.size(); ++i)
                    audio &= i < 10 && w.sounds[i].sample == chord[i] &&
                             w.sounds[i].volume == (i ? 8 : 9);
                check(audio && w.random.state() == 285719 && w.random.draws() == 1,
                      "award preserves original sample sequence/volume and consequential RNG draw");
                check(notice.text == names[goal] &&
                          notice.to.x == (occupied ? double(-.55f) : -.5) &&
                          notice.to.y == (occupied ? double(.35f) : double(.4f)) &&
                          w.sessionAchievements == (1u << goal),
                      "saved lifetime flags do not suppress fresh session feedback or original "
                      "layout");
                w.achievement(goal);
                check(w.sounds.size() == 10 && w.random.draws() == 1,
                      "same session award is gated before feedback/RNG");
            }
        World w(0, progress, sizes);
        w.achievement(4);
        w.achievement(5);
        check(w.gridView.lines[11].text == "NICE MULTIPLIER!!" &&
                  w.gridView.lines[12].text == "GREAT MULTIPLIER!!",
              "consecutive awards see immediate QuickMsg occupancy before any draw");
        World demo(0, progress, sizes, 17, WorldMode::Attract);
        const auto draws = demo.random.draws();
        demo.achievement(0);
        check(demo.sounds.empty() && !demo.sessionAchievements && demo.random.draws() == draws,
              "attract gate precedes achievement feedback/RNG");
    }
    {
        Progress saved;
        saved.bestMoves[0] = 2;
        saved.bestScores[0] = 100;
        saved.lastScores[0] = 10;
        World w(0, saved, sizes);
        check(w.context.lastScore == 10 && w.gridView.lines[6].text == "BEST: 100",
              "owner binds previous attempt and displays best score rather than moves");
        auto *target = static_cast<NumberedTarget *>(w.context.targets);
        target->phase = 1;
        target->collectProgress = 1;
        target->award = 20;
        w.tick(false);
        check(w.context.score == 20 && w.gridView.lines[11].text == "LAST ATTEMPT BEATEN",
              "persisted last score reaches production target settlement feedback");
        check(w.gridView.lines[0].text == "0", "HUD precedes score settlement in the source owner");
        w.tick(true);
        check(w.gridView.lines[0].text == "20",
              "next tick publishes the settled score even while paused");
        struct Capture : ActorRenderer {
            double alpha = -1;
            void emit(const ActorSprite &s) override {
                if (s.region)
                    alpha = s.colour.a;
            }
        } normal, paused, demo;
        auto &fragment = w.particles.add(ParticleLayer::Overlay);
        fragment.fragment = true;
        fragment.colour.a = .8;
        w.drawActors(normal, {512, 512}, true, false);
        w.drawActors(paused, {512, 512}, true, true);
        World attract(0, saved, sizes, 1, WorldMode::Attract);
        auto &bit = attract.particles.add(ParticleLayer::Overlay);
        bit.fragment = true;
        bit.colour.a = .8;
        attract.drawActors(demo, {512, 512}, true, false);
        check(normal.alpha == .8 && paused.alpha == .3 && demo.alpha == .3 &&
                  fragment.colour.a == .8,
              "shatter draw dims pause/demo packets without mutating stored colour");
        // Captured by an unpublished harness from original getDynamicColour, packRGBA
        // and iclerp. Current red/yellow endpoints are already byte-exact;
        // this checks the production binding, not an observed quantization bug.
        constexpr uint32_t expected[2][3] = {{1866424563u, 2675917047u, 4294902015u},
                                             {1862295795u, 2667593975u, 4278190335u}};
        constexpr float flash[] = {.25f, .5f, 1.f};
        for (int frame : {0, 1, 2, 7, 12, 25, 63, 128, 255})
            for (int i = 0; i < 3; ++i) {
                w.context.frameCount = uint32_t(frame) - 1;
                w.context.targetFlash = flash[i];
                w.beginTick(false);
                check(w.particleColour == expected[(frame & 14) ? 1 : 0][i],
                      "production ball colour matches scoped original packed calculation");
            }
    }
    for (int index = 0; index < kPlayableLevelCount; ++index) {
        World world(index, progress, sizes, 11);
        Draws draws;
        int audio = 0;
        std::printf("World stage %d: %s\n", index + 1, level::kLevelRecords[index].name());
        // Ordinary empty-cell flight is silent without a touch cue, a second
        // touch, a collision or a pickup. Exercise the original second-touch
        // cell-note path rather than assuming every crossed cell makes sound.
        world.input.touches[1] = {true, {.25, .25}};
        const auto initialTargets = world.level.targetsRemaining;
        for (int frame = 0; frame < 180; ++frame) {
            world.tick(false);
            world.drawGrid(draws, draws, {512, 512}, false);
            world.drawActors(draws, {512, 512}, false, false);
            world.drawActors(draws, {512, 512}, true, false);
            audio += int(world.sounds.size());
            world.sounds.clear();
        }
        check(draws.finite && draws.sprites > 1000 && draws.quads > 1000 && draws.alphaText &&
                  draws.additiveTiles,
              "live world/grid produces finite native sprite, tile and proportional text commands");
        check(world.gridView.lines[2].text == level::kLevelRecords[index].name(),
              "record descriptor zero displays the level name before instruction text");
        check(world.context.levelFrames == 180 && world.context.framesSinceTarget > 0 &&
                  world.level.targetsRemaining <= initialTargets,
              "production clock runs ball, target and timeout paths");
        check(audio > 0, "live ball/cell paths schedule imported samples");
        const auto before = world.ball().pos;
        const int frameBefore = world.context.levelFrames;
        world.tick(true);
        world.drawGrid(draws, draws, {512, 512}, true);
        world.drawActors(draws, {512, 512}, true, true);
        check(world.ball().pos.x == before.x && world.ball().pos.y == before.y &&
                  world.context.levelFrames == frameBefore,
              "paused world preserves ball simulation clock and position");
        world.input.nativeHeld = {true, false};
        world.tick(false);
        const int placed = world.level.deflectorsCreated;
        check(world.input.pending.has_value(),
              "native deflector activation queues after the current ball update");
        for (int i = 0; i < 8; ++i)
            world.tick(false);
        check(!world.input.pending && world.level.deflectorsCreated <= placed + 1,
              "held native input consumes at most its one queued placement");
        world.input.nativeHeld = {};
        world.tick(false);
        // Explicit contacts exercise the native collection/award/completion
        // pipeline. Teleporting the contact probe is not evidence of a played route.
        for (int pass = 0; pass < 100 && world.level.targetsRemaining > 0; ++pass) {
            bool hit = false;
            for (auto *p = world.context.targets; p; p = p->next)
                if (p->kind == 1) {
                    auto &t = static_cast<NumberedTarget &>(*p);
                    if (t.phase == 0 && t.marker < 2) {
                        world.ball().flight.position = t.pos;
                        world.contactTargets(world.ball().flight);
                        hit = true;
                        break;
                    }
                }
            if (!hit)
                break;
        }
        check(world.level.targetsRemaining == 0,
              "native contact dispatch collects all eligible standard-stage targets");
        world.level.deflectorsCreated = 1;
        world.tick(false);
        check(world.context.levelFinished && world.context.levelSucceeded,
              "native last-target completion gates motion and records success");
        for (int i = 0; i < 180; ++i) {
            world.tick(false);
            world.drawActors(draws, {512, 512}, false, false);
            world.drawActors(draws, {512, 512}, true, false);
        }
        check(world.scoreSettled() && world.context.score > 0 &&
                  progress.bestMoves[size_t(index)] == 1 &&
                  progress.bestScores[size_t(index)] == world.context.score,
              "collected targets finish their score flights before advancement and preserve "
              "per-level records");
    }
    check(progress.achievements[0] && progress.achievements[1],
          "first-four one-move achievement hooks survive integration");
    {
        // CUPS OF TEA's map markers make a real ordered contact chain. A locked
        // pickup damages the shell; collecting marker one advances the chain.
        World world(28, progress, sizes);
        NumberedTarget *first = nullptr, *locked = nullptr;
        for (auto *p = world.context.targets; p; p = p->next)
            if (auto *t = dynamic_cast<NumberedTarget *>(p)) {
                if (t->marker == 1)
                    first = t;
                if (t->marker == 2)
                    locked = t;
            }
        check(first && locked, "ordered stage builds its first and second original marker groups");
        if (first && locked) {
            const int remaining = world.level.targetsRemaining;
            world.ball().flight.radius = 0;
            world.ball().flight.position = locked->pos;
            world.contactTargets(world.ball().flight);
            check(locked->phase == 0 && locked->marker == 2 &&
                      world.level.targetsRemaining == remaining &&
                      world.context.pendingParticleLoss > 0,
                  "production contact with a locked pickup queues damage without collecting it");
            world.ball().flight.position = first->pos;
            world.contactTargets(world.ball().flight);
            check(first->phase == 1 && locked->marker == 1 &&
                      world.level.targetsRemaining == remaining - 1,
                  "collecting the active pickup advances the remaining ordered markers");
        }
    }
    {
        // DEJA MOO: empty floor is smashed on passage; a subsequent centre
        // crossing in the destroyed cell queues 40 particles of shell damage.
        World world(22, progress, sizes);
        world.ball().mesh.startupFrames = 0;
        CellCoords cell;
        bool smashed = false;
        // The source gates on crossing the cell centre, not merely occupying
        // an empty cell. Do not assume the initial rounded centre qualifies.
        for (int i = 0; i < 40 && !smashed; ++i) {
            world.tick(false);
            cell = world.level.grid.getCellCoords(world.ball().flight.logical);
            smashed = world.level.grid.peek(cell.x, cell.y) == 3;
        }
        check(smashed, "DEJA MOO centre crossing smashes its real floor cell");
        world.ball().flight.logical =
            world.level.grid.getPos(cell.x, cell.y) - world.ball().flight.velocity * .05;
        world.ball().flight.position = world.ball().flight.logical;
        world.ball().flight.turnedInCell = false;
        world.tick(false);
        check(world.context.pendingParticleLoss == 40,
              "destroyed-floor recontact reaches the shared damage queue");
    }
    {
        // Actual special startup, ordering and contact dispatch in WHAT A PONG.
        World world(23, progress, sizes);
        Draws out;
        world.tick(false);
        check(world.context.pongMotion.has_value(),
              "Pong startup ball publishes its state through the production hazard list");
        world.context.targetHit = true;
        world.tick(false);
        const auto p = world.context.pongMotion->position;
        world.ball().flight.position = p;
        world.ball().flight.radius = 0;
        world.contactHazards(world.ball().flight);
        check(world.context.pendingParticleLoss == 25 && !world.context.targetHit,
              "production Pong contact queues ball damage and its update consumes the pickup flag");
        world.tick(false);
        check(world.ball().mesh.depleted == 25,
              "production Pong damage reaches actual shell detachment");
        world.context.levelFinished = true;
        world.tick(false);
        check(world.scoreSettled(), "Pong-stage completion retires targets, bombs and special "
                                    "actors without dangling state");
        world.drawActors(out, {512, 512}, false, false);
        check(out.sprites > 0 && out.finite,
              "Pong completion effects render through the native particle path");
    }
    {
        // Source bomb::hitBy 5cc50: scan only deflectors, rows before columns,
        // then emit the local explosion and eight consecutive sample events.
        World world(4, progress, sizes);
        Bomb *bomb = nullptr;
        for (auto *p = world.context.targets; p; p = p->next)
            if (auto *b = dynamic_cast<Bomb *>(p)) {
                bomb = b;
                break;
            }
        check(bomb != nullptr, "MULTIPLES constructs its original mirrored bomb actors");
        if (bomb) {
            const auto origin = bomb->pos;
            placeDeflector(world.level.grid, 0, 0, 0, false);
            placeDeflector(world.level.grid, 1, 0, 1, false);
            world.level.grid.cell(0, 0).durability = world.level.grid.cell(0, 1).durability = 1;
            const auto remaining = world.level.targetsRemaining;
            world.ball().flight.position = origin;
            world.ball().flight.radius = 0;
            world.contactTargets(world.ball().flight);
            check(bomb->dead && world.level.grid.peek(0, 0) == 0 &&
                      world.level.grid.peek(1, 0) == 0,
                  "production bomb contact clears both player deflector orientations");
            check(world.level.grid.peek(2, 2) == 104 && world.level.targetsRemaining == remaining,
                  "bomb clears deflectors without consuming breakable walls or numbered targets");
            bool sequence = world.sounds.size() == 8;
            for (size_t i = 1; i < world.sounds.size(); ++i)
                sequence &= world.sounds[i].sample == world.sounds[0].sample + int(i);
            check(sequence,
                  "production bomb dispatch preserves eight overlapping original samples");
            check(world.particles.particles(ParticleLayer::World).size() == 3,
                  "production bomb uses real explosion effects for both deflectors and its own "
                  "position");
            world.tick(false);
            bool retired = true;
            for (auto *p = world.context.targets; p; p = p->next)
                retired &= p->pos.x != origin.x || p->pos.y != origin.y;
            check(retired, "dead bomb is removed from the contact list on the next world update");
        }
    }
    {
        // OPEN SKY's movement flags 55 wrap both axes and retain attraction.
        // Set a boundary case, rather than claim random flight covers wrapping.
        World world(9, progress, sizes);
        auto *target = static_cast<NumberedTarget *>(world.context.targets);
        target->anchor = {.49, .49};
        target->pos = target->anchor;
        target->vel = {.02, .02};
        world.tick(false);
        Draws out;
        world.drawActors(out, {512, 512}, false, false);
        check(target->anchor.x < -.48 && target->anchor.y < -.48 && target->vel.x > 0 &&
                  target->vel.y > 0,
              "wrapping target crosses both edges without reflection and publishes its contact "
              "position on draw");
        check(target->pos.x == target->anchor.x && target->pos.y == target->anchor.y,
              "source target draw publishes the new anchor for subsequent contacts");
    }
    {
        // DISKETTES sets bit 0x40: each pickup accelerates the native flight,
        // including the separate turnSpeed used by subsequent deflectors.
        World world(12, progress, sizes);
        auto *target = static_cast<NumberedTarget *>(world.context.targets);
        world.ball().flight.position = target->pos;
        world.ball().flight.velocity = {0, .01};
        const auto speed = world.ball().flight.turnSpeed;
        world.contactTargets(world.ball().flight);
        check(world.ball().flight.velocity.y > .0102 && world.ball().flight.turnSpeed > speed &&
                  target->phase == 1,
              "native DISKETTES pickup accelerates current velocity and future turn speed");
    }
    {
        // LABYRINTH creates variant-2 hazards from map '1'. Its hit queues one
        // shell particle loss and sets ravished; the ball applies it next tick.
        World world(13, progress, sizes);
        bool found = false;
        for (int y = 0; y < world.level.grid.rows && !found; ++y)
            for (int x = 0; x < level::kMapStride; ++x)
                if (level::kLevelRecords[13].map[y][x] == '1') {
                    world.ball().flight.position = world.level.grid.getPos(x, y);
                    world.ball().flight.radius = 0;
                    found = true;
                    break;
                }
        check(found && world.level.hazardsCreated > 0, "LABYRINTH creates its actual map hazards");
        world.contactHazards(world.ball().flight);
        check(world.context.pendingParticleLoss == 1 && world.context.ravished,
              "world hazard dispatch preserves variant-2 damage and the source contact flag");
        world.tick(false);
        check(world.ball().mesh.depleted == 1 && world.context.pendingParticleLoss == 0,
              "hazard damage reaches the shell prepass on the following tick");
    }
    World failure(0, progress, sizes);
    Draws sink;
    failure.ball().mesh.depleted = 128;
    for (auto &p : failure.ball().mesh.points) {
        p.state = 3;
        p.life = 0;
    }
    failure.drawActors(sink, {512, 512}, true, false);
    failure.tick(false);
    int fragments = 0;
    for (const auto &p : failure.particles.particles(ParticleLayer::Overlay))
        fragments += p.fragment;
    check(failure.context.levelFinished && !failure.context.levelSucceeded && fragments > 0 &&
              failure.context.targets == nullptr,
          "exhausted shell retires remaining targets through actual sprite-region shatter effects");
    bool rejected = false;
    try {
        World unsupported(kPlayableLevelCount, progress, sizes);
    } catch (const std::out_of_range &) {
        rejected = true;
    }
    check(rejected, "out-of-range stages are rejected after the complete 52-record sequence");
    if (failures)
        return 1;
    std::puts("Deflex live-world integration: all 52 stages, Pong startup/contacts/retirement, "
              "bombs, wrapping, acceleration, hazards, grid/text, input queue, pause, audio, score "
              "flights, completion and failure passed.");
}
