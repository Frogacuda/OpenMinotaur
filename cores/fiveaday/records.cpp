#include "records.h"
#include <filesystem>
#include <fstream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace fiveaday {
int Records::unlocked() const {
    int count = 1;
    while (count < int(days.size()) && days[size_t(count)].lives > 0)
        ++count;
    return count;
}
bool Records::checkpoint(int day, int lives, int score) {
    if (day < 0 || day >= int(days.size()))
        return false;
    auto &d = days[size_t(day)];
    if (lives < d.lives || (lives == d.lives && score <= d.score))
        return false;
    d = {lives, score};
    dirty = true;
    return true;
}
bool Records::load(const std::string &path) {
    std::ifstream in(std::filesystem::u8path(path));
    if (!in)
        return true;
    Records candidate;
    std::string magic;
    int version = 0;
    if (!(in >> magic >> version) || magic != "FIVEADAY" || version != 1) {
        writable = false;
        return false;
    }
    if (!(in >> candidate.highScore >> candidate.fruit >> candidate.minotaurs >> candidate.planes >>
          candidate.tilt)) {
        writable = false;
        return false;
    }
    for (auto &day : candidate.days)
        if (!(in >> day.lives >> day.score) || day.lives < 0 || day.lives > 9 || day.score < 0) {
            writable = false;
            return false;
        }
    if (candidate.days[0].lives < 1 || candidate.highScore < 0 || candidate.fruit < 0 ||
        candidate.minotaurs < 0 || candidate.planes < 0) {
        writable = false;
        return false;
    }
    in >> std::ws;
    if (!in.eof()) {
        writable = false;
        return false;
    }
    *this = candidate;
    return true;
}
bool Records::save(const std::string &path) {
    if (!writable || !dirty || path.empty())
        return writable;
    auto target = std::filesystem::u8path(path), temp = target;
    temp += ".tmp";
    std::ofstream out(temp, std::ios::trunc);
    if (!out)
        return false;
    out << "FIVEADAY 1\n"
        << highScore << ' ' << fruit << ' ' << minotaurs << ' ' << planes << ' ' << tilt << '\n';
    for (const auto &d : days)
        out << d.lives << ' ' << d.score << '\n';
    out.close();
    if (!out)
        return false;
#ifdef _WIN32
    if (!MoveFileExW(temp.c_str(), target.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return false;
#else
    std::error_code error;
    std::filesystem::rename(temp, target, error);
    if (error)
        return false;
#endif
    dirty = false;
    return true;
}
} // namespace fiveaday
