#pragma once
#include "stage_bank.h"
#include "neon/source_random.h"
#include "neon/math.h"
#include "terrain.h"
namespace goatup2 {
// Source editing mechanism; no gameplay actor objects or simulated touches.
bool serializeEditedStage(const Stage &, const PlacementTable &, std::string &, std::string &error,
                          const std::vector<uint32_t> *adjacentBackup = nullptr);
bool replaceEditedStage(const std::vector<uint8_t> &bank, int level, const std::string &stage,
                        std::vector<uint8_t> &result, std::string &error);
bool replaceEditedBankHeader(const std::vector<uint8_t> &, const Bank &, std::vector<uint8_t> &,
                             std::string &);
class EditorDocument {
  public:
    void open(const Stage &);
    const Stage &stage() const {
        return stage_;
    }
    Stage &settings() {
        return stage_;
    }
    PlacementTable placements;
    int brush = 0, style = 0, ink = 0, object = 0, variant = 0, transform = 0;
    std::string generatedName;
    bool sampled = false;
    uint32_t sampledTile = 0;
    bool paint(int x, int y);
    std::array<uint32_t, 3> previewCells() const;
    void sample(int x, int y);
    bool place(neon::vec2d);
    int selectNearest(neon::vec2d, bool deletion = false); // 0x51e20 float distances / slot ties
    bool moveSelected(neon::vec2d delta, TerrainBounds bounds);
    bool removeSelected(int state);
    void shift(int direction, TerrainBounds, neon::vec2d cellSize);
    void clear();
    void clearSelection();
    void category(int direction, bool objects);
    void variation(int direction, bool objects, bool secondary);
    bool commit(neon::DarwinRandom &, std::string &error,
                bool assignName = true); // name seed only if zero, resets records
    bool undo();                         // original eight-slot ring; no invented redo stack
    bool canUndo() const;
    const std::string &serialized() const {
        return history_[cursor_];
    }

  private:
    Stage stage_;
    std::vector<uint32_t> backup_;
    std::array<std::string, 8> history_;
    std::array<bool, 8> valid_{};
    unsigned cursor_ = 0;
    std::array<int, 12> styles_{}, inks_{};
    std::array<int, 10> variants_{}, transforms_{};
    bool writeCell(int, int, uint32_t);
};
} // namespace goatup2
