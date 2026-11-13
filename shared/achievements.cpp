#include "achievements.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace mino {
namespace {
bool valid(const std::string &s) {
    if (s.empty() || s.size() > 96)
        return false;
    for (unsigned char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.'))
            return false;
    return true;
}
} // namespace
bool Achievements::load(const std::string &path) {
    entries_.clear();
    dirty_ = false;
    writable_ = false;
    const auto file = std::filesystem::u8path(path);
    std::error_code ec;
    const bool exists = std::filesystem::exists(file, ec);
    if (ec)
        return false;
    if (!exists) {
        writable_ = true;
        return true;
    }
    std::ifstream in(file);
    std::string line;
    if (!std::getline(in, line))
        return false;
    if (!line.empty() && line.back() == '\r')
        line.pop_back(); // Windows saves on other hosts.
    if (line != "minotaur-achievements 1")
        return false;
    std::set<Key> loaded;
    while (std::getline(in, line)) {
        std::istringstream row(line);
        std::string game, id, extra;
        if (!(row >> game >> id) || (row >> extra) || !valid(game) || !valid(id))
            return false;
        loaded.emplace(game, id);
    }
    if (!in.eof() || in.bad())
        return false;
    entries_ = std::move(loaded);
    writable_ = true;
    return true;
}
bool Achievements::unlock(const std::string &game, const std::string &id) {
    if (!valid(game) || !valid(id))
        return false;
    const bool added = entries_.emplace(game, id).second;
    dirty_ |= added;
    return added;
}
bool Achievements::contains(const std::string &game, const std::string &id) const {
    return entries_.count({game, id}) != 0;
}
bool Achievements::save(const std::string &path) {
    if (!writable_)
        return false; // Never overwrite a damaged or newer-version file.
    if (!dirty_)
        return true;
    const auto file = std::filesystem::u8path(path);
    auto tmp = file;
    tmp += ".tmp";
    std::ofstream out(tmp, std::ios::trunc);
    out << "minotaur-achievements 1\n";
    for (const auto &key : entries_)
        out << key.first << ' ' << key.second << '\n';
    out.close();
    if (!out)
        return false;
#ifdef _WIN32
    if (!MoveFileExW(tmp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return false;
#else
    std::error_code ec;
    std::filesystem::rename(tmp, file, ec);
    if (ec)
        return false;
#endif
    dirty_ = false;
    return true;
}
} // namespace mino
