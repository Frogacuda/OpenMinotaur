#include "settings.h"

#include <SDL3/SDL.h>
#include <cstdlib>
#include <sstream>

namespace mino {

static std::string trim(const std::string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
        return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

bool Settings::load(const std::string &path) {
    path_ = path;
    sections_.clear();
    size_t len = 0;
    char *data = static_cast<char *>(SDL_LoadFile(path.c_str(), &len));
    if (!data)
        return false;
    std::istringstream in(std::string(data, len));
    SDL_free(data);

    std::string line, section = "global";
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;
        if (line.front() == '[' && line.back() == ']') {
            section = trim(line.substr(1, line.size() - 2));
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        sections_[section][trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
    }
    return true;
}

bool Settings::save() const {
    if (path_.empty())
        return false;
    std::ostringstream out;
    auto writeSection = [&](const std::string &name) {
        auto it = sections_.find(name);
        if (it == sections_.end())
            return;
        out << '[' << name << "]\n";
        for (auto &kv : it->second)
            out << kv.first << '=' << kv.second << '\n';
        out << '\n';
    };
    writeSection("global");
    for (auto &s : sections_)
        if (s.first != "global")
            writeSection(s.first);
    std::string text = out.str();
    return SDL_SaveFile(path_.c_str(), text.data(), text.size());
}

std::string Settings::get(const std::string &game, const std::string &key,
                          const std::string &def) const {
    for (const std::string *sec : {&game, static_cast<const std::string *>(nullptr)}) {
        auto it = sections_.find(sec ? *sec : std::string("global"));
        if (it == sections_.end())
            continue;
        auto kv = it->second.find(key);
        if (kv != it->second.end())
            return kv->second;
    }
    return def;
}

void Settings::set(const std::string &section, const std::string &key, const std::string &value) {
    sections_[section][key] = value;
}

DisplaySettings Settings::display(const std::string &game) const {
    DisplaySettings d;
    std::string f = get(game, "filter", "sharp");
    d.filter = f == "nearest"    ? Filter::Nearest
               : f == "bilinear" ? Filter::Bilinear
                                 : Filter::SharpBilinear;
    d.scale = get(game, "scale", "fit") == "integer" ? ScaleMode::Integer : ScaleMode::Fit;
    std::string a = get(game, "aspect", "ipad");
    d.aspect = a == "ipad"   ? AspectMode::IPad
               : a == "fill" ? AspectMode::Fill
                             : AspectMode::Original;
    std::string r = get(game, "internal_scale", "2");
    d.internalScale = r == "native" ? 0 : SDL_clamp(std::atoi(r.c_str()), 1, 6);
    const char *defaultOrientation = nativeOrientationDefault_ ? "bestfit" : "landscape";
    std::string o = get(game, "orientation", defaultOrientation);
    d.orientation = o == "landscape"                                 ? Orientation::Landscape
                    : o == "portrait" || o == "vertical_left"        ? Orientation::Portrait
                    : o == "portrait_right" || o == "vertical_right" ? Orientation::PortraitRight
                    : o == "bestfit"                                 ? Orientation::BestFit
                                                                     : Orientation::Auto;
    const auto bg = get(game, "backdrop", "colour");
    d.backdrop = bg == "image"     ? Backdrop::Image
                 : bg == "neon"    ? Backdrop::Neon
                 : bg == "widgets" ? Backdrop::Widgets
                                   : Backdrop::Colour;
    const auto mode = get(game, "screen_mode", "hires");
    d.screenMode = mode == "smooth"      ? ScreenMode::Smooth
                   : mode == "sharp"     ? ScreenMode::Sharp
                   : mode == "scanlines" ? ScreenMode::Scanlines
                   : mode == "crt"       ? ScreenMode::CRT
                   : mode == "crt2"      ? ScreenMode::CRT2
                                         : ScreenMode::HiRes;
    d.backdropColour =
        uint32_t(std::strtoul(get(game, "backdrop_colour", "000000").c_str(), nullptr, 16)) &
        0xffffffu;
    d.backdropImage = get(game, "backdrop_image", "");
    return d;
}

InputSettings Settings::input(const std::string &game) const {
    InputSettings i;
    i.deadzone =
        SDL_clamp(static_cast<float>(std::atof(get(game, "deadzone", "0.2").c_str())), 0.0f, 0.9f);
    i.disableTouch = get(game, "disable_touch", "off") == "on";
    i.rotateControls = get(game, "rotate_controls", "off") == "on";
    return i;
}

} // namespace mino
