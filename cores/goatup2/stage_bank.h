#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace goatup2 {
// Original tagged stage data is game-specific, separate from Neon effect DNA.
constexpr std::size_t GridCells = 10000, PlacementCapacity = 200, BankCapacity = 170000,
                      StageCapacity = 8192;
enum class ObjectKind {
    Goat,
    Kid,
    BonusPickup,
    GravityDevice,
    PlatformPatroller,
    HorizontalFloater,
    VerticalFloater,
    RoamingFloater,
    Machine,
    Exit
};
constexpr std::array<int, 10> PlacementLimits = {1, 10, 99, 99, 99, 99, 99, 99, 99, 1};
struct ObjectRecord {
    int type = -1, variant = 0;
    // Reader 0x491b8 widens the custom float decimal reader 0xab00 to double.
    double x = 0, y = 0;
};
struct Stage {
    int declaredColumns = 0, declaredRows = 0, displayWidth = 0, displayHeight = 0;
    int seed = 0, highScore = 0, bestTime = 0;
    std::vector<uint32_t> cells = std::vector<uint32_t>(GridCells);
    std::size_t expandedCells = 0;
    // Original writes beyond the working grid into the adjacent backup grid.
    // Retain these writes explicitly; never overflow a C++ array or discard data.
    std::vector<uint32_t> backupPrefix;
    std::vector<ObjectRecord> objects;
};
struct Bank {
    uint32_t decoderRandomState = 0xf105513bU; // original global rand state after decoding
    // Original tags: bank identity, pure-run score, fastest full-run ticks, furthest ordinal.
    int gid = 0, pur = 0, tim = 0, fur = 0;
    std::array<Stage, 20> stages;
};
// EAGLView 0x4f48/0x5080/0x51d0: same transform encodes and decodes.
// Seed resets per call; incomplete final words remain unchanged.
std::vector<uint8_t> transformBank(const std::vector<uint8_t> &input,
                                   uint32_t *finalRandomState = nullptr);
// Bounded host readers; failure leaves output unchanged and describes the error.
bool readBank(const std::vector<uint8_t> &encoded, Bank &output, std::string &error);
bool readStage(std::string_view text, Stage &output, std::string &error);
// Host boundary for loaded/custom banks. The source format reader stays usable
// for research fragments; runtime rejects unsupported geometry/table indices.
bool validateRuntimeBank(const Bank &, std::string &error);

struct Placement {
    int type = -1;
    double x = 0, y = 0;
    uint32_t flags = 0;
    int serial = 0, state = 0;
};
// Translation of 0x52844: this creates template records, NOT live entities.
// Runtime entity construction is a separate original function, 0x4cf50.
class PlacementTable {
  public:
    void clear();
    bool place(const ObjectRecord &object, uint32_t extraFlags = 0);
    void load(const Stage &stage);
    const std::array<Placement, PlacementCapacity> &entries() const {
        return entries_;
    }
    int nextSerial() const {
        return serial_;
    }
    std::array<Placement, PlacementCapacity> &editableEntries() {
        return entries_;
    }

  private:
    std::array<Placement, PlacementCapacity> entries_{};
    int serial_ = 0;
};
} // namespace goatup2
