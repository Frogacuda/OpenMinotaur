// highscores.cpp - see highscores.h.
#include "highscores.h"

#include <cstdio>
#include <cstring>

namespace mino {

void HighScores::init(int boards, const std::vector<std::string> &names, int topScore, int step) {
    tables_.assign(size_t(boards), std::vector<Entry>(kRows));
    for (int b = 0; b < boards; b++)
        for (int i = 0; i < kRows; i++) {
            Entry &e = tables_[size_t(b)][size_t(i)];
            e.name =
                names.empty() ? std::string("MINOTAUR") : names[size_t(b * 5 + i) % names.size()];
            e.score = topScore - i * step;
            e.level = kRows - i;
        }
}

bool HighScores::load(const std::string &path) {
    FILE *f = std::fopen(path.c_str(), "r");
    if (!f)
        return false;
    char line[160];
    while (std::fgets(line, sizeof line, f)) {
        line[std::strcspn(line, "\r\n")] = 0;
        if (std::strncmp(line, "name ", 5) == 0) {
            lastName = std::string(line + 5).substr(0, kNameLen);
            continue;
        }
        int b, r, score, level, n = 0;
        if (std::sscanf(line, "%d %d %d %d %n", &b, &r, &score, &level, &n) != 4 || n == 0)
            continue;
        if (b < 0 || b >= boards() || r < 0 || r >= kRows || !line[n])
            continue;
        Entry &e = tables_[size_t(b)][size_t(r)];
        e.name = std::string(line + n).substr(0, kNameLen);
        e.score = score;
        e.level = level;
    }
    std::fclose(f);
    return true;
}

bool HighScores::save(const std::string &path) {
    dirty = false;
    std::string tmp = path + ".tmp";
    FILE *f = std::fopen(tmp.c_str(), "w");
    if (!f)
        return false;
    std::fprintf(f, "name %s\n", lastName.c_str());
    for (int b = 0; b < boards(); b++)
        for (int r = 0; r < kRows; r++) {
            const Entry &e = entry(b, r);
            std::fprintf(f, "%d %d %d %d %s\n", b, r, e.score, e.level, e.name.c_str());
        }
    if (std::fclose(f) != 0)
        return false;
    std::remove(path.c_str());
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

bool HighScores::qualifies(int board, int score) const {
    return board >= 0 && board < boards() && score > 0 && score > entry(board, kRows - 1).score;
}

int HighScores::insert(int board, const std::string &name, int score, int level) {
    if (board < 0 || board >= boards())
        return -1;
    std::vector<Entry> &t = tables_[size_t(board)];
    int rank = 0;
    while (rank < kRows && t[size_t(rank)].score >= score)
        rank++;
    if (rank >= kRows)
        return -1;
    t.insert(t.begin() + rank, Entry{name.substr(0, kNameLen), score, level});
    t.pop_back();
    dirty = true;
    return rank;
}

} // namespace mino
