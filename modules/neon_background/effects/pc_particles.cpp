#include "pc_particles.h"
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
namespace neon::pc {
namespace {
uint32_t bits(float v) {
    uint32_t x;
    std::memcpy(&x, &v, 4);
    return x;
}
float value(uint32_t x) {
    float v;
    std::memcpy(&v, &x, 4);
    return v;
}
} // namespace
CubeFace particleBillboards(const ShapeSettings &s,
                            const std::array<PobParticle, 1024> &particles) {
    if (s.integers[14] != 0)
        throw std::runtime_error("POB object instances require another geometry path");
    CubeFace out;
    for (int i = 0; i < s.integers[0]; i++) {
        auto &a = particles[i].data;
        if (int32_t(bits(a[15])) < 0)
            continue;
        float hw = (s.floats[3] + 1) * a[9] * 128, hh = (s.floats[4] + 1) * a[10] * 128,
              c = std::cos(a[18]), sn = std::sin(a[18]), z = (s.integers[13] & 2) ? -a[2] : a[2];
        uint32_t colour = bits(a[16]);
        std::array<CubeVertex, 4> q;
        for (int k = 0; k < 4; k++) {
            float x = (k == 1 || k == 2) ? hw : -hw, y = k >= 2 ? hh : -hh,
                  u = k == 1 || k == 2 ? 1.f : 0.f, v = k >= 2 ? 1.f : 0.f;
            q[k] = {colour, u, v, u, v, x * c - y * sn + a[0], y * c + x * sn + a[1], z};
        }
        for (int k : {0, 1, 2, 2, 3, 0})
            out.push_back(q[k]);
    }
    return out;
}
void ParticlePlayback::reset() {
    particles_ = {};
    countdown_ = -1;
    rng_ = 1;
    ready_ = false;
}
void ParticlePlayback::readState(const std::filesystem::path &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.read((char *)particles_.data(), sizeof particles_) || !f.read((char *)&countdown_, 4))
        throw std::runtime_error("Invalid captured particle state");
    ready_ = true;
}
float ParticlePlayback::random(float scale) {
    rng_ = rng_ * 214013u + 2531011u;
    return float((rng_ >> 16) & 32767) * .000030517578125f * scale;
}
void ParticlePlayback::spawn(PobParticle &p, const ShapeSettings &s,
                             const std::array<uint32_t, 256> &palette) {
    auto &a = p.data;
    auto &n = s.integers;
    auto &f = s.floats;
    if (!(n[13] & 1)) {
        a[0] = random(float(n[3])) + f[0] - float(n[3]) * .5f;
        a[1] = random(float(n[4])) + f[1] - float(n[4]) * .5f;
        a[2] = -(random(0) + f[2]);
    } else {
        float theta = random(6.2831855f), r = random(float(n[3] - n[4])) + float(n[4]);
        a[0] = std::sin(theta) * r + f[0];
        a[1] = std::cos(theta) * r + f[1];
        a[2] = -f[2];
    }
    a[3] = random(f[7] * 2) - f[7] + f[6];
    a[4] = random(f[9] * 2) - f[9] + f[8];
    a[5] = random(f[11] * 2) - f[11] - f[10];
    a[6] = f[12];
    a[7] = f[13];
    a[8] = -f[14];
    for (int k = 0; k < 3; k++)
        a[12 + k] = std::abs(a[6 + k]) > .01f ? random(6.2831855f) : 0;
    a[9] = f[3];
    a[10] = f[4];
    a[11] = f[5];
    a[15] = value(0);
    a[16] = value((palette[unsigned(n[5]) & 255] & 0xffff0000u) | (uint32_t(n[12]) << 8));
    a[17] = 0;
    a[18] = (float(n[10]) * .0001220703125f + random(float(n[11]) * .0001220703125f)) -
            float(n[11]) * .0001220703125f * .5f;
    a[19] = a[18];
}
void ParticlePlayback::update(const ShapeSettings &s, const std::array<uint32_t, 256> &palette) {
    auto &n = s.integers;
    auto &f = s.floats;
    --countdown_;
    int created = 0;
    for (int i = 0; i < n[0]; i++) {
        auto &p = particles_[i];
        auto &a = p.data;
        if (int32_t(bits(a[15])) < 0 && countdown_ < 0 && created < n[1]) {
            spawn(p, s, palette);
            ++created;
        }
        if (int32_t(bits(a[15])) >= 0) {
            for (int k = 0; k < 3; k++) {
                a[k] += a[3 + k];
                a[12 + k] += a[6 + k];
            }
            float fade = a[5] * 16;
            if (a[2] > -f[2] || a[2] < f[2])
                a[15] = value(0xffffffffu);
            a[18] += a[19];
            if (fade != 0) {
                if (fade <= 0) {
                    if (f[2] - fade <= a[2]) {
                        if (-f[2] + fade < a[2])
                            a[17] = ((-f[2] - a[2]) / fade) * -255;
                    } else
                        a[17] = ((a[2] - f[2]) / fade) * -255;
                } else {
                    if (fade + f[2] <= a[2]) {
                        if (-f[2] - fade < a[2])
                            a[17] = ((-f[2] - a[2]) / fade) * 255;
                    } else
                        a[17] = ((a[2] - f[2]) / fade) * 255;
                }
                int alpha = int(a[17]);
                a[16] = value((bits(a[16]) & 0xffffff00u) | (uint32_t(alpha * n[8] >> 8) & 255));
            }
        }
    }
    if (countdown_ < 0)
        countdown_ = n[2];
}
void ParticlePlayback::tick(const ShapeSettings &s, const std::array<uint32_t, 256> &palette) {
    if (s.integers[0] < 0 || s.integers[0] > 1024 || s.integers[1] < 0 || s.integers[1] > 1024)
        throw std::runtime_error("Particle population outside original bounds");
    if (!ready_) {
        for (auto &p : particles_)
            p.data[15] = value(0xffffffffu);
        countdown_ = -1;
        for (int i = 0; i < 600; i++)
            update(s, palette);
        ready_ = true;
    }
    update(s, palette);
}
} // namespace neon::pc
