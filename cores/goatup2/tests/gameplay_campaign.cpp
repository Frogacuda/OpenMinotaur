// Scripted campaign smoke traces with normal death and actor callbacks enabled.
#include "stage_setup.h"
#include "stage_controls.h"
#include "stage_presentation.h"
#include "stage_session.h"
#include "campaign_actors.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
using namespace goatup2;
namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
size_t nodes(StageActor *a) {
    size_t n = 0;
    for (; a; a = a->next) {
        require(std::isfinite(a->stagePosition.x) && std::isfinite(a->stagePosition.y),
                "Nonfinite actor position");
        require(++n < 20000, "Actor list cycle or runaway spawning");
    }
    return n;
}
} // namespace
int main(int argc, char **argv) {
    try {
        require(argc == 2 || argc == 3, "Pass assets directory and optional trace output");
        const auto assets = std::filesystem::path(argv[1]);
        std::ifstream xml(assets / "NeonResources.xml");
        require(bool(xml), "Missing resource catalogue");
        neon::ResourceCatalog resources;
        std::string error;
        require(resources.load({std::istreambuf_iterator<char>(xml), {}}, &error),
                "Invalid resource catalogue");
        std::ofstream trace;
        if (argc == 3) {
            trace.open(argv[2]);
            require(bool(trace), "Cannot write campaign trace");
            trace
                << "bank stage route tick state x y hunger score lives followers fruit enemies effects\n";
            trace.precision(17);
        }
        size_t frames = 0, deaths = 0, restarts = 0, farts = 0, fruit = 0, exits = 0;
        std::set<std::pair<int, int>> variants;
        for (int bankIndex = 0; bankIndex < 4; ++bankIndex) {
            std::ifstream in(assets / ("DEFAULT_" + std::to_string(bankIndex)), std::ios::binary);
            require(bool(in), "Missing campaign bank");
            Bank bank;
            require(readBank({std::istreambuf_iterator<char>(in), {}}, bank, error),
                    "Invalid campaign bank");
            for (int level = 0; level < 20; ++level) {
                const auto &stage = bank.stages[size_t(level)];
                require(stage.seed != 0, "Unexpected empty shipped stage");
                for (int route = 0; route < 2; ++route) {
                    auto loaded = loadOriginalStage(stage, level, resources, {0, 0, 0, false, 0},
                                                    {0, 0, 0, 8, true}, bankIndex == 0);
                    auto &w = *loaded.world;
                    require(!w.deathDisabled, "Campaign audit must retain death");
                    StagePresentation presentation;
                    presentation.attach(w, stage.displayWidth, stage.displayHeight);
                    StageControls controls;
                    controls.beginStage();
                    StageSession session;
                    StageHud hud;
                    session.beginRun(w, level);
                    neon::input::ControllerState pad;
                    pad.connected = true;
                    for (const auto &o : stage.objects)
                        if (o.type >= 2)
                            variants.insert({o.type, o.variant & 255});
                    for (uint32_t tick = 0; tick < 900; ++tick) {
                        const int oldState = w.goat->state, oldFruit = w.requiredFruit;
                        w.random.next();
                        w.sourceTime = float(double(w.sourceTime) + .04);
                        w.beginLogicalFrame();
                        w.advanceCurrents();
                        session.prepareOuterFrame(w, hud, tick);
                        presentation.requestCamera(w);
                        // Two repeatable movement/jump schedules; neither grants
                        // energy, invulnerability, lives or completion states.
                        const unsigned phase = tick % (route ? 180U : 240U);
                        pad.lx = phase < (route ? 90U : 120U) ? 1.f : -1.f;
                        pad.buttons = tick % (route ? 43U : 67U) < 12U ? neon::input::South : 0;
                        controls.pads(&pad, 1);
                        const auto intent = controls.tick(w, w.goat->state == 0);
                        updateStagePalette(w, tick);
                        w.runLocomotion(intent.movement, intent.jump, tick, uint8_t(tick));
                        presentation.prepare(w);
                        ++frames;
                        deaths += oldState == 0 && w.goat->state == 1;
                        restarts += oldState == 3 && w.goat->state == 0;
                        farts += w.headbuttEvent;
                        fruit +=
                            oldFruit > w.requiredFruit ? size_t(oldFruit - w.requiredFruit) : 0;
                        exits += oldState == 0 && w.goat->state == 2;
                        require(std::isfinite(w.goat->hunger) &&
                                    std::isfinite(w.goat->stagePosition.x) &&
                                    std::isfinite(w.goat->stagePosition.y),
                                "Nonfinite player state");
                        const auto enemies = nodes(w.enemies);
                        const auto effects = nodes(w.ordinaryEffects) + nodes(w.foregroundEffects);
                        nodes(w.contactObjects);
                        nodes(w.grassBonuses);
                        nodes(w.overlayEffects);
                        if (trace && (tick % 60 == 0 || oldState != w.goat->state))
                            trace << bankIndex << ' ' << level << ' ' << route << ' ' << tick << ' '
                                  << w.goat->state << ' ' << w.goat->stagePosition.x << ' '
                                  << w.goat->stagePosition.y << ' ' << w.goat->hunger << ' '
                                  << w.score << ' ' << w.lives << ' ' << w.goat->followingCount
                                  << ' ' << w.requiredFruit << ' ' << enemies << ' ' << effects
                                  << '\n';
                    }
                }
            }
        }
        require(frames == 144000 && deaths > 0 && restarts > 0 && farts > 0 && fruit > 0,
                "Campaign scripts did not exercise the required live paths");
        std::cout << "80 stages, 160 scripted routes, " << frames << " frames; " << variants.size()
                  << " placed object variants; deaths=" << deaths << " restarts=" << restarts
                  << " farts=" << farts << " fruit=" << fruit << " exits=" << exits << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
