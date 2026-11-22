#pragma once
#include "read_bindings.h"
#include <deque>
#include <set>
namespace neon::effects {
enum class RigBoundary { Clamp, Wrap };
struct ValueRig {
    std::string target, field;
    double inputLow = 0, inputHigh = 1, outputLow = 0, outputHigh = 1, low = 0, high = 1;
    RigBoundary boundary = RigBoundary::Clamp;
    bool smooth = false;
    double evaluate(double) const;
};
// Indexed mappings derive their slot and completed loops from the current value.
struct IndexedRig {
    std::string target, field;
    double first = 1, baseline = 0, loopStep = 0, low = 0, high = 1;
    std::vector<double> entries;
    double evaluate(double) const;
};
struct PulseRig {
    std::string field, target;
    float attack = 1, hold = 0, decay = 1, baseline = 0, peak = 1;
    bool sweep = false, overlap = false, easeIn = false, stateTrigger = false;
};
// Counter-driven turns return to the baseline; advances retain each completed
// increment. Units are destination units (hue uses turns, UV rotation radians).
struct CounterRig {
    std::string field, target;
    double duration = 1, baseline = 0, increment = 1, low = 0, high = 1;
    bool retain = false, smooth = false, oscillate = false;
};
struct StateRig {
    struct Choice {
        std::string field;
        double minimum = 0;
        float value = 0;
    };
    std::string target;
    float baseline = 0;
    std::vector<Choice> choices;
};
// A state gate eases to its peak while active, then releases to the baseline.
struct StateEnvelopeRig {
    std::string field, target;
    float attack = .5f, decay = .5f, baseline = 1, peak = 2;
};
// A held state adds a repeating offset; releasing it removes that offset.
struct CycleRig {
    std::string field, target;
    double period = 2, increment = 1, baseline = 0;
};
// Ordered signed collection items become retained, sequential parameter steps.
struct StepRig {
    std::string collection, target;
    double duration = .5, baseline = 0, increment = 1, low = 0, high = 1;
    bool smooth = false;
};
// Absolute indexed choice changes only while its associated scalar is faded out.
struct FadeIndexRig {
    std::string target, field, mask;
    double first = 1, baseline = 0, down = .15, up = .15;
    std::vector<double> entries;
    double evaluate(double) const;
};
struct HarnessInput {
    std::string type, unit;
};
struct HarnessParameter {
    std::string target, unit;
};
struct SpriteInsertRig {
    std::string collection, emitter;
    unsigned count = 1;
    int node = -1;
    float speed = 1;
    bool followColours = false;
    std::map<int64_t, int> frames;
};
struct SpriteInsertAction {
    int node = -1, frame = 0;
    float speed = 1;
    int colourSource = -1;
    unsigned count = 1;
};
struct RigProgram {
    std::set<std::string> collections;
    std::map<std::string, int> emitters;
    std::map<std::string, std::pair<float, bool>> emitterStyles;
    std::vector<SpriteInsertRig> insertions;
    std::map<std::string, HarnessInput> inputs;
    std::map<std::string, HarnessParameter> parameters;
    std::vector<ReadRule> events;
    std::vector<ValueRig> values;
    std::vector<IndexedRig> indexed;
    std::vector<StepRig> steps;
    std::vector<FadeIndexRig> fadeIndices;
    std::vector<PulseRig> pulses;
    std::vector<CounterRig> counters;
    std::vector<StateRig> states;
    std::vector<CycleRig> cycles;
    std::vector<StateEnvelopeRig> envelopes;
    static RigProgram read(std::istream &);
};
// Host presentation time is independent of the synth's saved playback speed.
// Samples are read-only; transient outputs never replace authored baselines.
class RigPlayer {
  public:
    explicit RigPlayer(RigProgram);
    const std::map<std::string, float> &sample(const ReadSnapshot &, double elapsed, bool active,
                                               bool paused);
    const std::vector<SpriteInsertAction> &insertions() const {
        return insertions_;
    }
    const RigProgram &program() const {
        return program_;
    }

  private:
    void resetTimelines();
    RigProgram program_;
    std::vector<std::optional<uint64_t>> collectionHighWater_;
    std::vector<SpriteInsertAction> insertions_;
    ReadBindings reads_, eventReads_;
    std::map<std::string, uint64_t> eventCounts_;
    ResponseBank eventSink_;
    ResponseBank pulses_;
    std::map<std::string, float> outputs_;
    std::string game_;
    uint64_t session_ = 0;
    double seconds_ = 0;
    bool seeded_ = false;
    struct CounterState {
        std::optional<double> previous;
        double settled = 0;
        std::deque<double> starts;
    };
    std::vector<CounterState> counters_;
    struct StepState {
        std::optional<uint64_t> highWater;
        double settled = 0;
        std::deque<std::pair<double, double>> queue;
    };
    std::vector<StepState> steps_;
    struct FadeIndexState {
        double current = 0, pending = 0, start = 0;
        float from = 0, last = 0;
        bool ready = false, changing = false;
    };
    std::vector<FadeIndexState> fadeIndices_;
    struct PulseState {
        std::optional<double> previous;
        std::deque<double> starts;
    };
    std::vector<PulseState> pulseStates_;
    struct CycleState {
        double phase = 0;
        bool active = false;
    };
    std::vector<CycleState> cycleStates_;
    struct EnvelopeState {
        double start = 0;
        float from = 0, value = 0;
        bool active = false;
    };
    std::vector<EnvelopeState> envelopeStates_;
};
} // namespace neon::effects
