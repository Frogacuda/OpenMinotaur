#include "world.h"
#include <algorithm>
namespace superoxwars {
void PresentationState::beginAttract() {
    // 418c4 attract-start branch: RGB=.2, scale=.99; alpha is not written.
    for (int i = 0; i < 3; ++i)
        feedback.colour[i] = .2f;
    feedback.rotation = feedback.x = feedback.y = 0;
    feedback.scaleX = feedback.scaleY = .99f;
}
void PresentationState::beginPlay() {
    // moo::run 421e0 selected-start branch only changes RGB and scale.
    for (int i = 0; i < 3; ++i)
        feedback.colour[i] = .2f;
    feedback.scaleX = feedback.scaleY = 1.f;
}
void PresentationState::run(bool demo, neon::DarwinRandom &random) {
    // 418c4: after scene/palette work, before the gameplay and UI lists.
    // This block also runs while paused. Demo clears the event counters only.
    if (demo) {
        pulse = 0;
        shakeTicks = 0;
        return;
    }
    if (pulse > 0) {
        pulse = float(double(pulse) * .95);
        if (double(pulse) < .04)
            pulse = 0;
        for (float &c : feedback.colour)
            c = pulse;
    }
    if (shakeTicks) {
        if (shakeTicks & 1) {
            textScroll.x = random.range(-.2, .2);
            textScroll.y = random.range(-.2, .2);
        }
        if (--shakeTicks == 0)
            textScroll = {};
    }
}
Colour unpack(uint32_t c) {
    return {double(c >> 24) / 255., double((c >> 16) & 255) / 255., double((c >> 8) & 255) / 255.,
            double(c & 255) / 255.};
}
void Entity::run() {
    neon::entities::integrate(position, velocity, damping, age, noWrap, wrapped, extent);
}
void Entity::prepare(std::vector<ActorDraw> &out) {
    if (frame < 0 || hidden)
        return;
    Colour c = colour;
    if (hitFlash) {
        --hitFlash;
        const double t = double(float(hitFlash & 7) / 7.f);
        c.r = t * .5 + .5;
        c.g = t;
        c.b = 0;
    }
    if (skipDraw > 0) {
        --skipDraw;
        return;
    }
    out.push_back({position, scale, radius, angle, frame, c, z});
}
// Original tiny default hitBy 31750 is bx lr (confirmed in instructions).
void Entity::hitBy(Entity &) {}
GameEntity::GameEntity(World &w) : Entity(w) {
    retained = position;
}
int TileScreen::peek(int x, int y) const {
    return x >= 0 && x <= Columns && y >= 0 && y < BufferRows ? cells[size_t(y * Stride + x)] : 0;
}
void TileScreen::poke(int x, int y, int value) {
    if (x >= 0 && x <= Columns && y >= 0 && y < BufferRows)
        cells[size_t(y * Stride + x)] = value;
}
std::array<int, 2> TileScreen::cellAt(Vec2 p) const {
    const int x = int((p.x + .5) * 16. - scroll.x + 3.);
    const int y = (int((p.y + .5) * 16. - scroll.y) + rowOffset) % BufferRows;
    return {x, y};
}
World::World(uint32_t seed) : random(seed) {
    updatePalette();
    soundWanted.fill(-1);
    contacts = [this](Entity &source, Domain domain) {
        if (source.skipDraw || source.dead)
            return;
        const auto pos = source.position;
        const float radius = source.radius;
        bool sameGroup = false;
        for (auto *target = lists[size_t(domain)]; target; target = target->next) {
            if (target == &source || target->dead)
                continue;
            const auto delta = pos - target->position;
            const float distance = float(std::sqrt(delta.x * delta.x + delta.y * delta.y));
            if (float(radius + target->radius) <= distance) {
                source.touching = false;
                continue;
            }
            if (source.group > 0 && source.group == target->group)
                sameGroup = true;
            if (source.touching || target->skipDraw) {
                sameGroup = true;
                continue;
            }
            if (!target->collidable) {
                sameGroup = true;
                continue;
            }
            if (sameGroup)
                continue;
            source.hit = true;
            if (source.collisionFlags & 3) {
                unsupported("wasShot/blowUp dependency");
                return;
            }
            target->hitBy(source);
            contactPosition = target->position;
            contactNormal = (target->position - source.position).normalize();
        }
    };
}
World::~World() {
    for (auto *&head : lists)
        neon::entities::destroyList(head, [](Entity *e) { delete e; });
}
void World::clearGameplay() {
    // 408c0 leaves the foreground UI list alive; its actors retire separately.
    for (size_t d = 0; d < size_t(Domain::Always); ++d)
        neon::entities::destroyList(lists[d], [](Entity *e) { delete e; });
    player = nullptr;
}
void World::add(Domain d, Entity *e) {
    neon::entities::prepend(lists[size_t(d)], *e);
}
void World::runDomain(Domain d) {
    neon::entities::runAndCull(lists[size_t(d)], [](Entity *e) { delete e; });
}
void World::pointerDown(size_t n, Vec2 p) {
    pointers[n] = touchAnchor[n] = p;
    pointerHeld[n] = true;
    touchMoves[n] = 0;
    // 8bf0 sets the anchor, not the previous-move position. 8ce4 only
    // publishes a usable relative delta after the second move event.
}
void World::pointerMove(size_t n, Vec2 p) {
    pointers[n] = p;
    touchDelta[n] = touchPrevious[n] - p;
    touchPrevious[n] = p;
    ++touchMoves[n];
}
void World::pointerUp(size_t n) {
    pointerHeld[n] = false;
}
void World::updateTouch() {
    size_t n = 0;
    while (n < pointerHeld.size() && !pointerHeld[n])
        ++n;
    if (n == pointerHeld.size()) {
        touchRelative = {};
        return;
    }
    if (touchMoves[n] > 1)
        touchRelative = touchDelta[n] * -1.;
    Vec2 slope = (pointers[n] - touchAnchor[n]) * .1;
    if ((slope.x < 0 && touchSlope.x < slope.x) || (slope.x > 0 && slope.x < touchSlope.x))
        touchAnchor[n].x += (pointers[n].x - touchAnchor[n].x) * .1;
    if ((slope.y < 0 && touchSlope.y < slope.y) || (slope.y > 0 && slope.y < touchSlope.y))
        touchAnchor[n].y += (pointers[n].y - touchAnchor[n].y) * .1;
    touchSlope = slope;
    const float length = float(slope.length());
    if (length > .009f)
        slope = slope * double(.009f / length);
    if (std::abs(slope.x) < .0005)
        slope.x = 0;
    if (std::abs(slope.y) < .0005)
        slope.y = 0;
    touchStick = slope * 10.;
}
void World::tick(const PlayerInput &p) {
    if (!coverageStop.empty())
        return;
    input = p;
    if (!paused)
        ++ticks;
    const bool held = std::any_of(pointerHeld.begin(), pointerHeld.end(), [](bool p) { return p; });
    freshPress = held && !pressLatched;
    pressLatched = held;
    layerTilt = p.worldInfluence != 0 ? p.worldInfluence : lateralTilt;
    flushSound();
    if (ownerTick)
        ownerTick();
    else if (sceneTick)
        sceneTick();
    runMessages();
    updatePalette();
    presentation.run(demo, random);
    if (!paused) {
        if (gameMode > 0)
            runDomain(Domain::Player);
        runDomain(Domain::Decor);
        runDomain(Domain::LaunchPods);
        runDomain(Domain::Ambient);
        // Original +0xc >= 0 gate wraps the remaining eight lists.
        if (gameMode >= 0)
            for (auto d : {Domain::Scenery, Domain::PlayerShots, Domain::Ground, Domain::Air,
                           Domain::Enemies, Domain::Auxiliary, Domain::Pickups, Domain::Foreground})
                runDomain(d);
    }
    runDomain(Domain::Always);
}
void World::message(int row, uint32_t rgba, int frameTicks, std::string text, int x, int dynamic) {
    if (dynamic < 0 && rgba < 16)
        dynamic = int(rgba);
    TimedMessage *slot = nullptr;
    for (auto &m : messages)
        if (m.ticks && m.row == row) {
            slot = &m;
            break;
        }
    if (!slot)
        for (auto &m : messages)
            if (!m.ticks) {
                slot = &m;
                break;
            }
    if (slot)
        *slot = {std::move(text), row, x, frameTicks, dynamic, unpack(rgba)};
}
void World::runMessages() {
    messageLines.clear();
    for (auto &m : messages)
        if (m.ticks > 0) {
            --m.ticks;
            if (m.palette >= 0)
                m.colour = palette[size_t(m.palette)];
            messageLines.push_back({m.x, m.row, m.ticks ? m.text : std::string{}, m.colour});
        }
}
void World::clearMessages() {
    for (auto &m : messages)
        m.ticks = 0;
    messageLines.clear();
}
void World::flushSound() {
    for (size_t n = 0; n < soundWanted.size(); ++n) {
        const int wanted = soundWanted[n];
        if (wanted >= 0 && sound)
            sound(int(n), wanted ? 8 * wanted : 8 << 12);
        soundWanted[n] = -1;
    }
}
void World::prepare(std::vector<ActorDraw> &out) {
    out.clear();
    if (backgroundMode != 0)
        return;
    for (auto d : {Domain::Scenery, Domain::Ambient, Domain::LaunchPods, Domain::Ground,
                   Domain::Foreground, Domain::PlayerShots, Domain::Air, Domain::Pickups,
                   Domain::Enemies, Domain::Decor, Domain::Player}) {
        if (d == Domain::LaunchPods && terrainDraw)
            terrainDraw(out);
        for (auto *e = lists[size_t(d)]; e; e = e->next)
            e->prepare(out);
    }
}
void World::prepareForeground(std::vector<ActorDraw> &out) {
    out.clear();
    if (gameMode != 0 && gameMode != 1)
        return;
    for (auto *e = lists[size_t(Domain::Always)]; e; e = e->next)
        e->prepare(out);
}
void World::finishForeground() {
    // 4124c publishes the camera after foreground drawing, only in mode 1.
    if (gameMode == 1)
        tiles.scroll.x = camera.x;
}
void World::award(int points, int side, Vec2 position) {
    if (demo || (player && player->state == Ship::State::GameOver))
        return;
    if (side > 0)
        redScore += points;
    else if (side < 0)
        blueScore += points;
    else
        return;
    score = redScore + blueScore;
}
void World::achieveGoal(int id) {
    if (demo || id < 0 || id >= 10)
        return;
    const uint32_t bit = 1u << id;
    if (sessionAchievements & bit)
        return;
    sessionAchievements |= bit;
    if (achievement)
        achievement(id);
}
void World::resetChain() {
    if (chain > 0)
        noise(27);
    chain = 0;
}
} // namespace superoxwars
