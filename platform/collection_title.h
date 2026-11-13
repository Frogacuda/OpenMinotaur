#pragma once
#include "core_api.h"
#include <memory>
namespace mino {
// Collection-only startup route. Game modules and direct launches bypass it.
class CollectionTitle final : public Core {
  public:
    CollectionTitle();
    ~CollectionTitle() override;
    const CoreInfo &info() const override;
    bool init(Host &, const SurfaceInfo &) override;
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
    uint64_t framePeriodNs() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace mino
