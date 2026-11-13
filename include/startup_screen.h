#pragma once
#include "core_api.h"
#include "neon/render_device.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <vector>

namespace mino {
// Common host accommodation: original launch art precedes game initialization.
// The minimum is wall time from first presentation, never simulated game time.
class StartupScreen {
    using Clock = std::chrono::steady_clock;
    std::unique_ptr<neon::render::NeonDevice> device_;
    neon::render::NeonBaseRender draw_;
    SurfaceInfo surface_;
    Clock::time_point first_{};
    bool presented_ = false, waitNeutral_ = false;
    std::vector<uint32_t> blockedTouches_;
    void markPresented() {
        first_ = Clock::now();
        presented_ = true;
    }
    void render() {
        using namespace neon::render;
        auto &d = *device_;
        d.BeginFrame();
        d.SetCurrenRT(-1);
        d.alphaEnable(false);
        d.SetupDefaultProjection(surface_.width, surface_.height, PROJ_ORTHO);
        d.ClearScreen(255, 0, 0, 0);
        draw_.SetCurrenRenderTarget(-1);
        draw_.SetSourceFromRTs(-1, -1, -1);
        draw_.SetSourceFromFont(-1, -1, -1);
        draw_.SetSourceFromLiterals(0, -1, -1);
        const auto *t = d.literal(0);
        const float aspect = surface_.displayAspect > 0 ? surface_.displayAspect
                                                        : float(surface_.width) / surface_.height;
        const float imageAspect = float(t->imageW) / t->imageH;
        const float fw = std::min(1.f, imageAspect / aspect),
                    fh = std::min(1.f, aspect / imageAspect);
        const float w = surface_.width * fw, h = surface_.height * fh,
                    x = (surface_.width - w) * .5f, y = (surface_.height - h) * .5f;
        const float u = float(t->imageW) / t->width, v = float(t->imageH) / t->height;
        NeonVertex q[6] = {{x, -y, 610, 0, 0, 0, 0, 0, 0, 0xffffffffu},
                           {x, -y - h, 610, 0, v, 0, v, 0, v, 0xffffffffu},
                           {x + w, -y - h, 610, u, v, u, v, u, v, 0xffffffffu},
                           {x + w, -y - h, 610, u, v, u, v, u, v, 0xffffffffu},
                           {x + w, -y, 610, u, 0, u, 0, u, 0, 0xffffffffu},
                           {x, -y, 610, 0, 0, 0, 0, 0, 0, 0xffffffffu}};
        neon::D3DXMatrixTranslation(&draw_.world, surface_.width * -.5f, surface_.height * .5f, 0);
        draw_.UpdateBufferFromMem(q, 6);
        draw_.Render(6);
        d.PresentFrame();
    }

  public:
    ~StartupScreen() {
        release();
    }
    bool begin(Host &host, const SurfaceInfo &surface) {
        release();
        surface_ = surface;
        presented_ = waitNeutral_ = false;
        blockedTouches_.clear();
        device_ = std::make_unique<neon::render::NeonDevice>();
        device_->setHost(&host);
        if (device_->Init(0, surface.width, surface.height, surface.framebuffer, surface.width,
                          surface.height, 0))
            return false;
        device_->InitAllShaders();
        device_->InitLiteralTextures(1, {"Launch.png"});
        if (!device_->literal(0) || !device_->literal(0)->name) {
            host.log("Original Launch.png could not be loaded");
            return false;
        }
        draw_.Init(device_.get());
        if (draw_.GetVertexBufferWO(0, 6, -1, false))
            return false;
        draw_.initialised = 1;
        draw_.shader = 0;
        draw_.primitive = 4;
        for (auto &m : draw_.texMatrix)
            neon::D3DXMatrixIdentity(&m);
        render();
        if (host.presentStartupFrame())
            markPresented();
        return true;
    }
    bool blocking() const {
        return device_ && (!presented_ || Clock::now() - first_ < std::chrono::seconds(1));
    }
    bool frame(double &dt) {
        if (!device_)
            return false;
        if (!presented_) {
            render();
            markPresented();
            return true;
        }
        if (blocking()) {
            render();
            return true;
        }
        release();
        dt = 1. / 60;
        return false; // Loading time is not simulation catch-up.
    }
    void resize(const SurfaceInfo &s) {
        surface_ = s;
        if (device_)
            device_->SetBackbuffer(s.framebuffer, s.width, s.height, s.width, s.height);
    }
    bool pads(const PadState *p, int count) {
        bool held = false;
        for (int i = 0; p && i < count; ++i)
            if (p[i].connected)
                held |= p[i].buttons || std::abs(p[i].lx) > .25f || std::abs(p[i].ly) > .25f ||
                        std::abs(p[i].rx) > .25f || std::abs(p[i].ry) > .25f;
        if (blocking()) {
            waitNeutral_ = held;
            return true;
        }
        if (waitNeutral_) {
            waitNeutral_ = held;
            return true;
        }
        return false;
    }
    bool touch(const TouchEvent &e) {
        auto it = std::find(blockedTouches_.begin(), blockedTouches_.end(), e.id);
        const bool blocked = blocking() || it != blockedTouches_.end();
        if (e.phase == TouchPhase::Down && blocking() && it == blockedTouches_.end())
            blockedTouches_.push_back(e.id);
        if ((e.phase == TouchPhase::Up || e.phase == TouchPhase::Cancel) &&
            it != blockedTouches_.end())
            blockedTouches_.erase(it);
        return blocked;
    }
    void release() {
        if (device_) {
            draw_.device = nullptr;
            draw_.vertexBuffer = draw_.renderTarget = -1;
            device_->Destroy(true);
            device_.reset();
        }
    }
};
} // namespace mino
