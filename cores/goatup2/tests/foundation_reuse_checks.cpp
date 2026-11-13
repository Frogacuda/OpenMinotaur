// Source-derived witnesses for existing shared helpers, not a replacement game.
// Expected literals/order follow reviewed iOS entity::run 0x30690, prepend
// 0x9174 and the local traversal at 0x456ac. No original-execution claim.
#include "neon/entity_motion.h"
#include "neon/entity_list.h"
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (false)
namespace {
struct Point {
    double x, y;
};

void integration() {
    Point p{1.25, -.75}, v{.5, -.5}, damping{.25, .75}, extent{1, 2};
    int32_t age = 7;
    bool wrapped = false;
    neon::entities::integrate(p, v, damping, age, false, wrapped, extent);
    CHECK(p.x == .75 && p.y == .75 && v.x == .125 && v.y == -.375);
    CHECK(age == 8 && wrapped);

    // One correction per side, not modulo or repeated full normalization.
    p = {3.25, -4.25};
    v = {0, 0};
    wrapped = false;
    neon::entities::integrate(p, v, damping, age, false, wrapped, extent);
    CHECK(p.x == 2.25 && p.y == -2.25 && wrapped);

    // Equality is not an edge crossing, and ordinary wrapping resets the flag.
    p = {.5, -1};
    v = {0, 0};
    wrapped = true;
    neon::entities::integrate(p, v, damping, age, false, wrapped, extent);
    CHECK(p.x == .5 && p.y == -1 && !wrapped);

    // noWrap still integrates/damps/increments, retaining the prior flag.
    for (bool oldFlag : {false, true}) {
        p = {3, 4};
        v = {.5, -.5};
        wrapped = oldFlag;
        age = std::numeric_limits<int32_t>::max();
        neon::entities::integrate(p, v, damping, age, true, wrapped, extent);
        CHECK(p.x == 3.5 && p.y == 3.5 && v.x == .125 && v.y == -.375);
        CHECK(age == std::numeric_limits<int32_t>::min() && wrapped == oldFlag);
    }

    // Half extent is exactly halfway between two floats and rounds to .5.
    // The positive test subtracts an extent, then the negative test adds it
    // back: unchanged position with wrapped=true. Double comparisons alone
    // would leave wrapped=false; an else-if would leave a negative position.
    extent = {0x1.000001p+0, 2};
    p = {0x1.0000008p-1, 0};
    v = {0, 0};
    wrapped = false;
    neon::entities::integrate(p, v, damping, age, false, wrapped, extent);
    CHECK(p.x == 0x1.0000008p-1 && p.y == 0 && wrapped);
}

// Test-only nodes use explicit host defaults. They do not claim to reproduce
// original entity construction, polymorphic ownership or guest memory layout.
struct Node {
    int id;
    Node *prev = nullptr;
    Node *next = nullptr;
    bool dead = false, disposed = false;
    std::vector<int> &events;
    std::function<void()> action;
    void run() {
        CHECK(!disposed);
        events.push_back(id);
        if (action)
            action();
    }
};

void listExecution() {
    std::vector<int> events;
    Node a{1, nullptr, nullptr, true, false, events, {}},
        b{2, nullptr, nullptr, false, false, events, {}},
        c{3, nullptr, nullptr, false, false, events, {}},
        headChild{4, nullptr, nullptr, false, false, events, {}},
        tailChild{5, nullptr, nullptr, false, false, events, {}};
    Node *head = nullptr;
    neon::entities::prepend(head, c);
    neon::entities::prepend(head, b);
    neon::entities::prepend(head, a);
    a.action = [&] { neon::entities::prepend(head, headChild); };
    bool appended = false;
    b.action = [&] {
        if (!appended) {
            // Explicitly link a newborn tail; this does not certify the shared
            // append helper, which is outside this source-review scope.
            c.next = &tailChild;
            tailChild.prev = &c;
            c.dead = true;
            appended = true;
        }
    };
    tailChild.action = [&] { tailChild.dead = true; };
    auto release = [&](Node *n) {
        events.push_back(-n->id);
        n->disposed = true;
        n->prev = nullptr;
        n->next = nullptr; // continuation must already be retained
    };
    neon::entities::runAndCull(head, release);
    CHECK((events == std::vector<int>{1, -1, 2, 3, -3, 5, -5}));
    CHECK(head == &headChild && head->prev == nullptr && head->next == &b);
    CHECK(b.prev == head && b.next == nullptr);
    CHECK(a.disposed && c.disposed && tailChild.disposed && !headChild.disposed);
    events.clear();
    neon::entities::runAndCull(head, release);
    CHECK((events == std::vector<int>{4, 2}));

    // Last-node deletion clears the head, including empty-pass behavior.
    head = &b;
    b.prev = nullptr;
    b.dead = true;
    events.clear();
    neon::entities::runAndCull(head, release);
    CHECK(head == nullptr && (events == std::vector<int>{2, -2}));
    neon::entities::runAndCull(head, release);
    CHECK((events == std::vector<int>{2, -2}));
}

void prependContract() {
    std::vector<int> events;
    Node sentinel{9, nullptr, nullptr, false, false, events, {}};
    Node node{8, nullptr, &sentinel, false, false, events, {}};
    Node *head = nullptr;
    neon::entities::prepend(head, node);
    // Original empty-list branch leaves next untouched. This intentionally
    // demonstrates the caller's detached-next obligation, not a valid list.
    CHECK(head == &node && node.prev == nullptr && node.next == &sentinel);
}
} // namespace
int main() {
    try {
        integration();
        listExecution();
        prependContract();
        std::cout
            << "GoatUp 2 scoped shared-helper witnesses passed: integration, precision/wrap "
               "boundaries, "
               "age rollover, conditional insertion, post-callback traversal and release order.\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
