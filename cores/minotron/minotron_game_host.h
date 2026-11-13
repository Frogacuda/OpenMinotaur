#pragma once
#include "engine/game_presentation.h"
#include "engine/game.h"
#include <memory>

namespace minotron {
namespace module {
// The only bridge from effect presentation to Minotron's concrete game fields.
// Kept in the module, outside the engine effect implementation.
class GameHost final : public Neon_Internals::GamePresentationHost {
  public:
    void prepareGame() override {
        if (!game_)
            game_ = std::make_unique<Neon_Internals::theGame>();
    }
    void releaseGame() override {
        game_.reset();
    }
    Neon_Internals::theGame *game() const {
        return game_.get();
    }
    bool hasGame() const override {
        return game_ != nullptr;
    }
    bool isRunning() const override {
        return game_ && game_->running;
    }
    void attachTextGrid(Neon_Internals::cellGrid &grid) override {
        if (game_ && !game_->grid)
            game_->grid = &grid;
    }
    bool hasBackground() const override {
        return game_ && game_->bgLayer >= 0;
    }
    void drawForeground(Neon_Internals::sprite &target, double w, double h) override {
        if (game_)
            game_->_draw(&target, {w, h});
    }
    void drawBackground(Neon_Internals::sprite &target, double w, double h) override {
        if (game_)
            game_->_bgdraw(&target, {w, h});
    }
    neon::FeedbackState feedback() const override {
        neon::FeedbackState state;
        if (!game_)
            return state;
        state.inputFlipped = game_->inputFlipped;
        state.rotation = game_->fbRot;
        state.x = game_->fbX;
        state.y = game_->fbY;
        state.scaleX = game_->fbScaleX;
        state.scaleY = game_->fbScaleY;
        for (int i = 0; i < 4; i++)
            state.colour[i] = game_->fbColour[i];
        return state;
    }
    void finishForeground(Neon_Internals::sprite &target, double w, double h) override {
        neon::finishUnsubmittedFlash(*this, target, w, h);
    }
    float flash() const override {
        return game_ ? game_->flash : 0;
    }
    void decayFlash() override {
        if (game_)
            game_->flash *= 0.95f;
    }

  private:
    std::unique_ptr<Neon_Internals::theGame> game_;
};
} // namespace module
} // namespace minotron
