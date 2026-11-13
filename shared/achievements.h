#pragma once
#include <set>
#include <string>
#include <utility>

namespace mino {
// Collection lifetime records only. Game rules and per-run gates stay in cores.
// Keys are permanent identities, independent of display names or online services.
class Achievements {
  public:
    using Key = std::pair<std::string, std::string>; // catalog game ID, local ID
    bool load(const std::string &path);
    bool unlock(const std::string &game, const std::string &id);
    bool contains(const std::string &game, const std::string &id) const;
    bool save(const std::string &path);
    bool dirty() const {
        return dirty_;
    }
    const std::set<Key> &entries() const {
        return entries_;
    }

  private:
    std::set<Key> entries_;
    bool dirty_ = false, writable_ = true;
};
} // namespace mino
