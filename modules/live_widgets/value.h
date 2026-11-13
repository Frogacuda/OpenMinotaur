#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>
namespace neon::widgets {
struct Value {
    using Array = std::vector<Value>;
    using Object = std::map<std::string, Value>;
    std::variant<std::monostate, bool, int64_t, uint64_t, double, std::string, Array, Object> data;
    Value() = default;
    Value(bool v) : data(v) {}
    Value(int v) : data(int64_t(v)) {}
    Value(int64_t v) : data(v) {}
    Value(uint64_t v) : data(v) {}
    Value(double v) : data(v) {}
    Value(float v) : data(double(v)) {}
    Value(const char *v) : data(std::string(v)) {}
    Value(std::string v) : data(std::move(v)) {}
    Value(Array v) : data(std::move(v)) {}
    Value(Object v) : data(std::move(v)) {}
    bool missing() const;
    bool boolean(bool fallback = false) const;
    double number(double fallback = 0) const;
    std::string string(std::string fallback = "") const;
    const Array &array() const;
    const Object &object() const;
    Array &array();
    Object &object();
    const Value &at(const std::string &) const;
    Value &operator[](const std::string &);
    const Value &at(size_t) const;
    bool operator==(const Value &) const;
    bool operator!=(const Value &v) const {
        return !(*this == v);
    }
    static Value parse(const std::string &);
    std::string json(int indent = 0) const;
};
// Exact integer comparisons precede floating conversion. Returns -1/0/1.
int compare(const Value &, const Value &);
} // namespace neon::widgets
