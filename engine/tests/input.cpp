#ifdef NDEBUG
#undef NDEBUG
#endif
#include "neon/input.h"
#include "neon/menu_navigation.h"
#include "neon/controller_session.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
struct TextGrid {
    int rowsN = 3, rowOffset = 0, scrollCount[4]{};
    double scrollY = 0;
    int text[4]{};
    void cll(int, int) {}
    void printAt(int, int row, const char *s) {
        text[row] = std::atoi(s);
    }
};
int main() {
    using namespace neon::input;
    ControllerFrame latched;
    ControllerState event;
    event.connected = true;
    event.buttons = event.pressed = South;
    latched.captureLatched(&event, 1);
    event.buttons = event.pressed = 0;
    latched.captureLatched(&event, 1);
    assert(latched.consume()[0].pressed == South && latched.consume()[0].pressed == 0);
    event.buttons = South;
    latched.captureLatched(&event, 1);
    assert(latched.consume()[0].pressed == 0);
    event.pressed = South;
    latched.captureLatched(&event, 1);
    latched.captureLatched(nullptr, 0);
    assert(latched.consume()[0].pressed == 0);
    ControllerState p;
    p.buttons = Right | Up;
    p.lx = -.5f;
    p.ly = .5f;
    assert(movement(p).x == 0 && movement(p).y == 0);
    p.connected = true;
    assert(movement(p).x == 1 && movement(p).y == -1);
    p.buttons = 0;
    assert(movement(p).x == -.5f && movement(p).y == .5f);
    auto b = menuButtons(South | Back | Start);
    assert(b.confirm && b.cancel && b.pause && b.advance);
    MenuBindings custom;
    custom.confirm = North;
    assert(menuButtons(North, custom).confirm && !menuButtons(South, custom).confirm);
    std::vector<MenuItem> items;
    for (int i = 1; i <= 4; ++i) {
        MenuItem item;
        item.label = i;
        item.cx = i * 4.;
        item.cy = 0;
        item.x0 = i * 4;
        item.x1 = item.x0 + 2;
        item.y0 = 0;
        item.y1 = 1;
        items.push_back(item);
    }
    PadMenu focus;
    auto update = [&](float axis, bool confirm = false, bool cancel = false, int start = 0) {
        return focus.update(items, 1, axis, 0, confirm, cancel, start, {1}, {1});
    };
    update(1);
    assert(focus.sel == 2);
    for (int i = 0; i < 19; ++i)
        update(1);
    assert(focus.sel == 2);
    update(1);
    assert(focus.sel == 3); // original 20-tick repeat delay
    for (int i = 0; i < 7; ++i)
        update(1);
    assert(focus.sel == 3);
    update(1);
    assert(focus.sel == 4); // original 8-tick repeat interval
    update(0);
    update(-1);
    assert(focus.sel == 3);
    assert(update(0, true) == 3 && update(0, false, true) == 1 && update(0, false, false, 4) == 4);
    TextGrid grid;
    int next = 4;
    const char *lines[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
    auto line = [&](int i) { return lines[i]; };
    for (int i = 0; i < 100; ++i)
        scrollText(&grid, next, 10, 1, .3, line);
    assert(next == 10 && grid.scrollY == 0 && grid.text[0] == 6);
    scrollText(&grid, next, 10, 0, .3, line);
    assert(next == 10 && grid.scrollY == 0);
    for (int i = 0; i < 100; ++i)
        scrollText(&grid, next, 10, -1, .3, line);
    assert(next == 4 && grid.scrollY == 0 && grid.text[0] == 0);
    std::cout
        << "Portable controller contract, menu bindings/focus/repeat and bounded scrolling passed\n";
}
