#pragma once
#include "neon/render_state.h"
#include <stdexcept>
namespace neon {
// Adapter for the original free drawing functions. The module owns
// the state; a binding only borrows it for a callback and restores nested calls.
template <class Session> class ScopedRenderBinding {
  public:
    explicit ScopedRenderBinding(Session &state) : previous_(active_) {
        active_ = &state;
    }
    ~ScopedRenderBinding() {
        active_ = previous_;
    }
    ScopedRenderBinding(const ScopedRenderBinding &) = delete;
    ScopedRenderBinding &operator=(const ScopedRenderBinding &) = delete;
    static Session &current() {
        if (!active_)
            throw std::logic_error("Render operation outside a module session");
        return *active_;
    }

  private:
    inline static thread_local Session *active_ = nullptr;
    Session *previous_;
};
} // namespace neon
