// iOS local achievement messages. Independent of online achievement services.
#pragma once
#include "cores/gridrunner/message_data.h"
#include <array>
#include <vector>
#include <string>
#include <fstream>
#include <cstdint>
#include <algorithm>
namespace gridrunner_ef107 {
namespace messageData = ::gridrunner::messageData;
// The original run mask lives in Campaign/Classic; this is the lifetime record.
// Stable goal IDs can later be forwarded to a platform service without making
// either game simulation or local notifications depend on that service.
struct AchievementRecord {
    uint32_t unlocked = 0;
    bool dirty = false;
    void load(const std::string &path) {
        std::ifstream in(path);
        uint32_t version = 0, bits = 0;
        if (in >> version >> bits && version == 1)
            unlocked = bits & 0x7ff;
    }
    void record(int goal) {
        if (goal >= 0 && goal < 11) {
            const auto prior = unlocked;
            unlocked |= 1u << goal;
            dirty |= prior != unlocked;
        }
    }
    bool save(const std::string &path) {
        if (!dirty)
            return true;
        std::ofstream out(path);
        out << 1 << ' ' << unlocked << '\n';
        out.close();
        if (!out)
            return false;
        dirty = false;
        return true;
    }
};
struct MessageGlyph {
    double x, y, w, h;
    int atlasX, atlasY, width;
    uint32_t colour;
};
class GoalMessages {
  public:
    struct Line {
        std::string text;
        double y = 0, fromY = 0, targetY = 0, exitY = 0;
        double sx = 0, sy = 0;
        float amount = 0, alpha = 0, colourPhase = 0;
        int state = 0, hold = 4;
        bool active() const {
            return !text.empty();
        }
        void update() {
            // proportionalLine::update 0x35f40. Thumb 0x3605e IT LS
            // guards retirement; the C export loses that condition.
            colourPhase += 1.f;
            if (state == 1) {
                alpha = 1;
                if (hold > 0 && --hold < 1) {
                    amount = 1;
                    state = 2;
                }
                return;
            }
            if (state != 0 && state != 2)
                return;
            const double t = double(amount), target = state == 0 ? fromY : exitY;
            y = target + (targetY - target) * t;
            const double factor = state == 0 ? 4. : .8;
            sx = 1.2 * factor + (1.2 - 1.2 * factor) * t;
            sy = 2.4 * factor + (2.4 - 2.4 * factor) * t;
            amount += state == 0 ? .01f : -.02f;
            if (state == 0 && amount >= 1) {
                amount = 1;
                state = 1;
            }
            if (state == 2 && amount <= 0) {
                amount = 0;
                text.clear();
            }
            alpha = amount;
        }
    };
    void clear() {
        lines_ = {};
        glyphs_.clear();
    }
    void show(int goal) {
        if (goal < 0 || goal >= 11)
            return;
        // achieveGoal 0x3f1b0 alternates cellGrid slots 11 and 12.
        const bool second = lines_[0].active();
        auto &line = lines_[second ? 1 : 0];
        line.text = messageData::goals[goal];
        line.y = second ? .1 : 0.;
        line.exitY = line.y;
        line.fromY = second ? double(-.55f) : -.5;
        line.targetY = second ? double(.35f) : double(.4f);
        line.amount = line.alpha = line.colourPhase = 0;
        line.state = 0;
        line.hold = 4;
    }
    // Draw samples precede update, as in cellGrid::propLine 0x35930.
    // Called once per logical frame; host redraws consume only glyphs().
    void prepare(bool paused = false) {
        glyphs_.clear();
        for (auto &line : lines_)
            if (line.active()) {
                const double sx = line.state == 1 ? 1.2 : line.sx,
                             sy = line.state == 1 ? 2.4 : line.sy;
                int total = 0;
                for (unsigned char c : line.text)
                    total += messageData::glyphs[c][1];
                const float average = float(total) / float(line.text.size());
                const float extent = float(sx * double(float(total) + 1.f));
                // Source centring includes half an unscaled average glyph width.
                double x = -double(float(average * .5f + extent * .5f));
                float phase = line.colourPhase;
                for (unsigned char c : line.text) {
                    const int width = messageData::glyphs[c][1], atlas = messageData::glyphs[c][0];
                    const uint32_t rgba = messageData::colours[size_t(int(phase) % 15)];
                    if (atlas >= 0)
                        glyphs_.push_back(
                            {x / 512., line.y - sy * 9. / 512., sx * width / 512., sy * 18. / 512.,
                             atlas, messageData::rows[c >> 6], width,
                             (rgba & 0xffffff00u) | uint32_t(float(rgba & 255) * line.alpha)});
                    x += sx * double(width) + sx;
                    phase += .5f;
                }
                if (!paused)
                    line.update();
            }
    }
    const std::vector<MessageGlyph> &glyphs() const {
        return glyphs_;
    }
    const std::array<Line, 2> &lines() const {
        return lines_;
    }

  private:
    std::array<Line, 2> lines_{};
    std::vector<MessageGlyph> glyphs_;
};
} // namespace gridrunner_ef107
