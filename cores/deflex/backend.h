#pragma once
#include "core_api.h"
#include "world.h"
#include "neon_modules.h"
#include "neon_resources.h"
#include <functional>
#include <chrono>
namespace deflex {
class Backend final : public Neon_Internals::GamePresentationHost {
    Neon_Internals::RenderSession binding_;
    Neon_Internals::NeonResManager resources_;
    Neon_Internals::NeonDevice device_;
    Neon_Internals::NeonDrawContext drawing_;
    std::unique_ptr<Neon_Internals::NeonEffect> effect_;
    World *world_ = nullptr;
    bool ready_ = false, paused_ = false, startupOverlay_ = true;
    std::chrono::steady_clock::time_point loadingStarted_;
    int loadingLiteral_ = 0;
    void loadingImage(float alpha);
    void presentLoading();

  public:
    std::function<neon::FeedbackState()> sceneFeedback;
    std::function<void(GridRenderer &, ActorRenderer &, neon::vec2d)> overlay;
    std::function<void(GridRenderer &, ActorRenderer &, neon::vec2d, bool)> gridScene;
    std::function<void(ActorRenderer &, neon::vec2d, bool)> actorsScene;
    bool init(mino::Host &, const mino::SurfaceInfo &, bool startup = true);
    void resize(const mino::SurfaceInfo &);
    void execute(World *, bool paused);
    void present();
    void release();
    const neon::ResourceCatalog &resources() const {
        return resources_;
    }
    void grid(Neon_Internals::sprite &, double, double);
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
    void attachTextGrid(Neon_Internals::cellGrid &) override {}
    bool hasBackground() const override {
        return false;
    }
    void drawBackground(Neon_Internals::sprite &, double, double) override {}
    void drawForeground(Neon_Internals::sprite &, double, double) override;
    neon::FeedbackState feedback() const override {
        return sceneFeedback ? sceneFeedback() : neon::FeedbackState{};
    }
    neon::PresenterRules presenterRules() const override {
        return {true, 1, 0, 28};
    }
    float flash() const override {
        return 0;
    }
    void decayFlash() override {}
};
} // namespace deflex
