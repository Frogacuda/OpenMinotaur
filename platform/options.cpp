#include "launcher.h"
#include "ui_assets.h"
#include "control_mapping.h"
#include <glad/gl.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
namespace mino {
namespace {
const char *wallpaperNames[] = {"Waterdrop", "Vaporsheep", "Llama XP", "Wooly Sweater"};
}
void Launcher::configureOptions(Settings &s, bool desktop, bool pointer, int controllers,
                                const std::vector<std::string> &resolutions) {
    settings_ = &s;
    desktop_ = desktop;
    pointer_ = pointer;
    controllers_ = controllers;
    resolutions_ = resolutions;
}
bool Launcher::setBuiltinWallpaper(int index) {
    std::array<Wallpaper, 2> next{};
    for (int i = 0; i < 2; ++i) {
        auto image = ui::wallpaper(index, i == 1);
        next[size_t(i)].texture =
            loadTexture(image.data, image.size, &next[size_t(i)].w, &next[size_t(i)].h);
        if (!next[size_t(i)].texture) {
            if (next[0].texture)
                glDeleteTextures(1, &next[0].texture);
            return false;
        }
    }
    clearWallpapers();
    wallpapers_ = next;
    return true;
}
std::vector<Launcher::OptionRow> Launcher::optionRows() const {
    std::vector<OptionRow> rows;
    if (!settings_)
        return rows;
    auto get = [&](const char *key, const char *fallback) {
        return settings_->get("global", key, fallback);
    };
    if (optionsScreen_.tab == 0) {
        auto aspect = get("aspect", "ipad");
        rows.push_back({"Aspect ratio",
                        aspect == "fill"   ? "Fit to screen"
                        : aspect == "ipad" ? "4:3"
                                           : "2:3 (iPhone)",
                        aspect == "fill" ? "Developers of this game not responsible for any pixels "
                                           "squashed by the use of this option"
                        : aspect == "ipad"
                            ? "Original iPad aspect, also perfect for retro handhelds and CRTs"
                            : "Just like the original iPhone!",
                        "aspect",
                        {"ipad", "original", "fill"}});
        if (desktop_) {
            auto mode = get("window_mode", "windowed");
            rows.push_back({"Display Mode",
                            mode == "exclusive"    ? "Fullscreen"
                            : mode == "borderless" ? "Borderless"
                                                   : "Windowed",
                            mode == "exclusive" ? "How exclusive!"
                            : mode == "borderless"
                                ? "It looks like fullscreen, but it's not"
                                : "It's like a little screen floating on your big screen",
                            "window_mode",
                            {"windowed", "borderless", "exclusive"}});
            auto modes = resolutions_;
            modes.insert(modes.begin(), "desktop");
            auto res = get("fullscreen_resolution", "desktop");
            rows.push_back({"Resolution", res == "desktop" ? "Desktop" : res,
                            "You know what this does, right?", "fullscreen_resolution", modes,
                            mode != "borderless"});
        }
        auto orient =
            get("orientation", settings_->display("global").orientation == Orientation::Landscape
                                   ? "landscape"
                                   : "bestfit");
        const bool left = orient == "portrait" || orient == "vertical_left",
                   right = orient == "portrait_right" || orient == "vertical_right",
                   native = orient == "bestfit" || orient == "auto";
        rows.push_back({"Orientation",
                        left     ? "Vertical (Left)"
                        : right  ? "Vertical (Right)"
                        : native ? "Native"
                                 : "Horizontal",
                        left || right ? "Tate mode for your arcade cabinet, you nerd."
                        : native ? "Always use original display mode, perfect for mobile devices."
                                 : "For TVs, monitors, etc.",
                        "orientation",
                        {"landscape", "portrait", "portrait_right", "bestfit"}});
        auto screen = get("screen_mode", "hires");
        const std::vector<std::string> modes = {"hires",     "smooth", "sharp",
                                                "scanlines", "crt",    "crt2"};
        int i = int(std::find(modes.begin(), modes.end(), screen) - modes.begin());
        if (i == int(modes.size()))
            i = 0;
        const char *labels[] = {
            "Hi-res", "Original (smooth)", "Original (sharp)", "Scanlines", "CRT 1", "CRT 2"};
        const char *tips[] = {"Lovely hi-res rendering of the same old pixelated sprites.",
                              "Original res with bilinear upscale.",
                              "Don't cut yourself on the jagged edges.",
                              "Because pixels need elbow room.",
                              "Glowy retro tube TV. May impact performance on low-end devices.",
                              "Lightweight retro tube TV with S-Video."};
        rows.push_back({"Filters", labels[i], tips[i], "screen_mode", modes});
        auto bg = get("backdrop", "colour");
        rows.push_back(
            {"Letterboxing",
             bg == "neon" ? "Neon" : "Black",
             bg == "neon" ? "Trippy fuzzy colors. Very on-brand." : "Standard issue black bars.",
             "backdrop",
             {"colour", "neon"}});
        int wp = std::clamp(std::atoi(get("wallpaper", "0").c_str()), 0, 3);
        rows.push_back(
            {"iOX Wallpaper", wallpaperNames[wp], "Change your iOS Wallpaper.", "wallpaper", {}});
    } else {
        if (pointer_) {
            const bool disabled = get("disable_touch", "off") == "on";
            rows.push_back({"Disable Touch Input",
                            controllers_ == 0 ? "Off (controller required)"
                            : disabled        ? "On"
                                              : "Off",
                            "Perfect for clumsy, fat fingers that can't stop touching the screen.",
                            "disable_touch",
                            {"off", "on"},
                            controllers_ > 0});
        }
        rows.push_back({"Remap controls",
                        "Choose player...",
                        "Choose a player, then assign the D-pad, face buttons, sticks, triggers, "
                        "Start and Select. Release each input between prompts.",
                        "remap",
                        {}});
        rows.push_back(
            {"Tilt Calibration",
             "Set neutral position",
             "Hold the device comfortably, then tap or press a button to set neutral tilt.",
             "calibrate_tilt",
             {}});
        rows.push_back({"Reset Control Mapping",
                        "Restore defaults",
                        "Reset all controllers to default.",
                        "reset_mapping",
                        {}});
        auto rotate = get("rotate_controls", "off");
        rows.push_back({"Rotate controls to match screen",
                        rotate == "on" ? "On" : "Off",
                        "Rotate controls when game is rotated, perfect for weirdos who want to "
                        "play Gridrunner as a side-scroller.",
                        "rotate_controls",
                        {"off", "on"}});
    }
    return rows;
}
MenuRect Launcher::optionButton(int i) const {
    float w = layout_.width;
    constexpr float top = LauncherLayout::statusBarHeight;
    if (i == 0)
        return {w * .25f, top + 39, w * .25f, 28};
    if (i == 1)
        return {w * .5f, top + 39, w * .25f, 28};
    if (i == iox::options::back)
        return {18, top + 8, 84, 27};
    if (page() == Page::Options)
        return {18, top + 76 + float(i - iox::options::firstRow) * 38, w - 36, 33};
    if (page() == Page::Mapping)
        return {w / 2 - 90, layout_.height - 133, 180, 38};
    if (page() == Page::Players)
        return {w / 2 - 150, top + 82 + float(i - iox::options::firstRow) * 57, 300, 45};
    const int cell = i - iox::options::firstRow;
    float gap = 14, cw = (w - 50) / 2, ch = (layout_.height - top - 176) / 2;
    return {18 + (cell % 2) * (cw + gap), top + 77 + (cell / 2) * (ch + gap), cw, ch};
}
void Launcher::activateOption() {
    if (focus() == iox::options::back) {
        back();
        return;
    }
    if (page() == Page::Mapping) {
        request_ = {Action::CancelRemap, {}};
        return;
    }
    if (page() == Page::Wallpapers) {
        int i = focus() - iox::options::firstRow;
        if (i >= 0 && i < 4 && setBuiltinWallpaper(i)) {
            settings_->set("global", "wallpaper", std::to_string(i));
            settings_->save();
            close();
        }
        return;
    }
    if (page() == Page::Players) {
        int i = focus() - iox::options::firstRow;
        if (i >= 0 && i < 4) {
            mappingScreen_.player = i;
            mappingScreen_.step = 0;
            mappingScreen_.release = true;
            open(Page::Mapping, iox::options::firstRow);
            request_ = {Action::Remap, std::to_string(i)};
        }
        return;
    }
    if (focus() < iox::options::back) {
        optionsScreen_.tab = focus();
        focus() = iox::options::firstRow;
        return;
    }
    auto rows = optionRows();
    int index = focus() - iox::options::firstRow;
    if (index < 0 || index >= int(rows.size()))
        return;
    const auto &row = rows[size_t(index)];
    if (!row.enabled)
        return;
    if (row.key == "wallpaper") {
        open(Page::Wallpapers,
             iox::options::firstRow +
                 std::clamp(std::atoi(settings_->get("global", "wallpaper", "0").c_str()), 0, 3));
        return;
    }
    if (row.key == "remap") {
        open(Page::Players, iox::options::firstRow);
        return;
    }
    if (row.key == "calibrate_tilt") {
        request_ = {Action::CalibrateTilt, {}};
        return;
    }
    if (row.key == "reset_mapping") {
        request_ = {Action::ResetMapping, {}};
        return;
    }
    auto old = settings_->get("global", row.key, row.values.front());
    auto found = std::find(row.values.begin(), row.values.end(), old);
    int next = found == row.values.end()
                   ? 0
                   : (int(found - row.values.begin()) + 1) % int(row.values.size());
    settings_->set("global", row.key, row.values[size_t(next)]);
    settings_->save();
    request_ = {Action::SettingsChanged, {}};
}
void Launcher::mappingProgress(int step, bool release) {
    mappingScreen_.step = step;
    mappingScreen_.release = release;
}
void Launcher::mappingFinished(const std::string &message) {
    // Capture completes/cancels asynchronously in the host. Unwind the workflow
    // before presenting its result, retaining the originating remap row.
    while (page() == Page::Mapping || page() == Page::Players)
        close();
    showMessage("Controller mapping", message);
}
void Launcher::drawOptions() {
    const Colour ink{.06f, .07f, .08f}, valueInk{.23f, .34f, .49f};
    const Colour white{1, 1, 1}, disabledInk{.53f, .55f, .58f};
    const float width = layout_.width, bottom = layout_.height - 78;
    constexpr float top = LauncherLayout::statusBarHeight;

    // Opaque pinstriped Settings background, rather than a home-screen overlay.
    rect({0, top, width, bottom - top}, {.78f, .81f, .84f}, {.78f, .81f, .84f});
    for (float x = 0; x < width; x += 5)
        rect({x, top, 2, bottom - top}, {.83f, .85f, .87f}, {.83f, .85f, .87f});
    const bool main = page() == Page::Options;
    const float headerHeight = main ? 72.f : 43.f, headerBottom = top + headerHeight;
    rect({0, top, width, headerHeight}, {.65f, .72f, .81f}, {.36f, .46f, .59f});
    // Restrained glass sheen, with a light rim and dark lower bevel/shadow.
    rect({0, top, width, headerHeight * .48f}, {1, 1, 1, .17f}, {1, 1, 1, .025f});
    rect({0, top, width, 1}, {.91f, .94f, .98f}, {.91f, .94f, .98f});
    rect({0, headerBottom - 2, width, 1}, {.52f, .61f, .73f}, {.52f, .61f, .73f});
    rect({0, headerBottom - 1, width, 1}, {.22f, .30f, .41f}, {.22f, .30f, .41f});
    rect({0, headerBottom, width, 2}, {0, 0, 0, .20f}, {0, 0, 0, 0});
    const std::string title = page() == Page::Wallpapers ? "iOX Wallpaper"
                              : page() == Page::Players  ? "Choose player"
                              : page() == Page::Mapping
                                  ? "Remap Player " + std::to_string(mappingScreen_.player + 1)
                                  : "Settings";
    // Upper shadow and lower light catch give the centered lettering a soft inset.
    text(title, width / 2 + .3f, top + 8.3f, 22, {.12f, .19f, .28f, .55f}, true, true);
    text(title, width / 2 - .3f, top + 9.8f, 22, {1, 1, 1, .35f}, true, true);
    text(title, width / 2, top + 9, 22, {.96f, .97f, .99f}, true, true);

    // Reuse current control rectangles and actions; only their presentation changes.
    auto button = [&](int slot, const MenuRect &r, const std::string &label) {
        const bool selected = focused(slot);
        rect({r.x, r.y + 1, r.w, r.h}, {.24f, .30f, .38f}, {.24f, .30f, .38f}, true);
        rect(r, selected ? Colour{.21f, .56f, .94f} : Colour{.53f, .64f, .77f},
             selected ? Colour{0, .25f, .69f} : Colour{.26f, .39f, .56f}, true);
        text(label, r.cx(), r.y + (r.h - 16) * .5f + 1, 16, {.12f, .19f, .29f}, true, true);
        text(label, r.cx(), r.y + (r.h - 16) * .5f, 16, white, true, true);
    };
    recessedButton(optionButton(iox::options::back), "Back", focus() == iox::options::back, true);

    auto panel = [&](const MenuRect &r) {
        rect({r.x, r.y + 1, r.w, r.h}, {.91f, .93f, .94f}, {.91f, .93f, .94f}, true, false, 8);
        rect(r, {.59f, .62f, .65f}, {.59f, .62f, .65f}, true, false, 8);
        rect({r.x + 1, r.y + 1, r.w - 2, r.h - 2}, white, white, true, false, 7);
    };
    // Clip a full-panel rounded gradient to a row, preserving only the group's
    // outer corners. Drawing and input continue to use the same row rectangles.
    auto selection = [&](const MenuRect &group, const MenuRect &row) {
        const iox::CanvasTransform canvas(layout_, surface_.width, surface_.height);
        glEnable(GL_SCISSOR_TEST);
        glScissor(int(row.x * canvas.scaleX), int((layout_.height - row.y - row.h) * canvas.scaleY),
                  int(row.w * canvas.scaleX), int(row.h * canvas.scaleY));
        const auto gradient = [&](float y) {
            const float t = (y - row.y) / row.h;
            return Colour{0, .61f + (.28f - .61f) * t, 1 + (.84f - 1) * t};
        };
        rect({group.x + 1, group.y + 1, group.w - 2, group.h - 2}, gradient(group.y + 1),
             gradient(group.y + group.h - 1), true, false, 7);
        glDisable(GL_SCISSOR_TEST);
    };
    auto fit = [&](const std::string &label, float size, float available, bool bold) {
        return std::min(size, size * available / std::max(1.f, textWidth(label, size, bold)));
    };

    if (main) {
        // One recessed capsule; clipping each fill to its half leaves a square
        // shared edge and rounds only the control's two outer ends.
        const auto left = optionButton(0), right = optionButton(1);
        const MenuRect tabs{left.x, left.y, right.x + right.w - left.x, left.h};
        rect({tabs.x, tabs.y + 1, tabs.w, tabs.h}, {.87f, .91f, .96f}, {.87f, .91f, .96f}, true,
             false, 5);
        rect(tabs, {.24f, .33f, .45f}, {.30f, .40f, .53f}, true, false, 5);
        const MenuRect inner{tabs.x + 1, tabs.y + 1, tabs.w - 2, tabs.h - 2};
        const iox::CanvasTransform canvas(layout_, surface_.width, surface_.height);
        const char *labels[] = {"Display", "Controls"};
        // Depression identifies the displayed category; blue pulse identifies focus.
        const int selectedTab = optionsScreen_.tab;
        for (int tab = 0; tab < 2; ++tab) {
            const auto r = optionButton(tab);
            const bool current = selectedTab == tab;
            glEnable(GL_SCISSOR_TEST);
            glScissor(int(r.x * canvas.scaleX), int((layout_.height - r.y - r.h) * canvas.scaleY),
                      int(r.w * canvas.scaleX), int(r.h * canvas.scaleY));
            rect(inner, current ? Colour{.29f, .43f, .62f} : Colour{.57f, .67f, .79f},
                 current ? Colour{.20f, .35f, .55f} : Colour{.43f, .55f, .70f}, true, false, 4);
            glDisable(GL_SCISSOR_TEST);
            if (focused(tab))
                rect({r.x + 1, r.y + 1, r.w - 2, r.h - 2}, {.02f, .42f, 1.f, focusPulse()},
                     {.02f, .25f, .85f, focusPulse()}, true, false, 4);
            // Each segment gets its own curved glass reflection. Batch its thin
            // strips in one draw, and trim the upper outside corner to the capsule.
            for (float x = 1; x < r.w - 1; x += 1) {
                const float normalized = 2 * x / r.w - 1;
                float panelTop = r.y + 1;
                const float edgeDistance = tab == 0 ? x - 1 : r.w - x - 1;
                if (edgeDistance < 4)
                    panelTop +=
                        4 - std::sqrt(std::max(0.f, 16 - (4 - edgeDistance) * (4 - edgeDistance)));
                const float edge = r.y + r.h * (.47f - .075f * normalized * normalized);
                if (edge > panelTop)
                    quad({r.x + x, panelTop, 1, edge - panelTop}, 0, 0, 1, 1,
                         {1, 1, 1, current ? .16f : .20f}, {1, 1, 1, .015f});
            }
            draw(white_);
            rect({r.x + 4, r.y + 1, r.w - 8, 2}, {0, 0, 0, current ? .22f : .10f}, {0, 0, 0, 0});
            text(labels[tab], r.cx(), r.y + 4.4f, 16, {.12f, .20f, .31f, .6f}, true, true);
            text(labels[tab], r.cx(), r.y + 5, 16, white, true, true);
        }
        rect({right.x - .5f, tabs.y + 1, 1, tabs.h - 2}, {.23f, .33f, .47f}, {.23f, .33f, .47f});
        const auto rows = optionRows();
        if (rows.empty())
            return;
        const auto first = optionButton(iox::options::firstRow);
        const auto last = optionButton(iox::options::firstRow + int(rows.size()) - 1);
        const MenuRect group{first.x, first.y, first.w, last.y + last.h - first.y};
        panel(group);
        for (size_t i = 0; i < rows.size(); ++i) {
            const int slot = int(i) + iox::options::firstRow;
            const auto r = optionButton(slot);
            const auto &row = rows[i];
            const bool selected = focused(slot);
            const bool toggle = row.key == "disable_touch" || row.key == "rotate_controls";
            const bool child = row.key == "wallpaper" || row.key == "remap";
            if (selected && row.enabled)
                selection(group, {r.x, r.y, r.w, r.h + (i + 1 < rows.size() ? 5.f : 0.f)});
            if (i + 1 < rows.size() && !selected && !focused(slot + 1))
                rect({r.x + 1, r.y + r.h + 2, r.w - 2, 1}, {.70f, .71f, .73f}, {.70f, .71f, .73f});
            const Colour labelColour = !row.enabled ? disabledInk : selected ? white : ink;
            const Colour valueColour = !row.enabled ? disabledInk : selected ? white : valueInk;
            float labelSize = row.label.size() > 24 ? 13.f : 15.f;
            if (toggle) {
                labelSize = fit(row.label, labelSize, r.w - (row.enabled ? 120.f : 240.f), true);
                text(row.label, r.x + 12, r.cy() - labelSize * .5f, labelSize, labelColour, false,
                     true);
                const MenuRect track{r.x + r.w - 101, r.cy() - 11, 88, 22};
                if (!row.enabled)
                    text("Controller required", track.x - 112, r.cy() - 5.5f, 11, valueColour,
                         false);
                const bool on = settings_->get("global", row.key, "off") == "on" && row.enabled;
                rect(track, {.54f, .56f, .60f}, {.54f, .56f, .60f}, true);
                rect({track.x + 1, track.y + 1, track.w - 2, track.h - 2},
                     on ? Colour{.14f, .42f, .80f} : Colour{.86f, .87f, .88f},
                     on ? Colour{.29f, .62f, .95f} : Colour{.98f, .98f, .98f}, true);
                const MenuRect knob{on ? track.x + track.w - 35 : track.x + 1, track.y + 1, 34,
                                    track.h - 2};
                rect(knob, {1, 1, 1}, {.77f, .79f, .81f}, true);
                text(on ? "ON" : "OFF", on ? track.x + 27 : track.x + 61, track.y + 4, 13,
                     on ? white : Colour{.46f, .48f, .50f}, true, true);
            } else {
                const float rightEdge = r.x + r.w - (child ? 31.f : 12.f);
                float valueSize = 15;
                const float labelWidth = textWidth(row.label, labelSize, true);
                valueSize = fit(row.value, valueSize,
                                std::max(40.f, r.w - labelWidth - (child ? 58.f : 40.f)), false);
                text(row.label, r.x + 12, r.cy() - labelSize * .5f, labelSize, labelColour, false,
                     true);
                text(row.value, rightEdge - textWidth(row.value, valueSize),
                     r.cy() - valueSize * .5f, valueSize, valueColour, false);
                if (child)
                    text(">", r.x + r.w - 16, r.cy() - 10, 19,
                         selected ? white : Colour{.52f, .54f, .56f}, true, true);
            }
        }
    } else if (page() == Page::Players) {
        for (int i = 0; i < 4; ++i) {
            const int slot = iox::options::firstRow + i;
            const auto r = optionButton(slot);
            panel(r);
            if (focused(slot))
                selection(r, r);
            const Colour colour = focused(slot) ? white : ink;
            text("Player " + std::to_string(i + 1), r.x + 15, r.cy() - 9, 18, colour, false, true);
            text(">", r.x + r.w - 17, r.cy() - 10, 20, focused(slot) ? white : disabledInk, true,
                 true);
        }
    } else if (page() == Page::Wallpapers) {
        const int orientation = layout_.portrait ? 1 : 0;
        if (wallpaperPreviewOrientation_ != orientation) {
            for (auto &preview : wallpaperPreviews_) {
                if (preview.texture)
                    glDeleteTextures(1, &preview.texture);
                preview = {};
            }
            wallpaperPreviewOrientation_ = orientation;
        }
        for (int i = 0; i < 4; ++i) {
            auto &preview = wallpaperPreviews_[size_t(i)];
            if (!preview.texture) {
                auto image = ui::wallpaper(i, layout_.portrait);
                preview.texture = loadTexture(image.data, image.size, &preview.w, &preview.h);
            }
            const int slot = iox::options::firstRow + i;
            const auto r = optionButton(slot);
            panel(r);
            if (focused(slot))
                selection(r, r);
            auto imageRect = r;
            imageRect.x += 5;
            imageRect.y += 5;
            imageRect.w -= 10;
            imageRect.h -= 32;
            const float a = float(preview.w) / std::max(1, preview.h),
                        b = imageRect.w / imageRect.h;
            const float u = a > b ? (1 - b / a) / 2 : 0, v = a < b ? (1 - a / b) / 2 : 0;
            quad(imageRect, u, v, 1 - u, 1 - v, white, white);
            draw(preview.texture);
            text(wallpaperNames[i], r.cx(), r.y + r.h - 24, 16, focused(slot) ? white : ink, true,
                 true);
        }
    } else {
        const MenuRect card{18, top + 61, width - 36, layout_.height - top - 226};
        panel(card);
        text(std::to_string(std::min(mappingScreen_.step + 1, ControlMapping::Count)) + " / " +
                 std::to_string(ControlMapping::Count),
             width / 2, top + 79, 18, valueInk);
        text(mappingScreen_.release
                 ? "Release all controls"
                 : kMappingPrompts[std::min(mappingScreen_.step, ControlMapping::Count - 1)],
             width / 2, top + 114, 24, ink, true, true);
        const float cx = width / 2, cy = top + 201;
        const bool direction = mappingScreen_.step < 4 || mappingScreen_.step >= 8;
        const char *labels[] = {"UP", "RIGHT", "DOWN", "LEFT"};
        const int step = mappingScreen_.step;
        const int active = step < 4    ? (step == 0   ? 0
                                          : step == 1 ? 2
                                          : step == 2 ? 3
                                                      : 1)
                           : step >= 8 ? ((step % 4) == 0   ? 3
                                          : (step % 4) == 1 ? 1
                                          : (step % 4) == 2 ? 0
                                                            : 2)
                                       : (step == 4   ? 2
                                          : step == 5 ? 1
                                          : step == 6 ? 3
                                                      : 0);
        if (step < 16) {
            for (int i = 0; i < 4; ++i) {
                const float x = cx + (i == 1   ? 66
                                      : i == 3 ? -66
                                               : 0),
                            y = cy + (i == 0   ? -43
                                      : i == 2 ? 43
                                               : 0);
                const MenuRect r{x - 29, y - 18, 58, 36};
                rect(r, i == active ? Colour{0, .61f, 1} : Colour{.86f, .89f, .93f},
                     i == active ? Colour{0, .28f, .84f} : Colour{.65f, .72f, .82f}, true);
                text(direction ? labels[i]
                     : i == 0  ? "TOP"
                     : i == 1  ? "RIGHT"
                     : i == 2  ? "BOTTOM"
                               : "LEFT",
                     x, y - 7, 11, i == active ? white : valueInk);
            }
        } else {
            const char *extra[] = {"LT", "RT", "START", "SELECT"};
            for (int i = 0; i < 4; ++i) {
                const float x = cx + (i % 2 ? 72 : -72), y = cy + (i / 2 ? 30 : -30);
                const bool selected = i == step - 16;
                const MenuRect r{x - 52, y - 18, 104, 36};
                rect(r, selected ? Colour{0, .61f, 1} : Colour{.86f, .89f, .93f},
                     selected ? Colour{0, .28f, .84f} : Colour{.65f, .72f, .82f}, true);
                text(extra[i], x, y - 7, 14, selected ? white : valueInk, true);
            }
        }
        button(iox::options::firstRow, optionButton(iox::options::firstRow), "Cancel");
    }
}
void Launcher::moveOptions(int dx, int dy) {
    using namespace iox::options;
    constexpr int backSlot = iox::options::back;
    if (page() == Page::Options) {
        if (focus() < firstRow) {
            if (dx)
                focus() = (focus() + dx + firstRow) % firstRow;
            else if (dy > 0)
                focus() = firstRow;
        } else if (dy) {
            focus() = std::min(focus() + dy, buttonCount() - 1);
            if (focus() < firstRow)
                focus() = optionsScreen_.tab;
        }
    } else if (page() == Page::Wallpapers) {
        if (focus() == backSlot) {
            if (dy > 0)
                focus() = firstRow;
        } else {
            int cell = focus() - firstRow;
            if (dx)
                cell = (cell / 2) * 2 + (dx > 0 ? 1 : 0);
            if (dy)
                cell += dy * 2;
            focus() = cell < 0 ? backSlot : firstRow + std::clamp(cell, 0, 3);
        }
    } else if (page() == Page::Players) {
        if (dy)
            focus() = focus() == backSlot ? (dy > 0 ? firstRow : backSlot) : focus() + dy;
        if (focus() < firstRow)
            focus() = backSlot;
        focus() = std::min(focus(), firstRow + 3);
    } else if (page() == Page::Mapping && (dx || dy)) {
        focus() = focus() == backSlot ? firstRow : backSlot;
    }
}

} // namespace mino
