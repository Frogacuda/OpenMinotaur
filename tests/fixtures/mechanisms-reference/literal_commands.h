// Saved reference adapters. Only binding types, method names and resources member adapted.
template <class Sprite, class Resources, class Actors>
void old_goatup(Sprite &target, Resources &resources, double width, double height,
                const Actors &actors) {
    int literal = -1;
    for (const auto &actor : actors) {
        const int frame = actor.frame + 2;
        const auto *entry = resources.sprite(frame);
        if (!entry)
            throw std::logic_error("Original GoatUp actor frame is not in its resource catalog");
        if (entry->page != literal) {
            target.setupPurge();
            target.batchPtr = target.batchStack->verts;
            target.batchStack->render.SetSourceFromLiterals(entry->page, entry->page, entry->page);
            literal = entry->page;
        }
        const float extent = actor.particle ? actor.radius : float(actor.radius + actor.radius);
        target.flags = 3;
        target.frame = frame;
        target.angle = actor.angle;
        target.col = actor.colour;
        target.pos = {actor.position.x * width, actor.position.y * height};
        target.width = double(extent) * width * actor.scale.x;
        target.height = double(extent) * height * actor.scale.y;
        target.setupChecking();
    }
    if (literal != -1) {
        target.setupPurge();
        target.batchPtr = target.batchStack->verts;
        target.batchStack->render.SetSourceFromLiterals(0, 0, 0);
    }
}
template <class Sprite, class Resources, class Actors>
void old_fiveaday(Sprite &target, Resources &resources, double width, double height,
                  const Actors &actors) {
    int literal = -1;
    for (const auto &actor : actors) {
        const int frame = actor.frame + 2;
        const auto *entry = resources.sprite(frame);
        if (!entry)
            throw std::logic_error(
                "Original Five a Day actor frame is not in its resource catalog");
        if (entry->page != literal) {
            target.setupPurge();
            target.batchPtr = target.batchStack->verts;
            target.batchStack->render.SetSourceFromLiterals(entry->page, entry->page, entry->page);
            literal = entry->page;
        }
        const float extent = float(actor.radius + actor.radius);
        target.flags = 3;
        target.frame = frame;
        target.angle = actor.angle;
        target.col = {actor.colour.r, actor.colour.g, actor.colour.b, actor.colour.a};
        target.pos = {actor.position.x * width, actor.position.y * height};
        target.width = double(extent) * width * actor.scale.x;
        target.height = double(extent) * height * actor.scale.y;
        target.setupChecking();
    }
    if (literal != -1) {
        target.setupPurge();
        target.batchPtr = target.batchStack->verts;
        target.batchStack->render.SetSourceFromLiterals(0, 0, 0);
    }
}
template <class Sprite, class Resources, class Actors>
void old_superoxwars(Sprite &target, Resources &resources, double width, double height,
                     const Actors &actors) {
    int literal = -1;
    for (const auto &actor : actors) {
        const int frame = actor.frame + 2;
        const auto *entry = resources.sprite(frame);
        if (!entry)
            throw std::logic_error(
                "Original Super Ox Wars actor frame is not in its resource catalog");
        if (entry->page != literal) {
            target.setupPurge();
            target.batchPtr = target.batchStack->verts;
            target.batchStack->render.SetSourceFromLiterals(entry->page, entry->page, entry->page);
            literal = entry->page;
        }
        const float extent = float(actor.radius + actor.radius);
        target.flags = 3;
        target.zOffset = actor.z;
        target.frame = frame;
        target.angle = actor.angle;
        target.col = {actor.colour.r, actor.colour.g, actor.colour.b, actor.colour.a};
        target.pos = {actor.position.x * width, actor.position.y * height};
        Point pivot{actor.pivot.x * width, actor.pivot.y * height}, rotated = pivot;
        rotated.rotateBy(double(actor.angle));
        target.pos.x += rotated.x - pivot.x;
        target.pos.y += rotated.y - pivot.y;
        target.width = double(extent) * width * actor.scale.x;
        target.height = double(extent) * height * actor.scale.y;
        // stm::draw 41534 sets 0x1000 with normalized atlas UVs. SPR_REGION
        // instead means pixel offsets inside a sprite frame (white pixel here).
        const neon::sprites::ExplicitUV uv{actor.uv.x, actor.uv.y, actor.uvSize.x, actor.uvSize.y};
        target.explicitUv = actor.explicitUv ? &uv : nullptr;
        if (actor.explicitUv)
            target.flags |= neon::sprites::SPR_EXPLICIT_UV;
        target.setupChecking();
        target.explicitUv = nullptr; // setupChecking has copied the rectangle into vertices
    }
    if (literal != -1) {
        target.setupPurge();
        target.batchPtr = target.batchStack->verts;
        target.batchStack->render.SetSourceFromLiterals(0, 0, 0);
    }
}
