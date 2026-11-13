#include "title_presenter.h"

namespace neon::widgets {
void TitlePresenter::assets(const TitleSequence &title, ResourceReader reader) {
    renderer_.assets(title.document(), std::move(reader));
}

void TitlePresenter::render(const TitleSequence &title, const RenderTarget &target,
                            const Background &background) {
    if (background)
        background(target);
    renderer_.render(title.frame(), target);

    // Fade the completed composition once, including the host's background.
    // The authored output owns the curve and duration; this pass only applies it.
    if (title.opacity() < 1) {
        int turns = (target.turns % 4 + 4) % 4;
        double w = turns % 2 ? target.viewport.height : target.viewport.width;
        double h = turns % 2 ? target.viewport.width : target.viewport.height;
        Draw shade;
        shade.path = "title/composition-fade";
        shade.kind = "rect";
        shade.bounds = shade.clip = {0, 0, w, h};
        shade.color = {0, 0, 0, 1 - title.opacity()};
        Frame frame;
        frame.draws.push_back(std::move(shade));
        renderer_.render(frame, target);
    }
}
} // namespace neon::widgets
