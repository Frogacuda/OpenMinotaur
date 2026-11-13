#include "launcher.h"
#include "launcher_dock.h"
#include "about_screen.h"
#include "launcher_settings_icon.h"
#include "bundled_assets.h"
#include "game_catalog.h"
#include "ui_assets.h"
#include "build_info.h"
#include "gridrunner_choices.h"
#include "about_screen.h"
#include "game_preview.h"

#include <SDL3/SDL.h>
#include <glad/gl.h>
#include "stb_image.h"

#include <algorithm>
#include <cstddef>
#include <sstream>

namespace mino {
namespace {
const CoreInfo kLandscape = {"launcher", "iOX - Minotaur Collection", 640, 480, 4.0f / 3, 4.0f / 3,
                             nullptr};
const CoreInfo kPortrait = {"launcher", "iOX - Minotaur Collection", 480, 640, 3.0f / 4, 3.0f / 4,
                            nullptr};
const char *kVS = R"(
attribute vec2 aPos;
attribute vec2 aUV;
attribute vec4 aColour;
uniform vec2 uSize;
varying vec2 vUV;
varying vec4 vColour;
void main() {
    vUV = aUV;
    vColour = aColour;
    gl_Position = vec4(aPos.x / uSize.x * 2.0 - 1.0, 1.0 - aPos.y / uSize.y * 2.0, 0.0, 1.0);
}
)";
const char *kFS = R"(
precision mediump float;
uniform sampler2D uTexture;
uniform float uRounded;
uniform float uGloss;
uniform float uGray;
uniform float uSdfWidth;
uniform float uRoundEdge;
uniform vec2 uRoundScale;
varying vec2 vUV;
varying vec4 vColour;
void main() {
    if (uRounded > 4.5) {
        gl_FragColor=vec4(texture2D(uTexture,vUV).rgb*vColour.rgb,vColour.a);
        return;
    }
    if (uRounded > 3.5) {
        // Soft emission outside the icon silhouette; the opaque icon covers
        // the interior. This selection effect does not alter drop-shadow mode.
        vec2 q = abs(vUV-0.5)-vec2(0.32/1.56);
        float distance = length(max(q,vec2(0.0))) + min(max(q.x,q.y),0.0) - 0.18/1.56;
        float outside = max(distance,0.0);
        float coverage = exp(-0.5*outside*outside/(0.040*0.040));
        gl_FragColor = vec4(vColour.rgb,vColour.a*coverage);
        return;
    }
    if (uRounded > 2.5) {
        // Rounded icon silhouette in a quad padded by 13% per side. A soft
        // analytic penumbra needs one quad, with no blur textures or passes.
        vec2 q = abs(vUV-0.5)-vec2(0.32/1.26);
        float distance = length(max(q,vec2(0.0))) + min(max(q.x,q.y),0.0) - 0.18/1.26;
        float coverage = 1.0-smoothstep(-0.055,0.055,distance);
        gl_FragColor = vec4(vColour.rgb,vColour.a*coverage);
        return;
    }
    vec4 sampled = texture2D(uTexture, vUV);
    vec4 c = sampled * vColour;
    if (uSdfWidth > 0.0) {
        float coverage = smoothstep(0.5-uSdfWidth, 0.5+uSdfWidth, sampled.r);
        c = vec4(vColour.rgb, vColour.a * coverage);
    }
    if (uRounded > 0.5) {
        vec2 q = max(abs(vUV - 0.5)*uRoundScale - (uRoundScale*0.5-0.18), vec2(0.0));
        float distance = uRounded > 1.5 ? length(vUV-0.5)-0.5 : length(q)-0.18;
        c.a *= 1.0 - smoothstep(-uRoundEdge, uRoundEdge, distance);
    }
    float edge = 0.43 - 0.16 * (vUV.x - 0.5) * (vUV.x - 0.5);
    if (uGloss > 0.0 && vUV.y < edge)
        c.rgb = mix(c.rgb, vec3(1.0), 0.23 * uGloss * (1.0 - vUV.y / 0.6));
    if (uGray > 0.5) c.rgb = vec3(dot(c.rgb, vec3(0.299, 0.587, 0.114)));
    gl_FragColor = c;
}
)";

} // namespace

void Launcher::setPaths(const std::string &prefDir) {
    prefDir_ = prefDir;
}
void Launcher::setPortrait(bool portrait) {
    if (portrait != layout_.portrait && appLaunch_.active()) {
        const auto destination = launchPage_;
        const int initialFocus = launchFocus_;
        cancelAppLaunch();
        open(destination, initialFocus);
    }
    layout_ = LauncherLayout(portrait);
}
const CoreInfo &Launcher::info() const {
    return layout_.portrait ? kPortrait : kLandscape;
}

bool Launcher::init(Host &host, const SurfaceInfo &surface) {
    host_ = &host;
    surface_ = surface;
    auto compile = [&](GLenum kind, const char *source) {
        std::string adapted = host.adaptShader(source, kind == GL_VERTEX_SHADER ? 0 : 1);
        const char *p = adapted.c_str();
        unsigned shader = glCreateShader(kind);
        glShaderSource(shader, 1, &p, nullptr);
        glCompileShader(shader);
        GLint ok = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[2048]{};
            glGetShaderInfoLog(shader, sizeof log, nullptr, log);
            host.log("launcher shader: %s", log);
            glDeleteShader(shader);
            return 0u;
        }
        return shader;
    };
    unsigned vs = compile(GL_VERTEX_SHADER, kVS), fs = compile(GL_FRAGMENT_SHADER, kFS);
    if (!vs || !fs) {
        if (vs)
            glDeleteShader(vs);
        if (fs)
            glDeleteShader(fs);
        return false;
    }
    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glBindAttribLocation(program_, 0, "aPos");
    glBindAttribLocation(program_, 1, "aUV");
    glBindAttribLocation(program_, 2, "aColour");
    glLinkProgram(program_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048]{};
        glGetProgramInfoLog(program_, sizeof log, nullptr, log);
        host.log("launcher program: %s", log);
        shutdown();
        return false;
    }
    uSize_ = glGetUniformLocation(program_, "uSize");
    uRounded_ = glGetUniformLocation(program_, "uRounded");
    uGloss_ = glGetUniformLocation(program_, "uGloss");
    uGray_ = glGetUniformLocation(program_, "uGray");
    uSdfWidth_ = glGetUniformLocation(program_, "uSdfWidth");
    uRoundEdge_ = glGetUniformLocation(program_, "uRoundEdge");
    uRoundScale_ = glGetUniformLocation(program_, "uRoundScale");
    glUseProgram(program_);
    glUniform1i(glGetUniformLocation(program_, "uTexture"), 0);
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, 4096 * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);
    vertices_.reserve(4096);
    const unsigned char white[] = {255, 255, 255, 255};
    white_ = makeTexture(1, 1, white);
    const auto dockPixels = iox::DockGraphic::pixels();
    dockTexture_ =
        makeTexture(iox::DockGraphic::width, iox::DockGraphic::height, dockPixels.data());
    if (!makeFont()) {
        shutdown();
        return false;
    }
    setBuiltinWallpaper(
        settings_ ? std::clamp(std::atoi(settings_->get("global", "wallpaper", "0").c_str()), 0, 3)
                  : 0);
    refresh();
    homeEntrance_.start();
    return true;
}

unsigned Launcher::makeTexture(int w, int h, const unsigned char *rgba) {
    unsigned t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}
unsigned Launcher::loadTexture(const std::string &path, int *imageW, int *imageH) {
    size_t size = 0;
    void *data = SDL_LoadFile(path.c_str(), &size);
    if (!data)
        return 0;
    unsigned texture = loadTexture(static_cast<const unsigned char *>(data), size, imageW, imageH);
    SDL_free(data);
    return texture;
}
unsigned Launcher::loadTexture(const unsigned char *bytes, size_t size, int *imageW, int *imageH) {
    if (!bytes || !size || size > 16 * 1024 * 1024)
        return 0;
    int w = 0, h = 0;
    unsigned char *pixels = stbi_load_from_memory(bytes, int(size), &w, &h, nullptr, 4);
    unsigned texture = pixels ? makeTexture(w, h, pixels) : 0;
    stbi_image_free(pixels);
    if (texture) {
        if (imageW)
            *imageW = w;
        if (imageH)
            *imageH = h;
    }
    return texture;
}
bool Launcher::setWallpaper(const std::string &imagePath) {
    std::array<Wallpaper, 2> next{};
    if (imagePath.empty()) {
        for (size_t i = 0; i < next.size(); ++i) {
            const auto image = ui::defaultWallpaper(i == 1);
            next[i].texture = loadTexture(image.data, image.size, &next[i].w, &next[i].h);
            if (!next[i].texture) {
                if (next[0].texture)
                    glDeleteTextures(1, &next[0].texture);
                return false;
            }
        }
    } else {
        next[0].texture = loadTexture(imagePath, &next[0].w, &next[0].h);
        if (!next[0].texture)
            return false;
        next[1] = next[0];
    }
    clearWallpapers();
    wallpapers_ = next;
    return true;
}
void Launcher::clearWallpapers() {
    if (wallpapers_[1].texture && wallpapers_[1].texture != wallpapers_[0].texture)
        glDeleteTextures(1, &wallpapers_[1].texture);
    if (wallpapers_[0].texture)
        glDeleteTextures(1, &wallpapers_[0].texture);
    wallpapers_ = {};
}
bool Launcher::makeFont() {
    const auto noteImage = ui::notesFontAtlas();
    notesFont_ = loadTexture(noteImage.data, noteImage.size);
    if (!notesFont_)
        return false;
    const auto image = ui::fontAtlas();
    int w = 0, h = 0;
    unsigned char *pixels = stbi_load_from_memory(image.data, int(image.size), &w, &h, nullptr, 1);
    if (!pixels)
        return false;
    glGenTextures(1, &font_);
    glBindTexture(GL_TEXTURE_2D, font_);
    // One megabyte of GPU storage for both weights; no runtime font library.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(pixels);
    return true;
}
void Launcher::refresh() {
    const auto gameTexture = [&](const std::string &id, const std::string &name) {
        std::vector<uint8_t> bytes;
        return readBundledAsset(id, name, bytes) ? loadTexture(bytes.data(), bytes.size()) : 0u;
    };
    if (gridrunnerAtlas_)
        glDeleteTextures(1, &gridrunnerAtlas_);
    gridrunnerAtlas_ = gameTexture("gridrunner", "gridrunner.png");
    for (size_t i = 0; i < gridrunnerPreviews_.size(); ++i) {
        if (gridrunnerPreviews_[i])
            glDeleteTextures(1, &gridrunnerPreviews_[i]);
        gridrunnerPreviews_[i] =
            loadTexture(prefDir_ + "ui/previews/" + kGridrunnerChoices[i].id + ".png");
    }
    for (size_t i = 0; i < icons_.size(); ++i) {
        Icon &icon = icons_[i];
        if (icon.texture)
            glDeleteTextures(1, &icon.texture);
        const auto &game = kGameCatalog[i];
        icon.playable = findCore(game.id) != nullptr;
        icon.installed = bundledGameAvailable(game.id);
        icon.texture = loadTexture(prefDir_ + "ui/icons/" + game.id + ".png");
        for (const char *name : {"Icon@2x.png", "Icon-72.png", "Icon.png"}) {
            if (icon.texture)
                break;
            icon.texture = gameTexture(game.id, name);
        }
        if (!icon.texture) {
            const auto bundled = ui::gameIcon(game.id);
            icon.texture = loadTexture(bundled.data, bundled.size);
        }
    }
    for (size_t i = 0; i < systemIcons_.size(); ++i) {
        if (systemIcons_[i])
            glDeleteTextures(1, &systemIcons_[i]);
        systemIcons_[i] = loadTexture(prefDir_ + "ui/icons/" +
                                      (i == 0   ? "settings.png"
                                       : i == 1 ? "achievements.png"
                                                : "about.png"));
        if (i == 0 && !systemIcons_[i])
            systemIcons_[i] = loadTexture(prefDir_ + "ui/icons/options.png");
        if (systemIcons_[i])
            continue;
        if (i == 0) {
            static const auto pixels = iox::SettingsIcon::pixels();
            systemIcons_[i] =
                makeTexture(iox::SettingsIcon::size, iox::SettingsIcon::size, pixels.data());
        } else if (i == 1) {
            constexpr int side = 256;
            std::vector<uint8_t> pixels(side * side * 4);
            struct StrokePoint {
                float x, y, r;
            };
            std::vector<StrokePoint> stroke;
            const auto curve = [&](float ax, float ay, float bx, float by, float cx, float cy,
                                   float dx, float dy, bool rising) {
                for (int n = 0; n <= 48; ++n) {
                    const float t = n / 48.f, u = 1 - t;
                    const float radius = rising ? 6.2f - 2.7f * t : 4.3f + 1.9f * t;
                    stroke.push_back(
                        {u * u * u * ax + 3 * u * u * t * bx + 3 * u * t * t * cx + t * t * t * dx,
                         u * u * u * ay + 3 * u * u * t * by + 3 * u * t * t * cy + t * t * t * dy,
                         radius});
                }
            };
            curve(33, 68, 38, 71, 44, 82, 50, 87, false);
            curve(50, 87, 61, 75, 83, 48, 100, 36, true);
            for (int y = 0; y < side; ++y)
                for (int x = 0; x < side; ++x) {
                    const float px = (x + .5f) * .5f, py = (y + .5f) * .5f;
                    const bool top = py < 27, line = py > 34 && int(py) % 16 == 0,
                               margin = px >= 25 && px < 25.6f;
                    const int n = (y * side + x) * 4;
                    float red = top ? 98.f : margin ? 192.f : line ? 188.f : 255.f;
                    float green = top ? 62.f : margin ? 102.f : line ? 177.f : 245.f;
                    float blue = top ? 42.f : margin ? 85.f : line ? 123.f : 169.f;
                    float distance = 1000;
                    for (size_t k = 1; k < stroke.size(); ++k) {
                        const auto a = stroke[k - 1], b = stroke[k];
                        const float dx = b.x - a.x, dy = b.y - a.y;
                        const float t = std::clamp(((px - a.x) * dx + (py - a.y) * dy) /
                                                       std::max(.0001f, dx * dx + dy * dy),
                                                   0.f, 1.f);
                        const float ex = px - a.x - t * dx, ey = py - a.y - t * dy;
                        distance = std::min(distance,
                                            std::sqrt(ex * ex + ey * ey) - (a.r + (b.r - a.r) * t));
                    }
                    const float coverage = std::clamp(.5f - distance * 2, 0.f, 1.f);
                    const float grain = .96f + .04f * std::sin(px * 2.8f + py * .3f);
                    pixels[n] = uint8_t(red + (24 * grain - red) * coverage);
                    pixels[n + 1] = uint8_t(green + (108 * grain - green) * coverage);
                    pixels[n + 2] = uint8_t(blue + (40 * grain - blue) * coverage);
                    pixels[n + 3] = 255;
                }
            // Glass is applied by icon(), identically to the other dock icons.
            systemIcons_[i] = makeTexture(side, side, pixels.data());
        } else {
            std::vector<uint8_t> bytes;
            if (!readBundledAsset("deflex", "Default-LandscapeLeft.png", bytes))
                continue;
            int width = 0, height = 0, channels = 0;
            auto *pixels = stbi_load_from_memory(bytes.data(), int(bytes.size()), &width, &height,
                                                 &channels, 4);
            if (!pixels)
                continue;
            // Crop only the original sheep and its blue rim, above the wordmark.
            const int left = width * 361 / 1024, top = height * 148 / 768;
            const int side = std::min({width * 272 / 1024, width - left, height - top});
            if (side > 0) {
                std::vector<uint8_t> crop(size_t(side) * side * 4);
                for (int row = 0; row < side; ++row)
                    std::copy_n(pixels + (size_t(top + row) * width + left) * 4, side * 4,
                                crop.data() + size_t(row) * side * 4);
                systemIcons_[i] = makeTexture(side, side, crop.data());
            }
            stbi_image_free(pixels);
        }
    }
}
void Launcher::drawHome() {
    if (movingHome())
        drawTransform_ = homePose().dock(layout_);
    drawDock();
    drawTransform_ = {};
    drawHomeIcons();
}
void Launcher::drawDock() {
    quad(layout_.dock, 0, 0, 1, 1, {1, 1, 1}, {1, 1, 1});
    if (!layout_.portrait) {
        // Rotate the exact same graphic 90 degrees, with its inner bevel facing
        // the wallpaper. Shading and weave rotate together; no second variant.
        for (auto &vertex : vertices_) {
            const float u = vertex.u;
            vertex.u = 1 - vertex.v;
            vertex.v = u;
        }
    }
    draw(dockTexture_);
}
void Launcher::drawHomeIcons() {
    for (int i = 0; i < iox::home::count; ++i) {
        drawTransform_ = movingHome() ? homePose().icon(layout_, i) : iox::LayerTransform{};
        const MenuRect &r = layout_.items[size_t(i)];
        const auto *game = i < 9 ? &kGameCatalog[size_t(i)] : nullptr;
        const float artTop = i < 9 ? r.y + 7 : r.cy() - 50;
        MenuRect art{r.cx() - 38, artTop, 76, 76};
        icon(art, i < 9 ? icons_[size_t(i)].texture : systemIcons_[size_t(i - 9)],
             game     ? game->monogram
             : i == 9 ? "+"
                      : "i",
             menuFocusVisible() && !inputBlocked() && i == navigation_.homeFocus(),
             i == iox::home::about ? Colour{0.2f, 0.65f, 0.92f} : Colour{0.53f, 0.6f, 0.68f},
             i == iox::home::about ? Colour{0.02f, 0.28f, 0.55f} : Colour{0.2f, 0.27f, 0.36f},
             i < 9 && !(icons_[size_t(i)].playable && icons_[size_t(i)].installed));
        const char *label = game      ? game->label1
                            : i == 9  ? "Settings"
                            : i == 10 ? "Achievements"
                                      : "About";
        text(label, r.cx(), artTop + 84, i == 10 ? 13.f : 16.f, {0, 0, 0, .85f}, true, true);
        text(label, r.cx(), artTop + 83, i == 10 ? 13.f : 16.f, {1, 1, 1}, true, true);
        if (game && game->label2[0]) {
            text(game->label2, r.cx(), r.y + 109, 16, {0, 0, 0, .85f}, true, true);
            text(game->label2, r.cx(), r.y + 108, 16, {1, 1, 1}, true, true);
        }
    }
    drawTransform_ = {};
}
void Launcher::drawPage() {
    (this->*screenHandlers().draw)();
}
void Launcher::drawDialog() {
    const float w = layout_.width;
    std::string title, body;
    const char *first = "Back";
    const char *second = "Back";
    switch (page()) {
    case Page::Game:
        title = kGameCatalog[size_t(gameScreen_.game)].title;
        body = icons_[size_t(gameScreen_.game)].playable
                   ? "Bundled game resources are unavailable."
                   : "This title is not available in this build.";
        break;
    case Page::Quit:
        title = "Quit iOX?";
        body = "Leave the collection?";
        first = "Cancel";
        second = "Quit";
        break;
    case Page::Message:
        title = messageScreen_.title;
        body = messageScreen_.body;
        break;
    default:
        break;
    }
    text(title, w / 2, 76, 23, {1, 1, 1}, true, true);
    paragraph(body, 123, 17);
    for (int i = 0; i < (calibratingTilt_ ? 0 : buttonCount()); ++i) {
        MenuRect r = pageButton(i);
        if (focused(i))
            rect({r.x - 3, r.y - 3, r.w + 6, r.h + 6}, {0.5f, 0.87f, 1}, {0.25f, 0.65f, 0.88f},
                 true);
        rect(r, {0.45f, 0.56f, 0.68f}, {0.14f, 0.23f, 0.35f}, true, true);
        const char *label = i == 0 ? first : second;
        text(label, r.cx(), r.y + 12, 18, {1, 1, 1}, true, true);
    }
}
void Launcher::drawWallpaper() {
    const auto &wallpaper = wallpapers_[layout_.portrait ? 1 : 0];
    if (!wallpaper.texture) {
        rect({0, 0, layout_.width, layout_.height}, {0.17f, 0.21f, 0.27f}, {0.035f, 0.05f, 0.08f});
        return;
    }
    rect({0, 0, layout_.width, layout_.height}, {.055f, .08f, .15f}, {.055f, .08f, .15f});
    // Wallpaper continues behind every movable layer. Cover the canvas with
    // the existing artwork while no custom artwork is available.
    const MenuRect region{0, 0, layout_.width, layout_.height};
    const float scale = std::max(region.w / wallpaper.w, region.h / wallpaper.h);
    const float w = wallpaper.w * scale, h = wallpaper.h * scale;
    quad({region.cx() - w * .5f, region.cy() - h * .5f, w, h}, 0, 0, 1, 1, {1, 1, 1}, {1, 1, 1});
    draw(wallpaper.texture);
}
void Launcher::drawBarBacking() {
    // Reserve these strips throughout the entrance, clipping moving icons too.
    const float footer = iox::isOptionsScreen(page()) ? 78.f : 28.f;
    rect({0, 0, layout_.width, LauncherLayout::statusBarHeight}, {0, 0, 0}, {0, 0, 0});
    rect({0, layout_.height - footer, layout_.width, footer}, {0, 0, 0}, {0, 0, 0});
}
void Launcher::drawStatusBar() {
    rect({0, 0, layout_.width, LauncherLayout::statusBarHeight}, {0.6f, 0.69f, 0.8f},
         {0.22f, 0.35f, 0.5f}, false, true);
    // Upper shadow and lower light catch match the recessed Options lettering.
    text("iOX", layout_.width / 2 + .15f, 2.15f, 14, {.12f, .19f, .28f, .55f}, true, true);
    text("iOX", layout_.width / 2 - .15f, 2.9f, 14, {1, 1, 1, .35f}, true, true);
    text("iOX", layout_.width / 2, 2.5f, 14, {.96f, .97f, .99f}, true, true);
}
std::string Launcher::footerStatus() const {
    const bool optionsPage = iox::isOptionsScreen(page());
    if (notesPage() && !controllerPresent_)
        return "";
    if (!optionsPage && !controllerPresent_)
        return page() == Page::Home ? "Exit" : page() == Page::About ? "" : "Back";
    const std::string back = backLabel_ + (page() == Page::Home ? ": EXIT" : ": BACK");
    std::string status = selectLabel_ + ": SELECT   " + back;
    if (notesPage())
        status = controllerPresent_ ? "STICK: SCROLL   " + selectLabel_ + ": SELECT   " + back : "";
    if (page() == Page::About && focus() == 0)
        status = "UP/DOWN: SCROLL   RIGHT: DONATE   " + back;
    if (page() == Page::Home && navigation_.homeFocus() < 9) {
        const auto &icon = icons_[size_t(navigation_.homeFocus())];
        status = !icon.playable    ? "NOT AVAILABLE IN THIS BUILD"
                 : !icon.installed ? "RESOURCES UNAVAILABLE"
                                   : selectLabel_ + ": PLAY   " + back;
        if (icon.playable && icon.installed &&
            std::string(kGameCatalog[size_t(navigation_.homeFocus())].id) == "gridrunner")
            status = selectLabel_ + ": PLAY GRIDRUNNER   " + back;
    }
    if (optionsPage) {
        if (page() == Page::Options) {
            auto rows = optionRows();
            int i = focus() - iox::options::firstRow;
            status = i >= 0 && i < int(rows.size()) ? rows[size_t(i)].tip
                     : focus() == 2                 ? "Return to iOX."
                     : controllerPresent_ ? "Select a category. Click or press " + selectLabel_ +
                                                " to cycle each option."
                                          : "Select a category. Click or tap to cycle each option.";
        } else if (page() == Page::Wallpapers)
            status = "Change your iOS Wallpaper. Choose a preview to apply it.";
        else if (page() == Page::Players)
            status = "Choose player 1-4. Connect that player's controller before starting.";
        else
            status = "Use any button, trigger or stick direction. Release between prompts. Cancel, "
                     "Escape or hold Start + Back to discard.";
    }
    return status;
}
void Launcher::drawFooter() {
    const bool optionsPage = iox::isOptionsScreen(page());
    const float footer = optionsPage ? 78.f : 28.f;
    rect({0, layout_.height - footer, layout_.width, footer}, {0.17f, 0.22f, 0.29f},
         {0.04f, 0.07f, 0.11f});
    const auto status = footerStatus();
    if (optionsPage)
        wrappedText(status, 18, layout_.height - footer + 10, layout_.width - 36, 14,
                    {.88f, .93f, 1});
    else {
        const float size =
            13.f * std::min(1.f, (layout_.width - 24) / std::max(1.f, textWidth(status, 13)));
        text(status, layout_.width / 2, layout_.height - 23, size, {0.88f, 0.93f, 1});
    }
}
void Launcher::frame(double dt) {
    if (!appLaunch_.active() && !externalLaunch_ && std::isfinite(dt) && dt > 0)
        selectionTime_ = std::fmod(selectionTime_ + dt, 1.8);
    repeat_ -= std::clamp(dt, 0.0, 0.1);
    if (appLaunch_.active())
        exitPose_ = iox::AppLaunch::homePose(appLaunch_.progress());
    glBindFramebuffer(GL_FRAMEBUFFER, surface_.framebuffer);
    glViewport(0, 0, surface_.width, surface_.height);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(program_);
    glUniform2f(uSize_, layout_.width, layout_.height);
    glActiveTexture(GL_TEXTURE0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    for (unsigned i = 0; i < 3; ++i)
        glEnableVertexAttribArray(i);
    glDisableVertexAttribArray(3);
    glDisableVertexAttribArray(4);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void *>(offsetof(Vertex, x)));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void *>(offsetof(Vertex, u)));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void *>(offsetof(Vertex, r)));
    if (page() == Page::About && !inputBlocked() && aboutAxis_ != 0) {
        prepareAbout();
        const float end = std::max(0.f, aboutScreen_.height - AboutLayout(layout_).body.h);
        if (focus() == iox::about::body) {
            const float next =
                aboutScreen_.scroll + aboutAxis_ * 280.f * float(std::clamp(dt, 0.0, .05));
            if (aboutAxis_ > 0 && aboutScreen_.scroll >= end)
                focus() = iox::about::donate;
            else if (aboutAxis_ < 0 && aboutScreen_.scroll <= 0)
                focus() = iox::about::back;
            aboutScreen_.scroll = std::clamp(next, 0.f, end);
        } else if ((focus() == iox::about::donate && aboutAxis_ < 0) ||
                   (focus() == iox::about::back && aboutAxis_ > 0))
            focus() = iox::about::body;
    }
    if (notesPage() && !inputBlocked() && noteAxis_ != 0 && focus() == 0) {
        prepareAchievements();
        const float end = std::max(0.f, noteHeight_ - achievementBody().h);
        if (noteAxis_ > 0 && noteScroll_ >= end)
            focus() = 2;
        else if (noteAxis_ < 0 && noteScroll_ <= 0)
            focus() = 4;
        else
            noteScroll_ = std::clamp(
                noteScroll_ + noteAxis_ * 280.f * float(std::clamp(dt, 0.0, .05)), 0.f, end);
    }
    drawWallpaper();
    if (screenHandlers().dimBackground)
        rect({0, LauncherLayout::statusBarHeight, layout_.width,
              layout_.height - LauncherLayout::statusBarHeight - 28},
             {0, 0, 0, .65f}, {0, 0, 0, .65f});
    drawPage();
    drawBarBacking();
    if (movingHome())
        drawTransform_.dy = -LauncherLayout::statusBarHeight * (1 - homePose().barsProgress());
    drawStatusBar();
    drawTransform_ = {};
    if (movingHome())
        drawTransform_.dy = 28 * (1 - homePose().barsProgress());
    drawFooter();
    drawTransform_ = {};
    if (appLaunch_.active())
        drawAppLaunch();
    for (unsigned i = 0; i < 3; ++i)
        glDisableVertexAttribArray(i);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    // Render t=0 before advancing: the first presented frame is wallpaper with black bar strips.
    if (appLaunch_.active()) {
        if (appLaunch_.progress() >= 1) {
            const auto destination = launchPage_;
            const int initialFocus = launchFocus_;
            cancelAppLaunch();
            open(destination, initialFocus);
        } else
            appLaunch_.advance(dt);
    } else if (page() == Page::Home && !externalLaunch_)
        homeEntrance_.advance(dt);
}
void Launcher::shutdown() {
    cancelAppLaunch();
    externalLaunch_ = false;
    for (auto &p : wallpaperPreviews_) {
        if (p.texture)
            glDeleteTextures(1, &p.texture);
        p = {};
    }
    wallpaperPreviewOrientation_ = -1;
    clearWallpapers();
    if (gridrunnerAtlas_)
        glDeleteTextures(1, &gridrunnerAtlas_);
    gridrunnerAtlas_ = 0;
    for (auto &preview : gridrunnerPreviews_) {
        if (preview)
            glDeleteTextures(1, &preview);
        preview = 0;
    }
    for (auto &icon : icons_) {
        if (icon.texture)
            glDeleteTextures(1, &icon.texture);
        icon.texture = 0;
    }
    for (auto &icon : systemIcons_) {
        if (icon)
            glDeleteTextures(1, &icon);
        icon = 0;
    }
    if (font_)
        glDeleteTextures(1, &font_);
    if (notesFont_)
        glDeleteTextures(1, &notesFont_);
    notesFont_ = 0;
    if (white_)
        glDeleteTextures(1, &white_);
    if (dockTexture_)
        glDeleteTextures(1, &dockTexture_);
    if (vbo_)
        glDeleteBuffers(1, &vbo_);
    if (program_)
        glDeleteProgram(program_);
    font_ = white_ = dockTexture_ = vbo_ = program_ = 0;
}
void Launcher::abandonGraphics() {
    program_ = vbo_ = white_ = font_ = notesFont_ = dockTexture_ = gridrunnerAtlas_ = 0;
    appTexture_ = appFramebuffer_ = 0;
    for (auto &i : icons_)
        i.texture = 0;
    for (auto &i : systemIcons_)
        i = 0;
    for (auto &i : gridrunnerPreviews_)
        i = 0;
    for (auto &i : wallpapers_)
        i.texture = 0;
    for (auto &i : wallpaperPreviews_)
        i.texture = 0;
    wallpaperPreviewOrientation_ = -1;
    if (appLaunch_.active()) {
        const auto destination = launchPage_;
        const int initialFocus = launchFocus_;
        cancelAppLaunch();
        open(destination, initialFocus);
    }
}
bool Launcher::restoreGraphics(Host &host, const SurfaceInfo &surface) {
    const auto entrance = homeEntrance_;
    const bool ok = init(host, surface);
    homeEntrance_ = entrance;
    return ok;
}
} // namespace mino
