#include "neon/controller_session.h"
#include "neon/touch_slots.h"
#include "neon/entity_list.h"
#include "neon/entity_motion.h"
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(std::string(#x) + ":" + std::to_string(__LINE__));            \
    } while (false)
namespace {
void controllerFrames() {
    using namespace neon::input;
    ControllerState pads[4];
    pads[0].connected = true;
    pads[0].pressed = South;
    pads[0].lx = .75f;
    pads[3].connected = true;
    pads[3].buttons = Up;
    ControllerFrame frame;
    frame.capture(pads, 4);
    CHECK(frame[0].lx == .75f && frame[0].pressed == South && frame[3].buttons == Up);
    frame.capture(frame.slots().data(), 4);
    CHECK(frame[0].lx == .75f && frame[3].buttons == Up);
    frame.capture(pads, 1);
    CHECK(!frame[3].connected && frame[3].buttons == 0);
    frame.capture(nullptr, 99);
    CHECK(!frame[0].connected);
    frame.capture(pads, 99);
    CHECK(frame[3].connected);
    frame.clear();
    for (const auto &p : frame.slots())
        CHECK(!p.connected && p.buttons == 0 && p.pressed == 0);
    ControllerOwnership owner;
    std::array<PlayerMotion, 4> input{};
    bool touch[4]{};
    auto update = [&] {
        owner.update(
            input, true, [](int pad) { return pad % 2; }, [&](int p) { return touch[p]; },
            [](auto p) { return p; });
    };
    touch[0] = true;
    input[0] = {true, 1, 0};
    update();
    CHECK(!owner.motion(0).active);
    touch[0] = false;
    update();
    CHECK(!owner.motion(0).active);
    input[0] = {};
    update();
    input[0] = {true, 1, 0};
    update();
    CHECK(owner.motion(0).active);
    owner.clearOwners();
    update();
    CHECK(!owner.motion(0).active);
    owner.cancel();
    update();
    CHECK(owner.motion(0).active);
    input[0] = {};
    update();
    CHECK(!owner.motion(0).active);
    input[2] = {true, 0, 1};
    update();
    CHECK(owner.motion(0).y == 1);
    owner.cancel();
    CHECK(!owner.motion(0).active); // neutral/disconnect cancels claims, never touches
}
struct Node {
    Node *prev = nullptr;
    Node *next = nullptr;
    bool dead = false;
    int id;
    std::function<void(Node &)> action;
    std::vector<int> *deleted;
    void run() {
        action(*this);
    }
    ~Node() {
        deleted->push_back(id);
    }
};
void listMutation() {
    std::vector<int> ran, deleted;
    Node *head = nullptr;
    auto make = [&](int id) {
        auto *p = new Node{nullptr, nullptr, false, id, {}, &deleted};
        p->action = [&](Node &n) { ran.push_back(n.id); };
        return p;
    };
    auto *a = make(1);
    auto *b = make(2);
    auto *c = make(3);
    neon::entities::prepend(head, *c);
    neon::entities::prepend(head, *b);
    neon::entities::prepend(head, *a);
    bool first = true;
    a->action = [&](Node &n) {
        ran.push_back(n.id);
        if (first) {
            b->dead = true;
            neon::entities::append(head, *make(4));
        }
    };
    b->action = [&](Node &n) {
        ran.push_back(n.id);
        neon::entities::prepend(head, *make(5));
    };
    c->action = [&](Node &n) {
        ran.push_back(n.id);
        n.dead = true;
        a->dead = true;
    };
    neon::entities::runAndCull(head, [](Node *n) { delete n; });
    first = false;
    CHECK((ran == std::vector<int>{1, 2, 3, 4}));
    CHECK((deleted == std::vector<int>{2, 3}));
    CHECK(head->id == 5 && head->prev == nullptr && head->next == a && a->prev == head &&
          a->next->id == 4 && a->next->prev == a);
    ran.clear();
    neon::entities::runAndCull(head, [](Node *n) { delete n; });
    CHECK((ran == std::vector<int>{5, 1, 4}));
    CHECK(deleted.back() == 1);
    neon::entities::destroyList(head, [](Node *n) { delete n; });
    CHECK(!head && deleted.size() == 5);
    // Original conditional insertion does not clear a detached node's next
    // field on an empty list. Preserve that operation; callers initialize it.
    Node sentinel{nullptr, nullptr, false, 99, {}, &deleted};
    Node scratch{nullptr, &sentinel, false, 98, {}, &deleted};
    neon::entities::prepend(head, scratch);
    CHECK(head == &scratch && scratch.next == &sentinel);
}
void slotSelection() {
    bool active[4] = {true, false, true, false};
    uint64_t serial[4] = {3, 0, 1, 0};
    CHECK(neon::input::firstSlot(4, [&](int i) { return !active[i]; }) == 1);
    CHECK(neon::input::selectSlot(
              4, [&](int i) { return active[i]; },
              [&](int i, int best) { return serial[i] < serial[best]; }) == 2);
    CHECK(neon::input::selectSlot(
              4, [&](int i) { return active[i]; }, [](int, int) { return true; }) == 2);
    CHECK(neon::entities::add32(INT32_MAX, 1) == INT32_MIN);
}
} // namespace
int main() {
    try {
        controllerFrames();
        listMutation();
        slotSelection();
        std::cout
            << "Canonical controller frame/ownership, touch slot selection and original intrusive mutation/lifetime contracts passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
