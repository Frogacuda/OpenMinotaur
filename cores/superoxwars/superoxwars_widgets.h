#pragma once
#include "core_api.h"
#include "original_tables.h"
#include "pickups.h"
#include "records.h"
#include "scene.h"
#include <algorithm>

namespace superoxwars {
struct WidgetCourse {
    int blue, red, bossRow;
};

// Baked from the terrain scripts, including their mirror switches and odd-row
// actors. The existing terrain checks compare these totals with the native scanner.
inline constexpr std::array<WidgetCourse, Scene::SupportedAreas> WidgetCourses{{
    {17, 17, 242},
    {34, 34, 349},
    {11, 13, 334},
    {23, 23, 490},
    {61, 61, 328},
    {64, 64, 572},
    {129, 129, 579},
    {43, 43, 498},
    {148, 148, 674},
    {148, 112, 1330},
}};

inline mino::PresentationState widgetSnapshot(const World &world, const Scene &scene,
                                              const Records &records, uint64_t session,
                                              bool paused) {
    if (world.demo || !world.player || world.player->state == Ship::State::GameOver)
        return {};
    const auto &course = WidgetCourses.at(size_t(world.area));
    const auto index = size_t(world.level);
    const auto *best = index < records.starts.size() ? &records.starts[index] : nullptr;
    const double row = scene.terrain.worldRow + world.tiles.scroll.y;
    const double progress = scene.bossPhase ? 1. : std::clamp(row / course.bossRow, 0., 1.);
    mino::PresentationState state{
        true,
        paused,
        session,
        {{"highScore", int64_t(records.pureBest)},
         {"area", std::string(original::AreaNames.at(size_t(world.area)))},
         {"blueBest", best ? int64_t(best->blue) : -1},
         {"redBest", best ? int64_t(best->red) : -1},
         {"savedLives", best ? int64_t(best->lives) : -1},
         {"blueCleared",
          int64_t(std::clamp(world.blueTotal - world.blueRemaining, 0, course.blue))},
         {"redCleared", int64_t(std::clamp(world.redTotal - world.redRemaining, 0, course.red))},
         {"blueTotal", int64_t(course.blue)},
         {"redTotal", int64_t(course.red)},
         {"polarity", std::clamp(double(world.polarity), -1., 1.)},
         {"formationChain", int64_t(world.chain)},
         {"oxCombo", int64_t(world.oxStreak)},
         {"oxWindow", std::clamp(world.oxTimer / 60., 0., 1.)},
         {"areaProgress", progress},
         {"areaPhase", std::string(scene.bonusState  ? "Bonus"
                                   : scene.bossPhase ? "Boss"
                                                     : "To Boss")}},
        {},
        {}};
    if (world.player->state == Ship::State::Flying && world.player->power > 0) {
        auto &followers = state.collections["followers"];
        for (const auto *entity = world.lists[size_t(Domain::Decor)]; entity;
             entity = entity->next) {
            const auto *follower = dynamic_cast<const Follower *>(entity);
            if (follower && !follower->dead && follower->lifetime > 0 && follower->slot >= 0 &&
                follower->slot < 5)
                followers.push_back(
                    {uint64_t(follower->slot),
                     int64_t(std::clamp(double(follower->lifetime), 0., 1.) * 10000),
                     follower->frame == 28 ? 0 : 1});
        }
    }
    return state;
}
} // namespace superoxwars
