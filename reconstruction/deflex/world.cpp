#include "world.h"
#include "neon/entity_list.h"
#include "neon/foundation_rules.h"
#include <stdexcept>
#include <cmath>
#include <cstdio>
#include <cstring>
namespace deflex {
namespace {
#include "note_scales.inc"
neon::colour dynamic(int type, uint32_t frame) {
    const auto c = neon::paletteColour(type, 1, frame, {neon::WashRule::TriangularRainbow, 12, 4});
    return {c.r, c.g, c.b, c.a};
}
const level::LevelRecordData &playableRecord(int index) {
    if (index < 0 || index >= kPlayableLevelCount)
        throw std::out_of_range("Deflex checkpoint level");
    return level::kLevelRecords[index];
}
} // namespace
World::World(int index, Progress &records, const std::vector<neon::vec2d> &sizes, uint32_t seed,
             WorldMode mode)
    : spriteSizes_(sizes), mode_(mode), random(seed), level(playableRecord(index)),
      context(level, random, *this), flightContext(level, input, random, *this, &context),
      progress(records) {
    context.levelIndex = index;
    context.bestScore = progress.bestScores[size_t(index)];
    context.lastScore = progress.lastScores[size_t(index)];
    context.pointScale = level.definition.scalar(0x608);
    context.volumeScale = level.definition.scalar(0x60c);
    context.reactionFrames = 8;
    if (mode == WorldMode::Scenery) {
        level.grid.setSize(21, 17);
        return;
    }
    const bool demo = mode == WorldMode::Attract;
    flightContext.demo = demo;
    buildLevelBoard(level, level::kLevelRecords[index], *this, demo);
    // doSpecialStartup(1), after map births and before mainGameBall::init.
    // The original prepended list runs ball, right bat, left bat, then map actors.
    const int startup = level.definition.integer(0xd08);
    if (!demo && startup == 1) {
        prependHazard(std::make_unique<PongBat>(-.4));
        prependHazard(std::make_unique<PongBat>(.4));
        prependHazard(std::make_unique<PongBall>());
    } else if (startup != 0)
        throw std::logic_error("unknown Deflex special startup");
    input.particleCost = level.definition.integer(0x600);
    ball_ = std::make_unique<MainGameBall>();
    ball_->init(context, particleColour, demo);
    context.balls = ball_.get();
    if (demo)
        return;
    gridView.quick("0", {0, .35}, {2, 1.5}, -1, 0, 0);
    gridView.quick("MULTIPLIER 2.00", {0, .4}, {1.5, 1.3}, -1, 5, 1, .25f, 1);
    gridView.quick(context.bestScore > 0 ? "BEST: " + std::to_string(context.bestScore) : " ",
                   {0, .44}, {1.2, .8}, -1, 6, 2, .125f, .5f);
}
void World::deflector(const DeflectorButton &b) {
    particles.add(ParticleLayer::Overlay).button = b;
}
void World::target(const TargetBirth &b) {
    auto p = std::make_unique<NumberedTarget>(context, b);
    neon::entities::prepend(context.targets, *static_cast<Entity *>(p.get()));
    targets_.push_front(std::move(p));
}
void World::prependHazard(std::unique_ptr<Entity> p) {
    neon::entities::prepend(hazardHead_, *p);
    hazards_.push_front(std::move(p));
}
void World::hazard(neon::vec2d pos, int type) {
    prependHazard(std::make_unique<Hazard>(context, pos, type));
}
void World::weight(neon::vec2d pos) {
    prependHazard(std::make_unique<Weight>(pos));
}
void World::bomb(neon::vec2d pos) {
    auto p = std::make_unique<Bomb>(context, pos);
    neon::entities::prepend(context.targets, *static_cast<Entity *>(p.get()));
    targets_.push_front(std::move(p));
}
void World::sample(int id, int volume) {
    sounds.push_back({id, volume});
}
void World::note(int n) {
    const unsigned key = unsigned(n) & 63;
    if (notePlayed_[key])
        return;
    notePlayed_[key] = true;
    const auto &scale = NoteScales[musicScale_ % 13];
    const int count = scale[0];
    if (count < 1)
        return;
    const int pitch =
        scale[key % unsigned(count) + 1] + musicOffset_ + (int(key / unsigned(count)) % 4 + 1) * 12;
    sample(pitch % 60 + 14, int(context.ballFraction * 12.0f));
}
void World::spark(int n, const Entity &e) {
    particles.spark(n, e, random);
}
void World::trail(int n, const BallFlight &) {
    particles.trail(n, *ball_, level.flags, random);
}
void World::restoreBallParticles(BallFlight &, int count) {
    ball_->mesh.restore(count, random);
}
void World::blow(int x, int y) {
    blowCell(level, x, y, [&](neon::vec2d p) { explosion(p); });
}
void World::derez(const Entity &entity, int kind, float gain, int frames) {
    const auto size = spriteSizes_.at(size_t(entity.frame + 2));
    // derez mode 0: source pixelShatter(...,{gain,gain},frames,0,1,0,2).
    if (kind != 0)
        throw std::logic_error("unsupported derez caller");
    const int rows = int(size.y) / 2, dy = int(size.y) / rows, dx = int(size.x);
    const neon::vec2d scale{entity.scale.x / size.x * double(dx),
                            entity.scale.y / size.y * double(dy)};
    double y = entity.pos.y + double(entity.size) * (entity.scale.y / double(dy)) * .5 * -2;
    double spread = -1;
    for (int row = 0; row < rows; ++row) {
        auto &p = particles.add(ParticleLayer::Overlay);
        p.fragment = true;
        p.size = entity.size;
        p.scale = scale;
        p.frame = entity.frame;
        p.colour = entity.colour;
        p.pos = {entity.pos.x + (entity.scale.x / double(dx)) * .5 * double(entity.size) * -2, y};
        const double velocity = spread * .01;
        p.vel = {velocity * entity.pos.x * double(gain), velocity * double(gain)};
        p.fragmentFade = 1.0f / float(frames);
        p.regionOrigin = {.5, double(float(row * dy)) + .5};
        p.regionSize = {double(dx), double(dy)};
        spread += double(2.0f / float(rows));
        y += scale.y * double(entity.size) + scale.y * double(entity.size);
    }
}
void World::message(const ScoreNotice &m) {
    gridView.quick(m.text, m.position, m.scale, m.hold, m.row, m.sequence, m.rate, m.fade);
    auto &l = gridView.lines[size_t(m.row)];
    l.state = m.state;
    l.inScale = m.destinationScale;
    l.to = m.destination;
    // QuickMsg publishes occupancy immediately, before another award in the
    // same invocation can choose between the two notification rows.
    level.grid.messageLines[size_t(m.row)] = {l.state, l.text.empty() ? 0 : 1};
}
void World::achievement(int n) {
    // achieveGoal 5efd0: standard single-player only; the session mask gates
    // feedback/RNG, while the host retains earned identities across launches.
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
    if (n < 0 || n >= 10 || mode_ == WorldMode::Attract || (sessionAchievements & (1u << n)))
        return;
    const bool occupied = level.grid.messageLines[11].count != 0;
    ScoreNotice notice;
    notice.text = names[n];
    notice.row = occupied ? 12 : 11;
    notice.position = {level.definition.real(0x650),
                       level.definition.real(0x658) + (occupied ? .1 : 0)};
    notice.destination = {occupied ? double(-.55f) : -.5, occupied ? double(.35f) : double(.4f)};
    message(notice);
    sessionAchievements |= 1u << n;
    progress.achievements[size_t(n)] = true;
    sample(int(random.next() % 3) + 11, int(float(context.volume) * 1.2f));
    for (int pitch : {26, 30, 33, 38, 42, 45, 50, 54, 57})
        sample(pitch, context.volume);
}
void World::contacts(BallFlight &b, Entity *head) {
    if (ball_->dead || ball_->skipDrawFrames)
        return;
    bool blocked = false;
    const auto origin = b.position;
    for (auto *p = head; p; p = p->next) {
        if (p->dead)
            continue;
        const float sum = b.radius + p->size;
        const float distance = float((origin - p->pos).length());
        if (sum <= distance)
            continue;
        if (p->skipDrawFrames) {
            blocked = true;
            continue;
        }
        if (!blocked)
            p->hitBy(context, b);
    }
}
void World::updateList(std::list<std::unique_ptr<Entity>> &list, Entity *&head) {
    for (auto it = list.begin(); it != list.end();) {
        auto *p = it->get();
        p->run(context);
        if (p->dead) {
            if (p->prev)
                p->prev->next = p->next;
            else
                head = p->next;
            if (p->next)
                p->next->prev = p->prev;
            it = list.erase(it);
        } else
            ++it;
    }
}
void World::beginTick(bool music, bool advanceClock) {
    if (advanceClock)
        ++context.frameCount;
    context.voicePlayed = false;
    notePlayed_.fill(false);
    noteGate = (context.frameCount & 1) == 0;
    // Deflex e788 advances green/blue only; the red phase remains fixed.
    cyclingColour_.generate(6);
    static constexpr int types[] = {8, 5, 0, 4, 2, 3, 1, 6, 7, 9};
    for (int i = 0; i < 10; ++i)
        context.palette[size_t(i)] = dynamic(types[i], context.frameCount);
    // Owner run 67fd0 packs the dynamic endpoint before iclerp f268.
    neon::colour base, endpoint;
    base.unpackRGBA(0x400080f0);
    endpoint.unpackRGBA(context.palette[0].packRGBA());
    particleColour = base.lerp(endpoint, context.targetFlash).packRGBA();
    context.targetFlash = float(double(context.targetFlash) * .93);
    if (context.numberFlash > 0)
        context.numberFlash = float(double(context.numberFlash) - .05);
    if (music && --context.reactionFrames < 1) {
        musicPair_ += 2;
        if (level.definition.integer(0xd10 + musicPair_ * 4) < 0)
            musicPair_ = 0;
        musicScale_ = level.definition.integer(0xd14 + musicPair_ * 4);
        musicOffset_ = level.definition.integer(0xd10 + musicPair_ * 4);
    }
}
void World::tick(bool paused) {
    // The original formats HUD strings before palette preparation and actor
    // updates. Pickups/decay during this tick appear on the following tick.
    if (mode_ == WorldMode::Standard && showHUD) {
        gridView.lines[0].text = std::to_string(context.score);
        char multiplier[80];
        std::snprintf(multiplier, sizeof multiplier, "MULTIPLIER %.2f",
                      double(context.pointScale * context.ballFraction * context.multiplier));
        gridView.lines[5].text = multiplier;
    }
    beginTick();
    auto &message = level::kLevelRecords[context.levelIndex]
                        .messages[std::min(messageIndex_, level::kMessageSlots - 1)];
    if (mode_ == WorldMode::Standard && messageIndex_ < level::kMessageSlots && message.row >= 0 &&
        messageDelay_ >= 0 && gridView.lines[size_t(message.row + 1)].text.empty()) {
        if (messageDelay_ > 0)
            --messageDelay_;
        else {
            const int o = 0x610 + message.row * 32;
            gridView.quick(
                message.text, {level.definition.real(o), level.definition.real(o + 8)},
                {level.definition.real(o + 16), level.definition.real(o + 24)}, 129,
                message.row + 1, message.colourSequence,
                [&] {
                    float f;
                    auto bits = message.phaseRateBits;
                    std::memcpy(&f, &bits, 4);
                    return f;
                }(),
                [&] {
                    float f;
                    auto bits = message.charStepBits;
                    std::memcpy(&f, &bits, 4);
                    return f;
                }());
            ++messageIndex_;
            messageDelay_ =
                messageIndex_ < level::kMessageSlots
                    ? level::kLevelRecords[context.levelIndex].messages[messageIndex_].delayFrames
                    : -1;
        }
    }
    if (!paused) {
        ++context.framesSinceTarget;
        if (context.framesSinceTarget > level.definition.integer(0xe10))
            context.multiplier = 1;
        ++context.levelFrames;
        std::array<BallTouch, 3> touches;
        for (int i = 0; i < 3; ++i)
            touches[size_t(i)] = {input.touches[size_t(i)].down, input.touches[size_t(i)].position};
        ball_->run(context, flightContext, *this, touches);
        particles.run(ParticleLayer::World, level.grid, noteGate, *this);
        updateList(targets_, context.targets);
        updateList(hazards_, hazardHead_);
    }
    input.levelFinished = context.levelFinished;
    input.palette = context.palette;
    particles.run(ParticleLayer::Overlay, level.grid, noteGate, *this, &input, this);
    if (mode_ != WorldMode::Standard)
        return;
    // The original result state discards failed-attempt points, then commits
    // records only after target score flights have left the list.
    if (context.levelFinished && !context.levelSucceeded)
        context.score = 0;
    if (context.levelSucceeded && scoreSettled())
        progress.bestScores[size_t(context.levelIndex)] =
            std::max(progress.bestScores[size_t(context.levelIndex)], context.score);
}
void World::resultCue() {
    // run state 10, once the target list has settled: restart the first scale
    // pair with a 100-tick reaction clock and a twelve-step chord plinger.
    musicPair_ = 0;
    musicScale_ = level.definition.integer(0xd14);
    musicOffset_ = level.definition.integer(0xd10);
    context.reactionFrames = 100;
    auto &particle = particles.add(ParticleLayer::Overlay);
    particle.tone = Plinger{};
    particle.tone->mode = 1;
    particle.tone->count = 0;
    particle.tone->frames = 12;
}
void World::drawGrid(GridRenderer &quads, ActorRenderer &sprites, neon::vec2d view, bool paused) {
    std::array<neon::colour, 11> palette;
    palette[0] = cyclingColour_;
    for (int i = 0; i < 10; ++i)
        palette[size_t(i + 1)] = context.palette[size_t(i)];
    gridView.draw(level.grid, palette.data(), random, paused, quads, sprites, view);
}
void World::drawActors(ActorRenderer &renderer, neon::vec2d view, bool overlay, bool paused) {
    const bool dimFragments = paused || mode_ == WorldMode::Attract;
    const double actorAlpha = dimFragments ? double(.3f) : 1.0;
    if (overlay) {
        particles.draw(ParticleLayer::Overlay, renderer, view, dimFragments);
        if (ball_) {
            const float alpha = float(ball_->colour.a);
            ball_->colour.a = actorAlpha;
            ball_->draw(context, renderer, view, particleColour, paused,
                        mode_ == WorldMode::Attract);
            ball_->colour.a = double(alpha);
        }
        return;
    }
    particles.draw(ParticleLayer::World, renderer, view, dimFragments);
    // showJoy 4cb08: world particles, targets (+1c0), hazards (+1c8),
    // overlay (+1c4), main balls (+1b8); no sorting into actor categories.
    for (auto &p : targets_) {
        const float alpha = float(p->colour.a);
        p->colour.a = actorAlpha;
        p->draw(context, renderer, view);
        p->colour.a = double(alpha);
    }
    for (auto &p : hazards_) {
        const float alpha = float(p->colour.a);
        p->colour.a = actorAlpha;
        p->draw(context, renderer, view);
        p->colour.a = double(alpha);
    }
}
} // namespace deflex
