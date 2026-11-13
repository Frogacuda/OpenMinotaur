#include "jump.h"
#include "headbutt.h"
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(std::string(__func__) + ": " + #x);                           \
    } while (false)
using namespace goatup2;
namespace {
constexpr double normal = 0x1.61e4f8p-8, water = 0x1.3e8145999999ap-8, head = 0x1.cc100f3333333p-8;
struct Actor {
    uint32_t medium = 0;
    float gravity = .0002f, effective = .0002f, heading = 9, speed = -.012f;
    double y = 0, scale = 1, velocity = .002;
    int32_t bubbles = 17, forced = 0, held = 30, sprite = 9, grace = 0;
    bool resting = false, landed = false, c259 = false, c25a = false, finished = true,
         consumed = false, headbutt = false;
    JumpActorView view() {
        return {medium,  gravity,  effective, y,        scale,  velocity, heading,
                bubbles, resting,  landed,    c259,     c25a,   forced,   speed,
                held,    finished, consumed,  headbutt, sprite, grace};
    }
};
// Records explicit dependency boundaries; it is never installed as gameplay.
struct Actions : JumpActions {
    Actor &actor;
    bool succeeds = true;
    bool &event;
    std::string order;
    std::vector<int32_t> samples;
    std::vector<uint32_t> gains;
    uint64_t emissionStart = 0;
    int animations = 0, attempts = 0;
    explicit Actions(Actor &a, bool &e) : actor(a), event(e) {}
    void updateMovementAnimation() override {
        CHECK(actor.heading == 0 && !actor.landed);
        ++animations;
        order += 'a';
    }
    bool attemptHeadbutt() override {
        ++attempts;
        order += 'h';
        return succeeds;
    }
    void emitHeadbuttParticles(float emission, float strength, neon::DarwinRandom &rng) override {
        CHECK(emission == .04f && strength == 5.f && actor.headbutt && !event);
        CHECK(actor.velocity == (actor.effective > 0 ? -head : head));
        emissionStart = rng.draws();
        rng.next();
        rng.next();
        order += 'e';
    }
    void playSample(int32_t s, uint32_t gain, int32_t option) override {
        CHECK(option == 0);
        samples.push_back(s);
        gains.push_back(gain);
        order += 's';
    }
};
struct Case {
    Actor a;
    int32_t input = 0;
    bool exit = true, event = false;
    neon::DarwinRandom rng{1};
    Actions actions{a, event};
    void run(bool muted = false) {
        updateJumpDecisions(a.view(), input, 2.f, exit, event, {7, muted}, rng, actions);
    }
};
void grounded() {
    for (int input : {0, 1, 2})
        for (float g : {-.0002f, 0.f, .0002f}) {
            Case c;
            c.a.resting = c.a.landed = true;
            c.a.gravity = g;
            c.a.effective = -g;
            c.input = input;
            c.run();
            CHECK(c.actions.animations == 1 && c.exit && !c.a.landed && c.actions.attempts == 0);
            CHECK(c.a.resting == (input != 1));
            if (input == 1) {
                CHECK(c.a.velocity == (g > 0 ? -normal : normal));
                CHECK(c.a.held == 1 && !c.a.finished && c.a.consumed && c.a.sprite == 107);
                CHECK(c.actions.order == "as" && c.actions.gains[0] == 7U << 12);
            } else
                CHECK(c.actions.order == "a" && c.a.velocity == .002);
        }
    Case blocked;
    blocked.a.resting = true;
    blocked.a.c25a = true;
    blocked.input = 1;
    blocked.run();
    CHECK(blocked.a.resting && blocked.actions.samples.empty());
    Case wet;
    wet.a.medium = 1;
    wet.a.resting = true;
    wet.input = 1;
    wet.run();
    CHECK(wet.actions.samples == std::vector<int32_t>({62, 15}));
    CHECK(wet.actions.gains == std::vector<uint32_t>({7U << 9, 7U << 12}));
    CHECK(wet.a.velocity == -normal && wet.a.bubbles == 256);
}
void waterAndForce() {
    for (float g : {-.0002f, 0.f, .0002f}) {
        Case c;
        c.a.medium = 1;
        c.a.effective = g;
        c.input = 1;
        c.actions.succeeds = false;
        c.run();
        CHECK(c.a.velocity == (g < 0 ? water : -water));
        CHECK(c.a.bubbles == 256);
    }
    for (int ticks : {1, 2})
        for (float speed : {-.012f, 0.f, .012f}) {
            Case c;
            c.a.forced = ticks;
            c.a.speed = speed;
            c.a.heading = .25f;
            c.input = 1;
            c.run();
            CHECK(c.a.forced == ticks - 1 && c.input == 0 && !c.exit &&
                  c.a.velocity == double(speed));
            CHECK(c.a.held == 30 && c.a.finished && c.actions.attempts == 0);
            CHECK(c.a.heading ==
                  (ticks == 1 ? float(double(speed) * 29.) : float(.25 + (speed < 0 ? -.6 : .6))));
        }
    Case wetForce;
    wetForce.a.medium = 1;
    wetForce.input = 1;
    wetForce.a.forced = 2;
    wetForce.run(true);
    CHECK(wetForce.a.bubbles == 256 && wetForce.a.velocity == double(wetForce.a.speed));
    CHECK(wetForce.actions.samples.empty() && wetForce.rng.draws() == 0);
}
void holdAndContacts() {
    for (int input : {0, 1, 2})
        for (int ticks : {0, 28, 29, 30, std::numeric_limits<int32_t>::max()}) {
            Case c;
            c.input = input;
            c.a.held = ticks;
            c.a.finished = false;
            c.actions.succeeds = false;
            c.run();
            const int next = ticks == std::numeric_limits<int32_t>::max()
                                 ? std::numeric_limits<int32_t>::min()
                                 : ticks + 1;
            if (input && next < 30) {
                CHECK(!c.a.finished && c.a.held == next && c.a.velocity == -normal &&
                      c.actions.attempts == 0);
            } else {
                CHECK(c.a.finished && c.a.held == (input ? next : 30));
                CHECK(c.actions.attempts == (input == 1 ? 1 : 0));
            }
            CHECK(!c.exit);
        }
    for (bool finished : {false, true})
        for (int mask : {1, 2, 3}) {
            Case c;
            c.input = 1;
            c.a.finished = finished;
            c.a.c259 = mask & 1;
            c.a.c25a = mask & 2;
            c.actions.succeeds = false;
            c.run();
            CHECK(c.a.held == 30 && c.a.finished == finished && !c.a.c259 && !c.a.c25a);
            CHECK(c.actions.attempts == 1);
        }
    Case zero;
    zero.a.finished = false;
    zero.a.held = 0;
    zero.a.effective = 0;
    zero.input = 2;
    zero.run();
    CHECK(zero.a.velocity == normal);
}
void headbutting() {
    for (double y : {-2.1, std::nextafter(-2.1, 0.), 0.}) {
        Case c;
        c.a.y = y;
        c.input = 1;
        c.run();
        CHECK(c.actions.attempts == (y > -2.1 ? 1 : 0));
    }
    for (float g : {-.0002f, 0.f, .0002f})
        for (bool muted : {false, true}) {
            Case c;
            c.a.effective = g;
            c.input = 1;
            c.run(muted);
            CHECK(c.a.velocity == (g > 0 ? -head : head) && c.a.headbutt && c.event);
            CHECK(c.rng.draws() == 3 && c.actions.emissionStart == 0);
            CHECK(c.actions.order == (muted ? "he" : "hes"));
            if (!muted)
                CHECK(c.actions.samples[0] == 12 && c.actions.gains[0] == 7U << 12);
            Case ledge;
            ledge.a.grace = 2;
            ledge.input = 1;
            ledge.run(muted);
            CHECK(ledge.a.held == 1 && !ledge.a.finished && ledge.a.sprite == 107 &&
                  ledge.a.consumed);
            CHECK(ledge.a.headbutt && !ledge.event && ledge.rng.draws() == 0);
            CHECK(ledge.actions.order == (muted ? "h" : "hs"));
        }
}
struct Target {
    Target *next = nullptr;
    int32_t serial = 0, headbuttState = 1;
    bool dead = false;
    int32_t category = 0, rewardValue = 0;
    neon::vec2d stagePosition{1, 2}, savedStagePosition{3, 4};
};
struct Output {
    Target *selected = nullptr;
    int explodeCategory = -1, labelCategory = -1;
    std::string order;
    void explodeHeadbuttTarget(Target &a, int count) {
        CHECK(a.dead && count == 10);
        selected = &a;
        order += 'e';
        if (explodeCategory >= 0)
            a.category = explodeCategory;
    }
    void createHeadbuttMultiplierLabel(Target &a, int value) {
        CHECK(&a == selected && a.rewardValue == value);
        order += 'l';
        if (labelCategory >= 0)
            a.category = labelCategory;
    }
};
void dispatch() {
    Target first, second, third;
    first.next = &second;
    second.next = &third;
    first.serial = second.serial = 8;
    third.serial = 99;
    third.headbuttState = 2;
    first.dead = true;
    int32_t multiplier = 5, counter = 0;
    Output out;
    auto run = [&](bool disabled = false, int grace = 0, int shared = 0) {
        return dispatchHeadbutt(&first, disabled, grace, shared, multiplier, counter, out);
    };
    CHECK(!run(true, 1, 1) && !out.selected && counter == 0);
    CHECK(run(false, 1) && run(false, 0, 1) && !out.selected && counter == 0);
    CHECK(run() && out.selected == &first &&
          counter == -1); // tie, dead and distant targets retained
    out = {};
    out.explodeCategory = 2;
    out.labelCategory = 15;
    multiplier = std::numeric_limits<int32_t>::min();
    counter = std::numeric_limits<int32_t>::min();
    CHECK(run() && out.order == "el" && multiplier == std::numeric_limits<int32_t>::max());
    CHECK(counter == std::numeric_limits<int32_t>::max() && first.rewardValue == multiplier);
    CHECK(!first.dead && first.serial == -1 && first.headbuttState == 2);
    CHECK(first.stagePosition.x == 3 && first.stagePosition.y == 4);
    second.serial = -1;
    third.serial = -2;
    third.headbuttState = 1;
    out = {};
    CHECK(!run() && !out.selected); // signed serial, -1 initial threshold
}
template <class B, class V> B bits(V v) {
    B b;
    static_assert(sizeof(B) == sizeof(V));
    std::memcpy(&b, &v, sizeof b);
    return b;
}
float asFloat(const std::string &s) {
    const auto b = uint32_t(std::stoul(s, nullptr, 16));
    return bits<float>(b);
}
double asDouble(const std::string &s) {
    return bits<double>(std::stoull(s, nullptr, 16));
}
struct EmissionRecorder : HeadbuttParticleOutput {
    neon::DarwinRandom &rng;
    std::vector<StageParticleBirth> births;
    std::vector<uint32_t> states;
    std::vector<uint64_t> draws;
    int sounds = 0;
    float &hunger;
    bool &hazard;
    EmissionRecorder(neon::DarwinRandom &r, float &h, bool &z) : rng(r), hunger(h), hazard(z) {}
    void adoptHeadbuttParticle(const StageParticleBirth &b) override {
        births.push_back(b);
        states.push_back(rng.state());
        draws.push_back(rng.draws());
    }
    void playSample(int32_t sample, uint32_t gain, int32_t option) override {
        CHECK(sample == 16 && gain == 7U << 12 && option == 0);
        CHECK(hunger == 0 && hazard && rng.draws() == 0 && births.empty());
        ++sounds;
    }
};
void emissions(const char *fixture) {
    std::ifstream in(fixture);
    CHECK(in.good());
    std::string token;
    std::unique_ptr<neon::DarwinRandom> rng;
    std::unique_ptr<EmissionRecorder> out;
    neon::vec2d position{.125, -.25}, scale{1, 1};
    float hunger = 0, remainder = 0;
    bool hazard = false;
    unsigned cases = 0, births = 0, index = 0;
    while (in >> token) {
        if (token[0] == '#') {
            std::string rest;
            std::getline(in, rest);
            continue;
        }
        if (token == "case") {
            uint32_t seed;
            int muted, expectedHazard, count;
            std::string emission, strength, oldRemainder, gravity, sx, sy, h, expectedRemainder;
            in >> seed >> emission >> strength >> oldRemainder >> gravity >> sx >> sy >> h >>
                muted >> expectedRemainder >> expectedHazard >> count;
            CHECK(in.good());
            hunger = asFloat(h);
            remainder = asFloat(oldRemainder);
            hazard = false;
            scale = {asDouble(sx), asDouble(sy)};
            rng = std::make_unique<neon::DarwinRandom>(seed);
            out = std::make_unique<EmissionRecorder>(*rng, hunger, hazard);
            index = 0;
            emitHeadbuttParticles(
                {{position, scale, .035f, asFloat(gravity)}, position, hunger, hazard, remainder},
                {7, muted != 0}, asFloat(emission), asFloat(strength), *rng, *out);
            CHECK(bits<uint32_t>(remainder) == std::stoul(expectedRemainder, nullptr, 16));
            CHECK(hazard == bool(expectedHazard) && hunger == (expectedHazard ? 0 : asFloat(h)));
            CHECK(out->births.size() == size_t(count) &&
                  out->sounds == (expectedHazard && !muted ? 1 : 0));
            ++cases;
        } else if (token == "birth") {
            uint32_t state;
            uint64_t draws;
            int sprite;
            std::string radius, fade, x, y, g, spin, rate;
            in >> state >> draws >> radius >> sprite >> fade >> x >> y >> g >> spin >> rate;
            CHECK(in.good());
            const auto &b = out->births.at(index);
            CHECK(b.worldPosition && b.worldPosition->x == position.x &&
                  b.worldPosition->y == position.y);
            CHECK(out->states[index] == state && out->draws[index] == draws);
            CHECK(bits<uint32_t>(b.radius) == std::stoul(radius, nullptr, 16) &&
                  b.sprite == sprite);
            CHECK(bits<uint32_t>(b.fadePerUpdate) == std::stoul(fade, nullptr, 16));
            CHECK(std::abs(b.velocity.x - asDouble(x)) < 1e-16 &&
                  std::abs(b.velocity.y - asDouble(y)) < 1e-16);
            CHECK(bits<uint32_t>(b.gravity) == std::stoul(g, nullptr, 16));
            CHECK(bits<uint32_t>(b.spin) == std::stoul(spin, nullptr, 16));
            CHECK(bits<uint32_t>(b.phaseRate) == std::stoul(rate, nullptr, 16));
            CHECK(b.stagePosition.x == position.x + scale.x * double(.035f));
            CHECK(b.stagePosition.y == position.y + scale.y * .005);
            CHECK(b.savedVelocity.x == b.velocity.x && b.savedVelocity.y == b.velocity.y);
            CHECK(b.solidCollisionEnabled && b.opacity == 1 && b.phase == 0);
            CHECK(b.savedDrawScale.x == 1 && b.savedDrawScale.y == 1 && b.ledgeResponse == 0);
            CHECK(b.tileInteractionMask == 0x440000U && b.projectileContactGate == 0);
            CHECK(b.colourRamp && b.colourRamp->segments == 11 && b.colourRamp->stops[11] == 14);
            ++index;
            ++births;
        } else if (token == "end") {
            uint32_t state;
            uint64_t draws;
            in >> state >> draws;
            CHECK(in.good());
            CHECK(rng->state() == state && rng->draws() == draws && index == out->births.size());
        } else
            CHECK(false);
    }
    CHECK(cases == 7 && births == 32);
}
// Use the actual emission kernel under the actual jump kernel; only ownership
// and sound are recorded. This checks their shared random stream and sequence.
void jumpEmissionIntegration() {
    struct BoundActions : Actions, HeadbuttParticleOutput {
        neon::vec2d position{0, 0}, scale{1, 1};
        float hunger = -.01f, remainder = 0;
        bool hazard = false;
        int births = 0;
        neon::DarwinRandom &rng;
        BoundActions(Actor &a, bool &e, neon::DarwinRandom &r) : Actions(a, e), rng(r) {}
        void emitHeadbuttParticles(float emission, float strength, neon::DarwinRandom &r) override {
            CHECK(!event && actor.headbutt);
            goatup2::emitHeadbuttParticles(
                {{position, scale, .035f, actor.gravity}, position, hunger, hazard, remainder},
                {7, false}, emission, strength, r, *this);
        }
        void adoptHeadbuttParticle(const StageParticleBirth &) override {
            CHECK(!event);
            ++births;
        }
        void playSample(int32_t sample, uint32_t gain, int32_t option) override {
            if (sample == 16)
                CHECK(!event && births == 0 && hunger == 0 && hazard && rng.draws() == 0);
            else
                CHECK(event && births == 20 && rng.draws() == 161);
            Actions::playSample(sample, gain, option);
        }
    };
    Actor a;
    bool event = false, exit = true;
    int32_t input = 1;
    neon::DarwinRandom rng{1};
    BoundActions out{a, event, rng};
    updateJumpDecisions(a.view(), input, 2, exit, event, {7, false}, rng, out);
    CHECK(event && out.births == 20 && rng.draws() == 161 && out.samples.size() == 2 &&
          out.samples[0] == 16);
    CHECK(out.samples[1] == int32_t(11 + (rng.state() & 3)));
}
} // namespace
int main(int argc, char **argv) {
    try {
        CHECK(argc == 2);
        grounded();
        waterAndForce();
        holdAndContacts();
        headbutting();
        dispatch();
        emissions(argv[1]);
        jumpEmissionIntegration();
        std::cout
            << "GoatUp 2 jump/dispatch source-rule checks passed; live dependencies not tested\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
