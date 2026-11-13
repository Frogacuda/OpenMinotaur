#pragma once
#include "editor_document.h"
#include "stage_presentation.h"
#include "stage_hud.h"
#include "neon/controller_session.h"
#include "neon/menu_navigation.h"
namespace goatup2 {
// Original cursor/bo/editor toolbar, with direct native pad actions added.
class EditorView {
  public:
    enum Action {
        None = 0,
        Mode = 1,
        Test,
        Return,
        Clear,
        Rename,
        Undo,
        ShiftLeft,
        ShiftRight,
        ShiftUp,
        ShiftDown
    };
    EditorDocument document;
    int mode = 0, previousMode = 0, column = 50, row = 49;
    bool changed = false, stroke = false, keyboardNavigation = false;
    std::string error, bankName, levelName;
    void open(const Stage &);
    void cancel();
    void pads(const neon::input::ControllerState *, int);
    void touch(neon::input::TouchPhase, uint32_t, neon::vec2d);
    int tick(StageRuntime &);
    void prepare(StageRuntime &, StageHud &);
    bool consumes(neon::vec2d p) const;
    const std::vector<StageDraw> &draws() const {
        return draws_;
    }
    neon::vec2d phase{0, 0}, velocity{0, 0};

  private:
    struct Finger {
        uint32_t id = 0;
        bool held = false, down = false;
        neon::vec2d point, last;
    };
    struct Button {
        int action, sprite;
        neon::vec2d position;
    };
    std::array<Finger, 4> fingers_{};
    neon::input::ControllerFrame pads_;
    neon::input::PadMenu navigation_;
    std::vector<StageDraw> draws_;
    std::vector<Button> buttons() const;
    bool armed_ = false, canvas_ = false, multi_ = false, painted_ = false, axisX_ = false,
         axisY_ = false;
    int gesture_ = 0, finger_ = -1, lastColumn_ = 50, lastRow_ = 49, toolRepeat_ = 0;
    neon::vec2d pickerPosition{.5, .5}, pickerVelocity{0, 0}, pickerTravel{0, 0};
    int pickerFinger_ = -1, pickerRecent_ = 0, pickerLockTicks_ = 0;
    bool pickerBlockX_ = true, pickerBlockY_ = true;
    void mapping(StageRuntime &);
    void objectDraw(StageRuntime &, const Placement &, bool cursor);
    void remove(StageRuntime &, int state);
};
} // namespace goatup2
