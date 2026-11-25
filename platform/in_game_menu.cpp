#include "in_game_menu.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <exception>
#include <iterator>

namespace mino {
namespace {
using namespace neon::widgets;
constexpr int panelWidth = 600, rowHeight = 92, rowGap = 104, firstRow = -150;
struct MenuChoice {
    const char *label;
    InGameMenu::Action action;
};
constexpr MenuChoice choices[] = {{"Display", InGameMenu::Action::Display},
                                  {"Control", InGameMenu::Action::Control},
                                  {"Exit to iOX", InGameMenu::Action::Exit}};
constexpr int choiceCount = int(std::size(choices));

Value menuDocument() {
    Value::Array rows;
    for (int i = 0; i < choiceCount; ++i) {
        rows.emplace_back(Value::Object{
            {"id", "row-" + std::to_string(i)},
            {"kind", "group"},
            {"x", -panelWidth / 2},
            {"y", firstRow + i * rowGap},
            {"size", Value::Array{panelWidth, rowHeight}},
            {"children",
             Value::Array{
                 Value::Object{
                     {"id", "focus"},
                     {"kind", "vector"},
                     {"closed", true},
                     {"visible", Value::Array{"==", Value::Array{"read", "focus"}, i}},
                     {"points", Value::Array{Value::Array{0, 0}, Value::Array{panelWidth, 0},
                                             Value::Array{panelWidth, rowHeight},
                                             Value::Array{0, rowHeight}}},
                     {"lineWidth", 2.4},
                     {"color", Value::Array{.38, .86, 1, 1}}},
                 Value::Object{{"id", "label"},
                               {"kind", "text"},
                               {"y", 26},
                               {"fontSize", 37},
                               {"font", "vectrex-menu"},
                               {"align", "center"},
                               {"text", choices[i].label},
                               {"color", Value::Array{1, .87, .34, 1}}}}}});
    }
    return Value::Object{
        {"widgetML", 1},
        {"root",
         Value::Object{
             {"id", "menu"},
             {"kind", "group"},
             {"children",
              Value::Array{
                  Value::Object{{"id", "dim"},
                                {"kind", "rect"},
                                {"placement", Value::Object{{"origin", "screen-top-left"}}},
                                {"color", Value::Array{0, 0, 0, .72}}},
                  Value::Object{
                      {"id", "choices"}, {"kind", "group"}, {"children", std::move(rows)}}}}}}};
}
} // namespace

void InGameMenu::open() {
    focus_ = 0;
}

void InGameMenu::move(int dy) {
    focus_ = (focus_ + dy + choiceCount) % choiceCount;
}

InGameMenu::Action InGameMenu::selected() const {
    return choices[focus_].action;
}

InGameMenu::Action InGameMenu::touch(double x, double y, int width, int height) {
    const double scale = std::min(width, height) / 1000.;
    if (scale <= 0 || std::abs(x * width - width / 2.) > panelWidth * scale / 2.)
        return Action::None;
    const double localY = (y * height - height / 2.) / scale - firstRow;
    for (int i = 0; i < choiceCount; ++i)
        if (localY >= i * rowGap && localY <= i * rowGap + rowHeight) {
            focus_ = i;
            return selected();
        }
    return Action::None;
}

bool InGameMenu::draw(int width, int height) try {
    if (width <= 0 || height <= 0)
        return true;
    if (!ready_) {
        if (!Renderer::loadGlesEntryPoints([](const char *name) -> void * {
                return reinterpret_cast<void *>(SDL_GL_GetProcAddress(name));
            }))
            return false;
        runtime_.document(menuDocument());
        renderer_.assets(runtime_.document(), [](const auto &, auto &) { return false; });
        ready_ = true;
    }
    runtime_.observe({"iox-menu", 0, ++sequence_, {{"focus", focus_}}});
    const auto &frame = runtime_.update(0, Viewport{width, height});
    renderer_.renderCached(frame,
                           RenderTarget{0, width, height, {0, 0, double(width), double(height)}});
    return true;
} catch (const std::exception &error) {
    SDL_Log("In-game iOX menu: %s", error.what());
    return false;
}

void InGameMenu::release() {
    renderer_.release();
}

void InGameMenu::abandonGraphics() {
    renderer_.abandonGraphics();
    ready_ = false;
}
} // namespace mino
