// Integration contract for the optional post-contact hook. Original arithmetic
// remains in collideEarly; these checks establish ordering and legacy opt-out.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "neon/entity_contact.h"
#include "neon/math.h"
#include <cassert>
#include <vector>
#include <iostream>
struct Node {
    neon::vec2d pos{0, 0}, vel{0, 0};
    float size = 1, mass = 1, elasticity = 1;
    int flags = 3, group = 0;
    bool dead = false, touching = false, hit = false;
    Node *next = nullptr;
    std::vector<int> *events = nullptr;
    void wasShot(Node *) {
        if (events)
            events->push_back(1);
    }
    void hitBy(Node *) {
        if (events)
            events->push_back(2);
    }
};
int main() {
    const auto accept = [](const Node &) { return false; };
    std::vector<int> events;
    Node a, b, c;
    b.pos.x = .5;
    c.pos.x = .7;
    b.next = &c;
    b.events = c.events = &events;
    int hits = 0;
    assert(neon::entities::collideEarly(&a, &b, false, accept, [&](Node &attacker, Node &target) {
               assert(&attacker == &a && (&target == &b || &target == &c));
               assert(events.back() == 2);
               events.push_back(3);
               ++hits;
           }) == 1);
    assert(hits == 2 && events == std::vector<int>({1, 2, 3, 1, 2, 3}));
    Node legacyA, legacyB, hookA, hookB;
    legacyA.vel.x = hookA.vel.x = 1;
    legacyB.pos.x = hookB.pos.x = .5;
    assert(neon::entities::collideEarly(&legacyA, &legacyB, true, accept) == 1);
    bool afterBounce = false;
    assert(neon::entities::collideEarly(
               &hookA, &hookB, true, accept, [&](Node &attacker, Node &target) {
                   afterBounce = attacker.pos.x != 0 && target.pos.x != .5 && target.vel.x != 0;
               }) == 1);
    assert(afterBounce && hookA.pos.x == legacyA.pos.x && hookA.pos.y == legacyA.pos.y &&
           hookB.pos.x == legacyB.pos.x && hookB.pos.y == legacyB.pos.y &&
           hookA.vel.x == legacyA.vel.x && hookA.vel.y == legacyA.vel.y &&
           hookB.vel.x == legacyB.vel.x && hookB.vel.y == legacyB.vel.y);
    Node rejectedA, rejectedB;
    int rejectedCalls = 0;
    auto record = [&](Node &, Node &) { ++rejectedCalls; };
    assert(!neon::entities::collideEarly(
        &rejectedA, &rejectedB, false, [](const Node &) { return true; }, record));
    rejectedB.dead = true;
    assert(!neon::entities::collideEarly(&rejectedA, &rejectedB, false, accept, record));
    assert(!neon::entities::collideEarly<Node>(&rejectedA, nullptr, false, accept, record));
    assert(rejectedCalls == 0);
    std::cout
        << "Post-contact ordering, bounce timing, legacy opt-out and rejected-contact checks passed\n";
}
