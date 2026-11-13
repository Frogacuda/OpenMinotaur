// Original iOS state flow with consistent project navigation conventions.
// Addresses refer to the supplied ARMv7 export; actions cross the host boundary.
#pragma once
#include "original_stm.h"
#include "campaign.h"
#include <vector>

namespace gridrunner {
enum class UiAction {
    None,
    Play,
    Pause,
    Exit,
    Title,
    Acceleration,
    Scores,
    Previous,
    Next,
    Mode,
    Back,
    NameKey,
    NameDelete,
    NameDone
};
struct UiButton {
    UiAction action = UiAction::None;
    int sprite = 0;
    double x = 0, y = 0, rx = 0, ry = 0, alpha = 1, scale = 1;
    const char *label = nullptr;
};
class OriginalFrontend {
  public:
    void reset();
    void tick(int score, int best, int lives, GameMode mode);
    void selectLevel() {
        stage_ = 3;
        counter_ = 3;
        pageTarget_ = 0;
        demoWanted_ = false;
    }
    void playerSelected() {
        attractIndex_ = 1;
        demoWanted_ = false;
        autoReturn_ = false;
        idle_ = false;
    }
    void startCompletion(int level) {
        stage_ = 12;
        counter_ = 4;
        completionLevel_ = level;
    }
    void startTransition() {
        stage_ = 10;
        counter_ = 4;
        screen_.line(2);
    }
    void gameOver() {
        stage_ = 20;
        counter_ = 3;
    }
    void skipGameOver() {
        if (stage_ == 21)
            counter_ = 1000;
    }
    void finishDemo() {
        stage_ = 20;
        counter_ = 3;
        autoReturn_ = true;
        demoWanted_ = false;
    }
    bool demoRequested() const {
        return stage_ == 40 && demoWanted_;
    }
    void consumeDemoRequest() {
        demoWanted_ = false;
    }
    bool attractPage() const {
        return stage_ == 6 || stage_ == 7 || stage_ == 8;
    }
    bool acceptsPlay() const {
        return waitingForPlay() || attractPage() || demoWanted_;
    }
    void attractScore(int rank, const std::string &name, int score);
    bool readyToStart() const {
        return stage_ == 40;
    }
    bool title() const {
        return stage_ < 10;
    }
    bool levelSelect() const {
        return stage_ == 4;
    }
    bool waitingForPlay() const {
        return stage_ == 2 || stage_ == 4;
    }
    bool gameOverScreen() const {
        return stage_ == 20 || stage_ == 21;
    }
    int stage() const {
        return stage_;
    }
    int frame() const {
        return frames_;
    }
    int counter() const {
        return counter_;
    }
    void selectionInfo(int level, int score, int lives, GameMode mode);
    void draw(Neon_Internals::sprite *spr, Neon_Internals::vec2d size) const;
    OriginalStm &screen() {
        return screen_;
    }

  private:
    void returnToTitle();
    void header(int score, int best, int lives, GameMode mode);
    OriginalStm screen_;
    int stage_ = 0, counter_ = 0, frames_ = 0, titleAge_ = 0;
    int attractIndex_ = 0, pageTarget_ = 0;
    int completionLevel_ = 0;
    bool demoWanted_ = false, autoReturn_ = false, idle_ = true;
    double gridY_ = -1.1, runnerY_ = .7, gridVy_ = .001, runnerVy_ = -.001;
};
// Shared spatial navigation for sprite controls and controller-accessible
// text menus; touch and A dispatch the same actions.
int hitButton(const std::vector<UiButton> &buttons, double x, double y);
int moveFocus(const std::vector<UiButton> &buttons, int current, int dx, int dy);
void drawUiSprite(Neon_Internals::sprite *spr, Neon_Internals::vec2d size, int index, double x,
                  double y, double rx, double ry, uint32_t colour = 0xffffffff, double angle = 0);
uint32_t menuColour(bool selected, int frame);
// Final-surface text, with native game coordinates and the imported stm font.
void drawMenuText(Neon_Internals::vec2d size, const char *text, double x, double y, double scale,
                  uint32_t colour);
} // namespace gridrunner
