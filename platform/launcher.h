// iOX is a platform screen using the same input/display contract as the games.
#pragma once
#include "core_api.h"
#include "launcher_layout.h"
#include "launcher_navigation.h"
#include "launcher_canvas.h"
#include "launcher_entrance.h"
#include "launcher_launch.h"
#include "settings.h"
#include "achievements.h"
#include <array>
#include <string>
#include <vector>

namespace mino {

class Launcher final : public Core {
  public:
    enum class Action {
        None,
        Launch,
        Quit,
        Donate,
        SettingsChanged,
        Remap,
        CancelRemap,
        ResetMapping,
        CalibrateTilt,
        ConfirmTilt,
        CancelTilt
    };
    struct Request {
        Action action = Action::None;
        std::string gameId;
    };

    void setPaths(const std::string &prefDir);
    void setPortrait(bool portrait);
    void refresh(); // Reload icons and bundled-resource availability; GL must be current.
    // GL must be current. Empty selects the built-in orientation pair; a failed external
    // image leaves the current wallpaper intact. Selection/persistence belong
    // to the Options UI, independently of this external-image loader.
    bool setWallpaper(const std::string &imagePath);
    void returnHome();
    void selectGame(const std::string &id);
    void showMessage(const std::string &title, const std::string &message);
    Request takeRequest();
    void key(int dx, int dy, bool select, bool back);
    // The host supplies real startup frames; negative progress ends that launch.
    void externalLaunchProgress(float progress);
    void configureOptions(Settings &settings, bool desktop, bool pointer, int controllers,
                          const std::vector<std::string> &resolutions);
    void setControllerPrompts(bool connected, const std::string &select, const std::string &back) {
        controllerPresent_ = connected;
        selectLabel_ = select;
        backLabel_ = back;
    }
    void setAchievements(const Achievements &records) {
        achievementRecords_ = &records;
    }
    void mappingProgress(int step, bool release);
    void beginTiltCalibration() {
        showMessage("Tilt Calibration", "Hold the device in a comfortable neutral position. Hold "
                                        "still, then tap anywhere or press any button.");
        calibratingTilt_ = true;
        calibrationArmed_ = false;
    }
    void mappingFinished(const std::string &message);

    const CoreInfo &info() const override;
    bool init(Host &, const SurfaceInfo &) override;
    void resize(const SurfaceInfo &surface) override;
    void frame(double dt) override;
    void touch(const TouchEvent &) override;
    void pads(const PadState *, int count) override;
    void mixAudio(float *out, int frames) override;
    void pause(bool) override;
    void save() override {}
    bool back() override;
    void shutdown() override;
    void abandonGraphics();
    bool restoreGraphics(Host &, const SurfaceInfo &);

  private:
    friend struct LauncherChecks;
    using Page = iox::Screen;
    Page page() const {
        return navigation_.screen();
    }
    int &focus() {
        return navigation_.focus();
    }
    int focus() const {
        return navigation_.focus();
    }
    bool menuFocusVisible() const {
        return controllerPresent_ || (host_ && host_->keyboardNavigationAvailable());
    }
    bool focused(int item) const {
        return menuFocusVisible() && focus() == item;
    }
    void open(Page destination, int initialFocus = 0);
    void close();
    void openApp(Page destination, int initialFocus = 0);
    void cancelAppLaunch();
    void drawAppLaunch();
    void activateHome();
    void activateGame();
    void activateAbout();
    void moveOptions(int dx, int dy);
    void moveAbout(int dx, int dy);
    void drawDock();
    void drawHomeIcons();
    void drawStatusBar();
    void drawBarBacking();
    void drawFooter();
    std::string footerStatus() const;
    bool calibratingTilt_ = false, calibrationArmed_ = false;
    bool controllerPresent_ = false;
    std::string selectLabel_, backLabel_;
    // One registration point binds each screen's behavior. New screens do not
    // need parallel additions to draw/input/count/geometry switch statements.
    struct ScreenHandlers {
        void (Launcher::*draw)();
        void (Launcher::*activate)();
        void (Launcher::*move)(int, int);
        int (Launcher::*count)() const;
        MenuRect (Launcher::*button)(int) const;
        bool dimBackground;
    };
    const ScreenHandlers &screenHandlers() const;
    void moveHome(int dx, int dy);
    void moveGridrunner(int dx, int dy);
    void moveDialog(int dx, int dy);
    void activateDialog();
    void drawDialog();
    int homeButtonCount() const;
    int optionsButtonCount() const;
    int aboutButtonCount() const;
    int gridrunnerButtonCount() const;
    int dialogButtonCount() const;
    MenuRect homeButton(int i) const;
    MenuRect aboutButton(int i) const;
    MenuRect gridrunnerButton(int i) const;
    MenuRect dialogButton(int i) const;
    struct Vertex {
        float x, y, u, v, r, g, b, a;
    };
    struct Colour {
        float r, g, b, a = 1;
    };
    struct Icon {
        unsigned texture = 0;
        bool playable = false, installed = false;
    };
    void move(int dx, int dy);
    void activate();
    int hit(float x, float y) const;
    int buttonCount() const;
    MenuRect pageButton(int i) const;
    bool visibleControl(int i) const;
    void drawHome();
    void drawWallpaper();
    void clearWallpapers();
    void drawPage();
    void drawGridrunnerChoices();
    struct OptionRow {
        std::string label, value, tip, key;
        std::vector<std::string> values;
        bool enabled = true;
    };
    std::vector<OptionRow> optionRows() const;
    MenuRect optionButton(int i) const;
    void drawOptions();
    void activateOption();
    bool setBuiltinWallpaper(int index);
    void drawAchievements();
    void activateAchievements();
    void moveAchievements(int, int);
    int achievementButtonCount() const;
    MenuRect achievementButton(int) const;
    void prepareAchievements();
    MenuRect achievementBody() const;
    bool notesPage() const {
        return page() == Page::Achievements || page() == Page::AchievementIndex;
    }
    const Achievements *achievementRecords_ = nullptr;
    struct NoteLine {
        std::string text;
        float y;
        int id;
        bool first;
    };
    std::vector<NoteLine> noteLines_;
    int noteGame_ = 0, notePreparedGame_ = -1;
    float noteWidth_ = 0, noteScroll_ = 0, noteHeight_ = 0, noteAxis_ = 0;
    bool noteDragging_ = false, notesWriting_ = false;
    unsigned notesFont_ = 0;
    void prepareAbout();
    void drawAbout();
    float focusPulse() const;
    void recessedButton(const MenuRect &, const std::string &, bool, bool arrow = false,
                        bool brown = false);
    void drawGridrunnerPreview(const MenuRect &, bool classic);
    void wrappedText(const std::string &, float x, float y, float width, float size, Colour colour);
    void icon(const MenuRect &, unsigned texture, const char *fallback, bool selected, Colour top,
              Colour bottom, bool gray = false);
    void rect(const MenuRect &, Colour top, Colour bottom, bool rounded = false, float gloss = 0,
              float cornerRadius = -1);
    void text(const std::string &, float x, float y, float size, Colour colour,
              bool centered = true, bool bold = false);
    float textWidth(const std::string &, float size, bool bold = false) const;
    void paragraph(const std::string &, float y, float scale);
    enum class ImageMode { Normal, Shadow, Halo, Capture };
    void draw(unsigned texture, bool rounded = false, float gloss = 0, bool gray = false,
              float sdfWidth = 0, bool circle = false, ImageMode mode = ImageMode::Normal);
    unsigned loadTexture(const std::string &path, int *imageW = nullptr, int *imageH = nullptr);
    unsigned loadTexture(const unsigned char *bytes, size_t size, int *imageW = nullptr,
                         int *imageH = nullptr);
    unsigned makeTexture(int w, int h, const unsigned char *rgba);
    bool makeFont();
    void roundEdge(const MenuRect &, float cornerRadius = -1);
    void quad(const MenuRect &, float u0, float v0, float u1, float v1, Colour top, Colour bottom);

    Host *host_ = nullptr;
    SurfaceInfo surface_;
    LauncherLayout layout_{false};
    std::string prefDir_;
    std::array<Icon, 9> icons_{};
    std::array<unsigned, 3> systemIcons_{};
    unsigned dockTexture_ = 0;
    std::array<unsigned, 2> gridrunnerPreviews_{};
    unsigned gridrunnerAtlas_ = 0;
    struct Wallpaper {
        unsigned texture = 0;
        int w = 0, h = 0;
    };
    std::array<Wallpaper, 2>
        wallpapers_{}; // Landscape, portrait. A user image may share a texture.
    unsigned program_ = 0, vbo_ = 0, white_ = 0, font_ = 0;
    int uSize_ = -1, uRounded_ = -1, uGloss_ = -1;
    int uGray_ = -1, uSdfWidth_ = -1, uRoundEdge_ = -1, uRoundScale_ = -1;
    std::vector<Vertex> vertices_;
    iox::Navigation navigation_;
    iox::HomeEntrance homeEntrance_;
    iox::AppLaunch appLaunch_;
    iox::HomeEntrance exitPose_;
    bool externalLaunch_ = false;
    unsigned appTexture_ = 0, appFramebuffer_ = 0;
    Page launchPage_ = Page::Home;
    int launchFocus_ = 0;
    iox::LayerTransform drawTransform_;
    bool enteringHome() const {
        return page() == Page::Home && homeEntrance_.active();
    }
    bool inputBlocked() const {
        return enteringHome() || appLaunch_.active() || externalLaunch_;
    }
    bool movingHome() const {
        return enteringHome() || appLaunch_.active() || externalLaunch_;
    }
    const iox::HomeEntrance &homePose() const {
        return appLaunch_.active() || externalLaunch_ ? exitPose_ : homeEntrance_;
    }
    struct GameScreenState {
        int game = 0;
    } gameScreen_;
    struct MessageScreenState {
        std::string title, body;
    } messageScreen_;
    struct OptionsScreenState {
        int tab = 0;
    } optionsScreen_;
    struct MappingScreenState {
        int step = 0, player = 0;
        bool release = true;
    } mappingScreen_;
    bool touching_ = false;
    uint32_t touchId_ = 0;
    int pressedItem_ = -1;
    struct AboutLine {
        std::string text;
        float y, size;
        bool bold;
    };
    struct AboutScreenState {
        std::vector<AboutLine> lines;
        float textWidth = 0, height = 0, scroll = 0;
        bool dragging = false;
    } aboutScreen_;
    float touchStartY_ = 0, touchLastY_ = 0;
    int heldDx_ = 0, heldDy_ = 0;
    float aboutAxis_ = 0;
    double repeat_ = 0;
    double selectionTime_ = 0;
    Request request_;
    Settings *settings_ = nullptr;
    bool desktop_ = true, pointer_ = true;
    int controllers_ = 0;
    std::vector<std::string> resolutions_;
    std::array<Wallpaper, 4> wallpaperPreviews_{};
    int wallpaperPreviewOrientation_ = -1;
};

} // namespace mino
