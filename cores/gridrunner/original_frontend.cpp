#include "original_frontend.h"
#include <cmath>
#include <cstdio>

namespace gridrunner {
using namespace Neon_Internals;
void OriginalFrontend::reset() {
    const int next = attractIndex_;
    *this = OriginalFrontend{};
    attractIndex_ = next;
}
void OriginalFrontend::returnToTitle() {
    stage_ = 1;
    counter_ = 0;
    titleAge_ = 0;
    autoReturn_ = false;
    demoWanted_ = false;
    idle_ = true;
    gridY_ = -1.1;
    runnerY_ = .7;
    gridVy_ = .001;
    runnerVy_ = -.001;
}
void OriginalFrontend::header(int score, int best, int lives, GameMode mode) {
    screen_.stringAt(0, 0, "^7  1UP    HIGH SCORE   LIVES");
    screen_.scoreAt(0, 1, score);
    screen_.scoreAt(10, 1, best);
    screen_.livesAt(22, 1, lives);
    screen_.line(2);
    screen_.stringAt(8, 2,
                     mode == GameMode::Survival ? "^4 (SURVIVAL)  "
                     : mode == GameMode::Pure   ? "^4   (PURE)  "
                                                : "^4   (CASUAL)");
}
void OriginalFrontend::tick(int score, int best, int lives, GameMode mode) {
    ++frames_;
    // iOS title sequence (0x41380): title -> demo/range/scores/demo/credits -> title.
    switch (stage_) {
    case 0:
        screen_.clear(((frames_ & 5) << 8) | 0x201);
        if (frames_ > 16) {
            stage_ = 1;
            counter_ = 36;
        }
        break;
    case 1:
        screen_.line(counter_--);
        screen_.livesAt(22, 1, 1);
        if (counter_ < 0) {
            stage_ = 2;
            counter_ = 0;
            titleAge_ = 0;
            header(score, best, 1, mode);
        }
        break;
    case 2:
        ++counter_;
        ++titleAge_;
        if (titleAge_ > 50) {
            if (gridY_ < -.2) {
                gridY_ += gridVy_;
                gridVy_ += .0005;
                if (gridY_ > -.2)
                    gridY_ = -.2;
            }
            if (runnerY_ > -.2) {
                runnerY_ += runnerVy_;
                runnerVy_ -= .0005;
                if (runnerY_ < -.2)
                    runnerY_ = -.2;
            }
        }
        if (counter_ > 200)
            screen_.stringAt(0, 14, "^5     SUPER SPACE BATTLE");
        if (counter_ > 250)
            screen_.stringAt(0, 16, "_6  WITH THE POWER OF AN OX!");
        if (counter_ > 500) {
            const int page = attractIndex_;
            attractIndex_ = (attractIndex_ + 1) % 5;
            if (page == 0 || page == 3) {
                demoWanted_ = true;
                startTransition();
            } else {
                stage_ = 3;
                counter_ = 3;
                pageTarget_ = page == 1 ? 7 : page == 2 ? 6 : 8;
            }
        }
        break;
    case 3:
        screen_.vline(counter_ >> 1, 4, 33, 0x76);
        if (++counter_ > 59) {
            stage_ = pageTarget_ ? pageTarget_ : 4;
            counter_ = 0;
        }
        break;
    case 4:
        ++counter_;
        header(score, best, lives, mode);
        screen_.stringAt(0, 8, "^5         PLAYER ONE");
        if (counter_ > 1500)
            reset();
        break;
    case 6:
        ++counter_;
        screen_.stringAt(0, 5, "_6  HERE ARE 'SUPER PLAYERS'");
        if (counter_ > 1000) {
            stage_ = 20;
            counter_ = 3;
            autoReturn_ = true;
        }
        break;
    case 7:
    case 8: {
        ++counter_;
        // iOS text differs from Android's donationware/range screen.
        const char *range[] = {"_5 PLEASE TRY OTHER GAMES",    "^6   'CAVERNS OF MINOS'",
                               "^5    'FIVE A DAY'",           "^4     'SUPER OX WARS'",
                               "^3      'GOATUP 2'",           "^2       'MINOTRON'",
                               "_6THEY ARE VERY FUN AND NICE!"};
        const char *credits[] = {"_5      =- GRIDRUNNER -=",
                                 "^6  CREATED BY",
                                 "^5  JEFF MINOTAUR",
                                 "^4  AND",
                                 "^3  GILESGOAT",
                                 "^2  (C)1982, 2012 LLAMASOFT",
                                 "_6  30 YEARS OF FLUFFINESS!"};
        constexpr int rows[] = {8, 11, 13, 15, 17, 19, 23};
        for (int i = 0; i < 7; ++i)
            if (counter_ > (i + 1) * 30)
                screen_.stringAt(0, rows[i], stage_ == 7 ? range[i] : credits[i]);
        if (counter_ > 500) {
            stage_ = 20;
            counter_ = 1;
            autoReturn_ = true;
        }
        break;
    }
    case 10:
        screen_.hline(2, 25, counter_, 0xfcb);
        if (++counter_ > 33) {
            stage_ = 11;
            counter_ = 2;
        }
        break;
    case 11:
        screen_.vline(counter_, 4, 33, 0xfca);
        if (++counter_ > 25) {
            stage_ = 40;
            counter_ = 2;
        }
        break;
    case 12:
    case 13:
        screen_.hline(2, 25, counter_, stage_ == 12 ? 0xfcb : 0x76);
        if (++counter_ > 33) {
            ++stage_;
            counter_ = 4;
        }
        break;
    case 14:
        ++counter_;
        screen_.livesAt(22, 1, lives);
        if (counter_ > 10)
            screen_.stringAt(0, 6, "^6       LEVEL COMPLETED!");
        if (counter_ > 100)
            screen_.stringAt(0, 9, data::compliments[completionLevel_ % 20]);
        if (counter_ > 150)
            screen_.stringAt(0, 11,
                             mode == GameMode::Survival ? "^4   YOU GET BEST WISHES!"
                                                        : "^4   YOU GET EXTRA LIFE!");
        if (counter_ > 200) {
            char text[80];
            std::snprintf(text, sizeof text, "_3   NOW ENTER NEW GRID %d!", completionLevel_ + 2);
            screen_.stringAt(0, 20, text);
        }
        break;
    case 20:
        screen_.vline(counter_ >> 1, 4, 33, 0x76);
        if (++counter_ > 59) {
            if (autoReturn_)
                returnToTitle();
            else {
                stage_ = 21;
                counter_ = 2;
            }
        }
        break;
    case 21:
        screen_.stringAt(0, 29, "_2        GAME   OVER");
        if (++counter_ > 1000)
            reset();
        break;
    case 40:
        screen_.scoreAt(0, 1, score);
        screen_.scoreAt(10, 1, best);
        screen_.livesAt(22, 1, lives);
        break;
    default:
        break;
    }
    if (idle_) {
        screen_.stringAt(7, 28, "             ");
        screen_.stringAt(7, 29, "^2 GAME   OVER ");
        screen_.stringAt(7, 30, "             ");
    }
}
void OriginalFrontend::attractScore(int rank, const std::string &name, int score) {
    if (stage_ != 6 || rank < 0 || rank >= 10 || counter_ <= rank * 50)
        return;
    char text[80];
    std::snprintf(text, sizeof text, "^7%-8.8s", name.c_str());
    screen_.stringAt(2, 7 + rank * 2, text);
    std::snprintf(text, sizeof text, "^%d%13d", 6 - (rank >> 1), score);
    screen_.stringAt(12, 7 + rank * 2, text);
}
void OriginalFrontend::selectionInfo(int level, int score, int lives, GameMode mode) {
    for (int row : {10, 19, 21, 23})
        screen_.line(row);
    char text[80];
    if (mode == GameMode::Survival) {
        screen_.stringAt(0, 10, "_6       -SURVIVAL MODE-");
        screen_.stringAt(0, 19, "_4  GIVEN 1 LIFE AND SUPER");
        screen_.stringAt(0, 21, "_4  DIFFICULTY ABOVE LEVEL");
        screen_.stringAt(0, 23, "_4  8 - SURVIVE!");
    } else {
        if (level)
            screen_.stringAt(0, 10, "^6  -SELECT STARTING LEVEL-");
        std::snprintf(text, sizeof text, "_4   START AT LEVEL %d", level + 1);
        screen_.stringAt(0, 19, text);
        std::snprintf(text, sizeof text, "_5   WITH %d POINTS", score);
        screen_.stringAt(0, 21, text);
        if (lives == 1)
            screen_.stringAt(0, 23, "_6   AND 1 LIFE");
        else {
            std::snprintf(text, sizeof text, "_6   AND %d LIVES", lives);
            screen_.stringAt(0, 23, text);
        }
    }
}
} // namespace gridrunner
