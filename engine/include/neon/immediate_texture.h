#pragma once
#include "neon/sprite_draw.h"
namespace neon::sprites {
enum class LiteralSelectionRule { AlwaysSelect, ReuseSelectedLiteral };
struct ImmediateSource {
    int literal = -1, history = -1;
};
// Resolve the displayed effect through the module's borrowed context. A backend
// binding supplies its existing source-bank calls; their argument order matters.
template <class Session, class Flush, class Bind>
void selectImmediateTexture(Session &state, int literal, LiteralSelectionRule rule, Flush flush,
                            Bind bind) {
    if (rule == LiteralSelectionRule::ReuseSelectedLiteral && literal >= 0 &&
        literal == state.sourceTexture)
        return;
    flush();
    if (!state.context)
        return;
    ImmediateSource source;
    if (literal >= 0)
        source.literal = literal;
    else {
        auto *effect = state.context->effect;
        auto *stack = effect ? effect->getStack(state.context->effectStack) : nullptr;
        if (stack)
            source.history = stack->prevRT;
        else
            source.literal = 4;
    }
    bind(state.context->main, source);
    state.sourceTexture = source.literal >= 0 ? source.literal : kNoTexture;
}
// Six-argument legacy API calls overlap source banks. Keep their original
// ordering rather than treating the extra arguments as independent samplers.
template <class Render> void bindSixArgumentSources(Render &r, ImmediateSource source) {
    if (source.literal >= 0) {
        r.SetSourceFromLiterals(source.literal, -1, -1, source.literal, -1, -1);
        r.SetSourceFromRTs(-1, -1, -1, -1, -1, -1);
    } else {
        r.SetSourceFromLiterals(-1, -1, -1, -1, -1, -1);
        r.SetSourceFromRTs(source.history, -1, -1, source.history, -1, -1);
    }
}
// Existing Gridrunner host binding clears font sources and selects primary
// banks. This is a renderer adaptation, not an inferred engine revision.
template <class Render> void bindPrimarySources(Render &r, ImmediateSource source) {
    r.SetSourceFromFont(-1, -1, -1);
    if (source.literal >= 0) {
        r.SetSourceFromRTs(-1, -1, -1);
        r.SetSourceFromLiterals(source.literal, -1, -1);
    } else {
        r.SetSourceFromLiterals(-1, -1, -1);
        r.SetSourceFromRTs(source.history, -1, -1);
    }
}
} // namespace neon::sprites
