#pragma once
#include "core_api.h"
#include <filesystem>
#include <memory>
namespace mino {
// Optional collection service. Game adapters provide values, never renderer commands.
class LiveBezel {
  public:
    LiveBezel();
    ~LiveBezel();
    void update(const std::string &game, PresentationState, double elapsed, bool lowQuality);
    bool draw(unsigned framebuffer, int width, int height, int x, int y, int w, int h, int turns,
              bool exclusivePass = false);
    void setShaderCacheDirectory(const std::filesystem::path &);
    void setGameplayTexture(unsigned texture, int width, int height);
    void beginCadenceAssessment();
    void finishCadenceAssessment();
    bool assessingCadence() const;
    void discardPendingFrame();
    void observeFrame(double workMs, double intervalMs);
    unsigned cadenceDivisor() const;
    bool refreshedFrame() const;
    bool available() const;
    void reset();
    void abandonGraphics();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace mino
