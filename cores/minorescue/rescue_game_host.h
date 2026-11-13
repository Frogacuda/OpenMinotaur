#pragma once
#include "engine/game_presentation.h"
#include "engine/game.h"
#include "engine/neon_modules.h"
#include <memory>
namespace minorescue::module {
class GameHost final : public Neon_Internals::GamePresentationHost {
  public:
    explicit GameHost(float *titleHistory = nullptr)
        : titlePhase_(titleHistory ? titleHistory : &localPhase_) {}
    void prepareGame() override {
        if (!game_)
            game_ = std::make_unique<Neon_Internals::spaceWars>();
    }
    void releaseGame() override {
        game_.reset();
    }
    Neon_Internals::spaceWars *game() const {
        return game_.get();
    }
    bool hasGame() const override {
        return bool(game_);
    }
    bool isRunning() const override {
        return game_ && game_->running;
    }
    void attachTextGrid(Neon_Internals::cellGrid &grid) override {
        if (game_ && !game_->grid)
            game_->grid = &grid;
    }
    bool hasBackground() const override {
        return false;
    }
    void drawBackground(Neon_Internals::sprite &, double, double) override {}
    neon::PresenterRules presenterRules() const override {
        return {false, 1, 1, 0x8a};
    }
    neon::FeedbackState feedback() const override {
        neon::FeedbackState out;
        if (!game_)
            return out;
        out.inputFlipped = game_->inputFlipped;
        out.rotation = game_->fbRot;
        out.x = game_->fbX;
        out.y = game_->fbY;
        out.scaleX = game_->fbScaleX;
        out.scaleY = game_->fbScaleY;
        for (int i = 0; i < 4; ++i)
            out.colour[i] = game_->fbColour[i];
        return out;
    }
    float flash() const override {
        return game_ ? game_->flash : 0;
    }
    void decayFlash() override {
        if (game_)
            game_->flash = float(double(game_->flash) * .95);
    }
    void drawForeground(Neon_Internals::sprite &target, double W, double H) override {
        using namespace Neon_Internals;
        if (!game_)
            return;
        vec2d size(W, H);
        // Full-screen background tint.
        if (game_->bgColour) {
            target.pos = vec2d(0, 0);
            target.width = W;
            target.height = H;
            target.col.unpackRGBA(game_->bgColour);
            target.frame = 0xba;
            target.setupChecking();
        }
        // Centre flash.
        float flash = game_->flash;
        if (double(flash) > 0.01) {
            target.width = W * 0.1;
            target.height = H * 0.1;
            target.pos = vec2d(0, 0);
            target.col.white(flash > 1.0f ? 1.0 : double(flash));
            target.frame = 0xbd;
            target.setupChecking();
            game_->flash = float(double(game_->flash) * 0.95);
        }

        if (!game_->titleMode) {
            // (Bullets also call grid->passOverCell, which is empty.)
            for (entity *lists :
                 {game_->particles, game_->players, game_->bullets, game_->planets,
                  game_->enemyShots, game_->saucers, game_->list154, game_->minotaurs})
                for (entity *e = lists; e; e = e->next)
                    e->draw(&target, size);
            game_->showScore(&target, size);
        } else {
            // Title: seven letter sprites dropping into place one after another,
            // then the minotaur and a bobbing heart.
            static const int kFrames[7] = {0xa1, 0xa2, 0xa1, 0xa3, 0xa4, 0xa3, 0xa5};
            double x = W * -0.32;
            int i = 0;
            for (int k = -15; k >= -105; k -= 15, i++) {
                float t = float(k + game_->frameCount) / 15.0f;
                double y, alpha;
                if (t <= 1.0f) {
                    float d = (t - 1.0f) * 0.2f;
                    y = -(double(d) + 0.31);
                    alpha = t < 0.0f ? 0.0 : double(t);
                } else {
                    y = -0.31;
                    alpha = 1.0;
                }
                target.pos = vec2d(x, H * y);
                target.col.white(alpha);
                target.frame = kFrames[i] + 1;
                double f = double(0.1f);
                target.width = W * f;
                target.height = H * 0.2;
                target.setupChecking();
                x += W * (f * 1.05);
            }
            target.pos = vec2d(0, 120.0);
            target.col.white(1.0);
            target.frame = 0x98;
            target.width = 60.0;
            target.height = 150.0;
            target.setupChecking();
            float &testPhase = *titlePhase_;
            testPhase = float(double(testPhase) + 0.1);
            target.frame = 0x99;
            target.width = 20.0;
            target.height = 20.0;
            target.pos.y = target.pos.y - double(std::sin(testPhase) * 20.0f + 100.0f);
            target.setupChecking();
        }
    }

  private:
    std::unique_ptr<Neon_Internals::spaceWars> game_;
    float localPhase_ = 0;
    float *titlePhase_;
};
} // namespace minorescue::module
