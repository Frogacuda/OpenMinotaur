// highscores.h - local high score tables shared by the game cores (standing
// in for the OpenFeint / Game Center leaderboards of the iOS originals).
//
// A core owns one HighScores: some number of boards, each a top-10 list of
// (name, score, level), plus the last name entered. Stored as a small text
// file in the game's save folder:
//     name <last name>
//     <board> <rank> <score> <level> <name>
#pragma once

#include <string>
#include <vector>

namespace mino {

class HighScores {
  public:
    static constexpr int kRows = 10;
    static constexpr int kNameLen = 10;

    struct Entry {
        std::string name;
        int score = 0;
        int level = 0;
    };

    // `boards` tables; `defaultNames` (cycled) and a score ladder fill them
    // until something is loaded.
    void init(int boards, const std::vector<std::string> &defaultNames, int topScore, int step);

    bool load(const std::string &path);
    bool save(const std::string &path); // writes path.tmp, then renames

    int boards() const {
        return int(tables_.size());
    }
    const Entry &entry(int board, int rank) const {
        return tables_[size_t(board)][size_t(rank)];
    }
    Entry &at(int board, int rank) {
        return tables_[size_t(board)][size_t(rank)];
    }
    bool qualifies(int board, int score) const;
    // Inserts (below equal scores); returns the rank, or -1 if it didn't make it.
    int insert(int board, const std::string &name, int score, int level);

    std::string lastName;
    bool dirty = false; // changed since the last save

  private:
    std::vector<std::vector<Entry>> tables_;
};

} // namespace mino
