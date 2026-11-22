#include "live_rig.h"
#include "effect_runtime.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace neon::effects;
void checkAt(bool ok, int line) {
    if (!ok)
        throw std::runtime_error("Live bezel rig check failed at line " + std::to_string(line));
}
#define check(value) checkAt((value), __LINE__)
int main(int argc, char **argv) {
    try {
        if (argc != 2)
            return 2;
        std::filesystem::path assets = argv[1];
        std::ifstream definition(assets / "bezels/GoatUp.neonrig");
        RigProgram program = RigProgram::read(definition);
        RigPlayer player(program);
        ReadSnapshot s;
        s.game = "goatup";
        s.session = 1;
        s.sequence = 1;
        s.values = {{"player.x", -.5},
                    {"screen.elevation", 0.},
                    {"kids", uint64_t(0)},
                    {"billyGoats.kissed", uint64_t(0)}};
        auto frame = [&](double dt) {
            ++s.sequence;
            return player.sample(s, dt, true, false);
        };
        auto v = frame(0);
        check(std::abs(v.at("camera.yaw") - .8) < 1e-6 &&
              std::abs(v.at("node.25.f.3") + .506909966) < 1e-6);
        s.values["player.x"] = .5;
        s.values["screen.elevation"] = 1.;
        v = frame(0);
        check(std::abs(v.at("camera.yaw") + .8) < 1e-6 &&
              std::abs(v.at("node.25.f.3") + .006909966) < 1e-6);
        s.values["screen.elevation"] = 2.;
        auto up = frame(0).at("node.25.f.3");
        s.values["screen.elevation"] = 1.;
        check(frame(0).at("node.25.f.3") < up);
        for (double elevation : {-20., -4., 0., 4., 20.}) {
            s.values["screen.elevation"] = elevation;
            auto x = frame(0).at("node.25.f.3");
            check(x >= -1 && x < 1 && std::abs(x + .506909966) < 1e-6);
        }
        s.values["kids"] = uint64_t(1);
        check(frame(0).at("runtime.hue") == 0);
        check(std::abs(frame(.25).at("runtime.hue") * 360 - 90) < .001);
        check(std::abs(frame(.25).at("runtime.hue") * 360 - 180) < .001);
        check(std::abs(frame(.25).at("runtime.hue") * 360 - 270) < .001);
        check(frame(.25).at("runtime.hue") == 0);
        s.values["billyGoats.kissed"] = uint64_t(1);
        check(frame(0).at("runtime.hue") == 0);
        check(std::abs(frame(.5).at("runtime.hue") * 360 - 50) < .001);
        check(std::abs(frame(.5).at("runtime.hue") * 360 - 100) < .001);
        ++s.sequence;
        auto frozen = player.sample(s, 10, true, true);
        check(std::abs(frozen.at("runtime.hue") * 360 - 100) < .001);
        check(std::abs(frame(.5).at("runtime.hue") * 360 - 50) < .001);
        check(frame(.5).at("runtime.hue") == 0);
        s.values["kids"] = uint64_t(0);
        s.values["billyGoats.kissed"] = uint64_t(0);
        check(frame(0).at("runtime.hue") == 0);
        ++s.session;
        s.values["kids"] = uint64_t(10);
        s.values["billyGoats.kissed"] = uint64_t(10);
        check(frame(0).at("runtime.hue") == 0);
        s.values["kids"] = uint64_t(11);
        s.values["billyGoats.kissed"] = uint64_t(11);
        frame(0);
        check(std::abs(frame(.5).at("runtime.hue") * 360 - 230) < .001);
        check(std::abs(frame(.5).at("runtime.hue") * 360 - 100) < .001);
        check(frame(1).at("runtime.hue") == 0);
        {
            RigPlayer zones(program);
            ReadSnapshot snapshot;
            snapshot.game = "goatup";
            snapshot.session = 1;
            snapshot.values = {{"zone", int64_t(0)},
                               {"mushroom.active", false},
                               {"kids", uint64_t(0)},
                               {"billyGoats.kissed", uint64_t(0)}};
            auto sample = [&](double dt, bool paused = false) {
                ++snapshot.sequence;
                return zones.sample(snapshot, dt, true, paused);
            };
            auto hue = [&](double dt) { return sample(dt).at("runtime.hue") * 360; };
            check(sample(0).at("node.22.i.2") == 12);
            snapshot.values["zone"] = int64_t(1);
            check(std::abs(hue(0)) < .001 && std::abs(hue(.25) - 25) < .001 &&
                  std::abs(hue(.25) - 50) < .001);
            snapshot.values["mushroom.active"] = true;
            check(sample(0).at("node.22.i.2") == 46);
            check(std::abs(hue(.5) - 140) < .001);
            check(std::abs(sample(10, true).at("runtime.hue") * 360 - 140) < .001);
            check(std::abs(hue(1.5) - 50) < .001);
            snapshot.values["zone"] = int64_t(2);
            sample(0);
            check(std::abs(hue(.5) - 190) < .001);
            snapshot.values["mushroom.active"] = false;
            auto restored = sample(0);
            check(std::abs(restored.at("runtime.hue") * 360 - 100) < .001 &&
                  restored.at("node.22.i.2") == 12);
            snapshot.values["zone"] = int64_t(1);
            check(std::abs(hue(.5) - 100) < .001);
            snapshot.values["kids"] = uint64_t(1);
            sample(0);
            check(std::abs(hue(.5) - 280) < .001 && std::abs(hue(.5) - 100) < .001);
            snapshot.values["mushroom.active"] = true;
            sample(0);
            ++snapshot.session;
            check(std::abs(hue(0)) < .001);
            std::cout << "PASS: GoatUp zone baseline, mushroom shader/2-second turns, pause, "
                         "expiry, descent and kid cue composition.\n";
        }
        Runtime effect(assets / "neon");
        std::ifstream scene(assets / "bezels/GoatUp.neongles");
        check(std::abs(effect.readScene(scene) - .54) < 1e-6);
        bool mesh = false;
        for (auto &e : effect.authoring().controls(11))
            if (e.key == "mesh.19.sides")
                mesh = e.enabled && e.value == 16;
        check(mesh);
        for (auto &r : program.values) {
            bool target = false;
            for (auto &e : effect.authoring().controls(11))
                if (e.key == r.target)
                    target = r.low >= e.minimum && r.high <= e.maximum;
            if (!target)
                throw std::runtime_error("Unavailable GoatUp rig range: " + r.target);
        }
        std::ifstream rescueDefinition(assets / "bezels/MinotaurRescue.neonrig");
        auto rescueProgram = RigProgram::read(rescueDefinition);
        RigPlayer rescue(rescueProgram);
        ReadSnapshot rs;
        rs.game = "minorescue";
        rs.session = 1;
        rs.values = {{"player.orientation", 0.},
                     {"player.fireBoost", 0.},
                     {"level", int64_t(5)},
                     {"minotaurs.rescued", uint64_t(8)},
                     {"center.contacts", uint64_t(0)},
                     {"player1.hits", uint64_t(0)},
                     {"player2.hits", uint64_t(0)}};
        auto rf = [&](double dt) {
            ++rs.sequence;
            return rescue.sample(rs, dt, true, false);
        };
        auto near = [](double a, double b) { check(std::abs(a - b) < 2e-5); };
        constexpr double pi = 3.141592653589793, base = .166666672;
        auto rv = rf(0);
        near(rv.at("runtime.hue"), base);
        near(rv.at("clock.14.3.speed"), .25);
        rs.values["player.orientation"] = -pi / 2;
        rs.values["player.fireBoost"] = 1.;
        rv = rf(0);
        near(rv.at("node.17.f.4"), pi * 1.5);
        near(rv.at("clock.14.3.speed"), 1);
        rs.values["level"] = int64_t(6);
        rs.values["minotaurs.rescued"] = uint64_t(9);
        rf(0);
        near(rf(.5).at("runtime.hue"), base + 25. / 360 + .5);
        near(rf(.5).at("runtime.hue"), base + 50. / 360);
        near(rf(2).at("runtime.hue"), base + 50. / 360);
        rs.values["minotaurs.rescued"] = uint64_t(11);
        rf(0);
        near(rf(.5).at("runtime.hue"), base + 50. / 360 + .5);
        near(rf(.5).at("runtime.hue"), base + 50. / 360);
        near(rf(.5).at("runtime.hue"), base + 50. / 360 + .5);
        near(rf(.5).at("runtime.hue"), base + 50. / 360);
        rs.values["center.contacts"] = uint64_t(1);
        rs.values["player2.hits"] = uint64_t(1);
        rf(0);
        rv = rf(.25);
        near(rv.at("node.14.f.4"), 2 * pi * .15625);
        near(rv.at("node.4.f.4"), -2 * pi * .15625);
        ++rs.sequence;
        near(rescue.sample(rs, 10, true, true).at("node.14.f.4"), rv.at("node.14.f.4"));
        rv = rf(.75);
        near(rv.at("node.14.f.4"), 0);
        near(rv.at("node.4.f.4"), 0);
        rs.values["player1.hits"] = uint64_t(1);
        rf(0);
        near(rf(.5).at("node.14.f.4"), pi);
        ++rs.session;
        near(rf(0).at("runtime.hue"), base);
        near(rf(0).at("node.14.f.4"), 0);
        std::ifstream rescueScene(assets / "bezels/MinotaurRescue.neongles");
        near(effect.readScene(rescueScene), 1.99);
        check(effect.selectedPreset() == 22);
        const auto parameters = effect.responseParameters();
        auto target = [&](const std::string &key) {
            return std::find_if(parameters.begin(), parameters.end(),
                                [&](const auto &p) { return p.key == key; });
        };
        for (const auto &r : rescueProgram.values)
            check(target(r.target) != parameters.end());
        for (const auto &r : rescueProgram.counters)
            check(target(r.target) != parameters.end());
        auto preset =
            neon::pc::PresetPlayback::load(assets / "neon/presets/minotaur-rescue.grpreset",
                                           assets / "neon/reference/minotaur-rescue.routes.tsv");
        const auto &tunnelMaterial = preset.nodes[15].saved.floats;
        near(std::abs(16 * pi * (tunnelMaterial[6] - .5) * tunnelMaterial[5]), 2);
        auto slow = preset;
        slow.nodes[14].channels[3].speed = .25;
        for (int tick = 0; tick < 120; ++tick) {
            preset.tick();
            slow.tick();
        }
        check(std::abs(preset.nodes[14].values[3].value() - slow.nodes[14].values[3].value()) >
              .01);
        auto phase = slow.nodes[14].values[3].value();
        slow.nodes[14].channels[3].speed = 1;
        slow.tick();
        check(std::abs(phase - slow.nodes[14].values[3].value()) < .02);
        std::cout
            << "PASS: Rescue heading/boost, additive persistent hue, queued rescues, "
               "smooth player cues, pause/session reset, scene targets and continuous clocks.\n";
        std::cout
            << "PASS: camera elevation rig, descending reversal, cyclic wrap, left/right yaw, "
               "2-second hue envelope, pause/session baselines, scene import and 16-sided mesh.\n";
        {
            std::ifstream definition(assets / "bezels/Minotron.neonrig");
            RigPlayer minotron(RigProgram::read(definition));
            ReadSnapshot sample;
            sample.game = "minotron";
            sample.session = 1;
            sample.sequence = 1;
            sample.values = {{"level", int64_t(0)},
                             {"levels.completed", uint64_t(0)},
                             {"audio.level", 0.},
                             {"player.shots", int64_t(1)},
                             {"player.invincible", false},
                             {"bombs.used", uint64_t(0)},
                             {"extraLives.gained", uint64_t(0)}};
            auto read = [&](double dt) {
                ++sample.sequence;
                return minotron.sample(sample, dt, true, false);
            };
            auto initial = read(0);
            check(initial.at("node.7.i.1") == 0 && initial.at("clock.10.6.speed") == 1 &&
                  initial.at("node.4.i.2") == 59);
            sample.values["levels.completed"] = uint64_t(1);
            sample.values["level"] = int64_t(1);
            read(0);
            auto rise = read(.25);
            check(std::abs(rise.at("node.7.i.1") - 100) < .001 && rise.at("clock.10.6.speed") > 1 &&
                  rise.at("clock.10.6.speed") < 1.2);
            auto peak = read(.25);
            check(std::abs(peak.at("node.7.i.1") - 200) < .001 &&
                  std::abs(peak.at("colour.1.hue") - 25) < .001);
            auto plateau = read(1.5);
            check(plateau.at("node.7.i.1") == 200 &&
                  std::abs(plateau.at("clock.10.6.speed") - (1 + 7 * std::pow(2. / 2.25, 2))) <
                      .001);
            auto fall = read(.125);
            check(std::abs(fall.at("node.7.i.1") - 100) < .001);
            auto end = read(.125);
            check(end.at("node.7.i.1") == 0 && end.at("clock.10.6.speed") == 1 &&
                  std::abs(end.at("colour.1.hue") - 25) < .001);
            sample.values["player.shots"] = int64_t(3);
            check(read(0).at("node.4.i.2") == 61);
            sample.values["player.invincible"] = true;
            check(read(0).at("node.4.i.2") == 46);
            sample.values["player.invincible"] = false;
            check(read(0).at("node.4.i.2") == 61);
            sample.values["player.shots"] = int64_t(1);
            check(read(0).at("node.4.i.2") == 59);
            sample.values["audio.level"] = 1.;
            read(0);
            auto bright = read(.06);
            check(std::abs(bright.at("colour.1.brightness") - 1.9) < .001);
            auto paused = minotron.sample(sample, 10, true, true);
            check(paused == bright);
            sample.values["audio.level"] = .5;
            check(std::abs(read(0).at("colour.1.brightness") - 1.3) < .001);
            sample.values["audio.level"] = 0.;
            check(std::abs(read(.18).at("colour.1.brightness") - .7) < .001);
            sample.values["bombs.used"] = uint64_t(1);
            sample.values["audio.level"] = 0.;
            read(0);
            check(std::abs(read(.1).at("colour.1.brightness") - 3) < .001);
            check(std::abs(read(.4).at("colour.1.brightness") - .7) < .001);
            sample.values["extraLives.gained"] = uint64_t(1);
            read(0);
            check(std::abs(read(.25).at("colour.1.hue") - 115) < .01);
            check(std::abs(read(.75).at("colour.1.hue") - 25) < .01);
            ++sample.session;
            auto reset = read(0);
            check(reset.at("colour.1.hue") == 0 && reset.at("node.7.i.1") == 0);
            std::ifstream document(assets / "bezels/Minotron.neongles");
            Runtime effect(assets / "neon");
            effect.readScene(document);
            RigPlayer sustained(minotron.program());
            sample.values["audio.level"] = 0.;
            sustained.sample(sample, 0, true, false);
            sample.values["audio.level"] = 1.;
            check(std::abs(sustained.sample(sample, 5, true, false).at("colour.1.brightness") -
                           1.9) < .001);
            sample.values["audio.level"] = 0.;
            check(std::abs(sustained.sample(sample, 0, true, false).at("colour.1.brightness") -
                           .7) < .001);
            ResponseSequence clock;
            clock.name = "test clock";
            clock.source = "test.speed";
            clock.mode = ResponseMode::Input;
            clock.inputLow = 0;
            clock.inputHigh = 8;
            clock.targets = {
                {"clock.10.6.speed", ResponseWave::Constant, ResponseBlend::Replace, 1, 0, 0, 8}};
            effect.responses().sequences.push_back(clock);
            effect.responses().rebuild();
            effect.setInput("test.speed", 1);
            for (int frame = 0; frame < 45; ++frame)
                effect.advance(1. / 60.);
            auto &playback = effect.authoring().composition(23);
            float before = playback.nodes[10].values[6].value();
            effect.setInput("test.speed", 8);
            effect.advance(1. / 60.);
            float accelerated = playback.nodes[10].values[6].value();
            check(playback.nodes[10].channels[6].speed == 8 &&
                  std::abs(accelerated - before) < .1f);
            effect.setInput("test.speed", 1);
            effect.advance(1. / 60.);
            check(playback.nodes[10].channels[6].speed == 1 &&
                  std::abs(playback.nodes[10].values[6].value() - accelerated) < .1f);
            auto targets = effect.responseParameters();
            for (auto &r : minotron.program().pulses)
                check(std::any_of(targets.begin(), targets.end(),
                                  [&](auto &t) { return t.key == r.target; }));
            std::cout
                << "PASS: Minotron cue timing, hold/fade, 8x ramp reset, shader "
                   "precedence/restoration, persistent local hue, audio brightness, pause/session "
                   "reset and scene targets.\n";
        }
        {
            std::ifstream file(assets / "bezels/SuperOxWars.neonrig");
            RigPlayer ox(RigProgram::read(file));
            ReadSnapshot sample;
            sample.game = "superoxwars";
            sample.session = 1;
            sample.values = {{"player.followers", int64_t(0)},
                             {"score.redShare", .5},
                             {"score.blueShare", .5},
                             {"world.tilt", 0.}};
            auto values = ox.sample(sample, 0, true, false);
            check(values.at("clock.20.6.speed") == 1 && values.at("colour.7.red") == 2.125f &&
                  values.at("colour.10.blue") == 2.125f && values.at("node.20.f.2") == 0);
            sample.values["player.followers"] = int64_t(5);
            sample.values["score.redShare"] = 1.;
            sample.values["score.blueShare"] = 0.;
            sample.values["world.tilt"] = 1.;
            values = ox.sample(sample, 1. / 60, true, false);
            check(values.at("clock.20.6.speed") == 4 && values.at("colour.7.red") == 3 &&
                  values.at("colour.10.blue") == 1.25f &&
                  std::abs(values.at("node.20.f.2") - .2f) < 1e-6);
            sample.values["world.tilt"] = .5;
            values = ox.sample(sample, 1. / 60, true, false);
            check(std::abs(values.at("node.20.f.2") - .1375f) < 1e-6);
            Runtime effect(assets / "neon");
            std::ifstream scene(assets / "bezels/SuperOxWars.neongles");
            effect.readScene(scene);
            auto targets = effect.responseParameters();
            for (const auto &rig : ox.program().values)
                check(std::any_of(targets.begin(), targets.end(),
                                  [&](const auto &target) { return target.key == rig.target; }));
            std::cout << "PASS: Super Ox Wars follower speed, score balance, eased tilt and native "
                         "scene targets.\n";
        }
        {
            std::ifstream file(assets / "bezels/FiveADay.neonrig");
            RigPlayer rig(RigProgram::read(file));
            ReadSnapshot snapshot;
            snapshot.game = "fiveaday";
            snapshot.session = 1;
            for (double rotation : {-3.14159265, -1.57079633, 0., 1.57079633, 3.14159265}) {
                snapshot.values["player.rotation"] = rotation;
                auto values = rig.sample(snapshot, 1. / 60., true, false);
                check(std::abs(values.at("node.8.f.9") - rotation) < 1e-6);
            }
            snapshot.values = {{"player.rotation", 0.},
                               {"levels.completed", uint64_t(0)},
                               {"minotaurs.rescued", uint64_t(0)},
                               {"fruit.trail", int64_t(0)}};
            ++snapshot.session;
            auto sample = [&](double dt) { return rig.sample(snapshot, dt, true, false); };
            auto start = sample(0);
            check(start.at("node.12.i.4") == 174 && start.at("colour.8.brightness") == 1);
            snapshot.values["fruit.trail"] = int64_t(10);
            check(sample(0).at("colour.8.brightness") == 1.5f);
            snapshot.values["fruit.trail"] = int64_t(71);
            check(sample(0).at("colour.8.brightness") == 2);
            snapshot.values["minotaurs.rescued"] = uint64_t(1);
            sample(0);
            check(std::abs(sample(.25).at("colour.8.hue") + 150) < .001);
            check(std::abs(sample(.75).at("colour.8.hue") - 120) < .001);
            snapshot.values["levels.completed"] = uint64_t(1);
            sample(0);
            float low = 255, high = 0;
            for (int frame = 0; frame < 90; ++frame) {
                auto values = sample(1. / 60.);
                low = std::min(low, values.at("node.12.i.4"));
                high = std::max(high, values.at("node.12.i.4"));
                if (frame == 20) {
                    auto paused = rig.sample(snapshot, 10, true, true);
                    check(paused == values);
                }
            }
            check(low < .5f && high > 254.5f &&
                  std::abs(sample(.001).at("node.12.i.4") - 174) < .001);
            Runtime effect(assets / "neon");
            std::ifstream scene(assets / "bezels/FiveADay.neongles");
            effect.readScene(scene);
            auto targets = effect.responseParameters();
            check(std::any_of(targets.begin(), targets.end(),
                              [](const auto &target) { return target.key == "node.8.f.9"; }));
            std::cout
                << "PASS: Five a Day player angle drives feedback second UV rotation in radians.\n";
        }
        {
            std::ifstream definitions(assets / "bezels/Gridrunner.neonrig");
            RigPlayer grid(RigProgram::read(definitions));
            Runtime effect(assets / "neon");
            std::ifstream document(assets / "bezels/Gridrunner.neongles");
            effect.readScene(document);
            check(effect.components().at(4).values[Pitch] == 0);
            Runtime gridSource(assets / "neon");
            std::istringstream sourceDocument(effect.components().at(4).layerScene);
            gridSource.readScene(sourceDocument);
            bool neutralSource = false;
            for (auto &control : gridSource.authoring().controls(gridSource.selectedPreset()))
                if (control.key == "camera.pitch")
                    neutralSource = !control.enabled || (control.value == 0 && control.motion == 0);
            check(neutralSource);
            auto targets = effect.responseParameters();
            auto requireTarget = [&](const auto &rig) {
                check(std::any_of(targets.begin(), targets.end(),
                                  [&](const auto &p) { return p.key == rig.target; }));
            };
            for (auto &r : grid.program().values)
                requireTarget(r);
            for (auto &r : grid.program().states)
                requireTarget(r);
            for (auto &r : grid.program().envelopes)
                requireTarget(r);
            for (auto &r : grid.program().pulses)
                requireTarget(r);
            ReadSnapshot sample;
            sample.game = "gridrunner";
            sample.session = 1;
            sample.values = {{"player.x", -.4},          {"player.y", -.228},
                             {"power.slow", false},      {"power.multishot", false},
                             {"power.allAround", false}, {"lives", int64_t(3)}};
            auto frame = [&](double dt) {
                ++sample.sequence;
                return grid.sample(sample, dt, true, false);
            };
            auto near = [](double a, double b) {
                if (std::abs(a - b) >= 1e-5)
                    throw std::runtime_error("Gridrunner expected " + std::to_string(b) + " got " +
                                             std::to_string(a));
            };
            auto first = frame(0);
            near(first.at("component.4.4"), -.2);
            near(first.at("component.4.5"), .3);
            near(first.at("layer.3/node.18.i.2"), 14);
            sample.values["player.x"] = .43;
            sample.values["player.y"] = .428;
            sample.values["power.slow"] = true;
            sample.values["power.multishot"] = true;
            auto powered = frame(0);
            near(powered.at("component.4.4"), .2);
            near(powered.at("component.4.5"), 0);
            near(powered.at("layer.3/clock.8.6.speed"), .5);
            near(powered.at("layer.3/node.18.i.2"), 13);
            sample.values["power.allAround"] = true;
            frame(0);
            near(frame(.25).at("layer.3/colour.18.brightness"), 1.5);
            near(frame(.25).at("layer.3/colour.18.brightness"), 2);
            near(frame(3).at("layer.3/colour.18.brightness"), 2);
            sample.values["power.allAround"] = false;
            near(frame(0).at("layer.3/colour.18.brightness"), 2);
            near(frame(.25).at("layer.3/colour.18.brightness"), 1.5);
            near(frame(.25).at("layer.3/colour.18.brightness"), 1);
            sample.values["power.allAround"] = true;
            frame(0);
            near(frame(.25).at("layer.3/colour.18.brightness"), 1.5);
            near(grid.sample(sample, 5, true, true).at("layer.3/colour.18.brightness"), 1.5);
            sample.values["power.allAround"] = false;
            near(frame(0).at("layer.3/colour.18.brightness"), 1.5);
            near(frame(.25).at("layer.3/colour.18.brightness"), 1.25);
            sample.values["power.allAround"] = true;
            near(frame(0).at("layer.3/colour.18.brightness"), 1.25);
            near(frame(.5).at("layer.3/colour.18.brightness"), 2);
            ++sample.session;
            sample.values["power.allAround"] = false;
            near(frame(0).at("layer.3/colour.18.brightness"), 1);
            sample.values["player.y"] = .1;
            near(frame(0).at("component.4.5"), .15);
            sample.values["lives"] = int64_t(4);
            frame(0);
            near(frame(.5).at("runtime.hue"), .5);
            near(frame(.5).at("runtime.hue"), 0);
            sample.values["power.slow"] = false;
            sample.values["power.multishot"] = false;
            auto reset = frame(0);
            near(reset.at("layer.3/clock.8.6.speed"), 1);
            near(reset.at("layer.3/node.18.i.2"), 14);
            std::cout << "PASS: Gridrunner source targets, independent plane endpoints, slow-clock "
                         "multiplier, shader restore, sustained all-around envelope and extra-life "
                         "hue turn.\n";
        }
        {
            std::ifstream definitions(assets / "bezels/GridrunnerClassic.neonrig");
            RigPlayer classic(RigProgram::read(definitions));
            check(classic.program().values.size() == 2 && classic.program().pulses.empty() &&
                  classic.program().counters.empty());
            ReadSnapshot sample;
            sample.game = "gridrunner-classic";
            sample.session = 1;
            sample.sequence = 1;
            sample.values = {{"player.x", 1.}, {"player.y", 8.}};
            auto first = classic.sample(sample, 0, true, false);
            check(std::abs(first.at("component.4.4") + .2) < 1e-5 &&
                  std::abs(first.at("component.4.5") - .3) < 1e-5);
            sample.sequence++;
            sample.values = {{"player.x", 21.9}, {"player.y", 18.9}};
            auto last = classic.sample(sample, 0, true, false);
            check(std::abs(last.at("component.4.4") - .2) < 1e-5 &&
                  std::abs(last.at("component.4.5")) < 1e-5);
            Runtime scene(assets / "neon");
            std::ifstream saved(assets / "bezels/GridrunnerClassic.neongles");
            scene.readScene(saved);
            check(std::abs(scene.parameters().hueShift - 300.f / 360) < 1e-6);
            std::cout
                << "PASS: Classic position endpoints and 300-degree hue; no enhanced-game cues.\n";
        }
        {
            for (const auto &path : std::filesystem::directory_iterator(assets / "bezels")) {
                if (path.path().extension() != ".nhml")
                    continue;
                std::ifstream named(path.path());
                auto source = RigProgram::read(named);
                auto legacyPath = path.path();
                legacyPath.replace_extension(".neonrig");
                if (!std::filesystem::exists(legacyPath))
                    continue;
                std::ifstream legacy(legacyPath);
                RigPlayer a(source), b(RigProgram::read(legacy));
                ReadSnapshot input;
                input.game = path.path().stem().string();
                input.session = 1;
                for (int frame = 0; frame < 180; ++frame) {
                    input.sequence++;
                    for (const auto &[name, decl] : source.inputs)
                        input.values[name] =
                            decl.type == "boolean"   ? ReadValue(frame >= 30 && frame < 90)
                            : decl.type == "integer" ? ReadValue(int64_t(frame / 30))
                                                     : ReadValue(double(frame % 60) / 59);
                    auto namedOutput = a.sample(input, 1. / 60, true, false);
                    auto legacyOutput = b.sample(input, 1. / 60, true, false);
                    check(namedOutput == legacyOutput);
                }
            }
            auto rejects = [](const std::string &text) {
                try {
                    std::istringstream input(text);
                    RigProgram::read(input);
                    return false;
                } catch (const std::exception &) {
                    return true;
                }
            };
            check(rejects("NHML 1\ninput x number units\nvalue missing x 0 1 0 1 clamp 0 1\n"));
            check(
                rejects("NHML 1\nparameter a runtime.hue turns\nparameter b runtime.hue turns\n"));
            check(rejects("NHML 1\ninput x potato units\n"));
            check(rejects(
                "NHML 1\ninput n number units\nparameter p runtime.hue turns\nindexed p n 1 0 0 0 1 1 0\n"));
            check(rejects(
                "NHML 1\ninput n integer units\nparameter p runtime.hue turns\nindexed p n 1 0 0 0 1 0\n"));
            check(rejects(
                "NHML 1\ninput n integer units\nparameter p runtime.hue turns\nindexed p n 1.5 0 0 0 1 1 0\n"));
            check(rejects(
                "NHML 1\ninput n integer units\nparameter p runtime.hue turns\nindexed p n 1 0 0 0 1 1 0\nvalue p n 0 1 0 1 clamp 0 1\n"));
            std::istringstream valid("NHML 1\ninput x number units\nparameter p runtime.hue "
                                     "turns\nvalue p x 0 1 0 1 clamp 0 1\n");
            RigPlayer typed(RigProgram::read(valid));
            ReadSnapshot input;
            input.game = "typed";
            input.session = 1;
            input.sequence = 1;
            input.values = {{"x", true}};
            check(typed.sample(input, 0, true, false).empty());
            input.values["x"] = .5;
            input.sequence++;
            check(typed.sample(input, 0, true, false).at("runtime.hue") == .5f);
            std::istringstream eventDefinition(
                "NHML 1\ninput health number ratio\ninput phase state enum\nparameter flash "
                "runtime.exposure gain\nevent hurt health decrease n:0.1 0 0\nevent danger health "
                "below n:0.25 0.02 0\nevent entered phase enter s:attack 0 0\npulse hurt flash 0.1 "
                "0.1 1 2\n");
            auto events = RigProgram::read(eventDefinition);
            check(events.events.size() == 3);
            RigPlayer eventPlayer(events);
            ReadSnapshot eventInput;
            eventInput.game = "event";
            eventInput.session = 1;
            eventInput.sequence = 1;
            eventInput.values = {{"health", 1.}, {"phase", std::string("idle")}};
            eventPlayer.sample(eventInput, 0, true, false);
            eventInput.sequence++;
            eventInput.values["health"] = .2;
            eventPlayer.sample(eventInput, 0, true, false);
            eventInput.sequence++;
            check(eventPlayer.sample(eventInput, .1, true, false).at("runtime.exposure") == 2.f);
            eventInput.sequence++;
            eventInput.values["health"] = .19;
            check(eventPlayer.sample(eventInput, .1, true, false).at("runtime.exposure") == 1.f);
            check(rejects("NHML 1\ninput x number ratio\nevent e missing below n:0.2 0 0\n"));
            check(rejects("NHML 1\ninput x number ratio\nevent e x below n:0.2 -1 0\n"));
            std::cout
                << "PASS: migrated NHML profiles preserve legacy cue semantics; unknown exports, "
                   "duplicate destinations and invalid input types are rejected.\n";
        }
        {
            std::ifstream minosDefinition(assets / "bezels/Minos.nhml");
            RigPlayer minos(RigProgram::read(minosDefinition));
            ReadSnapshot observation;
            observation.game = "minos";
            observation.session = 1;
            const int selectors[] = {23, 26, 28, 16, 12, 6};
            auto sampleLevel = [&](int level, double scroll) {
                ++observation.sequence;
                observation.values = {{"level.number", int64_t(level)}, {"screen.scroll", scroll}};
                return minos.sample(observation, 1. / 60, true, false);
            };
            for (int level : {1, 2, 3, 4, 5, 6, 7, 12, 13, 21, 49, 1, 19, 8}) {
                auto result = sampleLevel(level, .25);
                check(result.at("node.18.i.5") == selectors[(level - 1) % 6]);
                check(std::abs(result.at("runtime.hue") -
                               std::fmod(((level - 1) / 6) * 50. / 360., 1.)) < 1e-6);
                check(std::abs(result.at("node.18.f.7") - .25f) < 1e-6);
                std::ifstream continuedDefinition(assets / "bezels/Minos.nhml");
                RigPlayer continued(RigProgram::read(continuedDefinition));
                const auto &direct = continued.sample(observation, 0, true, false);
                check(direct == result);
            }
            auto down = sampleLevel(7, .75), up = sampleLevel(7, .5),
                 wrapped = sampleLevel(7, 1.25);
            check(down.at("node.18.f.7") == .75f && up.at("node.18.f.7") == .5f &&
                  wrapped.at("node.18.f.7") == .25f);
            observation.values["level.number"] = std::string("invalid");
            auto missing = minos.sample(observation, 0, true, false);
            check(missing.at("node.18.i.5") == 23 && missing.at("runtime.hue") == 0);
            check(minos.sample(observation, 0, false, false).empty());
            check(sampleLevel(13, 0).at("runtime.hue") > .2777f);
            std::cout
                << "PASS: Minos absolute texture/hue indexing, direct level jumps, reverse scroll and wrapping.\n";
        }
        {
            std::ifstream definition(assets / "bezels/Minos.nhml");
            RigPlayer player(RigProgram::read(definition));
            ReadSnapshot state;
            state.game = "minos";
            state.session = 1;
            state.values = {{"level.number", int64_t(1)},
                            {"screen.scroll", 0.},
                            {"minotaurs.rescued", int64_t(0)},
                            {"game.phase", int64_t(4)},
                            {"gravity.influence", 0.}};
            auto sample = [&](double dt, bool paused = false) {
                ++state.sequence;
                return player.sample(state, dt, true, paused);
            };
            auto near = [&](const auto &values, const char *key, double expected) {
                check(std::abs(values.at(key) - expected) < 1e-5);
            };
            constexpr double baseline = 1. / 1024;
            auto initial = sample(0);
            near(initial, "node.8.f.3", baseline);
            near(initial, "node.25.f.5", 0);
            near(initial, "node.25.f.6", .5);
            for (auto [input, output] : {std::pair{-1., .4},
                                         {-.3, .4},
                                         {-.15, .43125},
                                         {0., .5},
                                         {.15, .56875},
                                         {.3, .6},
                                         {1., .6}}) {
                state.values["gravity.influence"] = input;
                near(sample(0), "node.25.f.6", output);
            }
            state.values["minotaurs.rescued"] = int64_t(1);
            sample(0);
            near(sample(.25), "node.8.f.3", baseline + .5);
            near(sample(.25, true), "node.8.f.3", baseline + .5);
            near(sample(.25), "node.8.f.3", baseline + 1);
            near(sample(.5), "node.8.f.3", baseline);
            state.values["game.phase"] = int64_t(5);
            sample(0);
            near(sample(.5), "node.25.f.5", 1);
            near(sample(.5), "node.25.f.5", 2);
            near(sample(1), "node.25.f.5", 0);
            near(sample(1), "node.25.f.5", 0);
            std::ifstream continuedDefinition(assets / "bezels/Minos.nhml");
            RigPlayer continued(RigProgram::read(continuedDefinition));
            auto seeded = continued.sample(state, 0, true, false);
            near(seeded, "node.25.f.5", 0);
            near(seeded, "node.8.f.3", baseline);
            check(player.sample(state, 0, false, false).empty());
            Runtime scene(assets / "neon");
            std::ifstream source(assets / "bezels/Minos.neongles");
            scene.readScene(source);
            const int mode = scene.selectedPreset();
            bool axisFound = false;
            for (auto &field : scene.authoring().controls(mode))
                if (field.key == "node.25.f.5") {
                    check(field.maximum == 4);
                    field.enabled = true;
                    field.value = 3.5f;
                    axisFound = true;
                }
            check(axisFound);
            std::ostringstream saved;
            scene.writeScene(saved);
            Runtime restored(assets / "neon");
            std::istringstream document(saved.str());
            restored.readScene(document);
            bool retained = false;
            for (const auto &field : restored.authoring().controls(mode))
                if (field.key == "node.25.f.5")
                    retained = field.maximum == 4 && field.value == 3.5f;
            check(retained);
            std::cout << "PASS: Minos rescue cycle, paused playback, full completion turn, "
                         "capped/eased gravity and continued-game seeding.\n";
        }
        {
            std::ifstream file(assets / "bezels/GoatUp2.nhml");
            RigPlayer rig(RigProgram::read(file));
            ReadSnapshot state;
            state.game = "goatup2";
            state.session = 1;
            state.values = {{"player.hunger", 1.}, {"power.invincible", false}};
            state.collections["inventory.pickups"] = {};
            auto sample = [&](double dt = 0, bool paused = false) {
                ++state.sequence;
                return rig.sample(state, dt, true, paused);
            };
            auto equal = [&](const auto &values, const char *key, double expected) {
                check(std::abs(values.at(key) - expected) < 1e-5);
            };
            equal(sample(), "node.11.f.5", 1);
            check(rig.insertions().empty());
            for (auto [hunger, saturation] :
                 {std::pair{0., .25}, {.25, .625}, {.5, 1.}, {1., 1.}}) {
                state.values["player.hunger"] = hunger;
                auto values = sample();
                equal(values, "node.11.f.5", hunger);
                equal(values, "colour.10.saturation", saturation);
            }
            state.collections["inventory.pickups"] = {{2, 120, -1}, {1, 71, 3}};
            sample();
            check(rig.insertions().size() == 2);
            check(rig.insertions()[0].speed == 2 && rig.insertions()[0].colourSource == 3 &&
                  rig.insertions()[1].colourSource == -1);
            check(rig.insertions()[0].node == 24 && rig.insertions()[0].count == 8 &&
                  rig.insertions()[1].count == 8 &&
                  rig.insertions()[0].frame != rig.insertions()[1].frame);
            sample();
            check(rig.insertions().empty());
            state.collections["inventory.pickups"].push_back({3, 19});
            sample(.1, true);
            check(rig.insertions().empty());
            sample();
            check(rig.insertions().size() == 1);
            state.values["power.invincible"] = true;
            sample();
            auto active = sample(.25);
            equal(active, "node.32.f.4", .4);
            equal(active, "colour.31.green", .4);
            equal(active, "runtime.hue", .25);
            equal(sample(.25, true), "runtime.hue", .25);
            equal(sample(.75), "runtime.hue", 0);
            state.values["power.invincible"] = false;
            auto normal = sample();
            equal(normal, "colour.31.green", 1);
            equal(normal, "runtime.hue", 0);
            check(normal.at("node.32.f.4") < .14f);
            ++state.session;
            sample();
            check(rig.insertions().empty());
            check(rig.sample(state, 0, false, false).empty());
            std::cout
                << "PASS: GoatUp2 hunger/saturation, eight-per-item ordered insertions, duplicate suppression, pause, session seeding and invincibility restoration.\n";
        }
        {
            Runtime scene(assets / "neon");
            scene.loadSceneFile(assets / "bezels/Deflex.neongles");
            auto mode = scene.selectedPreset();
            auto settings = scene.authoring().authoredScene(mode);
            check(std::abs(settings.at(11).floats[4]) < 1e-6);
            std::ifstream file(assets / "bezels/Deflex.nhml");
            RigPlayer rig(RigProgram::read(file));
            ReadSnapshot state;
            state.game = "deflex";
            state.session = 1;
            state.values = {{"ball.shell", 1.}, {"level.number", int64_t(1)}};
            state.collections["ball.turns"] = {};
            auto sample = [&](double dt = 0, bool paused = false) {
                ++state.sequence;
                return rig.sample(state, dt, true, paused);
            };
            auto equal = [&](const auto &values, const char *key, double expected) {
                check(std::abs(values.at(key) - expected) < 1e-5);
            };
            const double quarter = 1.5707963267948966;
            equal(sample(), "node.11.f.4", 0);
            equal(sample(), "node.20.i.2", 12);
            equal(sample(), "node.18.f.4", .305299997);
            state.collections["ball.turns"] = {{2, -1}, {1, 1}};
            sample();
            equal(sample(.25), "node.11.f.4", quarter / 2);
            equal(sample(.5, true), "node.11.f.4", quarter / 2);
            equal(sample(.25), "node.11.f.4", quarter);
            equal(sample(.25), "node.11.f.4", quarter / 2);
            equal(sample(.25), "node.11.f.4", 0);
            equal(sample(1), "node.11.f.4", 0);
            state.values["ball.shell"] = 0.;
            equal(sample(), "node.18.f.4", .15);
            state.values["ball.shell"] = .5;
            equal(sample(), "node.18.f.4", (.15 + .305299997) / 2);
            state.values["level.number"] = int64_t(2);
            auto start = sample();
            equal(start, "node.20.i.2", 12);
            equal(start, "node.18.f.4", (.15 + .305299997) / 2);
            equal(sample(.075), "node.18.f.4", (.15 + .305299997) / 4);
            auto midpoint = sample(.075);
            equal(midpoint, "node.20.i.2", 13);
            equal(midpoint, "node.18.f.4", 0);
            equal(sample(.075, true), "node.18.f.4", 0);
            equal(sample(.075), "node.18.f.4", (.15 + .305299997) / 4);
            equal(sample(.075), "node.18.f.4", (.15 + .305299997) / 2);
            ++state.session;
            state.values["level.number"] = int64_t(11);
            equal(sample(), "node.20.i.2", 12);
            equal(sample(), "node.11.f.4", 0);
            check(rig.sample(state, 0, false, false).empty());
            std::cout
                << "PASS: Deflex ordered signed quarter turns, pause/retention, shell replenishment, absolute shader rotation and midpoint fade.\n";
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
