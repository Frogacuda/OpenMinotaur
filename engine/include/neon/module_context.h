#pragma once
#include "neon/render_binding.h"

namespace neon {
// Module-owned batch state plus borrowed service references. Resource/device/
// drawing owners remain in the core/backend; connecting does not initialize,
// release, flush, reset history, or choose an execution schedule.
template <class Vertex, std::size_t Capacity, class Resources, class Device, class Drawing>
struct ModuleRenderContext : SpriteRenderState<Vertex, Capacity> {
    using Scope = ScopedRenderBinding<ModuleRenderContext>;
    Resources *resources = nullptr;
    Device *device = nullptr;
    Drawing *context = nullptr;

    // Original free drawing functions use this callback-scoped bridge. Nested
    // scopes restore their caller; a scope never owns the context or services.
    [[nodiscard]] Scope bind() {
        return Scope(*this);
    }

    // Separate attachment points preserve source initialization order, including
    // cores that attach resources before device and drawing setup.
    void attachResources(Resources &value) noexcept {
        resources = &value;
    }
    void attachDevice(Device &value) noexcept {
        device = &value;
    }
    void attachDrawing(Drawing &value) noexcept {
        context = &value;
    }
    void connect(Resources &r, Device &d, Drawing &c) noexcept {
        attachResources(r);
        attachDevice(d);
        attachDrawing(c);
    }
    // Call at the module's existing detach point, after dependent effects are
    // released. Deliberately leaves batch state and shader-cycle history alone.
    void disconnect() noexcept {
        context = nullptr;
        device = nullptr;
        resources = nullptr;
    }
};
} // namespace neon
