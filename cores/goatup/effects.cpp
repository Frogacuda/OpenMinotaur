#include "neon/score_notices.h"
#include "effects.h"
#include "climber.h"
#include "original_tables.h"
#include "neon/entity_list.h"
#include "neon/entity_motion.h"
#include "neon/particles.h"
#include <cmath>
#include <stdexcept>

namespace goatup {
namespace {
template <class Node> int count(Node *node) {
    int n = 0;
    for (; node; node = node->next)
        ++n;
    return n;
}
template <class Node> void dispose(Node *node) {
    delete node;
}
} // namespace
struct Effects::Star {
    Star *prev = nullptr;
    Star *next = nullptr;
    bool dead = false;
    Effects &owner;
    ActorDraw draw;
    double fall = 0;
    float spin = 0;
    bool title = false;
    explicit Star(Effects &effects) : owner(effects) {}
    void randomize() {
        if (title) {
            randomizeTitle();
            return;
        }
        auto &random = owner.random_;
        draw.position.x = random.range(-.5, -.3);
        if (random.next() & 1)
            draw.position.x = -draw.position.x;
        fall = random.range(.0005, .005);
        spin = float(random.range(-.02, .02));
        if ((random.next() & 4095) < 8) {
            draw.position.x = random.range(-.5, -.4);
            if (random.next() & 1)
                draw.position.x = -draw.position.x;
            draw.colour.white(random.range(.4, .7));
            draw.frame = 554;
            draw.radius = .04f;
        } else {
            draw.colour.unpackRGBA(original::StarColours[size_t((random.next() & 255) % 11 + 1)]);
            draw.colour.a = random.range(.2, .6);
            draw.radius = float(random.range(.0005, .01));
            draw.frame = 601;
        }
    }
    void randomizeTitle() {
        // titleStar 58d1c inherits star::run, but has its own creation draws.
        auto &random = owner.random_;
        draw.position.x = random.range(-.5, .5);
        fall = random.range(.0005, .005);
        spin = float(random.range(-.02, .02));
        if ((random.next() & 255) < 8) {
            draw.colour.white(random.range(.4, .7));
            int bonus = 586;
            if (owner.titleUnique_ > 0) {
                int ordinal = int(random.range(1., double(float(owner.titleUnique_)) + .9));
                for (int n = 0; n < 512; ++n)
                    if (((*owner.titleCollection_)[size_t(n >> 5)] >> (n & 31)) & 1u)
                        if (--ordinal < 1) {
                            bonus = original::Bonuses[size_t(n)];
                            break;
                        }
            }
            draw.frame = bonus;
            draw.radius = .05f;
            const auto size = owner.titleSpriteSize_(bonus + 2);
            if (size.x <= 0 || size.y <= 0)
                throw std::logic_error("Missing title bonus size");
            // Native randomise changes only one scale component, not both.
            if (size.x <= size.y)
                draw.scale.x = size.x / size.y;
            else
                draw.scale.y = size.y / size.x;
        } else {
            draw.colour.unpackRGBA(original::StarColours[size_t((random.next() & 255) % 11 + 1)]);
            draw.colour.a = random.range(.2, .6);
            draw.radius = float(random.range(.0005, .01));
            draw.frame = 601;
        }
    }
    void run() {
        draw.position.y += fall;
        if (draw.position.y > .6) {
            randomize();
            draw.position.y = -.6;
        }
        draw.angle = float(draw.angle + spin);
    }
};
struct Effects::Particle {
    Particle *prev = nullptr;
    Particle *next = nullptr;
    bool dead = false, wrapped = false;
    int age = 0;
    Vec2 position{}, velocity{};
    float energy = 1, decay = .03f;
    neon::colour from, to;
    void run() {
        // gpart adds gravity before _particle integrates and applies damping.
        velocity.y += .0006;
        neon::entities::integrate(position, velocity, Vec2{.98, .98}, age, true, wrapped,
                                  Vec2{1, 1});
        neon::particles::fadeLife(energy, decay, dead);
    }
    ActorDraw draw() const {
        const auto animation = neon::particles::scaleFrame(energy, 1, 2);
        ActorDraw result;
        result.position = position;
        result.frame = 0;
        result.particle = true;
        result.radius = .005f;
        const float size = animation.scale;
        result.scale = {double(size), double(size)};
        result.colour = neon::clerp(from, to, animation.progress);
        return result;
    }
};
struct Effects::UiActor {
    enum class Kind { Trail, Heart, Score, BridgeBit, ConveyorCap, SummitBird } kind = Kind::Trail;
    UiActor *prev = nullptr;
    UiActor *next = nullptr;
    bool dead = false;
    Effects &owner;
    ActorDraw draw;
    Vec2 velocity{}, surface{};
    float phase = 0, phaseSpeed = 0, spin = 0, alpha = 1, life = 0;
    int drawSkip = 0;
    std::vector<int> digits;
    Platform *platform = nullptr;
    int tile = 0;
    ClimbContext *game = nullptr;
    Climber *goat = nullptr;
    int birdDelay = 200;
    explicit UiActor(Effects &effects) : owner(effects) {}
    void projectHeart(float camera) {
        const float angle = float((surface.x - double(camera)) * 3.141592654);
        const float cosine = std::cos(angle);
        draw.position = {double(std::sin(angle)) * .5, surface.y};
        draw.scale = {double(cosine), 1};
        if (cosine < 0)
            drawSkip = 1;
    }
    void runTrail() {
        draw.position += velocity;
        phase = float(phase + phaseSpeed);
        draw.colour.unpackRGBA(original::KidColours[size_t(int(phase) & 7)]);
        alpha = float(double(alpha) * .99);
        draw.colour.a = double(alpha);
        draw.angle = float(draw.angle + spin);
        if (double(alpha) < .1)
            dead = true;
    }
    void run() {
        if (kind == Kind::SummitBird) {
            if (--birdDelay > 0)
                return;
            draw.position.x += velocity.x;
            constexpr int frames[] = {102, 103, 104, 103};
            draw.frame = frames[unsigned(int(draw.position.x * 50.)) & 3];
            if (!game->diveRequested && std::abs(float(draw.position.x - goat->position.x)) < .04) {
                game->diveRequested = game->lowGravity = true;
                game->returnSpeed = .003f;
                goat->velocity.y = -.016;
                goat->platform = -1;
                if (!game->demo) {
                    game->sound(43);
                    game->sound(37);
                }
            }
            draw.scale.x = velocity.x <= 0 ? -1 : 1;
            if ((velocity.x < 0 && draw.position.x < -.6) ||
                (velocity.x > 0 && draw.position.x > .6)) {
                dead = true;
                // theGoat retains its supporting platform pointer across dive.
                for (auto &p : game->tower.platforms)
                    if (p.level == goat->lastPlatform) {
                        p.tiles.fill(0);
                        break;
                    }
            }
        } else if (kind == Kind::ConveyorCap) {
            const float angle = float(double(float(float(surface.x) - owner.cameraAngle_)) *
                                      3.141592654),
                        cosine = std::cos(angle);
            draw.position = {double(std::sin(angle)) * .5,
                             double(platform->y) + double(draw.radius) * -.5};
            draw.scale = {double(cosine), 1};
            draw.colour.a = double(cosine) * .9 + .05;
            if (owner.gameOver_)
                draw.colour.a *= .4;
            if (cosine < 0)
                drawSkip = 1;
            if (!owner.returning_) {
                if ((platform->offscreenTicks == 0 && draw.position.y >= .5) ||
                    draw.position.y > .6)
                    dead = true;
            } else if (draw.position.y < -.55)
                dead = true;
            draw.position.y += .015;
            spin = float(spin + float(platform->belt[size_t(tile)] * -50.f));
        } else if (kind == Kind::Heart) {
            projectHeart(owner.cameraAngle_);
            if (!owner.paused_) {
                surface += velocity;
                life = float(double(life) - .01);
                if (life < 0)
                    dead = true;
                else
                    draw.colour.a = life <= 1 ? double(life) : 1.;
            }
        } else if (!owner.paused_) {
            if (kind == Kind::Trail)
                runTrail();
            else if (kind == Kind::BridgeBit) {
                draw.position += velocity;
                velocity.y += .0008;
                draw.angle = float(draw.angle + spin);
                draw.scale = draw.scale * (owner.tablet_ ? 1.04 : 1.02);
                alpha = float(double(alpha) * .98);
                draw.colour.a = double(alpha);
                if (draw.position.y > .6)
                    dead = true;
            } else {
                neon::notices::advanceFloating(draw.position, velocity, alpha, -.01f, dead,
                                               [&](double opacity) {
                                                   draw.colour = owner.cyclingColour_;
                                                   draw.colour.a = opacity;
                                               });
            }
        }
    }
    void prepare(std::vector<ActorDraw> &out) {
        if (kind == Kind::ConveyorCap) {
            if (drawSkip > 0) {
                --drawSkip;
                return;
            }
            out.push_back(draw);
            auto cap = draw;
            cap.frame = 185;
            cap.radius = .025f;
            cap.particle = true;
            cap.scale = {1, 1};
            cap.position.y += .014;
            cap.angle = spin;
            out.push_back(cap);
            return;
        }
        if (kind != Kind::Score) {
            if (drawSkip > 0) {
                --drawSkip;
                return;
            }
            out.push_back(draw);
            return;
        }
        const Vec2 size = draw.scale * double(draw.radius), spacing = size * 1.2;
        Vec2 at = neon::notices::centredOrigin(draw.position, spacing, int(digits.size()));
        for (int digit : digits) {
            ActorDraw glyph = draw;
            glyph.position = at;
            glyph.frame = 32 + digit;
            // floatingScore uses radius, whereas entity::draw uses diameter.
            glyph.particle = true;
            out.push_back(glyph);
            at.x += spacing.x;
        }
    }
};
Effects::Effects(neon::DarwinRandom &random) : random_(random) {}
Effects::~Effects() {
    reset();
}
void Effects::reset() {
    neon::entities::destroyList(stars_, dispose<Star>);
    neon::entities::destroyList(particles_, dispose<Particle>);
    neon::entities::destroyList(ui_, dispose<UiActor>);
}
void Effects::initializeStars() {
    if (stars_)
        throw std::logic_error("GoatUp new-level stars already exist");
    for (int n = 0; n < 50; ++n) {
        auto *star = new Star(*this);
        star->draw.position.y = random_.range(-.5, .5);
        star->randomize();
        neon::entities::prepend(stars_, *star);
    }
}
void Effects::initializeTitleStars(const std::array<uint32_t, 16> &collected, int unique,
                                   const std::function<Vec2(int)> &spriteSize) {
    if (stars_)
        throw std::logic_error("GoatUp title stars already exist");
    titleCollection_ = &collected;
    titleUnique_ = unique;
    titleSpriteSize_ = spriteSize;
    for (int n = 0; n < 50; ++n) {
        auto *star = new Star(*this);
        star->title = true;
        star->draw.position.y = random_.range(-.5, .5);
        star->randomize();
        neon::entities::prepend(stars_, *star);
    }
}
void Effects::splash(int count, float speed, float spread, Vec2 at, uint32_t from, uint32_t to) {
    const float low = float(speed + float(double(speed) / -10.));
    const float high = float(speed + float(double(speed) / 10.));
    for (int n = 0; n < count; ++n) {
        Vec2 velocity{0, double(float(random_.range(double(low), double(high))))};
        velocity.rotateBy(random_.range(double(-spread), double(spread)));
        auto *p = new Particle;
        p->position = at;
        p->velocity = velocity;
        p->from.unpackRGBA(from);
        p->to.unpackRGBA(to);
        neon::entities::prepend(particles_, *p);
    }
}
void Effects::playerTrail(Vec2 at, int flightTicks) {
    if ((random_.next() & 255) >= 96)
        return;
    auto *trail = new UiActor(*this);
    trail->draw.position = at;
    trail->velocity = {random_.range(-.0002, .0002), random_.range(-.0002, .0002)};
    trail->phase = float(random_.next() & 7);
    trail->draw.colour.unpackRGBA(original::KidColours[size_t(int(trail->phase))]);
    trail->phaseSpeed = float(random_.range(.8, 6.8));
    trail->spin = float(random_.range(-.02, .02));
    const auto choice = random_.next();
    if ((choice & 255) < 48 || flightTicks > 0) {
        trail->draw.frame = 605;
        trail->phaseSpeed = float(random_.range(.8, 1.8));
        trail->draw.radius = float(random_.range(.008, .02));
        trail->velocity = {random_.range(-.0007, .0007), random_.range(-.0007, .0007)};
    } else {
        trail->draw.frame = 601;
        trail->phaseSpeed = float(random_.range(.3, .8));
        trail->draw.radius = float(random_.range(.0005, .01));
    }
    neon::entities::prepend(ui_, *trail);
}
void Effects::loveHeart(float angle, double y, double xRange, double yRange, float cameraAngle) {
    auto *heart = new UiActor(*this);
    heart->kind = UiActor::Kind::Heart;
    heart->surface = {double(angle), y};
    heart->velocity.x = random_.range(-xRange, xRange);
    heart->velocity.y = random_.range(-yRange, yRange);
    heart->draw.frame = 7;
    heart->draw.radius = .015f;
    heart->draw.colour.white(1);
    heart->life = float(random_.range(1, 3));
    heart->projectHeart(cameraAngle);
    neon::entities::prepend(ui_, *heart);
}
void Effects::floatingScore(int points, Vec2 at, double vx, double vy, float radius) {
    auto *score = new UiActor(*this);
    score->kind = UiActor::Kind::Score;
    score->draw.position = at;
    score->velocity = {double(vx), double(vy)};
    score->draw.radius = radius;
    score->draw.colour.white(1);
    if (points > 0) {
        for (int n = points; n > 0; n /= 10)
            score->digits.insert(score->digits.begin(), n % 10);
    } else
        score->digits.push_back(0);
    neon::entities::prepend(ui_, *score);
}
void Effects::bridgeBit(Vec2 at) {
    auto *bit = new UiActor(*this);
    bit->kind = UiActor::Kind::BridgeBit;
    bit->draw.position = at;
    bit->draw.frame = 100;
    bit->draw.radius = .02f;
    bit->velocity.y = random_.range(-.01, -.02);
    bit->velocity.x = random_.range(-.02, .02);
    bit->spin = float(random_.range(-.2, .2));
    bit->draw.colour.white(1);
    neon::entities::prepend(ui_, *bit);
}
void Effects::summitBird(ClimbContext &game, Climber &goat) {
    auto *bird = new UiActor(*this);
    bird->kind = UiActor::Kind::SummitBird;
    bird->game = &game;
    bird->goat = &goat;
    bird->draw.position = {goat.position.x <= .5 ? .6 : -.6, goat.position.y};
    bird->velocity.x = goat.position.x <= .5 ? -.008 : .008;
    bird->draw.frame = 102;
    bird->draw.radius = .04f;
    bird->draw.colour.white(1);
    neon::entities::prepend(ui_, *bird);
}
void Effects::conveyorCap(Platform &platform, int frame, float angle, int tile) {
    auto *cap = new UiActor(*this);
    cap->kind = UiActor::Kind::ConveyorCap;
    cap->platform = &platform;
    cap->surface.x = double(angle);
    cap->tile = tile;
    cap->draw.frame = frame;
    cap->draw.radius = .03f;
    cap->draw.colour.white(1);
    cap->drawSkip = 2;
    // The native constructor projects immediately, retaining back-face suppression.
    const float radians = float(double(float(angle - cameraAngle_)) * 3.141592654);
    cap->draw.position = {double(std::sin(radians)) * .5, double(platform.y) + double(.03f) * -.5};
    cap->draw.scale = {double(std::cos(radians)), 1};
    cap->draw.colour.a = double(std::cos(radians)) * .9 + .05;
    if (std::cos(radians) < 0)
        cap->drawSkip = 1;
    neon::entities::prepend(ui_, *cap);
}
void Effects::tickParticles() {
    neon::entities::runAndCull(particles_, dispose<Particle>);
}
void Effects::tickStars() {
    neon::entities::runAndCull(stars_, dispose<Star>);
}
void Effects::tickUi(bool paused, float camera, const neon::colour &colour, bool returning,
                     bool gameOver, bool tablet) {
    paused_ = paused;
    cameraAngle_ = camera;
    cyclingColour_ = colour;
    returning_ = returning;
    gameOver_ = gameOver;
    tablet_ = tablet;
    neon::entities::runAndCull(ui_, dispose<UiActor>);
}
void Effects::drawStars(std::vector<ActorDraw> &out) const {
    for (auto *p = stars_; p; p = p->next)
        out.push_back(p->draw);
}
void Effects::drawParticles(std::vector<ActorDraw> &out) const {
    for (auto *p = particles_; p; p = p->next)
        out.push_back(p->draw());
}
void Effects::prepareUiDraw(std::vector<ActorDraw> &out) {
    for (auto *p = ui_; p; p = p->next)
        p->prepare(out);
}
int Effects::stars() const {
    return count(stars_);
}
int Effects::particles() const {
    return count(particles_);
}
int Effects::trails() const {
    int n = 0;
    for (auto *p = ui_; p; p = p->next)
        if (p->kind == UiActor::Kind::Trail)
            ++n;
    return n;
}
int Effects::hearts() const {
    int n = 0;
    for (auto *p = ui_; p; p = p->next)
        if (p->kind == UiActor::Kind::Heart)
            ++n;
    return n;
}
} // namespace goatup
