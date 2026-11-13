#include "launcher.h"
#include <glad/gl.h>

namespace mino {
void Launcher::resize(const SurfaceInfo &surface) {
    if (appLaunch_.active() &&
        (surface.width != surface_.width || surface.height != surface_.height)) {
        const auto destination = launchPage_;
        const int initialFocus = launchFocus_;
        cancelAppLaunch();
        open(destination, initialFocus);
    }
    surface_ = surface;
}
void Launcher::externalLaunchProgress(float progress) {
    externalLaunch_ = progress >= 0;
    if (externalLaunch_)
        exitPose_ = iox::AppLaunch::homePose(std::clamp(progress, 0.f, 1.f));
}
void Launcher::cancelAppLaunch() {
    appLaunch_.cancel();
    if (appFramebuffer_)
        glDeleteFramebuffers(1, &appFramebuffer_);
    if (appTexture_)
        glDeleteTextures(1, &appTexture_);
    appFramebuffer_ = appTexture_ = 0;
}
void Launcher::openApp(Page destination, int initialFocus) {
    if (inputBlocked())
        return;
    const auto homeNavigation = navigation_;
    const auto homeSurface = surface_;
    open(destination, initialFocus);
    glGenTextures(1, &appTexture_);
    glBindTexture(GL_TEXTURE_2D, appTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface_.width, surface_.height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &appFramebuffer_);
    glBindFramebuffer(GL_FRAMEBUFFER, appFramebuffer_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, appTexture_, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        cancelAppLaunch();
        return; // Keep the destination usable if capture is unavailable.
    }
    surface_.framebuffer = appFramebuffer_;
    frame(0); // Capture the actual page, including bars and its initial focus.
    surface_ = homeSurface;
    navigation_ = homeNavigation;
    launchPage_ = destination;
    launchFocus_ = initialFocus;
    exitPose_ = iox::AppLaunch::homePose(0);
    appLaunch_.start();
    pause(false);
}
void Launcher::drawAppLaunch() {
    const float p = appLaunch_.progress(), scale = iox::AppLaunch::imageScale(p);
    const float width = layout_.width * scale, height = layout_.height * scale;
    const Colour colour{1, 1, 1, iox::AppLaunch::imageProgress(p)};
    // FBO images have bottom-up UVs and their stored alpha is not display opacity.
    quad({(layout_.width - width) * .5f, (layout_.height - height) * .5f, width, height}, 0, 1, 1,
         0, colour, colour);
    draw(appTexture_, false, 0, false, 0, false, ImageMode::Capture);
}
} // namespace mino
