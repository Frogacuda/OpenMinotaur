#pragma once
#include "stage_presentation.h"
#include "stage_hud.h"
#include "name_generator.h"
#include "neon/menu_navigation.h"
#include "neon/controller_session.h"
namespace goatup2 {
// Original +3b4/+3d4 frontend lists, represented independently of live actors.
// Controller focus is a host addition; it invokes the same actions as touch.
class StageFrontend {
  public:
    enum class Page { Loading, Title, Banks, Preview, Playing, Editor, Credits, Attract };
    enum Action {
        None = 0,
        Begin = 1,
        Choose0 = 10,
        Choose1,
        Choose2,
        Choose3,
        Previous = 20,
        Next,
        Play,
        Return = 30,
        IdleCycle,
        Resume = 40,
        Exit,
        Hand,
        Reset,
        Title,
        Tilt,
        ToggleEdit = 60,
        BankRename0 = 70,
        BankRename1,
        BankRename2,
        BankRename3,
        BankLoad0 = 80,
        BankLoad1,
        BankLoad2,
        BankLoad3,
        BankSave0 = 90,
        BankSave1,
        BankSave2,
        BankSave3
    };
    struct Button {
        int action, sprite;
        neon::vec2d position, scale;
        float radius, baseRadius;
        double alpha;
    };
    // Reconstruction coverage, not an original unlock/progression rule.
    static constexpr bool campaignPlayable(int bank) {
        return bank >= 0 && bank < 4;
    }
    bool editing = false;
    Page page = Page::Loading;
    bool paused = false;
    int bank = 0, level = 0;
    std::array<Bank, 4> banks;
    std::array<std::string, 4> names;
    std::string previewName; // original level-name buffer, refreshed on template load
    void initialize(StageRuntime &);
    void enter(Page, StageRuntime &);
    void setPaused(bool, StageRuntime &);
    void pads(const neon::input::ControllerState *p, int count);
    void touch(neon::input::TouchPhase, uint32_t, neon::vec2d);
    void cancelInput(bool preserveHeld = false);
    int tick(StageRuntime &, uint32_t frame);
    void prepare(StageRuntime &, StageHud &);
    void prepareText(StageRuntime &, StageHud &);
    void prepareDraws(StageRuntime &);
    void activatePulse(int action);
    bool anyInput() const;
    const std::vector<StageDraw> &draws() const {
        return draws_;
    }
    const std::vector<StageDraw> &foregroundDraws() const {
        return foreground_;
    }
    const std::vector<Button> &buttons() const {
        return buttons_;
    }
    int focus() const {
        return navigation_.sel;
    }
    void focusAction(int action) {
        navigation_.sel = action;
        navigation_.screen = int(page) * 2 + int(paused);
    }
    int resetTicks() const {
        return resetTicks_;
    }
    int attractRemaining() const {
        return attractRemaining_;
    }
    void armReset() {
        resetTicks_ = 60;
    }
    void capabilities(bool touch, bool motion, StageRuntime &);
    int touchMode = 0; // Original left/right touch layout, independent of tilt.
    bool tiltEnabled = false, keyboardNavigation = false;

  private:
    void buildButtons(StageRuntime &);
    void add(StageRuntime &, int action, int sprite, neon::vec2d, float radius,
             neon::vec2d scale = {0, 0}, double alpha = 1);
    void sprite(StageRuntime &, int, neon::vec2d, float, neon::colour, neon::vec2d scale = {0, 0},
                double angle = 0);
    void credits(StageRuntime &, StageHud &);
    // Original selector table 0x76ea4 and control-mode table 0x76e98.
    static constexpr int originalPlaySprite_ = 154, originalNextSprite_ = 100,
                         originalPreviousSprite_ = 101;
    static constexpr int handSprites_[3] = {3, 7, 8};
    double playAspect_ = 1, nextAspect_ = 1, previousAspect_ = 1;
    neon::input::ControllerFrame pads_;
    neon::input::PadMenu navigation_;
    std::vector<Button> buttons_;
    std::vector<StageDraw> draws_, foreground_;
    int ticks_ = 0, buckTicks_ = 0, resetTicks_ = 0, attractRemaining_ = 0;
    float slide_ = 0, buckRadius_ = .1f, buckAngle_ = 0;
    bool touchAvailable_ = false, motionAvailable_ = false;
    bool touchHeld_ = false, touchPressed_ = false, armed_ = false, padUsed_ = false;
    uint32_t touchId_ = 0;
    neon::vec2d touchPoint_;
};
} // namespace goatup2
