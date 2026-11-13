#pragma once
#include "core_api.h"
#include "stage_presentation.h"
#include "stage_hud.h"
#include "neon_modules.h"
#include "neon_audio.h"
#include "neon_resources.h"
#include "neon/sample_audio.h"
#include <functional>

namespace goatup2 {
// Resource/effect adapter only. It owns no game clock, input, terrain or actors.
// Each original effect execution consumes a prepared logical frame; ordinary
// host redraws only present retained history.
class StageBackend final : public Neon_Internals::GamePresentationHost {
  public:
    bool init(mino::Host &, const mino::SurfaceInfo &, bool startup = false);
    bool startupPresented() const {
        return startupPresented_;
    }
    void resize(const mino::SurfaceInfo &);
    void execute(const StagePresentation &, StageRuntime &, neon::audio::SampleMixer &);
    void applyHud(const StageHud &);
    void clearHudRows(const std::vector<int32_t> &);
    void clearHud(); // original stage-start 0x33cb0
    void present();
    void presentLoading();
    void drawOverlay(Neon_Internals::sprite &, double, double, const std::vector<StageDraw> &);
    void release();
    const neon::ResourceCatalog &resources() const {
        return resources_;
    }
    Neon_Internals::cellGrid *grid = nullptr;
    std::function<void(Neon_Internals::sprite &, double, double)> foreground;
    void prepareGame() override {
        prepared_ = true;
    }
    void releaseGame() override {
        prepared_ = false;
        grid = nullptr;
    }
    bool hasGame() const override {
        return prepared_;
    }
    bool isRunning() const override {
        return prepared_;
    }
    void attachTextGrid(Neon_Internals::cellGrid &value) override {
        if (!grid)
            grid = &value;
    }
    bool hasBackground() const override {
        return true;
    }
    void drawForeground(Neon_Internals::sprite &, double, double) override;
    void drawBackground(Neon_Internals::sprite &, double, double) override;
    neon::FeedbackState feedback() const override {
        return {};
    }
    float flash() const override {
        return 0;
    }
    void decayFlash() override {} // source flash isn't active in this stage adapter
  private:
    Neon_Internals::RenderSession state_;
    Neon_Internals::NeonResManager resources_;
    Neon_Internals::NeonDevice device_;
    Neon_Internals::NeonDrawContext context_;
    Neon_Internals::WaveState waves_;
    std::unique_ptr<Neon_Internals::NeonEffect> effect_;
    const StagePresentation *frame_ = nullptr;
    int loadingLiteral_ = -1;
    bool prepared_ = false, deviceReady_ = false, startupPresented_ = false;
};
} // namespace goatup2
