// Caverns of Minos game host: connects title, selection, information, play,
// bonuses and completion states to map drawing, input, pause actions and records.
// Field offsets document the original ARMv7 image, not a binary-compatible layout.
#pragma once
#include "neon_modules.h"
#include "cavern_launch.h"
#include "cavern_draw.h"
#include "entity.h"
#include "game_states.h"
#include <memory>

namespace NEON_GAME {
namespace Neon_Internals {

class theGame final : public minos::GameServices, public minos::cavern::MapHost {
  public:
    theGame();
    ~theGame() override;

    void run();                           // 0x6c020 (PlayState::runFrame)
    void reset(int keep);                 // 0x5654c
    void newLevel();                      // 0x636f0 (map part here, then PlayState::newLevelState)
    int _draw(sprite *s, vec2d screen);   // 0x64868
    int _bgdraw(sprite *s, vec2d screen); // 0x7d12c

    // minos::GameServices
    minos::Vec2 scrollDelta() const override {
        return play.scrollDelta;
    }
    int32_t clearAll() const override {
        return play.clearAll;
    }
    minos::cavern::StripMap &map() override {
        return *map_;
    }
    neon::DarwinRandom &rand() override {
        return rand_;
    }
    void addDebris(minos::Entity *e) override;
    void addMapExplosion(const minos::ExplosionBirth &birth) override {
        play.addMapExplosion(birth);
    }
    // minos::cavern::MapHost (stm::translate -> theGame::launch)
    void launch(char ch, minos::Vec2 pos, int column, int row) override;

    // ---- presentation (same offsets as Minotron's theGame; read by showJoy/Mixer), mirrored
    // from the PlayState after each run ----
    int drawMode = -1;                // 0x04
    int bgLayer = -1;                 // 0x08 _bgdraw's mode
    float fbColour[4] = {0, 0, 0, 0}; // 0x0c..0x18 (+0x18: brightness)
    float fbRot = 0;                  // 0x1c
    float fbScaleX = 1, fbScaleY = 1; // 0x20, 0x24
    float fbX = 0, fbY = 0;           // 0x28, 0x2c
    float flash = 0;                  // 0x30
    bool inputFlipped = false;        // 0x6c
    int frameCount = 0;               // 0x2a0
    cellGrid *grid = nullptr;         // 0x2a4 (attached by the cellGrid module)
    bool running = true;              // 0x2ac (showJoy runs the game while set)

    int state = 1;       // 0x470 run() state (constructor 1)
    int cavernIndex = 0; // 0x450 caverns[] index
    int volume = 8;      // 0x454 (constructor 8)
    int shipKind = 0;    // 0x9e4
    minos::Vec2 camera;  // 0x320 map camera (cells)
    int launches = 0;    // diagnostic: launch requests

    // The theGame state the translated classes use (reconstruction/minos).
    struct Play final : minos::PlayState {
        void playSample(int id, int vol) override; // AudioSystemPlaySample(id, vol, 0)
    } play;
    // Input maps to the original hardware-button word
    // (prefs +0xc84): d-pad left 8 / right 2 (run() steps the analog axis), thrust 0x10.
    void setPadInput(double stickX, bool thrustButton);

  private:
    struct CoreHud;
    void setMapColour(sprite &s, const minos::cavern::QuadColour &c) const;
    void startGame(); // development entry into play, bypassing the normal title/select path
    void sync();      // mirror the presentation fields from the PlayState
    std::unique_ptr<CoreHud> hud_;
    std::unique_ptr<minos::cavern::StripMap> map_;
    std::unique_ptr<minos::cavern::WallProfile> profile_;
    minos::cavern::FlashChannels flash_;
    int32_t word9dc_ = 0;
    neon::DarwinRandom rand_;
    bool gridPending_ = false;
    bool devEntry_ = false;
};

} // namespace Neon_Internals
} // namespace NEON_GAME
