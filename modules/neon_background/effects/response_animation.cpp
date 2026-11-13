#include "response_animation.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <istream>
#include <ostream>
#include <stdexcept>
namespace neon::effects {
namespace {
void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
bool text(const std::string &s, size_t max) {
    return !s.empty() && s.size() <= max &&
           std::none_of(s.begin(), s.end(), [](unsigned char c) { return c < 32; });
}
bool range(float x, float a, float b) {
    return std::isfinite(x) && x >= a && x <= b;
}
float curve(float t, ResponseCurve c) {
    t = std::clamp(t, 0.f, 1.f);
    switch (c) {
    case ResponseCurve::Smooth:
        return t * t * (3 - 2 * t);
    case ResponseCurve::EaseIn:
        return t * t;
    case ResponseCurve::EaseOut:
        return 1 - (1 - t) * (1 - t);
    default:
        return t;
    }
}
float wave(const ResponseTarget &r, double seconds) {
    float p = float(seconds * r.frequency + r.phase - std::floor(seconds * r.frequency + r.phase));
    switch (r.wave) {
    case ResponseWave::Sine:
        return .5f + .5f * std::sin(p * 6.283185307f);
    case ResponseWave::Triangle:
        return 1 - std::abs(2 * p - 1);
    case ResponseWave::Ramp:
        return p;
    case ResponseWave::Square:
        return p < .5f ? 1.f : 0.f;
    default:
        return 1;
    }
}
} // namespace
void ResponseBank::rebuild() {
    require(sequences.size() <= 64, "Too many response sequences");
    decltype(routes_) routes;
    for (size_t i = 0; i < sequences.size(); ++i) {
        const auto &s = sequences[i];
        require(text(s.name, 128) && text(s.source, 128),
                "Response name/source must be a short nonempty line");
        require(int(s.mode) >= 0 && int(s.mode) <= 1 && int(s.attackCurve) >= 0 &&
                    int(s.attackCurve) <= 3 && int(s.decayCurve) >= 0 && int(s.decayCurve) <= 3,
                "Invalid response mode/curve");
        require(range(s.attack, 0, 60) && range(s.hold, 0, 60) && range(s.decay, 0, 60) &&
                    s.attack + s.hold + s.decay > 0,
                "Invalid response envelope duration");
        require(range(s.inputLow, -1000000, 1000000) && range(s.inputHigh, -1000000, 1000000) &&
                    s.inputHigh > s.inputLow,
                "Input upper bound must exceed lower bound");
        require(s.targets.size() <= 64, "Too many response targets");
        for (size_t j = 0; j < s.targets.size(); ++j) {
            const auto &r = s.targets[j];
            require(text(r.key, 256) && int(r.wave) >= 0 && int(r.wave) <= 4 && int(r.blend) >= 0 &&
                        int(r.blend) <= 2 && range(r.frequency, 0, 30) && range(r.phase, 0, 1) &&
                        range(r.low, -1000000, 1000000) && range(r.high, -1000000, 1000000),
                    "Invalid response target/range");
            if (s.enabled)
                routes[r.key].push_back({i, j});
        }
    }
    routes_ = std::move(routes);
    voices_.assign(sequences.size(), {});
    ++revision;
}
void ResponseBank::reset() {
    for (auto &v : voices_)
        v = {};
    inputs_.clear();
    ++revision;
}
float ResponseBank::envelope(size_t i, double time) const {
    if (i >= voices_.size() || i >= sequences.size())
        return 0;
    const auto &s = sequences[i];
    const auto &v = voices_[i];
    double t = time - v.start;
    if (!s.enabled || v.start < 0 || t < 0 || t >= double(s.attack) + s.hold + s.decay)
        return 0;
    float e = 1;
    if (t < s.attack)
        e = curve(float(t / s.attack), s.attackCurve);
    else if (t >= double(s.attack) + s.hold)
        e = 1 - curve(float((t - s.attack - s.hold) / s.decay), s.decayCurve);
    return e * v.strength;
}
bool ResponseBank::trigger(const std::string &name, double time, float strength) {
    require(std::isfinite(time) && time >= 0 && range(strength, 0, 1),
            "Invalid response trigger time/strength");
    bool hit = false;
    for (size_t i = 0; i < sequences.size(); ++i) {
        const auto &s = sequences[i];
        if (!s.enabled || s.mode != ResponseMode::Event || s.source != name)
            continue;
        const auto &v = voices_.at(i);
        bool active = v.start >= 0 && time - v.start < double(s.attack) + s.hold + s.decay;
        if (s.ignoreRetrigger && active)
            continue;
        voices_[i] = {time, strength};
        hit = true;
    }
    if (hit)
        ++revision;
    return hit;
}
void ResponseBank::setInput(const std::string &name, float value) {
    require(text(name, 128) && std::isfinite(value), "Invalid response input");
    require(inputs_.count(name) || inputs_.size() < 128, "Too many live response inputs");
    auto i = inputs_.find(name);
    if (i == inputs_.end() || i->second != value) {
        inputs_[name] = value;
        ++revision;
    }
}
void ResponseBank::clearInput(const std::string &name) {
    if (inputs_.erase(name))
        ++revision;
}
float ResponseBank::input(const std::string &name, float fallback) const {
    auto i = inputs_.find(name);
    return i == inputs_.end() ? fallback : i->second;
}
bool ResponseBank::targets(const std::string &key) const {
    return routes_.find(key) != routes_.end();
}
float ResponseBank::apply(const std::string &key, float baseline, double time, float minimum,
                          float maximum) const {
    auto found = routes_.find(key);
    if (found == routes_.end())
        return baseline;
    float value = baseline;
    bool changed = false;
    for (auto route : found->second) {
        const auto &s = sequences[route.first];
        const auto &r = s.targets[route.second];
        float weight = 1, position = 0;
        if (s.mode == ResponseMode::Input) {
            auto input = inputs_.find(s.source);
            if (input == inputs_.end())
                continue;
            position =
                std::clamp((input->second - s.inputLow) / (s.inputHigh - s.inputLow), 0.f, 1.f);
        } else {
            weight = envelope(route.first, time);
            if (weight <= 0)
                continue;
            position = wave(r, time - voices_[route.first].start);
        }
        float target = r.low + (r.high - r.low) * position;
        changed = true;
        switch (r.blend) {
        case ResponseBlend::Add:
            value += target * weight;
            break;
        case ResponseBlend::Multiply:
            value *= 1 + (target - 1) * weight;
            break;
        default:
            value += (target - value) * weight;
            break;
        }
    }
    return changed ? std::clamp(value, minimum, maximum) : baseline;
}
void ResponseBank::write(std::ostream &out) const {
    out << std::setprecision(9) << sequences.size() << '\n';
    for (const auto &s : sequences) {
        out << std::quoted(s.name) << ' ' << std::quoted(s.source) << ' ' << int(s.mode) << ' '
            << s.enabled << ' ' << s.ignoreRetrigger << ' ' << s.attack << ' ' << s.hold << ' '
            << s.decay << ' ' << int(s.attackCurve) << ' ' << int(s.decayCurve) << ' ' << s.inputLow
            << ' ' << s.inputHigh << ' ' << s.targets.size() << '\n';
        for (const auto &r : s.targets)
            out << std::quoted(r.key) << ' ' << int(r.wave) << ' ' << int(r.blend) << ' '
                << r.frequency << ' ' << r.phase << ' ' << r.low << ' ' << r.high << '\n';
    }
}
ResponseBank ResponseBank::read(std::istream &in) {
    ResponseBank bank;
    size_t count;
    require(bool(in >> count) && count <= 64, "Invalid response count");
    for (size_t i = 0; i < count; ++i) {
        ResponseSequence s;
        int mode, on, retrigger, attack, decay;
        size_t targets;
        require(bool(in >> std::quoted(s.name) >> std::quoted(s.source) >> mode >> on >>
                     retrigger >> s.attack >> s.hold >> s.decay >> attack >> decay >> s.inputLow >>
                     s.inputHigh >> targets) &&
                    on >= 0 && on <= 1 && retrigger >= 0 && retrigger <= 1 && targets <= 64,
                "Invalid response sequence record");
        s.mode = ResponseMode(mode);
        s.enabled = on != 0;
        s.ignoreRetrigger = retrigger != 0;
        s.attackCurve = ResponseCurve(attack);
        s.decayCurve = ResponseCurve(decay);
        for (size_t j = 0; j < targets; ++j) {
            ResponseTarget r;
            int wave, blend;
            require(bool(in >> std::quoted(r.key) >> wave >> blend >> r.frequency >> r.phase >>
                         r.low >> r.high),
                    "Invalid response target record");
            r.wave = ResponseWave(wave);
            r.blend = ResponseBlend(blend);
            s.targets.push_back(r);
        }
        bank.sequences.push_back(s);
    }
    bank.rebuild();
    return bank;
}
void writeResponseBanks(std::ostream &out, const std::map<int, ResponseBank> &banks) {
    out << "RESPONSES 1 " << banks.size() << '\n';
    for (const auto &p : banks) {
        out << p.first << '\n';
        p.second.write(out);
    }
}
std::map<int, ResponseBank> readResponseBanks(std::istream &in) {
    std::string tag;
    int version;
    size_t count;
    require(bool(in >> tag >> version >> count) && tag == "RESPONSES" && version == 1 &&
                count <= 32,
            "Invalid response banks header");
    std::map<int, ResponseBank> result;
    for (size_t i = 0; i < count; ++i) {
        int id;
        require(bool(in >> id) && id >= 0 && id <= 104 && !result.count(id),
                "Invalid or duplicate response preset");
        result.emplace(id, ResponseBank::read(in));
    }
    return result;
}
} // namespace neon::effects
