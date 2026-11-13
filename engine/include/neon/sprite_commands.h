#pragma once

namespace neon::sprites {
// Execute an ordered command range against the stack's literal pages. This is
// the common host adapter protocol, not a game update or a sorting operation.
// configureAndSubmit owns colour/depth/UV lifetimes and must submit synchronously.
// Missing frames throw so an invalid sprite command cannot continue silently.
// Empty ranges have no side effects. Finish restores literal page 0, not an
// inferred incoming route; callers needing route restoration use their own path.
template <class Sprite, class Resources, class Commands, class FrameOf, class Submit, class Missing>
void executeLiteralCommands(Sprite &target, Resources &resources, const Commands &commands,
                            FrameOf frameOf, Submit configureAndSubmit, Missing missing) {
    int literal = -1;
    for (const auto &command : commands) {
        const int frame = frameOf(command);
        const auto *entry = resources.sprite(frame);
        if (!entry)
            missing(frame);
        if (entry->page != literal) {
            target.setupPurge();
            target.batchPtr = target.batchStack->verts;
            target.batchStack->render.SetSourceFromLiterals(entry->page, entry->page, entry->page);
            literal = entry->page;
        }
        configureAndSubmit(target, command, frame);
    }
    if (literal != -1) {
        target.setupPurge();
        target.batchPtr = target.batchStack->verts;
        target.batchStack->render.SetSourceFromLiterals(0, 0, 0);
    }
}

// Radius/diameter selection stays at the call site. The intermediate extent is
// binary32, then X and Y multiply extent, view size, scale in this exact order.
// Do not apply this to Gridrunner's scale-first geometry or pixel-space packets.
template <class Sprite, class Command>
void normalizedActorRectangle(Sprite &target, const Command &command, double width, double height,
                              float extent) {
    target.pos = {command.position.x * width, command.position.y * height};
    target.width = double(extent) * width * command.scale.x;
    target.height = double(extent) * height * command.scale.y;
}
} // namespace neon::sprites
