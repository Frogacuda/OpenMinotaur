// Source-rule checks of complete map-actor families. Recording services expose
// callback order; they are not implementations of the remaining game effects.
#include "actors.h"
#include "particles.h"
#include "main_ball.h"
#include "neon/entity_list.h"
#include <cstdio>
#include <cmath>
#include <memory>
#include <cstring>
#include <functional>
using namespace deflex;
struct Services : ActorServices {
    Particles *effects = nullptr;
    neon::DarwinRandom *random = nullptr;
    std::vector<int> notes, samples, goals;
    std::vector<ScoreNotice> messages;
    std::vector<neon::vec2d> explosions;
    std::vector<HazardParticleBirth> particles;
    std::string order;
    int restored = 0, sparks = 0;
    void restoreBallParticles(BallFlight &, int n) override {
        restored += n;
        order += 'R';
    }
    void sample(int n, int) override {
        samples.push_back(n);
        order += 'V';
    }
    void note(int n) override {
        notes.push_back(n);
        order += 'N';
    }
    void spark(int n, const Entity &e) override {
        sparks += n;
        order += 'S';
        if (effects)
            effects->spark(n, e, *random);
    }
    void derez(const Entity &, int, float, int) override {
        order += 'D';
    }
    void plinger(int n) override {
        order += 'P';
        if (effects)
            effects->plinger(n);
    }
    void message(const ScoreNotice &n) override {
        messages.push_back(n);
        order += 'M';
    }
    void bonusGlow(neon::vec2d p) override {
        order += 'G';
        if (effects)
            effects->bonusGlow(p);
    }
    void achievement(int n) override {
        goals.push_back(n);
    }
    void hazardParticle(const HazardParticleBirth &n) override {
        particles.push_back(n);
        order += 'F';
        if (effects)
            effects->hazardParticle(n);
    }
    void explosion(neon::vec2d p) override {
        explosions.push_back(p);
        order += 'X';
        if (effects)
            effects->explosion(p);
    }
};
struct Renderer : ActorRenderer {
    std::vector<ActorSprite> sprites;
    void emit(const ActorSprite &n) override {
        sprites.push_back(n);
    }
};
struct BoardActors : LevelBirths {
    ActorContext &c;
    std::vector<std::unique_ptr<Entity>> actors;
    explicit BoardActors(ActorContext &context) : c(context) {}
    void deflector(const DeflectorButton &) override {}
    void target(const TargetBirth &b) override {
        auto n = std::make_unique<NumberedTarget>(c, b);
        neon::entities::prepend(c.targets, *static_cast<Entity *>(n.get()));
        actors.push_back(std::move(n));
    }
    void hazard(neon::vec2d p, int k) override {
        actors.push_back(std::make_unique<Hazard>(c, p, k));
    }
    void weight(neon::vec2d p) override {
        actors.push_back(std::make_unique<Weight>(p));
    }
    void bomb(neon::vec2d p) override {
        auto n = std::make_unique<Bomb>(c, p);
        neon::entities::prepend(c.targets, *static_cast<Entity *>(n.get()));
        actors.push_back(std::move(n));
    }
};
struct Records : BallRecords {
    int best = 1000;
    bool firstFour = false;
    int bestMoves() const override {
        return best;
    }
    void setBestMoves(int n) override {
        best = n;
    }
    bool firstFourInOneMove() const override {
        return firstFour;
    }
};
struct FlightServices : BallFlightEvents {
    MainGameBall &ball;
    ActorContext &c;
    Particles &particles;
    std::function<void()> contact;
    int trails = 0;
    FlightServices(MainGameBall &b, ActorContext &world, Particles &p)
        : ball(b), c(world), particles(p) {}
    void note(int n) override {
        c.services.note(n);
    }
    void trail(int n, const BallFlight &) override {
        ++trails;
        particles.trail(n, ball, c.level.flags, c.random);
    }
    void spark(int n, const BallFlight &) override {
        particles.spark(n, ball, c.random);
    }
    void blow(int x, int y) override {
        blowCell(c.level, x, y, [&](neon::vec2d p) { particles.explosion(p); });
    }
    void sample(int n) override {
        c.services.sample(n, c.volume);
    }
    void contactTargets(BallFlight &) override {}
    void contactHazards(BallFlight &) override {
        if (contact)
            contact();
    }
};
int main() {
    int failures = 0;
    auto check = [&](bool ok, const char *rule) {
        if (!ok) {
            ++failures;
            std::fprintf(stderr, "%s\n", rule);
        }
    };
    Services services;
    neon::DarwinRandom random(11);
    auto record = level::kLevelRecords[0];
    record.columns = record.rows = 8;
    record.flags = 0;
    std::memset(record.map, 0, sizeof record.map);
    LevelSetup level(record);
    ActorContext c{level, random, services};
    Entity firstBall;
    firstBall.pos = {0.4, 0.4};
    c.balls = &firstBall;
    BoardActors births(c);
    buildLevelBoard(level, record, births);
    TargetBirth birth{{0, 0}, {1, 1}, 51, 0xff0000ff, 0.02f, 0, 0, 0};
    NumberedTarget target(c, birth);
    check(random.draws() == 1 && target.kind == 1 && target.numberSize == (0.02f * 3.0f) / 5.0f,
          "target constructor preserves initial spin draw and number geometry");
    target.mass = 0;
    target.vel = {0.001, 0};
    target.drag = {1, 1};
    target.movementFlags = 1;
    target.run(c);
    check(target.anchor.x == 0.001 && target.pos.x == 0,
          "target movement changes anchor; draw publishes contact position");
    target.marker = 1;
    Renderer renderer;
    target.draw(c, renderer, {512, 512});
    check(target.pos.x == 0.001 && target.scale.x == 2 && renderer.sprites.size() == 1,
          "ordered target draw publishes anchor and doubles marker-one body");
    check(target.colour.a == 0, "first target update starts at zero alpha");
    target.run(c);
    check(target.colour.a == double(0.01f), "target alpha uses float product and double increment");
    target.movementFlags = 0;
    target.behaviourFlags = 2;
    target.spinPhase = 3.13f;
    target.spinLow = target.spinHigh = 0.1f;
    float expected = 3.13f + 0.1f;
    expected = float(double(expected) - 6.2831853072);
    target.run(c);
    check(target.spinPhase == expected,
          "target oscillation wraps with original binary64 constants");

    Entity other;
    other.pos = {0.25, 0};
    other.mass = 2;
    other.dead = true;
    Entity subject;
    subject.mass = 1;
    const auto force = attract(subject, &other, 1000000);
    check(force.x > 0 && force.y == 0 && subject.nearestDistance == 128,
          "attraction uses rounded scaled distance and does not discard dead list members");
    subject.flags = 1;
    check(attract(subject, &other, 1000000).x == 0 && subject.nearestDistance == 512,
          "subject flags gate attraction");
    subject.flags = 0;
    other.pos = subject.pos;
    check(std::isnan(attract(subject, &other, 1000000).x),
          "coincident attraction retains original zero-times-infinity result");

    services = Services{};
    random.seed(11);
    BallFlight ball;
    ball.init(level);
    NumberedTarget locked(c, birth);
    locked.marker = 2;
    c.frameCount = 0;
    const auto before = random.draws();
    locked.hitBy(c, ball);
    locked.hitBy(c, ball);
    check(c.pendingParticleLoss == 2 && services.sparks == 40 && services.samples.size() == 1 &&
              random.draws() == before + 2,
          "locked target damages on each contact; warning voice draws only once");
    services = Services{};
    c.voicePlayed = false;
    c.targets = &target;
    target.next = nullptr;
    target.marker = 1;
    target.phase = 0;
    c.framesSinceTarget = 20;
    c.multiplier = 1;
    c.score = 0;
    c.bestScore = 5;
    c.lastScore = 1;
    target.value = 10;
    target.hitBy(c, ball);
    check(target.phase == 1 && target.marker == 0 && c.multiplier == 2 && c.score == 0 &&
              target.award == 20,
          "ordered collection advances marker chain and defers score award");
    check(!services.messages.empty() && services.messages.front().text == "x2" &&
              services.order.substr(0, 4) == "RVPM",
          "collection orders particle restore, sample, plinger and multiplier notice");
    target.collectProgress = 1;
    target.run(c);
    check(target.dead && c.score == 20 && services.messages.back().text == "LAST ATTEMPT BEATEN",
          "collection awards score at flight end, preferring last-score notice");

    // Each variant's source init configuration, including sprite index and damage.
    constexpr int frames[] = {51, 39, 64, 70, 75, 51, 80, 89, 89, 96, 98, 64};
    constexpr int damage[] = {5, 20, 1, 10, 5, 5, 50, 2, 5, 5, 15, 100};
    level.hazardsCreated = 1;
    for (int i = 0; i < 12; ++i) {
        Hazard h(c, {0, 0}, i);
        check(h.frame == frames[i] && h.damage == damage[i],
              "all twelve hazard init variants retain sprite and damage");
    }
    Hazard lift(c, {0, 0}, 6);
    check(lift.vel.y == 0.004 && lift.movementFlags == 42 && lift.colour.a == 0,
          "lift creation count controls speed, flags and literal source colour");
    services = Services{};
    c.secondaryGain = 0;
    c.levelFinished = false;
    firstBall.pos = {0.45, 0.4};
    Hazard wall(c, {0.1, 0.1}, 1);
    wall.mass = 0;
    wall.vel = {0.03, 0};
    wall.drag = {1, 1};
    const auto start = level.grid.getCellCoords(wall.pos),
               end = level.grid.getCellCoords(wall.pos + wall.vel);
    level.grid.cell(start.y, end.x).ch = 104;
    wall.run(c);
    check(wall.pos.x == 0.1 && wall.vel.x == -0.03 && services.notes.empty(),
          "hazard wall reflection cancels movement and does not emit a cell-change note");
    level.grid.cell(start.y, end.x).ch = 0;
    wall.vel = {0.03, 0};
    wall.run(c);
    check(wall.pos.x == 0.13 && services.notes.size() == 1 && services.notes[0] == end.x,
          "unblocked hazard crossing emits the column note");
    Hazard passing(c, {0.1, 0.1}, 2);
    passing.mass = 0;
    passing.vel = {0.03, 0};
    passing.drag = {1, 1};
    level.grid.cell(start.y, end.x).ch = 104;
    passing.run(c);
    check(passing.pos.x == 0.13, "motion mode two passes through occupied cells");
    services = Services{};
    firstBall.pos = {0.12, 0.1};
    c.frameCount = 0;
    Hazard emitter(c, {0.1, 0.1}, 11);
    emitter.run(c);
    check(services.particles.size() == 1 && services.particles[0].frame == 30,
          "nearby frame-64 hazard emits its native particle birth");
    c.voicePlayed = false;
    emitter.hitBy(c);
    check(c.ravished && services.sparks == 20,
          "frame-64 contact marks ravished and emits damage feedback");
    Hazard xOnly(c, {0.48, 0}, 5);
    xOnly.mass = 0;
    xOnly.vel = {0.02, 0};
    xOnly.run(c);
    check(xOnly.pos.x == 0.48 && xOnly.vel.x < 0, "nonwrapping hazard cancels at the .49 boundary");
    lift.pos = {0, 0.499};
    lift.vel = {0, 0.004};
    lift.run(c);
    check(lift.pos.y < -.49 && lift.vel.y > 0, "lift wraps the vertical edge without reflecting");

    Weight weight({0, -0.2});
    firstBall.pos = {0, 0};
    weight.run(c);
    check(weight.phase == 1 && weight.pos.y == -0.2, "weight arms before its first falling step");
    weight.run(c);
    check(weight.vel.y == 0.0006 && weight.pos.y == -0.19940000000000002,
          "weight uses source gravity and position sequence");
    weight.pos.y = 0.49;
    weight.run(c);
    check(weight.phase == 2 && weight.vel.y == -0.01, "weight reverses after passing the floor");
    firstBall.pos.x = 0.4;
    weight.pos.y = -0.199;
    weight.run(c);
    check(weight.phase == 0 && weight.vel.y == 0 && weight.pos.y < weight.restPosition.y,
          "returning weight stops without snapping back to its anchor");
    weight.vel.x = 0.03;
    weight.hitBy(c);
    check(weight.vel.x == -0.03 && weight.pos.x == -0.03,
          "weight contact reverses and immediately applies horizontal velocity");

    services = Services{};
    Bomb bomb(c, {0, 0});
    bomb.oscillation = 3.13f;
    bomb.run(c);
    check(bomb.oscillation == float(double(float(double(3.13f) + 0.1)) - 6.2831853072),
          "bomb uses double increment and wrap with float stores");
    for (int y = 0; y <= level.grid.rows; ++y)
        for (int x = 0; x <= level.grid.cols; ++x)
            level.grid.cell(x, y) = GridCell{};
    placeDeflector(level.grid, 1, 2, 0, false);
    placeDeflector(level.grid, 3, 1, 1, false);
    level.grid.cell(1, 3).durability = 1;
    level.grid.cell(2, 1).durability = 1;
    level.grid.cell(2, 2).ch = 104;
    level.grid.cell(2, 2).durability = 1;
    bomb.hitBy(c);
    check(bomb.dead && services.explosions.size() == 3 &&
              services.explosions[0].y < services.explosions[1].y,
          "bomb destroys deflectors in row-major order before its own explosion");
    check(level.grid.peek(2, 2) == 104 && services.samples.size() == 8 &&
              services.samples.back() == services.samples.front() + 7,
          "bomb leaves walls intact and schedules eight consecutive samples");
    services = Services{};
    c.levelFinished = true;
    wall.run(c);
    weight.run(c);
    bomb.run(c);
    check(wall.dead && weight.dead && bomb.dead && services.explosions.size() == 3,
          "finished-level actors retire through source explosion calls");
    target.phase = 0;
    target.run(c);
    check(target.dead && target.phase == 2,
          "target derez retires its owner during level completion");

    Particles particles;
    random.seed(11);
    Entity emitterEntity;
    emitterEntity.pos = {0.1, -0.2};
    particles.spark(3, emitterEntity, random);
    check(random.draws() == 4 && particles.particles(ParticleLayer::World).size() == 3,
          "spark has one common speed draw plus one angle per particle");
    const auto &spark = particles.particles(ParticleLayer::World).front();
    check(spark.noWrap && spark.frame == 28 && spark.pos.x == 0.1 && spark.to.a == 0,
          "spark sets native no-wrap, sprite, location and fading colour");
    particles.spark(0, emitterEntity, random);
    check(random.draws() == 5, "zero-count spark still consumes the original common speed draw");
    emitterEntity.age = 127;
    particles.trail(2, emitterEntity, 0x20, random);
    const auto &trail = particles.particles(ParticleLayer::Overlay).front();
    check(
        random.draws() == 12 && trail.gridDisplacement && trail.startScale == 4 &&
            trail.endScale == 4 && trail.vel.x == 0,
        "trail draws radius once then angle/scale/fade per particle, retaining big-trail override");
    Particle p;
    p.pos = {0.1, 0.2};
    p.vel = {0.01, -0.02};
    p.drag = {1, 1};
    p.life = 0.5f;
    p.fade = 0.5f;
    p.startScale = 1;
    p.endScale = 3;
    p.from = {1, 0, 0, 1};
    p.to = {0, 0, 1, 0};
    p.size = 0.02f;
    p.scale = {2, 3};
    renderer.sprites.clear();
    p.draw(renderer, {512, 256});
    const auto rendered = renderer.sprites.front();
    check(rendered.size.x == double(0.02f) * 512 * 2 * 2 &&
              rendered.size.y == 2 * double(0.02f) * 256 * 3 && rendered.colour.a == 0.5,
          "particle drawing uses single-size geometry and source float interpolation");
    p.run(level.grid);
    check(!p.dead && p.life == 0 && p.pos.x == 0.11 && p.age == 1,
          "particle reaches zero life without early retirement");
    p.run(level.grid);
    check(p.dead && p.life == 0, "particle retires only after life goes below zero");
    services = Services{};
    particles.plinger(3);
    particles.run(ParticleLayer::Overlay, level.grid, false, services);
    check(services.notes.empty(), "plinger waits for game's note gate");
    for (int i = 0; i < 4; ++i)
        particles.run(ParticleLayer::Overlay, level.grid, true, services);
    check(services.notes == std::vector<int>({2, 2, 3, 3, 4, 4}),
          "plinger preserves initial count and includes its last retiring note pair");
    particles.clear();
    services = Services{};
    services.effects = &particles;
    services.random = &random;
    c.levelFinished = false;
    c.targets = nullptr;
    c.framesSinceTarget = 0;
    c.multiplier = 1;
    c.voicePlayed = false;
    NumberedTarget collected(c, birth);
    c.targets = &collected;
    const auto hitDraws = random.draws();
    collected.hitBy(c, ball);
    check(particles.particles(ParticleLayer::World).size() == 41 &&
              random.draws() - hitDraws >= 45 && random.draws() - hitDraws <= 46,
          "target collection runs real bonus and spark consumers on the shared random stream");

    // Exercise actual synchronous actor initialization for every original board.
    for (const auto &original : level::kLevelRecords) {
        LevelSetup board(original);
        neon::DarwinRandom rng(13);
        Services sink;
        ActorContext world{board, rng, sink};
        world.balls = &firstBall;
        BoardActors actors(world);
        buildLevelBoard(board, original, actors);
        check(int(actors.actors.size()) >= board.targetsCreated + board.hazardsCreated,
              "all original boards construct their real actor families");
        for (auto &actor : actors.actors)
            if (actor->kind == 1) {
                auto &n = static_cast<NumberedTarget &>(*actor);
                n.run(world);
                Renderer r;
                n.draw(world, r, {512, 512});
            }
    }
    // Full ball initialization/prepass/flight/draw, with real particle consumers.
    {
        auto boardRecord = record;
        std::strcpy(boardRecord.map[3], "...*");
        LevelSetup board(boardRecord);
        Services sink;
        neon::DarwinRandom rng(21);
        ActorContext world{board, rng, sink};
        BoardActors actors(world);
        buildLevelBoard(board, boardRecord, actors);
        board.targetsRemaining = 1;
        board.definition.setReal(0x5ec, 0);
        board.definition.setReal(0x5f4, 0.00390625);
        MainGameBall player;
        world.balls = &player;
        player.init(world, 0x80ff00ff);
        player.mesh.lossRate = 0;
        check(rng.draws() == 384 && player.mesh.startupFrames == 50,
              "ball init draws X/Y rotation and shrink for each of 128 shell points");
        for (const auto &point : player.mesh.points)
            check(std::fabs(point.direction.x * point.direction.x +
                            point.direction.y * point.direction.y +
                            point.direction.z * point.direction.z - 1) < 0.000001,
                  "source float matrix rotations produce unit shell directions");
        Particles pool;
        FlightServices flightEvents(player, world, pool);
        DeflectorInput controls;
        Records records;
        BallFlightContext movement{board, controls, rng, flightEvents, &world};
        std::array<BallTouch, 3> touches{};
        const auto startPosition = player.pos;
        for (int i = 0; i < 50; ++i) {
            player.run(world, movement, records, touches);
            Renderer r;
            player.draw(world, r, {512, 512}, 0x80ff00ff, false, false);
        }
        check(player.pos.x == startPosition.x && player.pos.y == startPosition.y &&
                  player.age == 50 && player.mesh.startupFrames == 0 && flightEvents.trails == 0,
              "fifty startup frames advance age and shell draw without flight or trail emission");
        flightEvents.contact = [&] {
            world.pendingParticleLoss += 2;
            world.voicePlayed = true;
        };
        touches[0] = {true, {-0.2, 0.1}};
        touches[1] = {true, {0.2, 0.1}};
        touches[2] = {true, {0, 0.2}};
        player.run(world, movement, records, touches);
        check(player.pos.y == startPosition.y + 0.00390625 && flightEvents.trails == 1 &&
                  pool.particles(ParticleLayer::Overlay).size() == 1,
              "first active ball frame enters real trail and flight paths");
        check(world.pendingParticleLoss == 2 && movement.pendingParticleLoss == 2 &&
                  world.voicePlayed && movement.voicePlayed,
              "contact callbacks and flight kernel share damage/voice state without a copy-back "
              "overwrite");
        check(movement.cuedNote.has_value() && movement.secondaryTouch &&
                  world.reactionFrames == 60 && board.grid.displacement[1].centre.x == -0.2 * 512,
              "three touches retain note, secondary-contact, reaction and displacement roles");
        flightEvents.contact = {};
        player.run(world, movement, records, touches);
        check(player.mesh.depleted == 2 && world.pendingParticleLoss == 0 &&
                  world.ballFraction == 126.0f / 128.0f,
              "queued contact damage detaches particles on the following frame and sets score "
              "fraction");
        const auto damagedDraws = rng.draws();
        player.mesh.restore(1, rng);
        check(player.mesh.depleted == 1 && player.mesh.points[1].state == 0 &&
                  player.mesh.points[1].radius == 10 && rng.draws() == damagedDraws + 1,
              "restoration decrements depleted before updating state, shrink and radius of that "
              "same point");
        records.firstFour = true;
        board.targetsRemaining = 0;
        board.deflectorsCreated = 1;
        const auto beforeCompletion = player.pos;
        player.run(world, movement, records, touches);
        check(world.levelFinished && world.levelSucceeded && records.best == 1 &&
                  player.mesh.depleted == 128 && player.pos.y == beforeCompletion.y,
              "last target completion detaches the shell, stores best moves and gates movement");
        check(std::find(sink.goals.begin(), sink.goals.end(), 0) != sink.goals.end() &&
                  std::find(sink.goals.begin(), sink.goals.end(), 1) != sink.goals.end(),
              "completion retains local achievement hooks for one-move and first-four records");
        const float life = player.mesh.points[5].life;
        const auto orbit = player.mesh.rotation;
        Renderer paused;
        player.draw(world, paused, {512, 512}, 0xff0000ff, true, false);
        check(player.mesh.points[5].life == life && player.mesh.rotation.x == orbit.x &&
                  player.mesh.rotation.y == orbit.y,
              "paused shell drawing retains orbit, detached life and history time");
        Renderer fading;
        player.draw(world, fading, {512, 512}, 0xff0000ff, false, false);
        check(player.mesh.points[5].life < life && player.mesh.points[5].historyCursor == 1,
              "active shell drawing advances detached life and history exactly once");
        for (auto &samplePoint : player.mesh.points) {
            samplePoint.state = 3;
            samplePoint.life = 0;
        }
        world.levelFinished = world.levelSucceeded = false;
        Renderer dead;
        player.draw(world, dead, {512, 512}, 0, false, true);
        check(!world.levelFinished, "an exhausted demo shell does not end the level");
        player.draw(world, dead, {512, 512}, 0, false, false);
        check(world.levelFinished && !world.levelSucceeded,
              "exhausted normal shell ends the run without reporting level success");
    }
    {
        // Source pongBall 562b8/617f4, pongBat 564e8/61938 and startup 5bc2c.
        LevelSetup board(level::kLevelRecords[23]);
        neon::DarwinRandom rng(42);
        Services events;
        board.grid.setSize(21, 20); // contact feedback requires the original cell store
        ActorContext game(board, rng, events);
        Entity player;
        player.pos = {-.45, .2};
        game.balls = &player;
        PongBall pong;
        PongBat left(-.4), right(.4);
        BallFlight probe;
        check(pong.pos.y == .1 && std::fabs(pong.vel.length() - double(.01f)) < 1e-15 &&
                  pong.scale.y == 1.2,
              "Pong startup retains source position, normalized speed and sprite scale");
        pong.pos = {-.399, 0};
        pong.vel = {-.01, .001};
        auto draws = rng.draws();
        pong.run(game);
        check(rng.draws() == draws + 1 && pong.vel.x > 0 &&
                  std::fabs(pong.vel.length() - double(pong.speed)) < 1e-15,
              "Pong side rebound near the player draws exactly one random steering coordinate");
        player.pos = {-.2, -.1};
        pong.pos = {.399, .1};
        pong.vel = {.01, 0};
        draws = rng.draws();
        pong.run(game);
        check(rng.draws() == draws && pong.vel.x < 0 && pong.vel.y < 0,
              "Pong far side rebound aims at the player without a random draw");
        pong.pos = {0, .449};
        pong.vel = {.001, .01};
        pong.run(game);
        check(pong.vel.y == -.01 && std::fabs(pong.pos.y - .449) < 1e-15,
              "Pong top/bottom rebound removes the old vertical step after reversing velocity");
        game.targetHit = true;
        const float previous = pong.speed;
        pong.run(game);
        check(!game.targetHit && pong.speed == float(double(previous) + .001),
              "one shared pickup flag raises Pong speed once with original float storage");
        game.pongMotion = ActorContext::PongMotion{{.1, .2}, {.01, 0}};
        right.run(game);
        left.run(game);
        check(
            right.pos.y == .2 * double(.7f) && left.pos.y == 0,
            "only the approached Pong bat follows, using the source narrowed interpolation factor");
        events = Services{};
        game.voicePlayed = false;
        game.pendingParticleLoss = 0;
        game.frameCount = 0;
        pong.vel = {.01, .02};
        const double x = pong.pos.x;
        pong.hitBy(game, probe);
        right.hitBy(game, probe);
        check(game.pendingParticleLoss == 30 && events.samples.size() == 1 && events.sparks == 40 &&
                  pong.vel.x == -.01 && pong.pos.x == x - .01,
              "Pong ball/bat hits queue 25/5 damage, share the warning latch and reflect only the "
              "ball");
        game.levelFinished = true;
        events.explosions.clear();
        pong.run(game);
        left.run(game);
        right.run(game);
        check(pong.dead && left.dead && right.dead && events.explosions.size() == 3,
              "all three Pong actors retire via source explosions on completion");
    }
    if (failures)
        return 1;
    std::puts("Deflex map-actor source rules passed; all 52 boards construct real "
              "target/hazard/weight/bomb births.");
}
