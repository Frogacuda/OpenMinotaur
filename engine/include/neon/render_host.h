#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace neon {
// Device-facing host services, independent of application and game classes.
class RenderHost {
  public:
    virtual ~RenderHost() = default;
    virtual bool readAsset(const std::string &name, std::vector<uint8_t> &out) const = 0;
    virtual std::string adaptShader(const std::string &source, int stage) const = 0;
    virtual void log(const char *format, ...) = 0;
};
} // namespace neon
