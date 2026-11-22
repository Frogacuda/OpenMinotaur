#include "live_rig.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
namespace neon::effects {
namespace {
void require(bool ok) {
    if (!ok)
        throw std::runtime_error("Invalid live bezel rig");
}
std::optional<double> number(const ReadValue &v) {
    return std::visit(
        [](const auto &x) -> std::optional<double> {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, std::string>)
                return {};
            else
                return double(x);
        },
        v);
}
} // namespace
double ValueRig::evaluate(double value) const {
    double t = (value - inputLow) / (inputHigh - inputLow);
    if (smooth) {
        t = std::clamp(t, 0., 1.);
        t = t * t * (3 - 2 * t);
    }
    double result = outputLow + t * (outputHigh - outputLow);
    if (boundary == RigBoundary::Wrap) {
        double phase = std::fmod(result - low, high - low);
        if (phase < 0)
            phase += high - low;
        return low + phase;
    }
    return std::clamp(result, low, high);
}
double IndexedRig::evaluate(double value) const {
    if (!std::isfinite(value) || value != std::floor(value) || value < first ||
        std::abs(value) > 9007199254740991.)
        return baseline;
    const double position = value - first;
    if (position > 9007199254740991.)
        return baseline;
    const double count = double(entries.size());
    const auto slot = size_t(std::fmod(position, count));
    const double result = entries[slot] + std::floor(position / count) * loopStep;
    if (!std::isfinite(result))
        return baseline;
    double phase = std::fmod(result - low, high - low);
    if (phase < 0)
        phase += high - low;
    return low + phase;
}
double FadeIndexRig::evaluate(double value) const {
    if (!std::isfinite(value) || value != std::floor(value) || value < first ||
        std::abs(value) > 9007199254740991.)
        return baseline;
    return entries[size_t(std::fmod(value - first, double(entries.size())))];
}
RigProgram RigProgram::read(std::istream &in) {
    RigProgram p;
    std::string magic;
    int version;
    require(bool(in >> magic >> version));
    const bool named = magic == "NHML" && version == 1;
    require(named || (magic == "NEONRIG" && (version == 1 || version == 2)));
    if (named)
        version = 2;
    std::string line;
    std::getline(in, line);
    std::set<std::string> targets;
    while (std::getline(in, line)) {
        auto comment = line.find('#');
        if (comment != std::string::npos)
            line.resize(comment);
        std::istringstream row(line);
        std::string kind;
        if (!(row >> kind))
            continue;
        if (named && kind == "collection") {
            std::string name;
            require(bool(row >> std::quoted(name)));
            row >> std::ws;
            require(row.eof() && !name.empty() && name.size() <= 128 &&
                    p.collections.insert(name).second && p.collections.size() <= 16);
            continue;
        }
        if (named && kind == "emitter") {
            std::string name;
            int node;
            require(bool(row >> std::quoted(name) >> node));
            row >> std::ws;
            float speed = 1;
            std::string colours = "none";
            if (!row.eof()) {
                require(bool(row >> speed >> colours));
                row >> std::ws;
            }
            require(std::isfinite(speed) && speed >= .25f && speed <= 8 &&
                    (colours == "none" || colours == "source"));
            require(row.eof() && !name.empty() && name.size() <= 128 && node >= 0 && node < 4096 &&
                    p.emitters.emplace(name, node).second && p.emitters.size() <= 16);
            p.emitterStyles.emplace(name, std::pair{speed, colours == "source"});
            continue;
        }
        if (named && kind == "insert") {
            SpriteInsertRig rule;
            size_t mappings;
            require(bool(row >> std::quoted(rule.collection) >> std::quoted(rule.emitter) >>
                         rule.count >> mappings));
            require(rule.count > 0 && rule.count <= 4096 && mappings > 0 && mappings <= 256);
            for (size_t n = 0; n < mappings; ++n) {
                int64_t source;
                int frame;
                require(bool(row >> source >> frame) && frame >= 0 && frame < 256 &&
                        rule.frames.emplace(source, frame).second);
            }
            row >> std::ws;
            require(row.eof() && p.insertions.size() < 16);
            p.insertions.push_back(std::move(rule));
            continue;
        }
        if (named && (kind == "input" || kind == "parameter")) {
            std::string name, typeOrTarget, unit;
            require(
                bool(row >> std::quoted(name) >> std::quoted(typeOrTarget) >> std::quoted(unit)));
            row >> std::ws;
            require(row.eof() && !name.empty() && name.size() <= 128 && !unit.empty() &&
                    unit.size() <= 64);
            if (kind == "input") {
                require(typeOrTarget == "number" || typeOrTarget == "integer" ||
                        typeOrTarget == "boolean" || typeOrTarget == "state");
                require(p.inputs.emplace(name, HarnessInput{typeOrTarget, unit}).second &&
                        p.inputs.size() <= 64);
            } else {
                require(
                    std::none_of(p.parameters.begin(), p.parameters.end(), [&](const auto &entry) {
                        return entry.second.target == typeOrTarget;
                    }));
                require(!typeOrTarget.empty() && typeOrTarget.size() <= 1024 &&
                        p.parameters.emplace(name, HarnessParameter{typeOrTarget, unit}).second &&
                        p.parameters.size() <= 64);
            }
            continue;
        }
        if (named && kind == "event") {
            ReadRule rule;
            std::string condition, value;
            require(bool(row >> std::quoted(rule.event) >> std::quoted(rule.field) >> condition >>
                         std::quoted(value) >> rule.hysteresis >> rule.cooldown));
            row >> std::ws;
            require(row.eof() && !rule.event.empty() && rule.event.size() <= 128);
            rule.name = rule.event;
            rule.value = parseReadValue(value);
            if (condition == "increase" || condition == "decrease") {
                rule.condition =
                    condition == "increase" ? ReadCondition::Increase : ReadCondition::Decrease;
                auto minimum = number(rule.value);
                require(minimum && std::isfinite(*minimum) && *minimum >= 0);
                rule.minimumChange = *minimum;
            } else if (condition == "above")
                rule.condition = ReadCondition::CrossAbove;
            else if (condition == "below")
                rule.condition = ReadCondition::CrossBelow;
            else if (condition == "changed")
                rule.condition = ReadCondition::Changed;
            else if (condition == "enter")
                rule.condition = ReadCondition::EnterState;
            else if (condition == "leave")
                rule.condition = ReadCondition::LeaveState;
            else
                require(false);
            require(p.events.size() < 32 &&
                    std::none_of(p.events.begin(), p.events.end(),
                                 [&](const auto &r) { return r.event == rule.event; }));
            p.events.push_back(rule);
            continue;
        }
        std::string target;
        if (named && kind == "steps") {
            StepRig r;
            std::string curve;
            require(bool(row >> std::quoted(r.collection) >> std::quoted(r.target) >> r.duration >>
                         r.baseline >> r.increment >> r.low >> r.high >> curve));
            for (double x : {r.duration, r.baseline, r.increment, r.low, r.high})
                require(std::isfinite(x));
            require(r.duration > 0 && r.low < r.high && r.baseline >= r.low &&
                    r.baseline < r.high && (curve == "linear" || curve == "smooth") &&
                    targets.insert(r.target).second);
            r.smooth = curve == "smooth";
            target = r.target;
            p.steps.push_back(std::move(r));
        } else if (named && kind == "fadeindex") {
            FadeIndexRig r;
            size_t count;
            require(bool(row >> std::quoted(r.target) >> std::quoted(r.field) >> r.first >>
                         r.baseline >> count));
            require(count > 0 && count <= 64 && std::isfinite(r.first) &&
                    r.first == std::floor(r.first) && std::abs(r.first) <= 4503599627370495.);
            auto shader = [](double x) {
                return std::isfinite(x) && x == std::floor(x) && x >= 0 && x <= 95;
            };
            require(shader(r.baseline));
            for (size_t i = 0; i < count; ++i) {
                double x;
                require(bool(row >> x) && shader(x));
                r.entries.push_back(x);
            }
            require(bool(row >> std::quoted(r.mask) >> r.down >> r.up));
            require(std::isfinite(r.down) && std::isfinite(r.up) && r.down > 0 && r.up > 0 &&
                    r.mask != r.target && targets.insert(r.target).second);
            target = r.target;
            p.fadeIndices.push_back(std::move(r));
        } else if (named && kind == "indexed") {
            IndexedRig r;
            size_t count;
            require(bool(row >> std::quoted(r.target) >> std::quoted(r.field) >> r.first >>
                         r.baseline >> r.loopStep >> r.low >> r.high >> count));
            require(count > 0 && count <= 64 && r.first == std::floor(r.first) &&
                    std::abs(r.first) <= 4503599627370495. && r.low < r.high &&
                    r.baseline >= r.low && r.baseline < r.high);
            for (double x : {r.first, r.baseline, r.loopStep, r.low, r.high})
                require(std::isfinite(x));
            for (size_t n = 0; n < count; ++n) {
                double value;
                require(bool(row >> value) && std::isfinite(value) && value >= r.low &&
                        value < r.high);
                r.entries.push_back(value);
            }
            target = r.target;
            require(targets.insert(target).second);
            p.indexed.push_back(std::move(r));
        } else if (kind == "value" || kind == "smoothvalue") {
            ValueRig r;
            r.smooth = kind == "smoothvalue";
            std::string boundary;
            require(bool(row >> std::quoted(r.target) >> std::quoted(r.field) >> r.inputLow >>
                         r.inputHigh >> r.outputLow >> r.outputHigh >> boundary >> r.low >>
                         r.high));
            for (double x : {r.inputLow, r.inputHigh, r.outputLow, r.outputHigh, r.low, r.high})
                require(std::isfinite(x));
            require(r.inputLow != r.inputHigh && r.low < r.high &&
                    (boundary == "wrap" || boundary == "clamp"));
            r.boundary = boundary == "wrap" ? RigBoundary::Wrap : RigBoundary::Clamp;
            target = r.target;
            p.values.push_back(r);
        } else if (kind == "pulse" || kind == "sweep" || kind == "envelope" ||
                   kind == "easesweep" ||
                   (version >= 2 && (kind == "pulsemax" || kind == "statepulse"))) {
            PulseRig r;
            r.sweep = kind == "sweep" || kind == "easesweep";
            r.easeIn = kind == "easesweep";
            r.overlap = kind == "pulsemax";
            r.stateTrigger = kind == "statepulse";
            require(bool(row >> std::quoted(r.field) >> std::quoted(r.target) >> r.attack));
            if (r.sweep)
                r.decay = 0;
            else {
                if (kind == "envelope")
                    require(bool(row >> r.hold));
                require(bool(row >> r.decay));
            }
            require(bool(row >> r.baseline >> r.peak));
            for (float x : {r.attack, r.hold, r.decay, r.baseline, r.peak})
                require(std::isfinite(x));
            require(r.attack > 0 && r.hold >= 0 && (r.sweep || r.decay > 0));
            require(!targets.count(r.target) || r.overlap);
            for (const auto &previous : p.pulses)
                require(previous.target != r.target || previous.baseline == r.baseline);
            target = r.target;
            p.pulses.push_back(r);
        } else if (kind == "advance" || kind == "turn" || kind == "oscillate") {
            CounterRig r;
            std::string curve;
            require(bool(row >> std::quoted(r.field) >> std::quoted(r.target) >> r.duration >>
                         r.baseline));
            r.oscillate = kind == "oscillate";
            if (!r.oscillate)
                require(bool(row >> r.increment));
            require(bool(row >> r.low >> r.high >> curve));
            require(!r.oscillate || (r.baseline >= r.low && r.baseline <= r.high));
            for (double x : {r.duration, r.baseline, r.increment, r.low, r.high})
                require(std::isfinite(x));
            require(r.duration > 0 && r.low < r.high && (curve == "linear" || curve == "smooth") &&
                    !targets.count(r.target));
            r.retain = kind == "advance";
            r.smooth = curve == "smooth";
            for (const auto &previous : p.counters)
                require(previous.target != r.target ||
                        (previous.baseline == r.baseline && previous.low == r.low &&
                         previous.high == r.high));
            target = r.target;
            p.counters.push_back(r);
        } else if (kind == "cycle" && version >= 2) {
            CycleRig r;
            require(bool(row >> std::quoted(r.field) >> std::quoted(r.target) >> r.period >>
                         r.increment));
            require(!r.field.empty() && !targets.count(r.target) && std::isfinite(r.period) &&
                    r.period > 0 && std::isfinite(r.increment));
            if (named)
                require(bool(row >> r.baseline) && std::isfinite(r.baseline));
            target = r.target;
            p.cycles.push_back(r);
        } else if (kind == "stateenvelope" && version >= 2) {
            StateEnvelopeRig r;
            require(bool(row >> std::quoted(r.field) >> std::quoted(r.target) >> r.attack >>
                         r.decay >> r.baseline >> r.peak));
            require(!r.field.empty() && !targets.count(r.target));
            for (float value : {r.attack, r.decay, r.baseline, r.peak})
                require(std::isfinite(value));
            require(r.attack > 0 && r.decay > 0);
            target = r.target;
            p.envelopes.push_back(r);
        } else if (kind == "select" && version >= 2) {
            StateRig r;
            size_t count;
            require(bool(row >> std::quoted(r.target) >> r.baseline >> count) &&
                    std::isfinite(r.baseline) && count > 0 && count <= 16 &&
                    !targets.count(r.target));
            for (size_t n = 0; n < count; ++n) {
                StateRig::Choice c;
                require(bool(row >> std::quoted(c.field) >> c.minimum >> c.value) &&
                        !c.field.empty() && std::isfinite(c.minimum) && std::isfinite(c.value));
                r.choices.push_back(c);
            }
            target = r.target;
            require(targets.insert(target).second);
            p.states.push_back(r);
        } else
            require(false);
        row >> std::ws;
        require(row.eof() && !target.empty() &&
                (kind != "value" || targets.insert(target).second) &&
                p.values.size() + p.pulses.size() + p.counters.size() + p.states.size() +
                        p.cycles.size() + p.envelopes.size() + p.indexed.size() + p.steps.size() +
                        p.fadeIndices.size() <=
                    32);
    }

    if (named) {
        for (auto &r : p.insertions) {
            require(p.collections.count(r.collection) && p.emitters.count(r.emitter));
            r.node = p.emitters.at(r.emitter);
            auto style = p.emitterStyles.at(r.emitter);
            r.speed = style.first;
            r.followColours = style.second;
        }
        for (const auto &event : p.events) {
            require(p.inputs.count(event.field) != 0 && !p.inputs.count(event.event));
            for (const auto &derived : p.events)
                require(event.field != derived.event);
        }
        for (const auto &event : p.events)
            require(p.inputs.emplace(event.event, HarnessInput{"integer", "count"}).second &&
                    p.inputs.size() <= 64);
        ReadBindings validate;
        validate.rules = p.events;
        validate.rebuild();
        auto bind = [&](auto &rule) {
            auto parameter = p.parameters.find(rule.target);
            require(parameter != p.parameters.end());
            require(p.inputs.count(rule.field) != 0);
            rule.target = parameter->second.target;
        };
        for (auto &r : p.values)
            bind(r);
        for (auto &r : p.indexed) {
            require(p.inputs.at(r.field).type == "integer");
            bind(r);
        }
        for (auto &r : p.steps) {
            require(p.collections.count(r.collection) && p.parameters.count(r.target));
            r.target = p.parameters.at(r.target).target;
        }
        std::set<std::string> masks;
        for (auto &r : p.fadeIndices) {
            require(p.inputs.count(r.field) && p.inputs.at(r.field).type == "integer" &&
                    p.parameters.count(r.mask) && masks.insert(r.mask).second &&
                    p.parameters.count(r.target) && p.parameters.at(r.target).unit == "shader-id");
            r.mask = p.parameters.at(r.mask).target;
            bind(r);
            require(std::any_of(p.values.begin(), p.values.end(),
                                [&](const auto &v) { return v.target == r.mask; }));
        }
        for (auto &r : p.pulses)
            bind(r);
        for (auto &r : p.counters)
            bind(r);
        for (auto &r : p.cycles)
            bind(r);
        for (auto &r : p.envelopes)
            bind(r);
        for (auto &r : p.states) {
            auto parameter = p.parameters.find(r.target);
            require(parameter != p.parameters.end());
            r.target = parameter->second.target;
            for (const auto &choice : r.choices)
                require(p.inputs.count(choice.field) != 0);
        }
        targets.clear();
        for (const auto &r : p.values)
            require(targets.insert(r.target).second);
        for (const auto &r : p.states)
            require(targets.insert(r.target).second);
        for (const auto &r : p.indexed)
            require(targets.insert(r.target).second);
        for (const auto &r : p.steps)
            require(targets.insert(r.target).second);
        for (const auto &r : p.fadeIndices)
            require(targets.insert(r.target).second);
    }
    for (const auto &r : p.counters) {
        require(!r.field.empty() && !targets.count(r.target));
        for (const auto &pulse : p.pulses)
            require(pulse.target != r.target || pulse.baseline == r.baseline);
    }
    return p;
}
RigPlayer::RigPlayer(RigProgram p) : program_(std::move(p)) {
    eventReads_.rules = program_.events;
    counters_.resize(program_.counters.size());
    steps_.resize(program_.steps.size());
    fadeIndices_.resize(program_.fadeIndices.size());
    pulseStates_.resize(program_.pulses.size());
    cycleStates_.resize(program_.cycles.size());
    envelopeStates_.resize(program_.envelopes.size());
    for (size_t i = 0; i < program_.pulses.size(); ++i) {
        const auto &r = program_.pulses[i];
        auto event = "rig.pulse." + std::to_string(i);
        ReadRule rule;
        rule.field = r.field;
        rule.event = event;
        rule.minimumChange = 1;
        rule.condition = r.stateTrigger ? ReadCondition::EnterState : ReadCondition::Increase;
        if (r.stateTrigger)
            rule.value = true;
        reads_.rules.push_back(rule);
        ResponseSequence s;
        s.name = event;
        s.source = event;
        s.attack = r.attack;
        s.hold = r.hold;
        s.decay = r.decay;
        s.attackCurve = r.easeIn  ? ResponseCurve::EaseIn
                        : r.sweep ? ResponseCurve::Linear
                                  : ResponseCurve::Smooth;
        s.decayCurve = ResponseCurve::Smooth;
        s.targets = {
            {r.target, ResponseWave::Constant, ResponseBlend::Replace, 1, 0, r.peak, r.peak}};
        pulses_.sequences.push_back(s);
    }
    reads_.rebuild();
    pulses_.rebuild();
}
void RigPlayer::resetTimelines() {
    seconds_ = 0;
    reads_.reset();
    eventReads_.reset();
    eventCounts_.clear();
    pulses_.reset();
    outputs_.clear();
    collectionHighWater_.assign(program_.insertions.size(), {});
    counters_.assign(program_.counters.size(), {});
    steps_.assign(program_.steps.size(), {});
    fadeIndices_.assign(program_.fadeIndices.size(), {});
    pulseStates_.assign(program_.pulses.size(), {});
    cycleStates_.assign(program_.cycles.size(), {});
    envelopeStates_.assign(program_.envelopes.size(), {});
}
const std::map<std::string, float> &RigPlayer::sample(const ReadSnapshot &source, double dt,
                                                      bool active, bool paused) {
    require(std::isfinite(dt) && dt >= 0);
    insertions_.clear();
    std::optional<ReadSnapshot> checked;
    if (!program_.inputs.empty()) {
        checked = source;
        for (const auto &[name, decl] : program_.inputs) {
            auto found = checked->values.find(name);
            if (found == checked->values.end())
                continue;
            auto n = number(found->second);
            bool valid = decl.type == "boolean" ? std::holds_alternative<bool>(found->second)
                         : decl.type == "state"
                             ? std::holds_alternative<std::string>(found->second) &&
                                   !std::get<std::string>(found->second).empty() &&
                                   std::get<std::string>(found->second).size() <= 128
                             : n && std::isfinite(*n) &&
                                   !std::holds_alternative<bool>(found->second) &&
                                   (decl.type != "integer" ||
                                    std::holds_alternative<int64_t>(found->second) ||
                                    std::holds_alternative<uint64_t>(found->second) ||
                                    (std::floor(*n) == *n && std::abs(*n) <= 9007199254740991.));
            if (!valid)
                checked->values.erase(found);
        }
    }
    const auto &snapshot = checked ? *checked : source;
    if (!active) {
        seeded_ = false;
        resetTimelines();
        return outputs_;
    }
    if (!seeded_ || game_ != snapshot.game || session_ != snapshot.session) {
        game_ = snapshot.game;
        session_ = snapshot.session;
        seeded_ = true;
        resetTimelines();
    }
    if (paused)
        return outputs_;
    for (size_t n = 0; n < program_.insertions.size(); ++n) {
        const auto &rule = program_.insertions[n];
        auto found = snapshot.collections.find(rule.collection);
        if (found == snapshot.collections.end()) {
            collectionHighWater_[n].reset();
            continue;
        }
        require(found->second.size() <= 4096);
        auto items = found->second;
        std::sort(items.begin(), items.end(),
                  [](const auto &a, const auto &b) { return a.id < b.id; });
        auto &high = collectionHighWater_[n];
        if (!high) {
            high = items.empty() ? 0 : items.back().id;
            continue;
        }
        for (const auto &item : items)
            if (item.id > *high) {
                auto frame = rule.frames.find(item.value);
                if (frame != rule.frames.end())
                    insertions_.push_back({rule.node, frame->second, rule.speed,
                                           rule.followColours ? item.colourSource : -1,
                                           rule.count});
                high = item.id;
            }
    }
    seconds_ += dt;
    if (!program_.events.empty()) {
        auto report = eventReads_.observe(snapshot, eventSink_, seconds_);
        for (const auto &event : report.events)
            ++eventCounts_[event.event];
        for (const auto &event : program_.events)
            checked->values[event.event] = eventCounts_[event.event];
    }
    reads_.observe(snapshot, pulses_, seconds_);
    outputs_.clear();
    for (const auto &r : program_.values) {
        auto v = snapshot.values.find(r.field);
        if (v != snapshot.values.end())
            if (auto x = number(v->second); x && std::isfinite(*x))
                outputs_[r.target] = float(r.evaluate(*x));
    }
    for (const auto &r : program_.indexed) {
        auto v = snapshot.values.find(r.field);
        auto x = v == snapshot.values.end() ? std::optional<double>{} : number(v->second);
        outputs_[r.target] = float(x ? r.evaluate(*x) : r.baseline);
    }
    for (const auto &r : program_.states) {
        float value = r.baseline;
        for (const auto &c : r.choices) {
            auto found = snapshot.values.find(c.field);
            if (found == snapshot.values.end())
                continue;
            auto x = number(found->second);
            if (x && *x >= c.minimum) {
                value = c.value;
                break;
            }
        }
        outputs_[r.target] = value;
    }
    for (size_t i = 0; i < program_.envelopes.size(); ++i) {
        const auto &r = program_.envelopes[i];
        auto &state = envelopeStates_[i];
        auto sample = snapshot.values.find(r.field);
        auto value =
            sample == snapshot.values.end() ? std::optional<double>{} : number(sample->second);
        const bool gateActive = value && std::isfinite(*value) && *value > 0;
        auto evaluate = [&] {
            double t =
                std::clamp((seconds_ - state.start) / (state.active ? r.attack : r.decay), 0., 1.);
            float smooth = float(t * t * (3 - 2 * t));
            return state.from + ((state.active ? 1.f : 0.f) - state.from) * smooth;
        };
        state.value = evaluate();
        if (gateActive != state.active) {
            state.from = state.value;
            state.start = seconds_;
            state.active = gateActive;
        }
        auto output = outputs_.try_emplace(r.target, r.baseline).first;
        output->second += (r.peak - r.baseline) * state.value;
    }
    // Cues sharing a destination add their offsets from a common baseline.
    // A complete hue turn returns to the same visible color without a decay.
    for (size_t i = 0; i < program_.pulses.size(); ++i) {
        const auto &r = program_.pulses[i];
        auto [value, inserted] = outputs_.try_emplace(r.target, r.baseline);
        float envelope = pulses_.envelope(i, seconds_);
        if (r.overlap) {
            auto &state = pulseStates_[i];
            auto found = snapshot.values.find(r.field);
            auto current =
                found == snapshot.values.end() ? std::optional<double>{} : number(found->second);
            if (current && std::isfinite(*current)) {
                if (state.previous && *current > *state.previous) {
                    if (state.starts.size() == 128)
                        state.starts.pop_front();
                    state.starts.push_back(seconds_);
                }
                state.previous = current;
            } else
                state.previous.reset();
            const double duration = double(r.attack) + r.hold + r.decay;
            while (!state.starts.empty() && seconds_ >= state.starts.front() + duration)
                state.starts.pop_front();
            envelope = 0;
            auto smooth = [](double t) { return float(t * t * (3 - 2 * t)); };
            for (double start : state.starts) {
                double t = seconds_ - start;
                float weight = t < r.attack ? smooth(t / r.attack)
                               : t < double(r.attack) + r.hold
                                   ? 1
                                   : 1 - smooth((t - r.attack - r.hold) / r.decay);
                envelope = std::max(envelope, weight);
            }
        }
        if (r.overlap) {
            float peak = r.baseline + (r.peak - r.baseline) * envelope;
            value->second = r.peak >= r.baseline ? std::max(value->second, peak)
                                                 : std::min(value->second, peak);
        } else
            value->second += (r.peak - r.baseline) * envelope;
    }
    for (size_t i = 0; i < program_.counters.size(); ++i) {
        const auto &r = program_.counters[i];
        auto &state = counters_[i];
        auto found = snapshot.values.find(r.field);
        if (found == snapshot.values.end()) {
            state.previous.reset();
        } else if (auto current = number(found->second); current && std::isfinite(*current)) {
            if (state.previous && *current > *state.previous) {
                double count = std::floor(*current - *state.previous);
                require(count <= 4096 && state.starts.size() + size_t(count) <= 4096);
                for (size_t event = 0; event < size_t(count); ++event)
                    state.starts.push_back(
                        state.starts.empty()
                            ? seconds_
                            : std::max(seconds_, state.starts.back() + r.duration));
            }
            state.previous = *current;
        } else {
            state.previous.reset();
        }
        while (!state.starts.empty() && seconds_ >= state.starts.front() + r.duration) {
            if (r.retain) {
                state.settled += r.increment;
                state.settled = std::fmod(state.settled, r.high - r.low);
            }
            state.starts.pop_front();
        }
        double offset = state.settled;
        if (!state.starts.empty()) {
            double t = std::clamp((seconds_ - state.starts.front()) / r.duration, 0., 1.);
            if (r.smooth)
                t = t * t * (3 - 2 * t);
            if (r.oscillate) {
                double half = (r.high - r.low) * .5, middle = (r.high + r.low) * .5;
                double phase = std::asin(std::clamp((r.baseline - middle) / half, -1., 1.));
                offset += middle + half * std::sin(phase + 6.283185307179586 * t) - r.baseline;
            } else
                offset += r.increment * t;
        }
        auto [value, inserted] = outputs_.try_emplace(r.target, float(r.baseline));
        value->second += float(offset);
    }
    for (size_t i = 0; i < program_.cycles.size(); ++i) {
        const auto &r = program_.cycles[i];
        auto &state = cycleStates_[i];
        auto found = snapshot.values.find(r.field);
        auto value =
            found == snapshot.values.end() ? std::optional<double>{} : number(found->second);
        const bool gateActive = value && std::isfinite(*value) && *value > 0;
        if (!gateActive)
            state = {};
        else {
            if (state.active)
                state.phase = std::fmod(state.phase + dt / r.period, 1.);
            state.active = true;
        }
        outputs_.try_emplace(r.target, float(r.baseline)).first->second +=
            float(state.phase * r.increment);
    }
    for (const auto &r : program_.counters) {
        auto &value = outputs_.at(r.target);
        if (r.oscillate) {
            value = float(std::clamp(double(value), r.low, r.high));
            continue;
        }
        double phase = std::fmod(double(value) - r.low, r.high - r.low);
        value = float(r.low + (phase < 0 ? phase + r.high - r.low : phase));
    }
    for (size_t i = 0; i < program_.steps.size(); ++i) {
        const auto &r = program_.steps[i];
        auto &state = steps_[i];
        auto found = snapshot.collections.find(r.collection);
        if (found == snapshot.collections.end())
            state.highWater.reset();
        else {
            require(found->second.size() <= 4096);
            auto items = found->second;
            std::sort(items.begin(), items.end(),
                      [](const auto &a, const auto &b) { return a.id < b.id; });
            if (!state.highWater)
                state.highWater = items.empty() ? 0 : items.back().id;
            else
                for (const auto &item : items)
                    if (item.id > *state.highWater) {
                        require(item.value >= -1 && item.value <= 1 && state.queue.size() < 4096);
                        if (item.value)
                            state.queue.emplace_back(
                                state.queue.empty()
                                    ? seconds_
                                    : std::max(seconds_, state.queue.back().first + r.duration),
                                item.value * r.increment);
                        state.highWater = item.id;
                    }
        }
        while (!state.queue.empty() && seconds_ >= state.queue.front().first + r.duration) {
            state.settled = std::fmod(state.settled + state.queue.front().second, r.high - r.low);
            state.queue.pop_front();
        }
        double value = r.baseline + state.settled;
        if (!state.queue.empty()) {
            double t = std::clamp((seconds_ - state.queue.front().first) / r.duration, 0., 1.);
            if (r.smooth)
                t = t * t * (3 - 2 * t);
            value += state.queue.front().second * t;
        }
        double phase = std::fmod(value - r.low, r.high - r.low);
        outputs_[r.target] = float(r.low + (phase < 0 ? phase + r.high - r.low : phase));
    }
    for (size_t i = 0; i < program_.fadeIndices.size(); ++i) {
        const auto &r = program_.fadeIndices[i];
        auto &state = fadeIndices_[i];
        auto found = snapshot.values.find(r.field);
        auto x = found == snapshot.values.end() ? std::optional<double>{} : number(found->second);
        const double desired = x ? r.evaluate(*x) : r.baseline;
        auto mask = outputs_.find(r.mask);
        if (mask == outputs_.end())
            continue;
        const float underlying = mask->second;
        auto evaluate = [&] {
            if (!state.changing)
                return underlying;
            double t = seconds_ - state.start;
            if (t >= r.down)
                state.current = state.pending;
            if (t >= r.down + r.up) {
                state.changing = false;
                return underlying;
            }
            double u = t < r.down ? t / r.down : (t - r.down) / r.up;
            float smooth = float(u * u * (3 - 2 * u));
            return t < r.down ? state.from * (1 - smooth) : underlying * smooth;
        };
        float value = evaluate();
        if (!state.ready) {
            state.current = state.pending = desired;
            state.ready = true;
        } else if (desired != (state.changing ? state.pending : state.current)) {
            state.from = state.changing ? value : state.last;
            state.start = seconds_;
            state.pending = desired;
            state.changing = true;
            value = state.from;
        }
        state.last = value;
        outputs_[r.target] = float(state.current);
        mask->second = value;
    }
    return outputs_;
}
} // namespace neon::effects
