// VIC Gridrunner, translated independently from the iOS theGame routines.
// Standalone rules, cell display and sprite commands for the original Classic mode.
#pragma once
#include "native_random.h"
#include <array>
#include <cstdint>
#include <vector>

namespace gr::reconstruction::ios {
// stm's floating record is indexed by its collision cell, not a transient
// sprite list. A later integer poke replaces that cell, including its marker.
class VicScreen {
  public:
    static constexpr int cols = 23, rows = 21, stride = cols + 1;
    struct Floating {
        int background = 0x201, ink = 0;
        float x = 0, y = 0;
        double scaleX = 1, scaleY = 1;
        float angle = 0;
    };
    struct Glyph {
        double x, y;
        int ink;
        double scaleX = 1, scaleY = 1;
        float angle = 0;
    };
    inline static constexpr uint32_t palette[] = {
        0xffffffff, 0xffffffff, 0x782922ff, 0x87d6ddff, 0xaa5fb6ff, 0x55a049ff,
        0x40318dff, 0xbfce72ff, 0xaa7449ff, 0xeab489ff, 0xb86962ff, 0xc7ffffff,
        0xea9ff6ff, 0x94e089ff, 0x8071ccff, 0xffffb2ff}; // iOS 0x6a540
    void clear();
    void put(int x, int y, int ink);
    int get(int x, int y) const;
    void floating(float x, float y, int ink, int background = 0x201, double scale = 1,
                  float angle = 0);
    void stringAt(int x, int y, const char *text, int font, uint32_t clock = 0);
    void scoreAt(int x, int y, int score);
    void valueAt(int x, int y, int value);
    std::vector<Glyph> draw() const; // base cells first, then marked floats in cell order
    std::array<int32_t, stride *(rows + 1)> cells{};
    std::array<Floating, stride *(rows + 1)> floats{};
};

struct VicRules {
    int delay, count, body;
    float speed, zapSpeed;
};
VicRules vicRules(int grid);

class Classic {
  public:
    enum class State { Title = 1, Playing = 10, Dying = 11, BetweenGrids = 12, GameOver = 13 };
    struct Point {
        float x = -1, y = 0;
    };
    // Original VIC slot stride 0x1f0; body positions and directions are float.
    struct Pede {
        int length = 0;
        Point head;
        float speed = 0;
        std::array<float, 40> x{}, y{}, direction{};
    };
    explicit Classic(DarwinRandom *shared = nullptr);
    void title(int high = 0, uint32_t clock = 0);
    void start(int grid = 1);
    void tick(double dx, double dy, uint32_t clock);
    State state() const {
        return state_;
    }
    int score() const {
        return score_;
    }
    int lives() const {
        return lives_;
    }
    int grid() const {
        return grid_;
    }
    const VicScreen &screen() const {
        return screen_;
    }
    const std::vector<int> &sounds() const {
        return sounds_;
    }
    const std::vector<int> &achievements() const {
        return achievements_;
    }

  private:
    friend struct ClassicSourceChecks;
    void initOldGR();
    void status();
    void drawGrid();
    void between(bool completed);
    void runGR();
    bool doPedes();
    void splitSnake(int x, int y);
    void doBombs();
    void updateGrid();
    void doZapper();
    void doBull();
    void doShip(double dx, double dy);
    void explodeShip();
    void addScore(int amount);
    void sound(int kind);
    VicScreen screen_;
    VicRules rules_{};
    std::array<Pede, 40> pedes_{};
    std::array<Point, 50> bombs_{};
    Point ship_{11, 18}, bullet_;
    float zapY_ = 2, zapX_ = 1, zap_ = -1, zapTimer_ = 9, transition_ = 1;
    int grid_ = 1, lives_ = 5, score_ = 0, high_ = 0, remaining_ = 0, spawnTimer_ = 0,
        fireTimer_ = 2;
    uint32_t clock_ = 0;
    bool side_ = false;
    State state_ = State::Title;
    DarwinRandom ownRandom_{1};
    DarwinRandom *random_;
    std::vector<int> sounds_;
    std::vector<int> achievements_;
    uint32_t goalsDone_ = 0;
};
} // namespace gr::reconstruction::ios
