// Original iOS campaign rules, independent of presentation and platform APIs.
#pragma once
#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <istream>
#include <ostream>

namespace gridrunner {
enum class GameMode { Pure, Casual, Survival };
inline const char *modeName(GameMode mode) {
    return mode == GameMode::Survival ? "SURVIVAL" : mode == GameMode::Casual ? "CASUAL" : "PURE";
}
struct Checkpoint {
    int score = 0, lives = 0;
};
class CampaignProgress {
  public:
    // The original save has 200 score/life pairs, one per four-grid boundary.
    // This is save-table capacity, not a limit on gameplay progression.
    static constexpr int kSlots = 200;
    CampaignProgress() {
        checkpoints_[0] = {0, 3};
    }
    int highestLevel() const {
        return highest_ * 4;
    }
    Checkpoint at(int level) const {
        return checkpoints_[size_t(std::clamp(level / 4, 0, highest_))];
    }
    bool reached(int level, int score, int lives) {
        if (level <= 0 || (level & 3) || level / 4 >= kSlots || score < 0 || lives < 1)
            return false;
        const int slot = level / 4;
        if (slot > highest_ + 1)
            return false; // never create gaps that cannot be restored
        auto &saved = checkpoints_[size_t(slot)];
        bool changed = false;
        // Preserve the source comparison, including the comparison with the
        // INITIAL life count (slot zero), rather than inventing a new ranking.
        if (lives > saved.lives || (lives == checkpoints_[0].lives && score > saved.score)) {
            saved = {score, lives};
            changed = true;
        }
        if (slot > highest_) {
            highest_ = slot;
            changed = true;
        }
        return changed;
    }
    bool read(std::istream &in) {
        CampaignProgress next;
        std::string tag;
        int version = 0;
        if (!(in >> tag >> version >> next.highest_) || tag != "GRIDRUNNER_CHECKPOINTS" ||
            version != 1 || next.highest_ < 0 || next.highest_ >= kSlots)
            return false;
        for (int i = 0; i <= next.highest_; ++i) {
            auto &c = next.checkpoints_[size_t(i)];
            if (!(in >> c.score >> c.lives) || c.score < 0 || c.lives < 1 || c.lives > 10000)
                return false;
        }
        if (next.checkpoints_[0].score != 0 || next.checkpoints_[0].lives != 3)
            return false;
        *this = next;
        return true;
    }
    void write(std::ostream &out) const {
        out << "GRIDRUNNER_CHECKPOINTS 1 " << highest_ << '\n';
        for (int i = 0; i <= highest_; ++i)
            out << checkpoints_[size_t(i)].score << ' ' << checkpoints_[size_t(i)].lives << '\n';
    }
    bool load(const std::string &path) {
        std::ifstream in(path);
        if (read(in))
            return true;
        std::ifstream backup(path + ".bak");
        return read(backup);
    }
    bool save(const std::string &path) const {
        const std::string temp = path + ".tmp", backup = path + ".bak";
        std::ofstream out(temp);
        write(out);
        out.close();
        if (!out)
            return false;
        if (std::rename(temp.c_str(), path.c_str()) == 0)
            return true;
        // Windows rename cannot replace an existing destination. Keep the last
        // valid file recoverable until its replacement has been committed.
        std::remove(backup.c_str());
        if (std::rename(path.c_str(), backup.c_str()) != 0)
            return false;
        if (std::rename(temp.c_str(), path.c_str()) == 0) {
            std::remove(backup.c_str());
            return true;
        }
        std::rename(backup.c_str(), path.c_str());
        return false;
    }

  private:
    std::array<Checkpoint, kSlots> checkpoints_{};
    int highest_ = 0;
};
} // namespace gridrunner
