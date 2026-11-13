#pragma once
#include <cstdint>
#include <iosfwd>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
namespace neon::effects {
enum class ResponseMode { Event, Input };
enum class ResponseWave { Constant, Sine, Triangle, Ramp, Square };
enum class ResponseCurve { Linear, Smooth, EaseIn, EaseOut };
enum class ResponseBlend { Replace, Add, Multiply };
struct ResponseTarget {
    std::string key;
    ResponseWave wave = ResponseWave::Constant;
    ResponseBlend blend = ResponseBlend::Replace;
    float frequency = 1, phase = 0, low = 0, high = 1;
};
struct ResponseSequence {
    std::string name = "New response", source = "death";
    ResponseMode mode = ResponseMode::Event;
    bool enabled = true, ignoreRetrigger = false;
    float attack = .1f, hold = .1f, decay = .6f;
    ResponseCurve attackCurve = ResponseCurve::Smooth, decayCurve = ResponseCurve::Smooth;
    float inputLow = 0, inputHigh = 1;
    std::vector<ResponseTarget> targets;
};
struct ResponseParameter {
    std::string key, label;
    float low = 0, high = 1, baseline = 0;
};
// CPU-only, caller-owned clock. Definitions persist; input values and active voices do not.
// Mutate sequences then call rebuild(). Evaluation never advances a clock or allocates.
class ResponseBank {
  public:
    std::vector<ResponseSequence> sequences;
    uint64_t revision = 0;
    void rebuild();
    void reset();
    bool trigger(const std::string &, double seconds, float strength = 1);
    void setInput(const std::string &, float value);
    void clearInput(const std::string &);
    bool targets(const std::string &) const;
    float apply(const std::string &, float baseline, double seconds, float minimum,
                float maximum) const;
    float envelope(size_t, double seconds) const;
    float input(const std::string &, float fallback = 0) const;
    void write(std::ostream &) const;
    static ResponseBank read(std::istream &);

  private:
    struct Voice {
        double start = -1;
        float strength = 1;
    };
    std::vector<Voice> voices_;
    std::map<std::string, float> inputs_;
    std::unordered_map<std::string, std::vector<std::pair<size_t, size_t>>> routes_;
};
void writeResponseBanks(std::ostream &, const std::map<int, ResponseBank> &);
std::map<int, ResponseBank> readResponseBanks(std::istream &);
} // namespace neon::effects
