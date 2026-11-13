#include "value.h"
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
namespace neon::widgets {
namespace {
const Value null;
const Value::Array emptyArray;
const Value::Object emptyObject;
void need(bool v, const char *s) {
    if (!v)
        throw std::runtime_error(s);
}
struct Parser {
    const std::string &s;
    size_t p = 0, nodes = 0;
    void space() {
        while (p < s.size() && std::isspace((unsigned char)s[p]))
            ++p;
    }
    char take() {
        need(p < s.size(), "Unexpected end of widget JSON");
        return s[p++];
    }
    std::string text() {
        need(take() == '"', "Expected JSON string");
        std::string out;
        while (true) {
            char c = take();
            if (c == '"')
                break;
            if (c == '\\') {
                c = take();
                if (c == 'n')
                    out += '\n';
                else if (c == 'r')
                    out += '\r';
                else if (c == 't')
                    out += '\t';
                else if (c == '"' || c == '\\' || c == '/')
                    out += c;
                else if (c == 'u') {
                    uint32_t v = 0;
                    for (int i = 0; i < 4; ++i) {
                        char h = take();
                        int n = h >= '0' && h <= '9'   ? h - '0'
                                : h >= 'a' && h <= 'f' ? h - 'a' + 10
                                : h >= 'A' && h <= 'F' ? h - 'A' + 10
                                                       : -1;
                        need(n >= 0, "Invalid JSON Unicode escape");
                        v = v * 16 + n;
                    }
                    if (v >= 0xd800 && v <= 0xdbff) {
                        need(take() == '\\' && take() == 'u', "Missing Unicode surrogate");
                        uint32_t low = 0;
                        for (int i = 0; i < 4; ++i) {
                            char h = take();
                            int n = h >= '0' && h <= '9'   ? h - '0'
                                    : h >= 'a' && h <= 'f' ? h - 'a' + 10
                                    : h >= 'A' && h <= 'F' ? h - 'A' + 10
                                                           : -1;
                            need(n >= 0, "Invalid surrogate");
                            low = low * 16 + n;
                        }
                        need(low >= 0xdc00 && low <= 0xdfff, "Invalid low surrogate");
                        v = 0x10000 + ((v - 0xd800) << 10) + (low - 0xdc00);
                    } else
                        need(v < 0xdc00 || v > 0xdfff, "Unexpected low surrogate");
                    if (v < 128)
                        out += char(v);
                    else if (v < 2048) {
                        out += char(0xc0 | (v >> 6));
                        out += char(0x80 | (v & 63));
                    } else if (v < 65536) {
                        out += char(0xe0 | (v >> 12));
                        out += char(0x80 | ((v >> 6) & 63));
                        out += char(0x80 | (v & 63));
                    } else {
                        out += char(0xf0 | (v >> 18));
                        out += char(0x80 | ((v >> 12) & 63));
                        out += char(0x80 | ((v >> 6) & 63));
                        out += char(0x80 | (v & 63));
                    }
                } else
                    throw std::runtime_error("Unsupported JSON escape");
            } else {
                need((unsigned char)c >= 32, "Control character in JSON string");
                out += c;
            }
            need(out.size() <= 65536, "Widget string too long");
        }
        return out;
    }
    Value value(int depth = 0) {
        need(depth < 48 && ++nodes <= 32768, "Widget JSON complexity limit");
        space();
        need(p < s.size(), "Missing JSON value");
        char c = s[p];
        if (c == '"')
            return text();
        if (c == '{' || c == '[') {
            ++p;
            bool obj = c == '{';
            Value::Object o;
            Value::Array a;
            space();
            if (p < s.size() && s[p] == (obj ? '}' : ']')) {
                ++p;
                return obj ? Value(o) : Value(a);
            }
            while (true) {
                space();
                if (obj) {
                    auto key = text();
                    space();
                    need(take() == ':', "Expected colon");
                    need(!o.count(key), "Duplicate JSON key");
                    o.emplace(std::move(key), value(depth + 1));
                } else
                    a.push_back(value(depth + 1));
                space();
                char end = take();
                if (end == (obj ? '}' : ']'))
                    break;
                need(end == ',', "Expected JSON comma");
            }
            return obj ? Value(o) : Value(a);
        }
        for (auto word : {"true", "false", "null"})
            if (s.compare(p, std::string(word).size(), word) == 0) {
                p += std::string(word).size();
                return word[0] == 'n' ? Value{} : Value(word[0] == 't');
            }
        size_t begin = p;
        if (c == '-')
            ++p;
        need(p < s.size() && std::isdigit((unsigned char)s[p]), "Invalid JSON number");
        if (s[p] == '0')
            ++p;
        else
            while (p < s.size() && std::isdigit((unsigned char)s[p]))
                ++p;
        bool real = false;
        if (p < s.size() && s[p] == '.') {
            real = true;
            ++p;
            size_t q = p;
            while (p < s.size() && std::isdigit((unsigned char)s[p]))
                ++p;
            need(p > q, "Invalid fraction");
        }
        if (p < s.size() && (s[p] == 'e' || s[p] == 'E')) {
            real = true;
            ++p;
            if (p < s.size() && (s[p] == '+' || s[p] == '-'))
                ++p;
            size_t q = p;
            while (p < s.size() && std::isdigit((unsigned char)s[p]))
                ++p;
            need(p > q, "Invalid exponent");
        }
        auto number = s.substr(begin, p - begin);
        if (real) {
            double v = std::stod(number);
            need(std::isfinite(v), "Nonfinite JSON number");
            return v;
        }
        if (number[0] == '-')
            return int64_t(std::stoll(number));
        return uint64_t(std::stoull(number));
    }
};
} // namespace
bool Value::missing() const {
    return std::holds_alternative<std::monostate>(data);
}
bool Value::boolean(bool fallback) const {
    if (auto v = std::get_if<bool>(&data))
        return *v;
    if (auto v = std::get_if<int64_t>(&data))
        return *v != 0;
    if (auto v = std::get_if<uint64_t>(&data))
        return *v != 0;
    if (auto v = std::get_if<double>(&data))
        return *v != 0;
    return fallback;
}
double Value::number(double fallback) const {
    if (auto v = std::get_if<double>(&data))
        return *v;
    if (auto v = std::get_if<int64_t>(&data))
        return double(*v);
    if (auto v = std::get_if<uint64_t>(&data))
        return double(*v);
    if (auto v = std::get_if<bool>(&data))
        return *v ? 1 : 0;
    return fallback;
}
std::string Value::string(std::string fallback) const {
    if (auto v = std::get_if<std::string>(&data))
        return *v;
    if (missing())
        return fallback;
    if (auto v = std::get_if<bool>(&data))
        return *v ? "true" : "false";
    if (auto v = std::get_if<int64_t>(&data))
        return std::to_string(*v);
    if (auto v = std::get_if<uint64_t>(&data))
        return std::to_string(*v);
    if (auto v = std::get_if<double>(&data)) {
        std::ostringstream s;
        s << std::setprecision(12) << *v;
        return s.str();
    }
    return fallback;
}
const Value::Array &Value::array() const {
    auto p = std::get_if<Array>(&data);
    return p ? *p : emptyArray;
}
const Value::Object &Value::object() const {
    auto p = std::get_if<Object>(&data);
    return p ? *p : emptyObject;
}
Value::Array &Value::array() {
    if (!std::holds_alternative<Array>(data))
        data = Array{};
    return std::get<Array>(data);
}
Value::Object &Value::object() {
    if (!std::holds_alternative<Object>(data))
        data = Object{};
    return std::get<Object>(data);
}
const Value &Value::at(const std::string &key) const {
    auto &o = object();
    auto p = o.find(key);
    return p == o.end() ? null : p->second;
}
Value &Value::operator[](const std::string &key) {
    return object()[key];
}
const Value &Value::at(size_t i) const {
    return i < array().size() ? array()[i] : null;
}
int compare(const Value &a, const Value &b) {
    if (auto x = std::get_if<int64_t>(&a.data)) {
        if (auto y = std::get_if<int64_t>(&b.data))
            return *x < *y ? -1 : *x > *y ? 1 : 0;
        if (auto y = std::get_if<uint64_t>(&b.data))
            return *x < 0 ? -1 : uint64_t(*x) < *y ? -1 : uint64_t(*x) > *y ? 1 : 0;
    }
    if (auto x = std::get_if<uint64_t>(&a.data)) {
        if (auto y = std::get_if<uint64_t>(&b.data))
            return *x < *y ? -1 : *x > *y ? 1 : 0;
        if (auto y = std::get_if<int64_t>(&b.data))
            return *y < 0 ? 1 : *x < uint64_t(*y) ? -1 : *x > uint64_t(*y) ? 1 : 0;
    }
    if (std::holds_alternative<std::string>(a.data) &&
        std::holds_alternative<std::string>(b.data)) {
        auto x = a.string(), y = b.string();
        return x < y ? -1 : x > y ? 1 : 0;
    }
    double x = a.number(), y = b.number();
    return x < y ? -1 : x > y ? 1 : 0;
}
bool Value::operator==(const Value &v) const {
    if (data.index() == v.data.index())
        return data == v.data;
    if ((std::holds_alternative<int64_t>(data) || std::holds_alternative<uint64_t>(data) ||
         std::holds_alternative<double>(data)) &&
        (std::holds_alternative<int64_t>(v.data) || std::holds_alternative<uint64_t>(v.data) ||
         std::holds_alternative<double>(v.data)))
        return compare(*this, v) == 0;
    return false;
}
Value Value::parse(const std::string &text) {
    need(text.size() <= 1048576, "Widget document exceeds 1 MiB");
    Parser p{text};
    auto v = p.value();
    p.space();
    need(p.p == text.size(), "Unexpected JSON suffix");
    return v;
}
std::string Value::json(int indent) const {
    if (missing())
        return "null";
    if (auto s = std::get_if<std::string>(&data)) {
        std::ostringstream out;
        out << '"';
        for (unsigned char c : *s) {
            if (c == '"' || c == '\\')
                out << '\\' << c;
            else if (c == '\n')
                out << "\\n";
            else if (c == '\r')
                out << "\\r";
            else if (c == '\t')
                out << "\\t";
            else if (c < 32)
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(c) << std::dec;
            else
                out << c;
        }
        out << '"';
        return out.str();
    }
    if (auto a = std::get_if<Array>(&data)) {
        std::string s = "[";
        for (size_t i = 0; i < a->size(); ++i) {
            if (i)
                s += ", ";
            s += (*a)[i].json(indent);
        }
        return s + "]";
    }
    if (auto o = std::get_if<Object>(&data)) {
        std::string s = "{";
        size_t i = 0;
        for (auto &p : *o) {
            s += i++ ? ",\n" : "\n";
            s += std::string(indent + 2, ' ') + Value(p.first).json() + ": " +
                 p.second.json(indent + 2);
        }
        return s + (o->empty() ? "" : "\n" + std::string(indent, ' ')) + "}";
    }
    if (auto v = std::get_if<double>(&data)) {
        need(std::isfinite(*v), "Nonfinite value");
        std::ostringstream s;
        s << std::setprecision(17) << *v;
        return s.str();
    }
    return string();
}
} // namespace neon::widgets
