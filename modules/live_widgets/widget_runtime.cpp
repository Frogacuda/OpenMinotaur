#include "widget_runtime.h"
#include "widgetml_layout.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace neon::widgets {
namespace {
void need(bool v, const std::string &s) {
    if (!v)
        throw std::runtime_error(s);
}
Value path(const Value::Object &values, const std::string &field) {
    auto exact = values.find(field);
    if (exact != values.end())
        return exact->second;
    size_t dot = field.find('.');
    auto first = values.find(field.substr(0, dot));
    if (first == values.end())
        return {};
    Value value = first->second;
    while (dot != std::string::npos) {
        size_t next = field.find('.', dot + 1);
        value = value.at(field.substr(dot + 1, next - dot - 1));
        dot = next;
    }
    return value;
}
Matrix mul(Matrix p, Matrix q) {
    return {p.a * q.a + p.c * q.b, p.b * q.a + p.d * q.b,       p.a * q.c + p.c * q.d,
            p.b * q.c + p.d * q.d, p.a * q.x + p.c * q.y + p.x, p.b * q.x + p.d * q.y + p.y};
}
Rect intersect(Rect a, Rect b) {
    double x = std::max(a.x, b.x), y = std::max(a.y, b.y);
    return {x, y, std::max(0., std::min(a.x + a.width, b.x + b.width) - x),
            std::max(0., std::min(a.y + a.height, b.y + b.height) - y)};
}
Rect bounds(Matrix m, Rect r) {
    auto a = m.point(r.x, r.y), b = m.point(r.x + r.width, r.y), c = m.point(r.x, r.y + r.height),
         d = m.point(r.x + r.width, r.y + r.height);
    double x = std::min({a[0], b[0], c[0], d[0]}), y = std::min({a[1], b[1], c[1], d[1]});
    return {x, y, std::max({a[0], b[0], c[0], d[0]}) - x, std::max({a[1], b[1], c[1], d[1]}) - y};
}
std::string key(const Value &v) {
    auto s = v.string();
    need(!s.empty() && s.size() <= 128,
         "Repeat entries need a stable, nonempty string/integer key");
    std::string r;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '_' || c == '-')
            r += c;
        else {
            const char *h = "0123456789abcdef";
            r += '%';
            r += h[c >> 4];
            r += h[c & 15];
        }
    }
    return r;
}
Value interpolate(const Value &a, const Value &b, double t) {
    if (std::holds_alternative<Value::Array>(a.data)) {
        Value::Array r;
        for (size_t i = 0; i < a.array().size(); ++i)
            r.push_back(interpolate(a.at(i), b.at(i), t));
        return r;
    }
    return a.number() + (b.number() - a.number()) * t;
}
Value combine(const Value &baseline, const Value &value, const std::string &mode) {
    if (std::holds_alternative<Value::Array>(value.data)) {
        Value::Array result;
        for (size_t i = 0; i < value.array().size(); ++i) {
            auto base =
                std::holds_alternative<Value::Array>(baseline.data) ? baseline.at(i) : baseline;
            result.push_back(combine(base, value.at(i), mode));
        }
        return result;
    }
    return mode == "add" ? Value(baseline.number() + value.number())
                         : Value(baseline.number(1) * value.number(1));
}
std::set<std::string> operators = {
    "get",       "read",      "output",    "event",      "entered",   "literal", "if",
    "coalesce",  "+",         "-",         "*",          "/",         "min",     "max",
    "clamp",     "lerp",      "normalize", "==",         "!=",        "<",       ">",
    "<=",        ">=",        "and",       "or",         "not",       "count",   "smooth",
    "increased", "decreased", "changed",   "crossAbove", "crossBelow"};
void expression(const Value &v, int depth = 0) {
    need(depth < 24, "Expression depth limit");
    if (!std::holds_alternative<Value::Array>(v.data))
        return;
    auto &a = v.array();
    if (!a.empty() && std::holds_alternative<std::string>(a[0].data)) {
        auto op = a[0].string();
        need(operators.count(op) != 0, "Unknown widget expression: " + op);
        if (op == "literal")
            return;
        size_t arguments = a.size() - 1;
        size_t minimum =
            (op == "get" || op == "read" || op == "event" || op == "entered" || op == "not" ||
             op == "count" || op == "increased" || op == "decreased" || op == "changed")
                ? 1
            : (op == "if" || op == "clamp" || op == "lerp" || op == "normalize") ? 3
                                                                                 : 2;
        need(arguments >= minimum, "Missing expression argument: " + op);
    }
    for (size_t i = 1; i < a.size(); ++i)
        expression(a[i], depth + 1);
}
Value expand(Value node, const Value::Object &templates, std::vector<std::string> stack = {},
             int depth = 0) {
    need(depth < 12, "Group nesting/template depth limit");
    auto use = node.at("use").string();
    if (!use.empty()) {
        need(std::find(stack.begin(), stack.end(), use) == stack.end(),
             "Recursive widget template");
        auto it = templates.find(use);
        need(it != templates.end(), "Unknown widget template: " + use);
        auto base = it->second;
        for (auto &f : node.object())
            if (f.first != "use") {
                if (f.first == "inputs" || f.first == "settings") {
                    auto merged = base.at(f.first);
                    for (auto &entry : f.second.object())
                        merged[entry.first] = entry.second;
                    base[f.first] = std::move(merged);
                } else
                    base[f.first] = f.second;
            }
        stack.push_back(use);
        node = expand(base, templates, stack, depth + 1);
    }
    if (node.object().count("children"))
        for (auto &c : node["children"].array())
            c = expand(c, templates, stack, depth + 1);
    if (node.object().count("template"))
        node["template"] = expand(node.at("template"), templates, stack, depth + 1);
    return node;
}
void checkNode(const Value &node, int &count, int depth) {
    need(++count <= 256 && depth < 12, "Widget object/depth budget exceeded");
    validateWidgetMLPlacement(node.at("placement"));
    auto id = node.at("id").string(), kind = node.at("kind").string();
    need(!id.empty() && id.size() <= 64 &&
             id.find_first_not_of(
                 "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") ==
                 std::string::npos,
         "Widget IDs use letters, digits, underscore or hyphen");
    need(std::set<std::string>{"group", "repeat", "logic", "sequence", "vector", "sprite", "text",
                               "rect"}
                 .count(kind) != 0,
         "Unknown widget object kind: " + kind);
    for (auto name : {"enabled", "visible", "x", "y", "size", "scale", "rotation", "color",
                      "opacity", "fill", "fontSize", "when", "value", "items", "where"})
        if (node.object().count(name))
            expression(node.at(name));
    for (auto &p : node.at("inputs").object())
        expression(p.second);
    for (auto &p : node.at("outputs").object())
        expression(p.second);
    need(node.at("frames").array().size() <= 256, "Flipbook frame budget exceeded");
    need(node.at("parameters").object().size() <= 32, "Widget parameter budget exceeded");
    for (auto &p : node.at("parameters").object()) {
        need(!p.second.at("default").missing(), "Widget parameter needs a default");
        need(!node.at("inputs").object().count(p.first), "Input and parameter names overlap");
    }
    for (auto &p : node.at("inputs").object())
        need(p.first != "time" && p.first != "state" && p.first != "stateTime",
             "Reserved group input name");
    std::set<std::string> ids;
    for (auto &c : node.at("children").array()) {
        need(ids.insert(c.at("id").string()).second, "Duplicate child ID");
        checkNode(c, count, depth + 1);
    }
    std::map<std::string, const Value *> logic;
    for (auto &c : node.at("children").array())
        if (c.at("kind").string() == "logic") {
            need(!node.at("inputs").object().count(c.at("id").string()),
                 "Logic name shadows a declared input");
            logic[c.at("id").string()] = &c.at("value");
        }
    std::set<std::string> active, done;
    std::function<void(const Value &)> dependencies;
    std::function<void(const std::string &)> visit = [&](const std::string &name) {
        if (done.count(name))
            return;
        need(active.insert(name).second, "Cyclic local widget logic: " + name);
        dependencies(*logic.at(name));
        active.erase(name);
        done.insert(name);
    };
    dependencies = [&](const Value &expression) {
        auto &a = expression.array();
        if (a.size() > 1 && a[0].string() == "get") {
            auto name = a[1].string();
            name = name.substr(0, name.find('.'));
            if (logic.count(name))
                visit(name);
        }
        if (!a.empty() && a[0].string() == "literal")
            return;
        for (auto &child : a)
            dependencies(child);
    };
    for (auto &l : logic)
        visit(l.first);
    if (kind == "repeat") {
        need(node.at("maximum").number(32) >= 1 && node.at("maximum").number(32) <= 64,
             "Repeat maximum must be 1..64");
        need(!node.at("template").missing(), "Repeat requires a subgroup template");
        need(node.at("template").at("kind").string() == "group", "Repeat template must be a group");
        checkNode(node.at("template"), count, depth + 1);
    }
    for (auto &a : node.at("animations").array()) {
        expression(a.at("trigger"));
        need(!a.at("id").string().empty(), "Animation needs an ID");
        need(a.at("tracks").array().size() <= 32, "Track budget exceeded");
        for (auto &t : a.at("tracks").array()) {
            need(std::set<std::string>{"x", "y", "scale", "rotation", "opacity", "fill", "color"}
                         .count(t.at("property").string()) != 0,
                 "Unsupported animation property");
            double last = -1;
            need(t.at("keys").array().size() >= 2 && t.at("keys").array().size() <= 64,
                 "Track needs 2..64 keys");
            for (auto &k : t.at("keys").array()) {
                double at = k.at(0).number(-1);
                need(at >= 0 && at > last && at <= 3600,
                     "Animation keys must be ordered and bounded");
                need(!k.at(1).missing(), "Animation key needs a value");
                last = at;
            }
        }
    }
    for (auto &t : node.at("transitions").array()) {
        expression(t.at("when"));
        need(!t.at("to").string().empty(), "Transition needs a target state");
    }
    if (kind == "sequence") {
        double last = -1;
        for (auto &a : node.at("actions").array()) {
            double at = a.at("at").number(-1);
            need(at >= last && at >= 0 && at <= 3600,
                 "Sequence actions must be ordered and bounded");
            need(!a.at("emit").string().empty(), "Sequence action needs emit");
            last = at;
        }
    }
}
} // namespace
struct Runtime::Impl {
    bool widgetML = false;
    WidgetMLLayout layout;
    Value source, root;
    Snapshot snapshot;
    bool hasSnapshot = false, newSnapshot = false, reseed = true, on = true;
    double seconds = 0;
    Frame output;
    std::map<std::string, std::set<std::string>> localEvents, pendingLocal;
    uint64_t generation = 0;
    Value::Object previousOutputs;
    std::set<std::string> events, pendingEvents, visited, disabledPrefixes;
    struct Edge {
        Value previous, result;
        bool ready = false, predicate = false;
        uint64_t evaluation = 0;
        double lastClock = 0;
    };
    struct Animation {
        double start = -1;
        bool trigger = false, complete = false;
    };
    struct State {
        double clock = 0, stateStart = 0;
        std::string state;
        bool initialized = false, disabled = false, entered = false;
        std::map<std::string, Edge> edges;
        std::map<std::string, Animation> animations;
        double sequenceStart = -1;
        size_t cue = 0;
        bool sequenceTrigger = false;
    };
    std::map<std::string, State> states;
    struct Scope {
        Value::Object values;
        std::map<std::string, const Value *> logic;
        std::set<std::string> evaluating;
        std::map<std::string, Value> computed;
        std::string path;
        State *state = nullptr;
        bool fresh = false;
    };
    Value get(Scope &s, const std::string &field) {
        auto dot = field.find('.');
        auto name = field.substr(0, dot);
        auto it = s.logic.find(name);
        if (it != s.logic.end() && !s.computed.count(name)) {
            need(s.evaluating.insert(name).second, "Cyclic local widget logic: " + name);
            auto v = eval(it->second->at("value"), s, "logic." + name);
            s.evaluating.erase(name);
            s.computed[name] = v;
            s.values[name] = v;
        }
        return path(s.values, field);
    }
    Value eval(const Value &v, Scope &s, const std::string &slot, int depth = 0) {
        need(++output.evaluated <= 16384 && depth < 24, "Widget expression budget exceeded");
        if (!std::holds_alternative<Value::Array>(v.data))
            return v;
        auto &a = v.array();
        if (a.empty() || !std::holds_alternative<std::string>(a[0].data)) {
            Value::Array r;
            for (size_t i = 0; i < a.size(); ++i)
                r.push_back(eval(a[i], s, slot + "." + std::to_string(i), depth + 1));
            return r;
        }
        auto op = a[0].string();
        auto arg = [&](size_t i) {
            return i < a.size() ? eval(a[i], s, slot + "." + std::to_string(i), depth + 1)
                                : Value{};
        };
        if (op == "literal")
            return a.size() > 1 ? a[1] : Value{};
        if (op == "get")
            return get(s, a.at(1).string());
        if (op == "read")
            return path(snapshot.values, a.at(1).string());
        if (op == "output") {
            auto p = s.path + "/" + a.at(1).string() + "." + a.at(2).string();
            auto it = previousOutputs.find(p);
            return it == previousOutputs.end() ? Value{} : it->second;
        }
        if (op == "event") {
            auto found = localEvents.find(s.path);
            return events.count(a.at(1).string()) != 0 ||
                   (found != localEvents.end() && found->second.count(a.at(1).string()) != 0);
        }
        if (op == "entered")
            return s.state && s.state->entered && s.state->state == a.at(1).string();
        if (op == "if")
            return arg(1).boolean() ? arg(2) : arg(3);
        if (op == "coalesce") {
            auto x = arg(1);
            return x.missing() ? arg(2) : x;
        }
        if (op == "and" || op == "or") {
            bool result = op == "and", unknown = false;
            for (size_t i = 1; i < a.size(); ++i) {
                auto operand = arg(i);
                if (operand.missing()) {
                    unknown = true;
                    continue;
                }
                bool x = operand.boolean();
                if (op == "and" && !x)
                    return false;
                if (op == "or" && x)
                    return true;
            }
            return unknown ? Value{} : Value(result);
        }
        if (op == "not") {
            auto operand = arg(1);
            return operand.missing() ? Value{} : Value(!operand.boolean());
        }
        auto x = arg(1);
        if (op == "count")
            return x.missing() ? Value{} : Value(uint64_t(x.array().size()));
        auto y = arg(2);
        if (op == "smooth") {
            auto &edge = s.state->edges[slot];
            if (edge.evaluation == generation)
                return edge.result;
            edge.evaluation = generation;
            if (x.missing()) {
                edge.ready = false;
                return edge.result = {};
            }
            double current = x.number();
            if (s.fresh || !edge.ready)
                edge.previous = current;
            else {
                double elapsed = std::max(0., s.state->clock - edge.lastClock),
                       duration = std::max(0., y.number());
                edge.previous = duration == 0 ? current
                                              : edge.previous.number() +
                                                    (current - edge.previous.number()) *
                                                        (1 - std::exp(-elapsed / duration));
            }
            edge.lastClock = s.state->clock;
            edge.ready = true;
            return edge.result = edge.previous;
        }
        if (op == "increased" || op == "decreased" || op == "changed" || op == "crossAbove" ||
            op == "crossBelow") {
            auto &edge = s.state->edges[slot];
            if (edge.evaluation == generation)
                return edge.result;
            edge.evaluation = generation;
            edge.result = false;
            bool fired = false;
            if (x.missing() || ((op == "crossAbove" || op == "crossBelow") && y.missing())) {
                edge.ready = false;
                return false;
            }
            if (s.fresh || !edge.ready) {
                edge.previous = x;
                edge.ready = true;
                edge.predicate = op == "crossBelow" ? compare(x, y) < 0 : compare(x, y) > 0;
                return false;
            }
            if (!newSnapshot)
                return false;
            if (op == "increased")
                fired = compare(x, edge.previous) > 0;
            if (op == "decreased")
                fired = compare(x, edge.previous) < 0;
            if (op == "changed")
                fired = x != edge.previous;
            if (op == "crossAbove" || op == "crossBelow") {
                double h = std::max(0., arg(3).number());
                bool predicate = edge.predicate;
                if (op == "crossAbove") {
                    if (compare(x, y) > 0)
                        predicate = true;
                    else if (h == 0 ? compare(x, y) < 0 : x.number() < y.number() - h)
                        predicate = false;
                } else {
                    if (compare(x, y) < 0)
                        predicate = true;
                    else if (h == 0 ? compare(x, y) > 0 : x.number() > y.number() + h)
                        predicate = false;
                }
                fired = !edge.predicate && predicate;
                edge.predicate = predicate;
            }
            edge.previous = x;
            return edge.result = fired;
        }
        if (x.missing() || y.missing())
            return {};
        if (op == "==")
            return x == y;
        if (op == "!=")
            return x != y;
        if (op == "<")
            return compare(x, y) < 0;
        if (op == ">")
            return compare(x, y) > 0;
        if (op == "<=")
            return compare(x, y) <= 0;
        if (op == ">=")
            return compare(x, y) >= 0;
        double u = x.number(), w = y.number(), r = 0;
        if (op == "+")
            r = u + w;
        else if (op == "-")
            r = u - w;
        else if (op == "*")
            r = u * w;
        else if (op == "/")
            return w == 0 ? Value{} : Value(u / w);
        else if (op == "min")
            r = std::min(u, w);
        else if (op == "max")
            r = std::max(u, w);
        else if (op == "clamp")
            r = std::clamp(u, std::min(w, arg(3).number()), std::max(w, arg(3).number()));
        else if (op == "lerp")
            r = u + (w - u) * arg(3).number();
        else if (op == "normalize") {
            double high = arg(3).number();
            return high == w ? Value{} : Value(std::clamp((u - w) / (high - w), 0., 1.));
        } else
            throw std::runtime_error("Unknown expression: " + op);
        return std::isfinite(r) ? Value(r) : Value{};
    }
    std::string format(std::string text, Scope &scope) {
        need(text.size() <= 4096, "Widget text too long");
        std::string out;
        for (size_t p = 0; p < text.size();) {
            if (text[p] != '{') {
                out += text[p++];
                continue;
            }
            size_t end = text.find('}', p + 1);
            if (end == std::string::npos) {
                out += text.substr(p);
                break;
            }
            auto token = text.substr(p + 1, end - p - 1);
            auto colon = token.find(':');
            auto value = get(scope, token.substr(0, colon));
            auto spec = colon == std::string::npos ? "" : token.substr(colon + 1);
            if (value.missing())
                out += "--";
            else if (spec == "percent") {
                std::ostringstream s;
                s << std::fixed << std::setprecision(0) << value.number() * 100 << '%';
                out += s.str();
            } else if (spec == "integer") {
                if (std::holds_alternative<uint64_t>(value.data) ||
                    std::holds_alternative<int64_t>(value.data))
                    out += value.string();
                else {
                    std::ostringstream s;
                    s << std::fixed << std::setprecision(0) << value.number();
                    out += s.str();
                }
            } else
                out += value.string();
            p = end + 1;
            need(out.size() <= 4096, "Formatted widget text exceeds budget");
        }
        return out;
    }
    void signal(const std::string &value, const std::string &group) {
        if (value.empty())
            return;
        need(output.signals.size() < 128, "Widget signal budget exceeded");
        output.signals.push_back(value);
        output.scopedSignals.push_back({group, value});
        pendingLocal[group].insert(value);
    }
    void node(const Value &n, Scope &parent, Matrix matrix, double parentW, double parentH,
              Rect clip, std::array<double, 4> parentColor, bool parentVisible, bool bezel,
              const std::string &full, double dt, Value item = {}, double flowX = 0,
              double flowY = 0) {
        need(++output.instances <= 512, "Widget instance budget exceeded");
        visited.insert(full);
        need(states.size() < 1024 || states.count(full), "Widget state budget exceeded");
        auto &state = states[full];
        auto enabled = n.at("enabled").missing()
                           ? true
                           : eval(n.at("enabled"), parent, full + ".enabled").boolean();
        if (!enabled) {
            state.disabled = true;
            disabledPrefixes.insert(full);
            for (auto &child : states)
                if (child.first.rfind(full + "/", 0) == 0)
                    child.second.disabled = true;
            return;
        }
        bool fresh = reseed || state.disabled || !state.initialized;
        state.disabled = false;
        state.clock += dt;
        state.entered = false;
        if (!state.initialized) {
            state.initialized = true;
            state.state = n.at("initialState").string();
            state.entered = !state.state.empty();
            state.stateStart = state.clock;
        }
        auto kind = n.at("kind").string();
        Scope local;
        Scope *scope = &parent;
        if (kind == "group" || kind == "repeat") {
            local.path = full;
            local.state = &state;
            local.fresh = fresh;
            for (auto &parameter : n.at("parameters").object()) {
                const auto &spec = parameter.second;
                auto given = n.at("settings").at(parameter.first);
                auto value = given.missing()
                                 ? spec.at("default")
                                 : eval(given, parent, full + ".setting." + parameter.first);
                if (value.missing())
                    value = spec.at("default");
                auto type = spec.at("type").string("number");
                if (type == "number") {
                    need(std::holds_alternative<int64_t>(value.data) ||
                             std::holds_alternative<uint64_t>(value.data) ||
                             std::holds_alternative<double>(value.data),
                         "Numeric widget setting has another type");
                    if (!spec.at("min").missing() || !spec.at("max").missing()) {
                        double low = spec.at("min").number(-65536),
                               high = spec.at("max").number(65536);
                        need(low <= high, "Invalid setting range");
                        value = std::clamp(value.number(), low, high);
                    }
                } else if (type == "bool")
                    need(std::holds_alternative<bool>(value.data),
                         "Boolean widget setting has another type");
                else if (type == "string")
                    need(std::holds_alternative<std::string>(value.data),
                         "String widget setting has another type");
                else if (type == "color")
                    need(value.array().size() == 4, "Color widget setting requires RGBA");
                else
                    throw std::runtime_error("Unknown widget setting type");
                local.values[parameter.first] = value;
            }
            for (auto &input : n.at("inputs").object())
                local.values[input.first] = eval(input.second, parent, "input." + input.first);
            if (!item.missing())
                local.values["item"] = item;
            local.values["time"] = state.clock;
            local.values["state"] = state.state;
            local.values["stateTime"] = state.clock - state.stateStart;
            for (auto &c : n.at("children").array())
                if (c.at("kind").string() == "logic")
                    local.logic[c.at("id").string()] = &c;
            scope = &local;
            for (auto &l : local.logic) {
                auto result = get(local, l.first);
                auto name = l.second->at("emit").string();
                if (!name.empty()) {
                    auto &edge = state.edges["$emit." + l.first];
                    bool current = result.boolean();
                    if (fresh)
                        edge.ready = false;
                    if (current && ((edge.ready && !edge.previous.boolean()) ||
                                    (!edge.ready && l.second->at("emitOnInitial").boolean())))
                        signal(name, local.path);
                    edge.ready = !result.missing();
                    edge.previous = current;
                }
            }
            for (auto &t : n.at("transitions").array())
                if ((t.at("from").string() == state.state || t.at("from").string() == "*") &&
                    eval(t.at("when"), local,
                         "transition." + t.at("from").string() + "." + t.at("to").string())
                        .boolean()) {
                    state.state = t.at("to").string();
                    state.stateStart = state.clock;
                    state.entered = true;
                    local.values["state"] = state.state;
                    local.values["stateTime"] = 0;
                    local.computed.clear();
                    signal(t.at("emit").string(), local.path);
                    break;
                }
        }
        auto &s = *scope;
        if (kind == "logic")
            return;
        if (kind == "sequence") {
            bool trigger = n.at("autoplay").boolean(true) ||
                           eval(n.at("trigger"), s, full + ".sequence.trigger").boolean();
            if (trigger && !state.sequenceTrigger) {
                state.sequenceStart = state.clock;
                state.cue = 0;
            }
            state.sequenceTrigger = trigger;
            if (state.sequenceStart >= 0) {
                auto &actions = n.at("actions").array();
                while (state.cue < actions.size() &&
                       state.clock - state.sequenceStart >= actions[state.cue].at("at").number()) {
                    signal(actions[state.cue].at("emit").string(), s.path);
                    ++state.cue;
                }
            }
            return;
        }
        Value::Object properties;
        for (auto name : {"x", "y", "size", "scale", "rotation", "color", "opacity", "fill"})
            if (!n.at(name).missing())
                properties[name] = eval(n.at(name), s, full + ".property." + name);
        auto overrides = n.at("states").at(state.state);
        for (auto &p : overrides.object())
            properties[p.first] = eval(p.second, s, "state." + p.first);
        for (auto &animation : n.at("animations").array()) {
            auto &voice = state.animations[animation.at("id").string()];
            bool trigger =
                eval(animation.at("trigger"), s, full + ".animation." + animation.at("id").string())
                    .boolean();
            if ((animation.at("autoplay").boolean() && voice.start < 0) ||
                (trigger && !voice.trigger)) {
                if (!animation.at("ignoreRetrigger").boolean() || voice.start < 0 ||
                    voice.complete) {
                    voice.start = state.clock;
                    voice.complete = false;
                }
            }
            voice.trigger = trigger;
            if (voice.start < 0)
                continue;
            double duration = 0;
            for (auto &track : animation.at("tracks").array())
                duration = std::max(duration, track.at("keys").array().back().at(0).number());
            double elapsed = state.clock - voice.start;
            if (elapsed >= duration && !voice.complete) {
                voice.complete = true;
                signal(animation.at("onComplete").string(), s.path);
            }
            bool loop = animation.at("loop").boolean();
            if (loop) {
                elapsed = std::fmod(elapsed, duration);
                voice.complete = false;
            } else if (elapsed >= duration &&
                       animation.at("completion").string("baseline") != "hold")
                continue;
            for (auto &track : animation.at("tracks").array()) {
                auto &keys = track.at("keys").array();
                Value value = keys.front().at(1);
                if (elapsed >= keys.back().at(0).number())
                    value = keys.back().at(1);
                else
                    for (size_t i = 1; i < keys.size(); ++i)
                        if (elapsed <= keys[i].at(0).number()) {
                            double t = std::clamp(
                                (elapsed - keys[i - 1].at(0).number()) /
                                    (keys[i].at(0).number() - keys[i - 1].at(0).number()),
                                0., 1.);
                            auto ease = track.at("curve").string("linear");
                            if (ease == "smooth")
                                t = t * t * (3 - 2 * t);
                            if (ease == "in")
                                t *= t;
                            if (ease == "out")
                                t = 1 - (1 - t) * (1 - t);
                            value = interpolate(keys[i - 1].at(1), keys[i].at(1), t);
                            break;
                        }
                auto property = track.at("property").string();
                auto mode = track.at("mode").string("replace");
                if (mode != "replace") {
                    Value base = properties.count(property)
                                     ? properties[property]
                                     : Value(property == "opacity" || property == "scale" ||
                                                     property == "color"
                                                 ? 1
                                                 : 0);
                    value = combine(base, value, mode);
                }
                properties[property] = value;
            }
        }
        auto number = [&](const std::string &name, double fallback) {
            auto it = properties.find(name);
            return it == properties.end() ? fallback : it->second.number(fallback);
        };
        double w = parentW, h = parentH;
        if (properties.count("size")) {
            w = properties["size"].at(0).number(parentW);
            h = properties["size"].at(1).number(parentH);
        }
        need(std::isfinite(w) && std::isfinite(h) && w >= 0 && h >= 0 && w <= 65536 && h <= 65536,
             "Invalid widget size");
        auto scale = properties.count("scale") ? properties["scale"] : Value(1);
        double sx = !std::holds_alternative<Value::Array>(scale.data) ? scale.number(1)
                                                                      : scale.at(0).number(1),
               sy = !std::holds_alternative<Value::Array>(scale.data) ? scale.number(1)
                                                                      : scale.at(1).number(1);
        need(std::isfinite(sx) && std::isfinite(sy) && std::abs(sx) <= 64 && std::abs(sy) <= 64 &&
                 std::abs(number("x", 0)) <= 65536 && std::abs(number("y", 0)) <= 65536 &&
                 std::abs(number("rotation", 0)) <= 1000000,
             "Widget transform outside bounded range");
        double a = number("rotation", 0) * 3.141592653589793 / 180, cs = std::cos(a),
               sn = std::sin(a), px = n.at("pivot").at(0).number(),
               py = n.at("pivot").at(1).number();
        auto origin = widgetML ? widgetMLOrigin(n.at("placement"), layout, matrix)
                               : std::array<double, 2>{0, 0};
        double x = origin[0] + number("x", 0) + n.at("anchor").at(0).number() * parentW + flowX,
               y = origin[1] + number("y", 0) + n.at("anchor").at(1).number() * parentH + flowY;
        Matrix transform{cs * sx, sn * sx, -sn * sy, cs * sy, x, y};
        transform.x -= transform.a * px * w + transform.c * py * h;
        transform.y -= transform.b * px * w + transform.d * py * h;
        matrix = mul(matrix, transform);
        auto color = parentColor;
        if (properties.count("color")) {
            auto v = properties["color"];
            for (int i = 0; i < 4; ++i) {
                double c = std::clamp(v.at(i).number(1), 0., i == 3 ? 1. : 16.);
                color[i] = (i < 3 && !n.at("inheritColor").boolean(true)) ? c : color[i] * c;
            }
        }
        color[3] *= std::clamp(number("opacity", 1), 0., 1.);
        bool visible = parentVisible &&
                       (n.at("visible").missing() || eval(n.at("visible"), s, "visible").boolean());
        bezel |= n.at("bezelOnly").boolean();
        if (n.at("clip").boolean() || n.at("layout").at("overflow").string() == "clip")
            clip = intersect(clip, bounds(matrix, {0, 0, w, h}));
        if (kind == "group" || kind == "repeat") {
            if (kind == "repeat") {
                auto collection = eval(n.at("items"), parent, "repeat.items");
                size_t maximum = size_t(n.at("maximum").number(32)), index = 0;
                std::set<std::string> keys;
                for (auto &entry : collection.array()) {
                    if (index >= maximum) {
                        ++output.truncated;
                        continue;
                    }
                    Scope filter = s;
                    filter.values["item"] = entry;
                    if (!n.at("where").missing() && !eval(n.at("where"), filter, "where").boolean())
                        continue;
                    auto identity = key(entry.at(n.at("key").string("id")));
                    need(keys.insert(identity).second, "Duplicate repeat entry key");
                    auto objectLayout = n.at("layout");
                    double gapX = objectLayout.at("gap").at(0).number(),
                           gapY = objectLayout.at("gap").at(1).number(),
                           cw = objectLayout.at("cell").at(0).number(w),
                           ch = objectLayout.at("cell").at(1).number(h);
                    int columns =
                        objectLayout.at("columns").string() == "auto"
                            ? std::max(
                                  1, int((w + objectLayout.at("gap").at(0).number()) /
                                         std::max(1., cw + objectLayout.at("gap").at(0).number())))
                            : std::max(1, int(objectLayout.at("columns").number(1)));
                    auto type = objectLayout.at("type").string("column");
                    double fx = type == "row"    ? index * (cw + gapX)
                                : type == "grid" ? (index % columns) * (cw + gapX)
                                                 : 0,
                           fy = type == "column" ? index * (ch + gapY)
                                : type == "grid" ? (index / columns) * (ch + gapY)
                                                 : 0;
                    node(n.at("template"), s, matrix, cw, ch, clip, color, visible, bezel,
                         full + "/@" + identity, dt, entry, fx, fy);
                    ++index;
                }
            } else {
                size_t index = 0;
                auto objectLayout = n.at("layout");
                for (auto &child : n.at("children").array()) {
                    if (child.at("kind").string() == "logic")
                        continue;
                    double fx = 0, fy = 0;
                    auto type = objectLayout.at("type").string();
                    double cw = objectLayout.at("cell").at(0).number(w),
                           ch = objectLayout.at("cell").at(1).number(h),
                           gx = objectLayout.at("gap").at(0).number(),
                           gy = objectLayout.at("gap").at(1).number();
                    int columns = std::max(1, int(objectLayout.at("columns").number(1)));
                    if (type == "row")
                        fx = index * (cw + gx);
                    if (type == "column")
                        fy = index * (ch + gy);
                    if (type == "grid") {
                        fx = (index % columns) * (cw + gx);
                        fy = (index / columns) * (ch + gy);
                    }
                    node(child, s, matrix, type.empty() ? w : cw, type.empty() ? h : ch, clip,
                         color, visible, bezel, full + "/" + child.at("id").string(), dt, {}, fx,
                         fy);
                    if (child.at("kind").string() != "sequence")
                        ++index;
                }
            }
            for (auto &p : n.at("outputs").object())
                output.outputs[full + "." + p.first] = eval(p.second, s, "output." + p.first);
            return;
        }
        if (!visible || color[3] <= 0 || w <= 0 || h <= 0 || clip.width <= 0 || clip.height <= 0)
            return;
        need(output.draws.size() < 512, "Widget draw budget exceeded");
        Draw draw;
        draw.path = full;
        draw.kind = kind;
        draw.transform = matrix;
        draw.bounds = {0, 0, w, h};
        draw.clip = clip;
        draw.color = color;
        draw.fill = std::clamp(number("fill", 1), 0., 1.);
        draw.fillDirection = n.at("fillDirection").string("right");
        draw.bezelOnly = bezel;
        draw.lineWidth = n.at("lineWidth").number(1);
        draw.fontSize = eval(n.at("fontSize"), s, full + ".fontSize").number(h);
        draw.align = n.at("align").string("left");
        draw.closed = n.at("closed").boolean();
        draw.points = n.at("points");
        draw.frame = eval(n.at("frame"), s, full + ".frame");
        draw.fit = n.at("fit").string("contain");
        draw.asset = format(n.at("asset").string(), s);
        draw.font = n.at("font").string("neon-hud");
        draw.text = format(n.at("text").string(), s);
        if (!n.at("frames").array().empty()) {
            auto &frames = n.at("frames").array();
            auto &playback = state.animations["$flipbook"];
            bool trigger = eval(n.at("playOn"), s, full + ".flipbook.trigger").boolean();
            if ((n.at("playOn").missing() && playback.start < 0) || (trigger && !playback.trigger))
                playback.start = state.clock;
            playback.trigger = trigger;
            if (playback.start < 0)
                return;
            auto mode = n.at("playback").string("loop");
            std::vector<size_t> order;
            for (size_t i = 0; i < frames.size(); ++i)
                order.push_back(i);
            if (mode == "pingpong" && frames.size() > 1)
                for (size_t i = frames.size() - 1; i > 1; --i)
                    order.push_back(i - 1);
            double fps = std::clamp(n.at("fps").number(12), .1, 120.);
            auto duration = [&](size_t i) {
                return std::clamp(n.at("frameDurations").at(i).number(1 / fps), .001, 60.);
            };
            double length = 0;
            for (auto i : order)
                length += duration(i);
            double elapsed = state.clock - playback.start;
            if (mode == "once" && elapsed >= length) {
                if (n.at("end").string() == "hide")
                    return;
                draw.frame = frames.back();
            } else {
                elapsed = std::fmod(elapsed, length);
                for (auto i : order) {
                    draw.frame = frames[i];
                    if (elapsed < duration(i))
                        break;
                    elapsed -= duration(i);
                }
            }
            if (std::holds_alternative<std::string>(draw.frame.data))
                draw.asset = draw.frame.string();
        }
        output.draws.push_back(std::move(draw));
    }
};
Runtime::Runtime() : impl_(std::make_unique<Impl>()) {}
Runtime::~Runtime() = default;
Runtime::Runtime(Runtime &&) noexcept = default;
Runtime &Runtime::operator=(Runtime &&) noexcept = default;
void Runtime::validate(const Value &document) {
    need(document.json().size() <= 1048576, "Widget document exceeds 1 MiB");
    if (!document.at("widgetML").missing())
        validateWidgetMLCoordinates(document);
    else {
        need(document.at("widgetVersion").number() == 1, "Expected widgetVersion 1");
        need(document.at("canvas").at(0).number() >= 1 &&
                 document.at("canvas").at(0).number() <= 8192 &&
                 document.at("canvas").at(1).number() >= 1 &&
                 document.at("canvas").at(1).number() <= 8192,
             "Invalid widget reference canvas");
    }
    auto root = expand(document.at("root"), document.at("templates").object());
    need(root.at("kind").string() == "group", "Widget root must be a group");
    int count = 0;
    checkNode(root, count, 0);
    std::function<void(const Value &)> coordinates = [&](const Value &n) {
        if (!document.at("widgetML").missing())
            need(n.at("anchor").missing(),
                 "WidgetML uses placement and local x/y, not legacy anchor");
        else
            need(n.at("placement").missing(), "Placement requires a widgetML document");
        for (auto &c : n.at("children").array())
            coordinates(c);
        if (!n.at("template").missing())
            coordinates(n.at("template"));
    };
    coordinates(root);
}
void Runtime::document(Value d) {
    validate(d);
    auto root = expand(d.at("root"), d.at("templates").object());
    impl_->source = std::move(d);
    impl_->root = std::move(root);
    reset();
}
const Value &Runtime::document() const {
    return impl_->source;
}
bool Runtime::observe(const Snapshot &s) {
    need(s.values.size() <= 256, "Widget input field budget exceeded");
    size_t budget = 0;
    std::function<void(const Value &, int)> check = [&](const Value &v, int depth) {
        need(++budget <= 8192 && depth <= 16, "Widget input complexity limit");
        need(v.array().size() <= 256 && v.object().size() <= 256 && v.string().size() <= 4096,
             "Widget input collection/string limit");
        for (auto &a : v.array())
            check(a, depth + 1);
        for (auto &a : v.object())
            check(a.second, depth + 1);
        if (auto n = std::get_if<double>(&v.data))
            need(std::isfinite(*n), "Nonfinite widget input");
    };
    for (auto &v : s.values)
        check(v.second, 0);
    auto &i = *impl_;
    if (i.hasSnapshot && i.snapshot.game == s.game && i.snapshot.session == s.session &&
        s.sequence <= i.snapshot.sequence)
        return false;
    if (!i.hasSnapshot || i.snapshot.game != s.game || i.snapshot.session != s.session)
        reset();
    i.snapshot = s;
    i.hasSnapshot = true;
    i.newSnapshot = true;
    return true;
}
void Runtime::event(std::string name, std::string groupPath) {
    need(!name.empty() && name.size() <= 128 && impl_->pendingEvents.size() < 128,
         "Invalid widget event/budget");
    if (impl_->on) {
        if (groupPath.empty())
            impl_->pendingEvents.insert(std::move(name));
        else {
            need(impl_->pendingLocal.size() < 512, "Widget local event budget exceeded");
            impl_->pendingLocal[std::move(groupPath)].insert(std::move(name));
        }
    }
}
void Runtime::enabled(bool on) {
    if (impl_->on == on)
        return;
    impl_->on = on;
    impl_->reseed = true;
    impl_->pendingEvents.clear();
    impl_->pendingLocal.clear();
    impl_->localEvents.clear();
    if (!on)
        impl_->output = {};
}
bool Runtime::enabled() const {
    return impl_->on;
}
void Runtime::reset() {
    auto &i = *impl_;
    i.states.clear();
    i.output = {};
    i.previousOutputs.clear();
    i.pendingEvents.clear();
    i.events.clear();
    i.pendingLocal.clear();
    i.localEvents.clear();
    i.seconds = 0;
    i.reseed = true;
    i.newSnapshot = false;
}
void Runtime::resetGroup(const std::string &prefix) {
    for (auto events : {&impl_->pendingLocal, &impl_->localEvents})
        for (auto it = events->begin(); it != events->end();)
            if (it->first == prefix || it->first.rfind(prefix + "/", 0) == 0)
                it = events->erase(it);
            else
                ++it;
    for (auto it = impl_->previousOutputs.begin(); it != impl_->previousOutputs.end();)
        if (it->first.rfind(prefix + ".", 0) == 0 || it->first.rfind(prefix + "/", 0) == 0)
            it = impl_->previousOutputs.erase(it);
        else
            ++it;
    for (auto it = impl_->states.begin(); it != impl_->states.end();)
        if (it->first == prefix || it->first.rfind(prefix + "/", 0) == 0)
            it = impl_->states.erase(it);
        else
            ++it;
}
const Frame &Runtime::update(double dt, const Viewport &viewport) {
    auto &i = *impl_;
    need(std::isfinite(dt) && dt >= 0 && dt <= 1, "Widget updates accept 0..1 second steps");
    need(viewport.width > 0 && viewport.height > 0 && viewport.width <= 8192 &&
             viewport.height <= 8192,
         "Invalid widget viewport");
    if (!i.on) {
        i.output = {};
        return i.output;
    }
    i.output = {};
    ++i.generation;
    i.seconds += dt;
    i.output.seconds = i.seconds;
    i.events = std::move(i.pendingEvents);
    i.pendingEvents.clear();
    i.localEvents = std::move(i.pendingLocal);
    i.pendingLocal.clear();
    i.visited.clear();
    i.disabledPrefixes.clear();
    if (i.root.missing())
        return i.output;
    int turns = (viewport.turns % 4 + 4) % 4;
    double w = turns % 2 ? viewport.height : viewport.width,
           h = turns % 2 ? viewport.width : viewport.height;
    auto native = [&](Rect r) {
        auto convert = [&](double x, double y) {
            double a = x / viewport.width, b = y / viewport.height;
            if (turns == 1)
                return std::array<double, 2>{b * w, (1 - a) * h};
            if (turns == 2)
                return std::array<double, 2>{(1 - a) * w, (1 - b) * h};
            if (turns == 3)
                return std::array<double, 2>{(1 - b) * w, a * h};
            return std::array<double, 2>{a * w, b * h};
        };
        auto a = convert(r.x, r.y), b = convert(r.x + r.width, r.y + r.height);
        return Rect{std::min(a[0], b[0]), std::min(a[1], b[1]), std::abs(a[0] - b[0]),
                    std::abs(a[1] - b[1])};
    };
    Rect region{0, 0, w, h};
    auto mode = i.source.at("region").string("viewport");
    if (mode == "gameplay" && viewport.gameplay.width > 0)
        region = native(viewport.gameplay);
    if (mode == "safe" && viewport.safe.width > 0)
        region = native(viewport.safe);
    double cw, ch;
    Matrix matrix;
    i.widgetML = !i.source.at("widgetML").missing();
    if (i.widgetML) {
        i.layout = resolveWidgetMLLayout(i.source, viewport);
        cw = i.layout.width;
        ch = i.layout.height;
        matrix = i.layout.transform;
        region = i.layout.screen;
    } else {
        cw = i.source.at("canvas").at(0).number();
        ch = i.source.at("canvas").at(1).number();
        double scale = std::min(region.width / cw, region.height / ch);
        matrix = {scale,
                  0,
                  0,
                  scale,
                  region.x + (region.width - cw * scale) * .5,
                  region.y + (region.height - ch * scale) * .5};
    }
    Impl::Scope parent;
    parent.path = "";
    Impl::State rootState;
    parent.state = &rootState;
    parent.fresh = i.reseed;
    i.node(i.root, parent, matrix, cw, ch, region, {1, 1, 1, 1}, true, mode == "bezel",
           i.root.at("id").string(), dt);
    for (auto it = i.states.begin(); it != i.states.end();) {
        bool keep = i.visited.count(it->first) != 0;
        for (auto &p : i.disabledPrefixes)
            keep |= it->first == p || it->first.rfind(p + "/", 0) == 0;
        if (!keep)
            it = i.states.erase(it);
        else
            ++it;
    }
    i.previousOutputs = i.output.outputs;
    i.reseed = false;
    i.newSnapshot = false;
    return i.output;
}
const Frame &Runtime::frame() const {
    return impl_->output;
}
double Runtime::time() const {
    return impl_->seconds;
}
} // namespace neon::widgets
