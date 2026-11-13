#ifdef NDEBUG
#undef NDEBUG
#endif
#include "controls.h"
#include "campaign.h"
#include "modern_profile.h"
#include "sample_audio.h"
#include "original_messages.h"
#include "native_classic.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <sstream>
using namespace gridrunner;
namespace gr::reconstruction::ios {
struct ClassicSourceChecks {
    static void touchScale() {
        for (auto mode : {TouchMode::Normal, TouchMode::Expert}) {
            Classic game;
            game.start();
            game.screen_.clear();
            game.ship_ = {10, 17};
            TouchControl touch;
            touch.begin({0, 0});
            touch.move(mode == TouchMode::Normal ? Axis{.012, -.006} : Axis{.1, -.05});
            const auto delta = touch.tick(mode, TouchTarget::Classic);
            game.doShip(delta.x, delta.y);
            assert(game.ship_.x == 10.4f && game.ship_.y == 16.8f);
        }
    }
};
} // namespace gr::reconstruction::ios
bool near(double a, double b) {
    return std::abs(a - b) < 1e-10;
}
int main() {
    gr::reconstruction::ios::ClassicSourceChecks::touchScale();
    assert(near(response({.5, 0}, kFastAnalog).x, .375));
    assert(near(response({1, 0}, kFastAnalog).x, 3));
    assert(near(std::hypot(response({1, 1}, kDpad).x, response({1, 1}, kDpad).y), 2));
    for (bool digital : {false, true})
        for (bool accelerated : {false, true})
            for (Axis axis : {Axis{1, 0}, Axis{.4, -.7}, Axis{1, 1}}) {
                const auto normal = controllerResponse(axis, digital, accelerated, false),
                           precise = controllerResponse(axis, digital, accelerated, true);
                const double ratio = accelerated ? 2. : .5;
                assert(near(precise.x, normal.x * ratio) && near(precise.y, normal.y * ratio));
            }
    assert(near(controllerResponse({1, 0}, false, false, false).x, 3));
    assert(near(controllerResponse({1, 0}, false, false, true).x, 1.5));
    assert(near(controllerResponse({1, 0}, false, true, false).x, 1.5));
    assert(near(controllerResponse({1, 0}, false, true, true).x, 3));
    TouchControl t;
    t.begin({0, 0});
    t.move({.1, .2});
    auto d = t.tick(TouchMode::Normal);
    assert(near(d.x, .075) && near(d.y, .15));
    t.release();
    assert(near(t.tick(TouchMode::Normal).x, .0675));
    t.cancel();
    assert(t.tick(TouchMode::Normal).x == 0);
    t.begin({0, 0});
    t.move({.012, .024});
    d = t.tick(TouchMode::Normal, TouchTarget::Classic);
    assert(near(d.x, .1) && near(d.y, .2));
    t.release();
    assert(near(t.tick(TouchMode::Normal, TouchTarget::Classic).x, .09));
    t.begin({0, 0});
    t.move({.1, .2});
    d = t.tick(TouchMode::Expert, TouchTarget::Classic);
    assert(near(d.x, .1) && near(d.y, .2));
    t.release();
    assert(t.tick(TouchMode::Expert, TouchTarget::Classic).x == 0);
    t.begin({0, 0});
    t.move({.1, .2});
    d = t.tick(TouchMode::Expert);
    assert(near(d.x, .009) && near(d.y, .018));
    t.release();
    assert(t.tick(TouchMode::Expert).x == 0);
    t.begin({0, 0});
    t.move({.1, 0});
    t.tick(TouchMode::Expert);
    t.move({.08, 0});
    assert(t.tick(TouchMode::Expert).x == 0);
    t.move({.07, 0});
    assert(t.tick(TouchMode::Expert).x < 0);
    MultiTouchControl touch;
    touch.down(12, {0, 0});
    touch.down(99, {.3, .3});
    touch.move(12, {.1, 0});
    d = touch.tick(TouchMode::Normal);
    assert(d.x > 0 && d.y == 0);
    touch.up(12, {.1, 0});
    d = touch.tick(TouchMode::Normal);
    assert(d.x == 0 && d.y == 0);
    touch.move(99, {.3, .4});
    d = touch.tick(TouchMode::Normal);
    assert(d.x == 0 && d.y > 0);
    touch.cancel();
    assert(!touch.active());
    CampaignProgress progress;
    for (int level = 4; level <= 48; level += 4)
        assert(progress.reached(level, level * 100, level + 3));
    assert(!progress.reached(48, 999999, 50));
    assert(progress.reached(48, 4801, 52));
    std::stringstream saved;
    progress.write(saved);
    CampaignProgress restored;
    assert(restored.read(saved));
    assert(restored.highestLevel() == 48 && restored.at(48).score == 4801 &&
           restored.at(48).lives == 52);
    std::stringstream corrupt("GRIDRUNNER_CHECKPOINTS 1 200\n");
    assert(!restored.read(corrupt) && restored.highestLevel() == 48);
    namespace fs = std::filesystem;
    const auto root = fs::current_path() /
                      ("build-profile-fixture-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto modern = root / "gridrunner", preview = root / "gridrunner-native";
    fs::create_directories(modern);
    fs::create_directories(preview);
    AchievementRecord goals;
    goals.record(4);
    goals.record(4);
    goals.record(9);
    assert(goals.unlocked == 0x210 && goals.save((modern / "achievements.dat").string()) &&
           !goals.dirty);
    AchievementRecord reloaded;
    reloaded.load((modern / "achievements.dat").string());
    assert(reloaded.unlocked == 0x210);
    const auto write = [](const fs::path &p, const std::string &s) {
        std::ofstream out(p);
        out << s;
        assert(out.good());
    };
    const auto read = [](const fs::path &p) {
        std::ifstream in(p);
        std::stringstream out;
        out << in.rdbuf();
        return out.str();
    };
    write(modern / "ios-rules-highscores.txt", "DISCARDED GAMEPLAY SCORE");
    migratePreviewProfile(modern.string());
    assert(!fs::exists(modern / "highscores.txt"));
    write(preview / "native-preview-highscores.txt", "ACCEPTED RECONSTRUCTION SCORE");
    write(preview / "native-preview-controls.txt", "0 1 0 0\n");
    write(preview / "native-preview-checkpoints.txt", saved.str());
    write(preview / "native-preview-checkpoints.txt.bak", saved.str());
    migratePreviewProfile(modern.string() + "/");
    assert(read(modern / "highscores.txt") == "ACCEPTED RECONSTRUCTION SCORE");
    assert(read(modern / "controls.txt") == "0 1 0 0\n");
    assert(restored.load((modern / "checkpoints.txt").string()) && restored.highestLevel() == 48);
    assert(fs::exists(modern / "checkpoints.txt.bak"));
    write(modern / "highscores.txt", "CURRENT SCORE");
    migratePreviewProfile(modern.string());
    assert(read(modern / "highscores.txt") == "CURRENT SCORE");
    assert(read(preview / "native-preview-highscores.txt") == "ACCEPTED RECONSTRUCTION SCORE");
    Sample sample;
    sample.pcm.assign(8, 32767);
    sample.rate = 11025;
    sample.maxInstances = 1;
    SampleMixer mixer;
    mixer.setSamples({sample});
    float audio[160];
    mixer.play(0, 7 << 12, 1);
    mixer.mix(audio, 80);
    for (float v : audio)
        assert(std::isfinite(v));
    assert(audio[80] > .4f && audio[80] < .5f);
    mixer.stop(0);
    mixer.mix(audio, 80);
    for (int i = 8; i < 160; ++i)
        assert(audio[i] == 0);
    std::cout
        << "Direct controls, both touch modes, multitouch, checkpoints, non-overwriting preview migration and audio passed\n";
}
