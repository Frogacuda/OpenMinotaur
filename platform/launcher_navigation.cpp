#include "launcher.h"
#include "game_catalog.h"
#include "gridrunner_choices.h"
#include "about_screen.h"
#include "build_info.h"
#include <algorithm>

namespace mino {
void Launcher::returnHome() {
    cancelAppLaunch();
    externalLaunch_ = false;
    navigation_.home();
    homeEntrance_.start();
    request_ = {};
    pause(false);
}
void Launcher::open(Page destination, int initialFocus) {
    cancelAppLaunch();
    homeEntrance_.finish();
    pause(false);
    navigation_.open(destination, initialFocus);
}
void Launcher::close() {
    pause(false);
    if (!navigation_.back())
        navigation_.home();
}
void Launcher::selectGame(const std::string &id) {
    for (size_t i = 0; i < kGameCatalog.size(); ++i)
        if (id == kGameCatalog[i].id)
            navigation_.selectHome(int(i));
}
void Launcher::showMessage(const std::string &title, const std::string &message) {
    messageScreen_.title = title;
    messageScreen_.body = message;
    // Replace the contents of an existing message without nesting another route.
    if (page() != Page::Message)
        open(Page::Message);
}
Launcher::Request Launcher::takeRequest() {
    Request result = request_;
    request_ = {};
    return result;
}
void Launcher::pause(bool suspended) {
    if (suspended && calibratingTilt_) {
        calibratingTilt_ = false;
        request_ = {Action::CancelTilt, {}};
    }
    aboutAxis_ = 0;
    noteAxis_ = 0;
    touching_ = false;
    aboutScreen_.dragging = false;
    pressedItem_ = -1;
    heldDx_ = heldDy_ = 0;
    repeat_ = 0;
}
void Launcher::mixAudio(float *out, int frames) {
    std::fill(out, out + frames * 2, 0.0f);
}

int Launcher::buttonCount() const {
    return (this->*screenHandlers().count)();
}
int Launcher::homeButtonCount() const {
    return iox::home::count;
}
int Launcher::aboutButtonCount() const {
    return iox::about::count;
}
int Launcher::gridrunnerButtonCount() const {
    return GridrunnerChoiceLayout::count + 1;
}
int Launcher::optionsButtonCount() const {
    if (page() == Page::Options)
        return iox::options::firstRow + int(optionRows().size());
    return iox::options::firstRow + (page() == Page::Mapping ? 1 : 4);
}
int Launcher::dialogButtonCount() const {
    return page() == Page::Quit ? 2 : 1;
}
MenuRect Launcher::pageButton(int i) const {
    return (this->*screenHandlers().button)(i);
}
MenuRect Launcher::homeButton(int i) const {
    return layout_.items[size_t(i)];
}
MenuRect Launcher::aboutButton(int i) const {
    const AboutLayout about(layout_);
    return i == iox::about::body ? about.body : i == iox::about::donate ? about.donate : about.back;
}
MenuRect Launcher::gridrunnerButton(int i) const {
    const GridrunnerChoiceLayout choice(layout_.portrait);
    return i == GridrunnerChoiceLayout::count ? choice.back : choice.rows[size_t(i)];
}
MenuRect Launcher::dialogButton(int i) const {
    const float w = std::min(340.0f, layout_.width - 80);
    const float top =
        std::min(layout_.height - 158, layout_.height - 94 - (buttonCount() - 1) * 56.0f);
    return {(layout_.width - w) * 0.5f, top + i * 56.0f, w, 46};
}
int Launcher::hit(float x, float y) const {
    if (!controllerPresent_ && !notesPage() && page() != Page::About &&
        !iox::isOptionsScreen(page()) &&
        MenuRect{layout_.width / 2 - 42, layout_.height - 28, 84, 28}.contains(x, y))
        return -2;
    if (page() == Page::Home)
        return layout_.hit(x, y);
    if (page() == Page::Gridrunner)
        return GridrunnerChoiceLayout(layout_.portrait).hit(x, y);
    for (int i = 0; i < buttonCount(); ++i)
        if (visibleControl(i) && pageButton(i).contains(x, y))
            return i;
    return -1;
}
bool Launcher::visibleControl(int i) const {
    if (i < 0 || i >= buttonCount())
        return false;
    if (iox::isOptionsScreen(page()) && page() != Page::Options)
        return i == iox::options::back || i >= iox::options::firstRow;
    return true;
}
void Launcher::touch(const TouchEvent &e) {
    if (calibratingTilt_) {
        if (e.phase == TouchPhase::Down) {
            calibratingTilt_ = false;
            request_ = {Action::ConfirmTilt, {}};
        }
        return;
    }
    if (inputBlocked())
        return;
    const iox::CanvasTransform canvas(layout_, surface_.width, surface_.height);
    float x = canvas.canvasX(e.x);
    float y = canvas.canvasY(e.y);
    if (e.phase == TouchPhase::Down && !touching_) {
        touching_ = true;
        touchId_ = e.id;
        touchStartY_ = touchLastY_ = y;
        aboutScreen_.dragging = false;
        noteDragging_ = false;
        pressedItem_ = hit(x, y);
        if (pressedItem_ >= 0) {
            if (page() == Page::Home)
                navigation_.homeFocus() = pressedItem_;
            else
                focus() = pressedItem_;
        }
    } else if (touching_ && touchId_ == e.id) {
        if (page() == Page::Achievements && (pressedItem_ == 0 || noteDragging_)) {
            if (e.phase == TouchPhase::Move && (noteDragging_ || std::abs(y - touchStartY_) > 5)) {
                prepareAchievements();
                noteDragging_ = true;
                pressedItem_ = -1;
                noteScroll_ = std::clamp(noteScroll_ + touchLastY_ - y, 0.f,
                                         std::max(0.f, noteHeight_ - achievementBody().h));
            }
            touchLastY_ = y;
        }
        if (page() == Page::About && (pressedItem_ == 0 || aboutScreen_.dragging)) {
            if (e.phase == TouchPhase::Move &&
                (aboutScreen_.dragging || std::abs(y - touchStartY_) > 5)) {
                prepareAbout();
                aboutScreen_.dragging = true;
                pressedItem_ = -1;
                aboutScreen_.scroll =
                    std::clamp(aboutScreen_.scroll + touchLastY_ - y, 0.f,
                               std::max(0.f, aboutScreen_.height - AboutLayout(layout_).body.h));
            }
            touchLastY_ = y;
        }
        if (e.phase == TouchPhase::Move && hit(x, y) != pressedItem_)
            pressedItem_ = -1;
        if (e.phase == TouchPhase::Cancel) {
            touching_ = false;
            pressedItem_ = -1;
        } else if (e.phase == TouchPhase::Up) {
            const bool footerBack = pressedItem_ == -2 && hit(x, y) == -2;
            bool activateButton = pressedItem_ >= 0 && pressedItem_ == hit(x, y);
            if (activateButton) {
                if (page() == Page::Home)
                    navigation_.homeFocus() = pressedItem_;
                else
                    focus() = pressedItem_;
            }
            touching_ = false;
            pressedItem_ = -1;
            if (footerBack)
                back();
            else if (activateButton)
                activate();
        }
    }
}
void Launcher::move(int dx, int dy) {
    (this->*screenHandlers().move)(dx, dy);
}
void Launcher::moveHome(int dx, int dy) {
    focus() = layout_.move(focus(), dx, dy);
}
void Launcher::moveGridrunner(int dx, int dy) {
    focus() = GridrunnerChoiceLayout(layout_.portrait).move(focus(), dx, dy);
}
void Launcher::moveDialog(int, int dy) {
    if (dy)
        focus() = std::clamp(focus() + dy, 0, buttonCount() - 1);
}
void Launcher::key(int dx, int dy, bool select, bool goBack) {
    if (inputBlocked())
        return;
    if (dx || dy)
        move(dx, dy);
    if (goBack)
        back();
    else if (select)
        activate();
}
void Launcher::pads(const PadState *pads, int count) {
    controllerPresent_ = false;
    if (pads)
        for (int i = 0; i < std::clamp(count, 0, kMaxPads); ++i)
            controllerPresent_ |= pads[i].connected;
    if (calibratingTilt_) {
        bool held = false;
        for (int i = 0; i < count; ++i)
            if (pads[i].connected)
                held |= pads[i].buttons || pads[i].lt > .5f || pads[i].rt > .5f;
        if (!held)
            calibrationArmed_ = true;
        else if (calibrationArmed_) {
            calibratingTilt_ = false;
            request_ = {Action::ConfirmTilt, {}};
        }
        return;
    }
    if (inputBlocked())
        return;
    uint32_t pressed = 0;
    int dx = 0, dy = 0;
    aboutAxis_ = 0;
    noteAxis_ = 0;
    for (int i = 0; i < count; ++i) {
        const auto &p = pads[i];
        if (!p.connected)
            continue;
        pressed |= p.pressed;
        if (page() == Page::Achievements && focus() == 0 && std::abs(p.ly) > .18f &&
            std::abs(p.ly) >= std::abs(p.lx) &&
            !(p.buttons & (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT))) {
            if (noteAxis_ == 0)
                noteAxis_ = p.ly;
            continue;
        }
        if (page() == Page::About && std::abs(p.ly) > .18f && std::abs(p.ly) >= std::abs(p.lx) &&
            !(p.buttons & (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT))) {
            if (aboutAxis_ == 0)
                aboutAxis_ = p.ly;
            continue;
        }

        if (dx || dy)
            continue; // First controller with a direction owns movement.
        dx = (p.buttons & PAD_LEFT) ? -1 : (p.buttons & PAD_RIGHT) ? 1 : 0;
        dy = (p.buttons & PAD_UP) ? -1 : (p.buttons & PAD_DOWN) ? 1 : 0;
        if (!dx && !dy) {
            if (std::abs(p.lx) > 0.45f && std::abs(p.lx) >= std::abs(p.ly))
                dx = p.lx < 0 ? -1 : 1;
            else if (std::abs(p.ly) > 0.45f)
                dy = p.ly < 0 ? -1 : 1;
        }
        if (dx)
            dy = 0;
    }
    if (dx != heldDx_ || dy != heldDy_) {
        heldDx_ = dx;
        heldDy_ = dy;
        repeat_ = 0.32;
        if (dx || dy)
            move(dx, dy);
    } else if ((dx || dy) && repeat_ <= 0) {
        move(dx, dy);
        repeat_ = 0.12;
    }
    if (pressed & (PAD_B | PAD_BACK))
        back();
    else if (pressed & PAD_A)
        activate();
}
bool Launcher::back() {
    if (calibratingTilt_) {
        calibratingTilt_ = false;
        request_ = {Action::CancelTilt, {}};
        close();
        return true;
    }
    if (inputBlocked())
        return true;
    pause(false);
    if (page() == Page::Mapping)
        request_ = {Action::CancelRemap, {}};
    else if (page() == Page::Home)
        open(Page::Quit);
    else
        close();
    return true;
}
void Launcher::activate() {
    if (inputBlocked() || request_.action != Action::None)
        return;
    touching_ = false;
    pressedItem_ = -1;
    (this->*screenHandlers().activate)();
}
void Launcher::activateDialog() {
    if (page() == Page::Quit && focus() == 1)
        request_ = {Action::Quit, {}};
    else
        close();
}

const Launcher::ScreenHandlers &Launcher::screenHandlers() const {
    // Ordered by iox::Screen. Register presentation, actions and geometry together.
    static const ScreenHandlers screens[] = {
        {&Launcher::drawHome, &Launcher::activateHome, &Launcher::moveHome,
         &Launcher::homeButtonCount, &Launcher::homeButton, false},
        {&Launcher::drawOptions, &Launcher::activateOption, &Launcher::moveOptions,
         &Launcher::optionsButtonCount, &Launcher::optionButton, true},
        {&Launcher::drawOptions, &Launcher::activateOption, &Launcher::moveOptions,
         &Launcher::optionsButtonCount, &Launcher::optionButton, true},
        {&Launcher::drawOptions, &Launcher::activateOption, &Launcher::moveOptions,
         &Launcher::optionsButtonCount, &Launcher::optionButton, true},
        {&Launcher::drawOptions, &Launcher::activateOption, &Launcher::moveOptions,
         &Launcher::optionsButtonCount, &Launcher::optionButton, true},
        {&Launcher::drawAbout, &Launcher::activateAbout, &Launcher::moveAbout,
         &Launcher::aboutButtonCount, &Launcher::aboutButton, false},
        {&Launcher::drawDialog, &Launcher::activateGame, &Launcher::moveDialog,
         &Launcher::dialogButtonCount, &Launcher::dialogButton, true},
        {&Launcher::drawGridrunnerChoices, &Launcher::activateGame, &Launcher::moveGridrunner,
         &Launcher::gridrunnerButtonCount, &Launcher::gridrunnerButton, false},
        {&Launcher::drawDialog, &Launcher::activateDialog, &Launcher::moveDialog,
         &Launcher::dialogButtonCount, &Launcher::dialogButton, true},
        {&Launcher::drawDialog, &Launcher::activateDialog, &Launcher::moveDialog,
         &Launcher::dialogButtonCount, &Launcher::dialogButton, true},
        {&Launcher::drawAchievements, &Launcher::activateAchievements, &Launcher::moveAchievements,
         &Launcher::achievementButtonCount, &Launcher::achievementButton, false},
        {&Launcher::drawAchievements, &Launcher::activateAchievements, &Launcher::moveAchievements,
         &Launcher::achievementButtonCount, &Launcher::achievementButton, false},
    };
    static_assert(sizeof(screens) / sizeof(screens[0]) == size_t(Page::Count),
                  "Register every iOX screen");
    return screens[size_t(page())];
}
} // namespace mino
