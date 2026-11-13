// Source-rule integration checks, not an original-execution witness.
#include "game_states.h"
#include "touch_input.h"
#include <cstdio>
#include <cmath>

int main() {
    using namespace minos;
    int failures = 0;
    auto check = [&](bool ok, const char *rule) {
        if (!ok) {
            std::fprintf(stderr, "%s\n", rule);
            ++failures;
        }
    };
    PlayState g;
    TouchInput touch;
    touch.down(g, 17, {-0.2, 0.3});
    touch.down(g, 42, {0.2, -0.3});
    check(g.touchDown[0] && g.touchDown[1] && g.touchPressed[0],
          "independent slots and held binding");
    touch.move(g, 17, {-0.1, 0.25});
    check(!g.touchFlag[0] && g.touchPos[0][0] == -0.2 && g.touchNowX[0] == -0.2 &&
              g.touchStartX[0] == -0.1,
          "first motion preserves hit/rotation origins");
    touch.move(g, 17, {0.1, 0.2});
    check(g.touchFlag[0] && std::fabs(g.touchDeltaX[0] + 0.2) < 1e-15,
          "second motion enables signed dragging");
    touch.up(g, 17);
    check(!g.touchDown[0] && g.touchDown[1] && !g.touchPressed[0],
          "release affects matching ID only");
    g.purgeTouches(); // the source watchdog/death path, outside the adapter
    touch.down(g, 99, {0, 0});
    check(g.touchDown[0] && !g.touchDown[1] && g.touchPressed[0],
          "source purge releases adapter ownership");
    touch.up(g, 42);
    check(g.touchDown[0] && g.touchPressed[0], "stale release cannot cancel a reused slot");
    touch.cancel(g);
    check(!g.touchDown[0] && !g.touchPressed[0], "lifecycle cancellation");

    neon::DarwinRandom random(7);
    g.rand = &random;
    g.gameOver = true;
    g.openPause();
    check(!g.paused && !g.debris && random.draws() == 0, "game-over pause guard");
    g.gameOver = false;
    g.iPad = true;
    g.openPause();
    auto *handed = dynamic_cast<PauseControl *>(g.debris);
    auto *scores = dynamic_cast<OpenFeintButton *>(g.debris->next);
    auto *quit = dynamic_cast<PauseControl *>(g.debris->next->next);
    auto *resume = dynamic_cast<PauseControl *>(g.debris->next->next->next);
    check(g.paused && random.draws() == 3 && handed && scores && quit && resume,
          "pause creation order / three RNG draws");
    check(handed && handed->spriteIndex == 93 && handed->pos.y == -0.25 && handed->growRate == 0,
          "tablet handedness placement");
    check(scores && scores->spriteIndex == 92 && scores->pos.y == 0.1 && scores->kill,
          "pause score button lifetime");
    if (handed) {
        g.prefsControl = 2;
        handed->activate();
        handed->run();
        check(g.prefsControl == 1 && handed->scale.x == -3 && g.goalState == 17,
              "handedness action and mirrored art");
    }
    if (resume) {
        resume->activate();
        resume->run();
        check(!g.paused && resume->dead, "resume action and retirement");
    }
    scores->run();
    check(scores->dead, "score button retires on resume");
    if (quit) {
        g.paused = true;
        g.scoreTotal = 123;
        g.frameCount = 90;
        quit->activate();
        check(!g.paused && g.runState == 1 && g.scoreTotal == 0 && g.frameCount == 0 &&
                  g.buttonLatch,
              "quit resets title transition and latches input");
    }
    g.resetLists(0);
    // Project UX conventions are opt-in; original-layout replay keeps source menus.
    g.updatedMenus = true;
    g.paused = false;
    g.gameOver = false;
    g.goSelect();
    bool homeIcon = false;
    for (auto *e = g.debris; e; e = e->next)
        homeIcon |= dynamic_cast<OpenFeintButton *>(e) != nullptr;
    check(!homeIcon && g.runState == 7,
          "updated home removes OpenFeint without changing select state");
    g.resetLists(0);
    const auto before = random.draws();
    g.openPause();
    PauseControl *newGame = nullptr, *highScores = nullptr, *exit = nullptr;
    bool pauseIcon = false;
    for (auto *e = g.debris; e; e = e->next) {
        pauseIcon |= dynamic_cast<OpenFeintButton *>(e) != nullptr;
        if (auto *b = dynamic_cast<PauseControl *>(e)) {
            if (b->action == PauseControl::Action::Quit)
                newGame = b;
            if (b->action == PauseControl::Action::Scores)
                highScores = b;
            if (b->action == PauseControl::Action::Exit)
                exit = b;
        }
    }
    check(!pauseIcon && newGame && highScores && exit && random.draws() - before == 3,
          "updated pause replaces service icon, adds exit, retains three RNG draws");
    if (newGame && exit)
        check(newGame->pos.x == -exit->pos.x && newGame->pos.y == exit->pos.y &&
                  newGame->size == exit->size && newGame->scale.x == exit->scale.x,
              "new game and exit have mirrored placement and matching dimensions");
    if (highScores) {
        highScores->activate();
        check(g.goalState == 14 && g.paused, "scores opens the table while retaining pause");
    }
    if (exit) {
        exit->activate();
        check(g.exitToLauncherRequested && g.paused,
              "exit requests launcher without resuming play");
    }
    if (newGame) {
        newGame->activate();
        check(g.runState == 1 && !g.paused, "new game preserves return-to-title action");
    }
    g.resetLists(0);
    std::printf("Minos source input/pause checks: %d failures\n", failures);
    return failures ? 1 : 0;
}
