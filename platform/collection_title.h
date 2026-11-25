#pragma once
#include "core_api.h"
#include <filesystem>
#include <memory>
namespace mino {
class ShellAudio;
class Launcher;
class Display;
struct DisplaySettings;
// Collection-only startup route. Game modules and direct launches bypass it.
class CollectionTitle final : public Core {
  public:
    CollectionTitle();
    ~CollectionTitle() override;
    const CoreInfo &info() const override;
    bool init(Host &, const SurfaceInfo &) override;
    void setShaderCacheDirectory(const std::filesystem::path &);
    void setHighQuality(bool);
    void setShellAudio(ShellAudio &);
    void resize(const SurfaceInfo &) override;
    void frame(double) override;
    void touch(const TouchEvent &) override;
    void pads(const PadState *, int) override;
    void mixAudio(float *, int) override {}
    void pause(bool) override;
    void save() override {}
    bool back() override;
    void shutdown() override;
    void abandonGraphics();
    bool handoffActive() const;
    void presentHandoff(Launcher &, const Display &, const DisplaySettings &);
    uint64_t framePeriodNs() const;

  private:
    void updateInput(bool buttons, bool pointer);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace mino
