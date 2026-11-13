#include "read_bindings.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
namespace neon::effects {
namespace {
void need(bool x, const char *s) {
    if (!x)
        throw std::runtime_error(s);
}
bool name(const std::string &s, size_t limit = 128) {
    return !s.empty() && s.size() <= limit &&
           std::none_of(s.begin(), s.end(), [](unsigned char c) { return c < 32; });
}
bool numeric(const ReadValue &v) {
    return v.index() < 3;
}
bool integer(const ReadValue &v) {
    return v.index() < 2;
}
bool valid(const ReadValue &v) {
    return v.index() == 2 ? std::isfinite(std::get<double>(v))
                          : v.index() != 4 || name(std::get<std::string>(v));
}
struct Integer {
    bool negative;
    uint64_t magnitude;
};
Integer integral(const ReadValue &v) {
    if (auto u = std::get_if<uint64_t>(&v))
        return {false, *u};
    int64_t i = std::get<int64_t>(v);
    return {i < 0, i < 0 ? uint64_t(-(i + 1)) + 1 : uint64_t(i)};
}
int order(uint64_t a, uint64_t b) {
    return a < b ? -1 : a > b ? 1 : 0;
}
int compareInteger(Integer a, Integer b) {
    if (a.negative != b.negative)
        return a.negative ? -1 : 1;
    return (a.negative ? -1 : 1) * order(a.magnitude, b.magnitude);
}
int compareFloat(Integer a, double d) {
    bool negative = d < 0;
    if (a.negative != negative)
        return a.negative ? -1 : 1;
    double magnitude = std::abs(d);
    int cmp;
    if (magnitude >= 18446744073709551616.0)
        cmp = -1;
    else {
        auto truncated = static_cast<uint64_t>(magnitude);
        cmp = order(a.magnitude, truncated);
        if (cmp == 0 && magnitude > double(truncated))
            cmp = -1;
    }
    return negative ? -cmp : cmp;
}
int compare(const ReadValue &a, const ReadValue &b) {
    if (integer(a) && integer(b))
        return compareInteger(integral(a), integral(b));
    if (integer(a))
        return compareFloat(integral(a), std::get<double>(b));
    if (integer(b))
        return -compareFloat(integral(b), std::get<double>(a));
    double x = std::get<double>(a), y = std::get<double>(b);
    return x < y ? -1 : x > y ? 1 : 0;
}
bool equal(const ReadValue &a, const ReadValue &b) {
    return numeric(a) && numeric(b) ? compare(a, b) == 0 : a == b;
}
long double number(const ReadValue &v) {
    if (auto i = std::get_if<int64_t>(&v))
        return static_cast<long double>(*i);
    if (auto u = std::get_if<uint64_t>(&v))
        return static_cast<long double>(*u);
    return std::get<double>(v);
}
long double distance(const ReadValue &a, const ReadValue &b) {
    if (integer(a) && integer(b)) {
        auto x = integral(a), y = integral(b);
        if (x.negative != y.negative)
            return (long double)x.magnitude + y.magnitude;
        return x.magnitude >= y.magnitude ? (long double)(x.magnitude - y.magnitude)
                                          : (long double)(y.magnitude - x.magnitude);
    }
    return std::abs(number(a) - number(b));
}
bool distanceAtLeast(const ReadValue &a, const ReadValue &b, double limit) {
    if (integer(a) && integer(b)) {
        auto x = integral(a), y = integral(b);
        if (x.negative == y.negative) {
            uint64_t d =
                x.magnitude >= y.magnitude ? x.magnitude - y.magnitude : y.magnitude - x.magnitude;
            return compareFloat({false, d}, limit) >= 0;
        }
    }
    return distance(a, b) >= limit;
}
bool compatible(const ReadValue &a, const ReadValue &b) {
    return (numeric(a) && numeric(b)) || a.index() == b.index();
}
bool predicate(const ReadRule &r, const ReadValue &v, bool previous) {
    if (r.condition == ReadCondition::EnterState || r.condition == ReadCondition::LeaveState)
        return equal(v, r.value);
    if (r.condition != ReadCondition::CrossAbove && r.condition != ReadCondition::CrossBelow)
        return false;
    int c = compare(v, r.value);
    bool p = r.condition == ReadCondition::CrossAbove ? c >= 0 : c <= 0;
    return p || (previous && r.hysteresis > 0 && !distanceAtLeast(v, r.value, r.hysteresis));
}
void valueOut(std::ostream &o, const ReadValue &v) {
    o << std::quoted(formatReadValue(v));
}
ReadValue valueIn(std::istream &in) {
    std::string s;
    need(bool(in >> std::quoted(s)), "Missing typed read value");
    return parseReadValue(s);
}
} // namespace
std::string formatReadValue(const ReadValue &v) {
    std::ostringstream o;
    o << std::setprecision(17);
    switch (v.index()) {
    case 0:
        o << "i:" << std::get<int64_t>(v);
        break;
    case 1:
        o << "u:" << std::get<uint64_t>(v);
        break;
    case 2:
        o << "n:" << std::get<double>(v);
        break;
    case 3:
        o << "b:" << (std::get<bool>(v) ? "true" : "false");
        break;
    default:
        o << "s:" << std::get<std::string>(v);
    }
    return o.str();
}
ReadValue parseReadValue(const std::string &s) {
    need(s.size() >= 2 && s.size() <= 258 && s[1] == ':',
         "Use typed values: i:10, u:10, n:0.5, b:true or s:playing");
    auto text = s.substr(2);
    ReadValue v;
    if (s[0] == 'i') {
        int64_t x = 0;
        auto r = std::from_chars(text.data(), text.data() + text.size(), x);
        need(r.ec == std::errc{} && r.ptr == text.data() + text.size(), "Invalid signed counter");
        v = x;
    } else if (s[0] == 'u') {
        uint64_t x = 0;
        auto r = std::from_chars(text.data(), text.data() + text.size(), x);
        need(r.ec == std::errc{} && r.ptr == text.data() + text.size(), "Invalid unsigned counter");
        v = x;
    } else if (s[0] == 'n') {
        std::istringstream in(text);
        double x;
        need(bool(in >> x), "Invalid numeric read value");
        in >> std::ws;
        need(in.eof(), "Invalid numeric read value");
        v = x;
    } else if (s[0] == 'b') {
        need(text == "true" || text == "false", "Boolean must be b:true or b:false");
        v = text == "true";
    } else {
        need(s[0] == 's', "Unknown read value type");
        v = text;
    }
    need(valid(v), "Read values must be finite or a short nonempty state name");
    return v;
}
void ReadBindings::rebuild() {
    need(inputs.size() <= 128 && rules.size() <= 128, "Too many read bindings/rules");
    std::set<std::string> channels;
    for (const auto &i : inputs) {
        need(name(i.field, 256) && name(i.input) && int(i.mapping) >= 0 && int(i.mapping) <= 2 &&
                 valid(i.low) && valid(i.high) && valid(i.state),
             "Invalid read input mapping");
        need(channels.insert(i.input).second, "Each mapped input channel must have one owner");
        if (i.mapping == ReadMapping::Normalize)
            need(numeric(i.low) && numeric(i.high) && compare(i.low, i.high) < 0,
                 "Normalization requires increasing numeric source bounds");
    }
    for (const auto &r : rules) {
        need(name(r.name) && name(r.field, 256) && name(r.event) && int(r.condition) >= 0 &&
                 int(r.condition) <= 6 && valid(r.value),
             "Invalid read trigger rule");
        need(std::isfinite(r.minimumChange) && r.minimumChange >= 0 &&
                 std::isfinite(r.hysteresis) && r.hysteresis >= 0 && std::isfinite(r.cooldown) &&
                 r.cooldown >= 0 && r.cooldown <= 3600 && std::isfinite(r.strength) &&
                 r.strength >= 0 && r.strength <= 1,
             "Invalid trigger sensitivity/cooldown/strength");
        if (r.condition == ReadCondition::CrossAbove || r.condition == ReadCondition::CrossBelow)
            need(numeric(r.value), "Threshold must be numeric");
    }
    state_.assign(rules.size(), {});
    reseed_ = true;
}
void ReadBindings::reset() {
    state_.assign(rules.size(), {});
    latest_.reset();
    reseed_ = true;
}
ReadReport ReadBindings::observe(const ReadSnapshot &snapshot, ResponseBank &bank, double seconds) {
    need(name(snapshot.game) && snapshot.values.size() <= 512 && std::isfinite(seconds) &&
             seconds >= 0,
         "Invalid observation snapshot");
    for (const auto &p : snapshot.values)
        need(name(p.first, 256) && valid(p.second), "Invalid observed field/value");
    ReadReport report;
    bool newSession =
        !latest_ || latest_->game != snapshot.game || latest_->session != snapshot.session;
    if (!newSession && snapshot.sequence <= latest_->sequence) {
        report.status = "Ignored duplicate or out-of-order snapshot";
        return report;
    }
    bool seed = newSession || reseed_;
    if (seed) {
        bank.reset();
        state_.assign(rules.size(), {});
    }
    for (const auto &input : inputs) {
        if (!input.enabled) {
            bank.clearInput(input.input);
            continue;
        }
        auto found = snapshot.values.find(input.field);
        bool available = found != snapshot.values.end();
        float mapped = 0;
        if (available) {
            const auto &v = found->second;
            if (input.mapping == ReadMapping::StateEquals) {
                available = compatible(v, input.state);
                if (available)
                    mapped = equal(v, input.state) ? 1.f : 0.f;
            } else if (std::holds_alternative<bool>(v) && input.mapping == ReadMapping::Raw)
                mapped = std::get<bool>(v) ? 1.f : 0.f;
            else if (!numeric(v))
                available = false;
            else if (input.mapping == ReadMapping::Raw) {
                auto x = number(v);
                available = std::abs(x) <= std::numeric_limits<float>::max();
                if (available)
                    mapped = float(x);
            } else {
                if (compare(v, input.low) <= 0)
                    mapped = 0;
                else if (compare(v, input.high) >= 0)
                    mapped = 1;
                else
                    mapped = float(distance(v, input.low) / distance(input.high, input.low));
            }
        }
        if (available)
            bank.setInput(input.input, mapped);
        else {
            bank.clearInput(input.input);
            report.unavailable.push_back(input.field);
        }
    }
    for (size_t i = 0; i < rules.size(); ++i) {
        const auto &r = rules[i];
        auto &state = state_.at(i);
        auto found = snapshot.values.find(r.field);
        if (!r.enabled) {
            state = {};
            continue;
        }
        bool needsNumber = int(r.condition) <= 3;
        if (found == snapshot.values.end() || (needsNumber && !numeric(found->second)) ||
            ((r.condition == ReadCondition::EnterState ||
              r.condition == ReadCondition::LeaveState) &&
             !compatible(found->second, r.value))) {
            state = {};
            report.unavailable.push_back(r.field);
            continue;
        }
        const auto &current = found->second;
        bool p = predicate(r, current, state.predicate);
        bool fire = false;
        if (state.previous && compatible(*state.previous, current)) {
            switch (r.condition) {
            case ReadCondition::CrossAbove:
            case ReadCondition::CrossBelow:
            case ReadCondition::EnterState:
                fire = !state.predicate && p;
                break;
            case ReadCondition::LeaveState:
                fire = state.predicate && !p;
                break;
            case ReadCondition::Increase:
                fire = compare(current, *state.previous) > 0 &&
                       distanceAtLeast(current, *state.previous, r.minimumChange);
                break;
            case ReadCondition::Decrease:
                fire = compare(current, *state.previous) < 0 &&
                       distanceAtLeast(current, *state.previous, r.minimumChange);
                break;
            case ReadCondition::Changed:
                fire = !equal(current, *state.previous);
                break;
            }
        }
        state.previous = current;
        state.predicate = p;
        if (!seed && fire && (state.lastEvent < 0 || seconds - state.lastEvent >= r.cooldown)) {
            state.lastEvent = seconds;
            report.events.push_back({r.name, r.event, bank.trigger(r.event, seconds, r.strength)});
        }
    }
    latest_ = snapshot;
    reseed_ = false;
    report.applied = true;
    report.baseline = seed;
    report.status = seed ? "Observation baseline established; no events fired" : "Snapshot applied";
    return report;
}
void ReadBindings::write(std::ostream &o) const {
    o << std::setprecision(17) << inputs.size() << ' ' << rules.size() << '\n';
    for (const auto &i : inputs) {
        o << std::quoted(i.field) << ' ' << std::quoted(i.input) << ' ' << i.enabled << ' '
          << int(i.mapping) << ' ';
        valueOut(o, i.low);
        o << ' ';
        valueOut(o, i.high);
        o << ' ';
        valueOut(o, i.state);
        o << '\n';
    }
    for (const auto &r : rules) {
        o << std::quoted(r.name) << ' ' << std::quoted(r.field) << ' ' << std::quoted(r.event)
          << ' ' << r.enabled << ' ' << int(r.condition) << ' ';
        valueOut(o, r.value);
        o << ' ' << r.minimumChange << ' ' << r.hysteresis << ' ' << r.cooldown << ' ' << r.strength
          << '\n';
    }
}
ReadBindings ReadBindings::read(std::istream &in) {
    ReadBindings b;
    size_t ni, nr;
    need(bool(in >> ni >> nr) && ni <= 128 && nr <= 128, "Invalid read binding counts");
    for (size_t j = 0; j < ni; ++j) {
        ReadInput i;
        int on, kind;
        need(bool(in >> std::quoted(i.field) >> std::quoted(i.input) >> on >> kind) && on >= 0 &&
                 on <= 1,
             "Invalid read input");
        i.enabled = on != 0;
        i.mapping = ReadMapping(kind);
        i.low = valueIn(in);
        i.high = valueIn(in);
        i.state = valueIn(in);
        b.inputs.push_back(i);
    }
    for (size_t j = 0; j < nr; ++j) {
        ReadRule r;
        int on, kind;
        need(bool(in >> std::quoted(r.name) >> std::quoted(r.field) >> std::quoted(r.event) >> on >>
                  kind) &&
                 on >= 0 && on <= 1,
             "Invalid read rule");
        r.enabled = on != 0;
        r.condition = ReadCondition(kind);
        r.value = valueIn(in);
        need(bool(in >> r.minimumChange >> r.hysteresis >> r.cooldown >> r.strength),
             "Invalid trigger controls");
        b.rules.push_back(r);
    }
    b.rebuild();
    return b;
}
void writeReadBanks(std::ostream &o, const std::map<int, ReadBindings> &banks) {
    o << "READBINDINGS 1 " << banks.size() << '\n';
    for (const auto &b : banks) {
        o << b.first << '\n';
        b.second.write(o);
    }
}
std::map<int, ReadBindings> readReadBanks(std::istream &in) {
    std::map<int, ReadBindings> result;
    std::string tag;
    int version;
    size_t count;
    need(bool(in >> tag >> version >> count) && tag == "READBINDINGS" && version == 1 &&
             count <= 32,
         "Invalid read bindings header");
    for (size_t i = 0; i < count; ++i) {
        int id;
        need(bool(in >> id) && id >= 0 && id <= 104 && !result.count(id),
             "Invalid read bindings preset");
        result.emplace(id, ReadBindings::read(in));
    }
    return result;
}
} // namespace neon::effects
