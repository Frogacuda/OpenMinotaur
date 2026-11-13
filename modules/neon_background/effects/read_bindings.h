#pragma once
#include "response_animation.h"
#include <optional>
#include <variant>
namespace neon::effects {
// Keep counters exact until conversion to a visual modulation value is requested.
using ReadValue = std::variant<int64_t, uint64_t, double, bool, std::string>;
struct ReadSnapshot {
    std::string game;
    uint64_t session = 0, sequence = 0;
    std::map<std::string, ReadValue> values; // full snapshot: omitted means unavailable
};
class ReadSource {
  public:
    virtual ~ReadSource() = default;
    virtual ReadSnapshot read() const = 0; // supplied by host/adapter; no game dependency here
};
enum class ReadMapping { Raw, Normalize, StateEquals };
struct ReadInput {
    std::string field = "player.x", input = "player.x";
    bool enabled = true;
    ReadMapping mapping = ReadMapping::Raw;
    ReadValue low = 0.0, high = 1.0, state = true;
};
enum class ReadCondition {
    CrossAbove,
    CrossBelow,
    Increase,
    Decrease,
    Changed,
    EnterState,
    LeaveState
};
struct ReadRule {
    std::string name = "Counter increase", field = "deaths", event = "death";
    bool enabled = true;
    ReadCondition condition = ReadCondition::Increase;
    ReadValue value = uint64_t(1);
    double minimumChange = 0, hysteresis = 0, cooldown = 0;
    float strength = 1;
};
struct ReadEvent {
    std::string rule, event;
    bool responseTriggered = false;
};
struct ReadReport {
    bool applied = false, baseline = false;
    std::string status;
    std::vector<std::string> unavailable;
    std::vector<ReadEvent> events;
};
class ReadBindings {
  public:
    std::vector<ReadInput> inputs;
    std::vector<ReadRule> rules;
    void rebuild(); // definitions changed: next snapshot seeds fresh baselines
    void reset();
    ReadReport observe(const ReadSnapshot &, ResponseBank &, double presentationSeconds);
    const std::optional<ReadSnapshot> &latest() const {
        return latest_;
    }
    void write(std::ostream &) const;
    static ReadBindings read(std::istream &);

  private:
    struct RuleState {
        std::optional<ReadValue> previous;
        bool predicate = false;
        double lastEvent = -1;
    };
    std::vector<RuleState> state_;
    std::optional<ReadSnapshot> latest_;
    bool reseed_ = true;
};
std::string formatReadValue(const ReadValue &);
ReadValue
parseReadValue(const std::string &); // i: signed, u: unsigned, n: number, b: bool, s: state
void writeReadBanks(std::ostream &, const std::map<int, ReadBindings> &);
std::map<int, ReadBindings> readReadBanks(std::istream &);
} // namespace neon::effects
