#include "records.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
namespace superoxwars {
void Records::checkpoint(const World &w, int level) {
    if (w.demo || w.lives <= 0 || level < 1 || level >= int(starts.size()))
        return;
    unlocked = std::max(unlocked, level);
    auto &p = starts[size_t(level)];
    // 48b1c: lower-life revisits do not improve the saved starting scores.
    if (w.lives >= p.lives) {
        p.lives = w.lives;
        p.blue = std::max(p.blue, w.blueScore);
        p.red = std::max(p.red, w.redScore);
    }
}
void Records::captureTotals(const World &w) {
    // Collection counters live in the original settings block; their source
    // writers also run during attract play. Achievement awards remain gated.
    blueCollected = std::max(blueCollected, w.blueCollected);
    redCollected = std::max(redCollected, w.redCollected);
    if (!w.demo)
        achievements |= w.sessionAchievements;
}
void Records::bank(const World &w, bool fromBeginning) {
    captureTotals(w);
    if (w.demo)
        return;
    blueBest = std::max(blueBest, w.blueScore);
    redBest = std::max(redBest, w.redScore);
    if (fromBeginning)
        pureBest = std::max(pureBest, w.score);
    achievements |= w.sessionAchievements;
}
bool Records::load(const std::string &path) {
    if (path.empty())
        return true;
    std::ifstream in(std::filesystem::u8path(path));
    if (!in)
        return !std::filesystem::exists(std::filesystem::u8path(path));
    Records r;
    std::string magic;
    int version;
    if (!(in >> magic >> version >> r.unlocked >> r.blueBest >> r.redBest >> r.pureBest >>
          r.blueCollected >> r.redCollected >> r.achievements) ||
        magic != "SOW-RECORDS" || (version != 1 && version != 2))
        return false;
    if (version >= 2 && (!(in >> r.touchMode) || r.touchMode < 0 || r.touchMode > 1))
        return false;
    if (r.unlocked < 0 || r.unlocked >= int(r.starts.size()) || r.blueBest < 0 || r.redBest < 0 ||
        r.pureBest < 0 || r.blueCollected < 0 || r.redCollected < 0 || (r.achievements & ~0x3ffu))
        return false;
    for (auto &p : r.starts)
        if (!(in >> p.blue >> p.red >> p.lives) || p.blue < 0 || p.red < 0 || p.lives < 1 ||
            int64_t(p.blue) + p.red > std::numeric_limits<int>::max())
            return false;
    std::string extension;
    if (in >> extension) {
        int enabled;
        if (extension != "TILT" || !(in >> enabled) || (enabled != 0 && enabled != 1))
            return false;
        r.tiltEnabled = enabled != 0;
    }
    *this = r;
    return true;
}
bool Records::save(const std::string &path) const {
    if (path.empty())
        return true;
    const auto target = std::filesystem::u8path(path);
    auto temporary = target;
    temporary += ".tmp";
    {
        std::ofstream out(temporary);
        out << "SOW-RECORDS 2\n"
            << unlocked << ' ' << blueBest << ' ' << redBest << ' ' << pureBest << ' '
            << blueCollected << ' ' << redCollected << ' ' << achievements << ' ' << touchMode
            << '\n';
        for (const auto &p : starts)
            out << p.blue << ' ' << p.red << ' ' << p.lives << '\n';
        // Optional tail keeps existing score/checkpoint readers compatible.
        out << "TILT " << int(tiltEnabled) << '\n';
        out.flush();
        if (!out)
            return false;
    }
    // Keep the previous file until the new record has been fully written.
    std::error_code error;
    std::filesystem::copy_file(temporary, target, std::filesystem::copy_options::overwrite_existing,
                               error);
    if (error)
        return false;
    std::filesystem::remove(temporary, error);
    return true;
}
} // namespace superoxwars
