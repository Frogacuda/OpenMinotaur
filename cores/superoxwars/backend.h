#pragma once
#include "core_api.h"
#include "neon_modules.h"
#include "neon_resources.h"
#include "world.h"

namespace superoxwars {
// Game-facing binding over maintained renderer/resource/effect execution.
// No original binary, memory layout or host graphics API enters game rules.
class Backend final : public Neon_Internals::GamePresentationHost {
  public:
    bool init(mino::Host &, const mino::SurfaceInfo &);
    void resize(const mino::SurfaceInfo &);
    void execute(World &);
    void present();
    void presentLoading();
    void release();
    void drawActors(Neon_Internals::sprite &, double, double, const std::vector<ActorDraw> &);
    std::function<void(Neon_Internals::sprite &, double, double)> background, foreground;
    std::function<void(Neon_Internals::cellGrid &)> text;
    const neon::ResourceCatalog &resources() const {
        return resources_;
    }
    void prepareGame() override {
        ready_ = true;
    }
    void releaseGame() override {
        ready_ = false;
    }
    bool hasGame() const override {
        return ready_;
    }
    bool isRunning() const override {
        return ready_;
    }
    void attachTextGrid(Neon_Internals::cellGrid &grid) override {
        if (text)
            text(grid);
    }
    bool hasBackground() const override {
        return true;
    }
    void drawBackground(Neon_Internals::sprite &, double, double) override;
    void drawForeground(Neon_Internals::sprite &, double, double) override;
    neon::FeedbackState feedback() const override {
        return feedbackState;
    }
    float flash() const override {
        return 0;
    }
    void decayFlash() override {}
    neon::FeedbackState feedbackState;

  private:
    Neon_Internals::RenderSession binding_;
    Neon_Internals::NeonResManager resources_;
    std::unique_ptr<Neon_Internals::NeonDevice> device_ =
        std::make_unique<Neon_Internals::NeonDevice>();
    Neon_Internals::NeonDrawContext context_;
    std::unique_ptr<Neon_Internals::NeonEffect> effect_;
    bool ready_ = false, deviceReady_ = false;
};
} // namespace superoxwars
