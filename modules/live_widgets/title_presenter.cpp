#include "title_presenter.h"

namespace neon::widgets {
void TitlePresenter::assets(const TitleSequence &title, ResourceReader reader) {
    renderer_.assets(title.document(), std::move(reader));
}

void TitlePresenter::render(const TitleSequence &title, const RenderTarget &target,
                            const Background &background, double exitBackgroundOpacity) {
    if (background)
        background(target);
    const auto shade = [&](double opacity) {
        if (opacity >= 1)
            return;
        int turns = (target.turns % 4 + 4) % 4;
        double w = turns % 2 ? target.viewport.height : target.viewport.width;
        double h = turns % 2 ? target.viewport.width : target.viewport.height;
        Draw shade;
        shade.path = "title/composition-fade";
        shade.kind = "rect";
        shade.bounds = shade.clip = {0, 0, w, h};
        shade.color = {0, 0, 0, 1 - opacity};
        Frame frame;
        frame.draws.push_back(std::move(shade));
        renderer_.render(frame, target);
    };
    if (exitBackgroundOpacity >= 0) {
        shade(exitBackgroundOpacity);
        if (title.opacity() > 0 || exitBackgroundOpacity > 0) {
            Frame foreground;
            for (const auto &source : title.frame().draws) {
                const double opacity = source.path.rfind("title/legal/", 0) == 0
                                           ? exitBackgroundOpacity
                                           : title.opacity();
                if (opacity > 0) {
                    auto &draw = foreground.draws.emplace_back(source);
                    draw.color[3] *= opacity;
                    draw.trackColor[3] *= opacity;
                }
            }
            renderer_.render(foreground, target);
        }
    } else {
        renderer_.render(title.frame(), target);
        shade(title.opacity());
    }
}
} // namespace neon::widgets
