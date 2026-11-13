#include "neon/render_device.h"
#include "legacy_render_device.h"
#include <glad/gl.h>
#include <iostream>
#include <vector>

namespace {
std::vector<double> trace;
GLuint nextId = 1;
int clears = 0;
bool complete = true;
template <class... T> void event(int code, T... values) {
    trace.push_back(code);
    (trace.push_back(double(values)), ...);
}
void GLAD_API_PTR gen(GLsizei n, GLuint *ids) {
    for (int i = 0; i < n; i++)
        ids[i] = nextId++;
    event(1, n);
}
void GLAD_API_PTR bindFbo(GLenum kind, GLuint id) {
    event(2, kind, id);
}
void GLAD_API_PTR bindTexture(GLenum kind, GLuint id) {
    event(3, kind, id);
}
void GLAD_API_PTR parameter(GLenum a, GLenum b, GLint c) {
    event(4, a, b, c);
}
void GLAD_API_PTR image(GLenum a, GLint b, GLint c, GLsizei w, GLsizei h, GLint d, GLenum e,
                        GLenum f, const void *) {
    event(5, a, b, c, w, h, d, e, f);
}
void GLAD_API_PTR attach(GLenum a, GLenum b, GLenum c, GLuint d, GLint e) {
    event(6, a, b, c, d, e);
}
GLenum GLAD_API_PTR status(GLenum a) {
    event(7, a);
    return complete ? GL_FRAMEBUFFER_COMPLETE : GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
}
void GLAD_API_PTR deleted(GLsizei n, const GLuint *ids) {
    event(8, n);
    for (int i = 0; i < n; i++)
        event(9, ids[i]);
}
void GLAD_API_PTR viewport(GLint x, GLint y, GLsizei w, GLsizei h) {
    event(10, x, y, w, h);
}
void GLAD_API_PTR colour(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    event(11, r, g, b, a);
}
void GLAD_API_PTR clear(GLbitfield mask) {
    clears++;
    event(12, mask);
}
void GLAD_API_PTR disable(GLenum a) {
    event(13, a);
}
void GLAD_API_PTR use(GLuint a) {
    event(14, a);
}
void GLAD_API_PTR buffer(GLenum a, GLuint b) {
    event(15, a, b);
}
void install() {
    glad_glGenFramebuffers = gen;
    glad_glGenTextures = gen;
    glad_glBindFramebuffer = bindFbo;
    glad_glBindTexture = bindTexture;
    glad_glTexParameteri = parameter;
    glad_glTexImage2D = image;
    glad_glFramebufferTexture2D = attach;
    glad_glCheckFramebufferStatus = status;
    glad_glDeleteTextures = deleted;
    glad_glDeleteFramebuffers = deleted;
    glad_glViewport = viewport;
    glad_glClearColor = colour;
    glad_glClear = clear;
    glad_glDisable = disable;
    glad_glUseProgram = use;
    glad_glBindBuffer = buffer;
}
template <class Device> std::vector<double> exercise() {
    trace.clear();
    nextId = 1;
    clears = 0;
    complete = true;
    Device device;
    event(20, device.Init(0, 480, 320, 77, 960, 640, 0));
    int a = device.GetRTTexture(128, 64), b = device.GetRTTexture(256, 128);
    event(21, a, b);
    event(22, device.SetCurrenRT(a));
    int w = 0, h = 0;
    device.GetCurrentRtSize(&w, &h);
    event(23, w, h);
    device.FillSurface(b, 0);
    device.SetCurrenRT(-1);
    event(24, device.ReleaseRTTexture(a));
    event(25, device.GetRTTexture(128, 64));
    device.SetBackbuffer(78, 480, 320, 1440, 960);
    device.setUpsideDown(true);
    device.SetupDefaultProjection(480, 320, 1);
    for (auto &row : device.projection.m)
        for (float x : row)
            event(26, x);
    device.ReleaseRTTexture(a);
    device.ReleaseRTTexture(b);
    device.Destroy(true);
    return trace;
}
} // namespace
int main() {
    install();
    const auto old = exercise<frozen::render::NeonDevice>();
    const auto shared = exercise<neon::render::NeonDevice>();
    if (old != shared) {
        std::cerr << "Renderer GL trace changed\n";
        return 1;
    }
    trace.clear();
    clears = 0;
    complete = true;
    neon::render::NeonDevice cleared;
    int rt = cleared.GetRTTexture(64, 32);
    if (rt < 0 || clears != 0)
        return 1;
    cleared.ReleaseRTTexture(rt);
    cleared.GetRTTexture(64, 32);
    if (clears != 0)
        return 1; // reusing a target must retain its contents
    cleared.SetBackbuffer(0, 480, 320, 960, 640);
    if (clears != 0)
        return 1; // resizing allocates storage, without a speculative clear
    cleared.FillSurface(rt, 0);
    if (clears != 1)
        return 1;
    cleared.ClearScreen(255, 1, 2, 3);
    if (clears != 2)
        return 1;
    cleared.Destroy(true);
    complete = false;
    clears = 0;
    neon::render::NeonDevice failed;
    if (failed.GetRTTexture(32, 32) != -1 || clears != 0)
        return 1;
    failed.Destroy(true);
    std::cout
        << "Renderer parity: " << shared.size()
        << " GL trace values; allocation, reuse, resize, projection, teardown and explicit clears passed\n";
}
