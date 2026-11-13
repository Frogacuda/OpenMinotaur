#include "title_sequence.h"
#include <algorithm>
#include <stdexcept>

namespace neon::widgets {
TitleSequence::TitleSequence(Value document) {
    if (document.at("widgetML").number() != 1 || document.at("root").at("id").string() != "title")
        throw std::runtime_error("TitleSequence requires the title WidgetML document");
    runtime_.document(std::move(document));
    begin();
}

Rect TitleSequence::presentationViewport(int width, int height) const {
    if (width <= 0 || height <= 0)
        return {};
    const auto &presentation = document().at("presentation");
    auto limit = [](int extent, int other, const Value &ratio, size_t numerator) {
        const double n = ratio.at(numerator).number(), d = ratio.at(1 - numerator).number();
        if (n <= 0 || d <= 0)
            return extent;
        return int(std::clamp(double(other) * n / d, 1., double(extent)));
    };
    const int w = limit(width, height, presentation.at("maximumAspect"), 0);
    const int h = limit(height, width, presentation.at("minimumAspect"), 1);
    return {double((width - w) / 2), double((height - h) / 2), double(w), double(h)};
}

double TitleSequence::maxFrameRate() const {
    return document().at("presentation").at("maxFrameRate").number();
}

void TitleSequence::begin(bool buttonsHeld, bool pointerHeld) {
    runtime_.reset();
    runtime_.enabled(true);
    buttons_ = buttonsHeld;
    pointer_ = pointerHeld;
    armed_ = !buttonsHeld && !pointerHeld;
    leaving_ = finished_ = launchPending_ = false;
    opacity_ = 0;
    exitBase_ = 1;
}

void TitleSequence::input(bool buttonsHeld, bool pointerHeld) {
    bool pressed = (buttonsHeld && !buttons_) || (pointerHeld && !pointer_);
    buttons_ = buttonsHeld;
    pointer_ = pointerHeld;
    if (!armed_) {
        armed_ = !buttonsHeld && !pointerHeld;
        return;
    }
    if (pressed && !leaving_ && !finished_) {
        exitBase_ = opacity_;
        leaving_ = true;
        runtime_.event("start");
    }
}

void TitleSequence::update(double elapsedSeconds, const Viewport &viewport) {
    if (finished_)
        return;
    const auto &result = runtime_.update(elapsedSeconds, viewport);
    const auto found = result.outputs.find("title.opacity");
    if (found == result.outputs.end())
        throw std::runtime_error("Title document must publish composition opacity");
    opacity_ = std::clamp(found->second.number(1), 0., 1.) * (leaving_ ? exitBase_ : 1);
    for (const auto &signal : result.scopedSignals) {
        if (signal.group == "title" && signal.name == "iox.launch") {
            finished_ = launchPending_ = true;
            opacity_ = 0;
        }
    }
}

bool TitleSequence::takeLaunchRequest() {
    bool request = launchPending_;
    launchPending_ = false;
    return request;
}
} // namespace neon::widgets
