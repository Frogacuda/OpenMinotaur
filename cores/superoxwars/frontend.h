#pragma once
#include "scene.h"
#include "records.h"
#include <deque>
namespace superoxwars {
struct TextLine {
    int x = 0, y = 0;
    std::string text;
    uint32_t colour = 0xffffffff;
    bool centred = false;
};
// moo's frontend owner. Commands write retained cellGrid state once per update;
// clearing a row is explicit, rather than erasing the screen every draw.
class Frontend {
  public:
    enum class Screen { Loading, Title, Select, Playing, Paused, Scores, Goals, GameOver, Error };
    enum class Phase { ResetAttract = -999, Select = -1, Attract = 0, Start = 1, Play = 2 };
    Screen screen = Screen::Loading, scoreReturn = Screen::Title;
    Phase phase = Phase::ResetAttract;
    bool menuFocus = false;
    Records records;
    int selectedLevel = 0, selection = 0, screenTicks = 0, attractIndex = 0, attractTick = 0;
    float pauseOpacity = 1, exitOpacity = 0, levelPulse = 1;
    int pulseDirection = 0;
    bool fromBeginning = true, quitRequested = false, touchBlocked = true;
    uint64_t textRevision = 0;
    bool clearGrid = true;
    std::vector<int> clearRows;
    float playRowOffset = -.5f;
    void enter(Screen);
    void attach(World &, Scene &);
    void command(World &, Scene &, uint32_t edge);
    void activate(World &, int action);
    void run(World &, Scene &);
    void tick(World &);
    void goal(int);
    void clearGoals() {
        goals_.clear();
    }
    int popGoal() {
        if (goals_.empty())
            return -1;
        const int id = goals_.front();
        goals_.pop_front();
        return id;
    }
    void finish(World &);
    void prepare(const World &, const Scene &, std::vector<TextLine> &,
                 std::vector<ActorDraw> &) const;
    void capabilities(bool motion, bool controller);
    bool tiltChoiceVisible() const {
        return motionAvailable_ && controllerConnected_;
    }
    bool controllerConnected() const {
        return controllerConnected_;
    }
    int pauseCount() const {
        return tiltChoiceVisible() ? 4 : 3;
    }
    int pauseAction(int index) const {
        return index == 0 ? 5 : index == 1 ? 7 : index == 2 && tiltChoiceVisible() ? 8 : 6;
    }

  private:
    bool motionAvailable_ = false, controllerConnected_ = false;
    std::deque<int> goals_;
    std::vector<TextLine> writes_;
    bool enterSelect_ = false, banked_ = false, pendingClear_ = false;
    int lastPage_ = -1;
    void startAttract(World &, Scene &);
    void startPlay(World &, Scene &);
    void makeSelector(World &);
    void makePause(World &);
    void attract(World &);
    void selector(World &);
    void hud(World &);
    void scores(const World &, bool staged);
    void line(int x, int row, std::string text, uint32_t colour = 0xffffffffu);
    void centre(int row, std::string text, uint32_t colour = 0xffffffffu);
    void clear(int first, int last);
};
} // namespace superoxwars
