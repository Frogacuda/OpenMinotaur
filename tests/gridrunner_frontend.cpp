#ifdef NDEBUG
#undef NDEBUG
#endif
#include "original_frontend.h"
#include "controls.h"
#include "text_menus.h"
#include "gridrunner_choices.h"
#include "original_messages.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
int main() {
    using namespace gridrunner;
    {
        GoalMessages m;
        m.show(4);
        assert(m.lines()[0].text == "WELCOME TO BLOBBYTOWN");
        m.prepare();
        assert(m.lines()[0].amount == .01f && m.lines()[0].y == -.5);
        m.show(5);
        assert(m.lines()[1].text == "DARK SIDE OF THE BLOB");
        const float phase = m.lines()[0].amount;
        m.prepare(true);
        assert(m.lines()[0].amount == phase);
        for (int i = 0; i < 99; ++i)
            m.prepare();
        assert(m.lines()[0].state == 0); // binary32 .01 accumulation
        m.prepare();
        assert(m.lines()[0].state == 1 && m.lines()[0].amount == 1);
        m.prepare();
        assert(!m.glyphs().empty() && m.glyphs()[0].atlasY == 483);
        for (int i = 0; i < 60; ++i)
            m.prepare();
        assert(!m.lines()[0].active() && !m.lines()[1].active());
    }
    {
        assert(nameSlots("", 10) == "__________");
        assert(nameSlots("ABC", 10) == "ABC_______");
        std::string name = "ABCDEFGHI";
        int selected = 8;
        assert(appendName(name, 'J', 10, selected) && selected == kNameDone &&
               nameSlots(name, 10) == "ABCDEFGHIJ");
        assert(!appendName(name, 'K', 10, selected) && name.size() == 10);
        std::vector<UiButton> buttons;
        for (int i = 0; i < 36; ++i)
            buttons.push_back(
                {UiAction::NameKey, 0, (i % 9 - 4) * .095, -.08 + (i / 9) * .1, .04, .04});
        buttons.push_back({UiAction::NameDelete, 0, -.24, .36, .2, .04});
        buttons.push_back({UiAction::NameDone, 0, .24, .36, .2, .04});
        assert(moveNameFocus(buttons, kNameDelete, 1, 0) == kNameDone);
        assert(moveNameFocus(buttons, kNameDone, -1, 0) == kNameDelete);
        assert(moveNameFocus(buttons, kNameDelete, -1, 0) == kNameDelete &&
               moveNameFocus(buttons, kNameDone, 1, 0) == kNameDone);
        assert(moveNameFocus(buttons, kNameDone, 0, -1) < 36 &&
               moveNameFocus(buttons, kNameDelete, 0, -1) < 36);
    }
    {
        const auto menu = pauseMenu(true);
        assert(menu.size() == 5 && menu[0].sprite == 3 && menu[1].sprite == 98 &&
               menu[2].sprite == 99 && menu[3].sprite == 83 && menu[4].sprite == 101);
        assert(pauseMenu(false)[4].sprite == 100);
        const auto landscape = pauseMenu(true, 1, 4. / 3);
        for (size_t i = 0; i < menu.size(); ++i) {
            assert(landscape[i].action == menu[i].action);
            assert(std::abs(landscape[i].rx * (4. / 3) - menu[i].rx * .75) < 1e-12);
        }
        for (int i = 0; i < 5; ++i) {
            assert(!menu[i].label);
            assert(hitButton(menu, menu[i].x, menu[i].y) == i);
            if (i < 4)
                assert(moveFocus(menu, i, 0, 1) == i + 1);
        }
    }
    OriginalFrontend f;
    const auto tick = [&] { f.tick(0, 0, 1, GameMode::Pure); };
    const auto until = [&](int state, int limit) {
        int i = 0;
        while (f.stage() != state && ++i <= limit)
            tick();
        assert(f.stage() == state);
    };
    // Do not leave the title parked forever. Visit both gameplay demos and
    // every original information page, then prove the cycle repeats.
    const int sequence[] = {40, 7, 6, 40, 8, 40};
    for (int state : sequence) {
        until(state, 3000);
        if (state == 40) {
            assert(f.demoRequested());
            f.consumeDemoRequest();
            f.finishDemo();
        } else {
            assert(f.attractPage() && f.acceptsPlay());
            const int end = state == 6 ? 1000 : 500;
            for (int i = 0; i < end + 1; ++i)
                tick();
        }
        until(2, 100);
    }
    // Starting a player game changes subsequent attract order and cannot
    // leave a demo start queued behind the new game's transition.
    f.playerSelected();
    f.startTransition();
    until(40, 100);
    assert(!f.demoRequested());
    f.gameOver();
    until(21, 100);
    f.skipGameOver();
    tick();
    until(2, 100);
    until(7, 1000);
    f.selectLevel();
    until(4, 100);
    assert(f.levelSelect() && !f.demoRequested());
    OriginalFrontend completion;
    completion.playerSelected();
    completion.startCompletion(0);
    const auto completeTick = [&] { completion.tick(100, 100, 4, GameMode::Pure); };
    completeTick();
    assert(completion.screen().peek(2, 4) == 0xfcb);
    for (int i = 1; i < 30; ++i)
        completeTick();
    assert(completion.stage() == 13);
    completeTick();
    assert(completion.screen().peek(2, 4) == 0x76);
    for (int i = 1; i < 30; ++i)
        completeTick();
    assert(completion.stage() == 14 && completion.counter() == 4);
    for (int i = 0; i < 7; ++i)
        completeTick();
    assert((completion.screen().peek(7, 6) & 255) == ('L' + 0x56));
    while (completion.counter() <= 100)
        completeTick();
    assert((completion.screen().peek(3, 9) & 255) == ('Y' + 0x56));
    using namespace mino;
    assert(std::string(kGridrunnerChoices[0].id) == "gridrunner");
    assert(std::string(kGridrunnerChoices[1].id) == "gridrunner-classic");
    static_assert(GridrunnerChoiceLayout::count == 2);
    for (bool portrait : {false, true}) {
        GridrunnerChoiceLayout l(portrait);
        for (int i = 0; i < GridrunnerChoiceLayout::count; ++i) {
            const auto &row = l.rows[size_t(i)];
            const auto &preview = l.previews[size_t(i)];
            assert(l.hit(row.cx(), row.cy()) == i);
            assert(preview.x > row.cx() && preview.x + preview.w < row.x + row.w);
            assert(preview.y >= row.y && preview.y + preview.h <= row.y + row.h);
            assert(row.h >= 44);
        }
        assert(l.hit(l.back.cx(), l.back.cy()) == GridrunnerChoiceLayout::count);
        assert(l.hit(-1, -1) == -1);
        assert(l.move(0, 0, -1) == GridrunnerChoiceLayout::count &&
               l.move(GridrunnerChoiceLayout::count, 0, 1) == 0);
        assert(l.move(0, 0, 1) == 1 && l.move(1, 0, -1) == 0);
    }
    std::cout
        << "Two full idle cycles, demo requests/interrupt/reset, both launch-panel layouts passed.\n";
}
