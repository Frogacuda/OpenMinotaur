// The original Deflex title/selector/result flow with direct host menu input.
#pragma once
#include "history.h"
#include "neon/touch_slots.h"
#include "neon/game_presentation.h"
namespace deflex {
class Frontend {
  public:
    enum class Page { Title, Selector, Game, Paused, Result, Info, Scores, History };

  private:
    struct Sparkle : Entity {
        using Entity::draw;
        using Entity::run;

        neon::vec2d phase, speed;
        float radius, spin;
        Sparkle(neon::DarwinRandom &, int);
        void run();
        void draw(ActorRenderer &, neon::vec2d, neon::DarwinRandom &, int threshold);
    };
    Progress &progress_;
    const std::vector<neon::vec2d> &sizes_;
    std::unique_ptr<World> world_;
    std::unique_ptr<History> history_;
    std::vector<Sparkle> sparkles_;
    int selected_ = 0, focus_ = 0, ticks_ = 0, infoScroll_ = 0, volume_ = 8;
    float selectorPosition_ = 0, selectorVelocity_ = 0;
    bool selectorLevelRow_ = true, selectorAnimating_ = false;
    bool dragging_ = false;
    double currentDrag_ = 0, lastDrag_ = 0, previousDrag_ = 0;
    neon::input::TouchState<neon::vec2d> touches_;
    uint32_t held_ = 0, menuDirections_ = 0;
    bool release_ = true, historyPressed_ = false, historyOK_ = false;
    Page returnPage_ = Page::Title;
    Page infoOrigin_ = Page::Title;
    bool resultPrepared_ = false;
    bool resultSummary_ = false;
    int resultTicks_ = 0;
    float feedbackScale_ = .99f;
    float pauseOpacity_ = 1;
    void scene(WorldMode, int level = 0);
    void enter(Page);
    void selectorText();
    void stepLevel(int direction);
    void drawSelectorArrows(GridRenderer &, neon::vec2d);
    void activate();
    void settleResult();
    void showResultSummary();
    void drawSelector(ActorRenderer &, neon::vec2d);
    void sprite(ActorRenderer &, neon::vec2d, int frame, neon::vec2d pos, neon::vec2d scale,
                int item = -1);

  public:
    Page page = Page::Title;
    bool quit = false, keyboardNavigation = false;
    bool controllerFocus = false;
    std::vector<Sound> sounds;
    Frontend(Progress &, const std::vector<neon::vec2d> &, uint32_t seed = 1);
    World &world() {
        return history_ ? history_->world : *world_;
    }
    const History *history() const {
        return history_.get();
    }
    int selected() const {
        return selected_;
    }
    int unlocked() const;
    neon::FeedbackState feedback() const;
    bool resultSummary() const {
        return resultSummary_;
    }
    void pads(uint32_t held, uint32_t pressed, const neon::input::ControllerState *, int count);
    void touch(uint32_t id, int phase, neon::vec2d); // phase: down=0, move=1, up=2
    void back();
    void cancel();
    void tick();
    bool paused() const {
        return page == Page::Paused || (page == Page::Scores && returnPage_ == Page::Paused);
    }
    void drawGrid(GridRenderer &, ActorRenderer &, neon::vec2d, bool);
    void drawActors(ActorRenderer &, neon::vec2d, bool);
    void drawUI(GridRenderer &, ActorRenderer &, neon::vec2d);
};
} // namespace deflex
