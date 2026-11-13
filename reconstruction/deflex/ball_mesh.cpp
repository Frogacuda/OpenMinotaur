#include "ball_mesh.h"
#include <cmath>
namespace deflex {
namespace {
void wrapRotation(double &x) {
    if (x > .5)
        x -= 1;
    if (x < -.5)
        x += 1;
}
// Rotation in the original draw uses two vec2d rotations, retaining their
// intermediate doubles instead of substituting a combined float matrix.
neon::vec3d project(const neon::vec3d &direction, double radius, neon::vec2d rotation) {
    neon::vec2d xz{direction.x * radius, direction.z * radius};
    xz.rotateBy(rotation.x * 6.2831853072 * -5.0);
    neon::vec2d yz{direction.y * radius, xz.y};
    yz.rotateBy(rotation.y * 6.2831853072 * -5.0);
    return {xz.x, yz.x, yz.y};
}
} // namespace
void BallMesh::init(neon::DarwinRandom &random, uint32_t colour, float rate) {
    depleted = 0;
    startupFrames = 50;
    lossPhase = 1;
    lossRate = rate;
    rotation = {0, 0};
    for (auto &p : points) {
        neon::matrix4f x, y;
        neon::D3DXMatrixRotationX(&x, float(random.range(-3.141592654, 3.141592654)));
        neon::D3DXMatrixRotationY(&y, float(random.range(-3.141592654, 3.141592654)));
        const auto matrix = x * y;
        neon::D3DXVECTOR3 unit{0, 1, 0};
        neon::D3DXVECTOR4 result;
        neon::D3DXVec3Transform(&result, &unit, &matrix);
        p.direction = {double(result.x), double(result.y), double(result.z)};
        p.colour = colour;
        p.radius = 10;
        p.shrink = float(random.range(.95, .97));
        p.state = 0;
        p.rotation = {0, 0}; // +21b8; origin/drift/history are supplied by derez
    }
}
void BallMesh::derez(const Entity &ball, int count, float velocityGain, float fade,
                     neon::DarwinRandom &random) {
    if (depleted == 128)
        return;
    for (int i = 0; i < count; ++i)
        if (depleted < 128) {
            auto &p = points[size_t(depleted)];
            p.state = 3;
            p.detachedRadius = p.radius;
            p.origin = ball.pos;
            p.fade = fade;
            p.drift = ball.vel * double(velocityGain);
            // Sentinel x coordinates suppress unwritten history entries. y/z are
            // never read before a real point replaces the sentinel.
            for (auto &history : p.history)
                history.x = 1000;
            p.historyCursor = 0;
            p.rotation = rotation;
            p.life = float(random.range(1, 2));
            p.growth = float(random.range(.06, .1));
            ++depleted;
        }
}
void BallMesh::restore(int count, neon::DarwinRandom &random) {
    for (int i = 0; i < count; ++i)
        if (depleted > 0) {
            // hitBy 61dbc decrements before computing all three array addresses.
            auto &p = points[size_t(--depleted)];
            p.state = 0;
            p.shrink = float(random.range(.95, .97));
            p.radius = 10;
        }
}
void BallMesh::draw(Entity &ball, ActorContext &c, ActorRenderer &renderer, neon::vec2d view,
                    uint32_t colour, bool paused, bool demo) {
    const float diameter = float(double(ball.size) * view.x + double(ball.size) * view.x);
    const float inner = float(double(diameter) * .6);
    const float shellRadius = (diameter - inner) + inner * c.ballFraction;
    if (c.levelFinished) {
        if (uint32_t(ball.age) & 32)
            ball.skipDrawFrames = 1;
        ball.colour = c.palette[0];
    }
    ball.drawEntity(renderer, view);
    const float bodyDiameter = ball.size + ball.size;
    const neon::vec2d smallSize{double(bodyDiameter) * view.x * ball.scale.x * .2,
                                double(bodyDiameter) * view.y * ball.scale.y * .2};
    if (!paused)
        rotation += ball.vel;
    wrapRotation(rotation.x);
    wrapRotation(rotation.y);
    bool anyAlive = false;
    for (size_t i = 0; i < points.size(); ++i) {
        auto &p = points[i];
        p.colour = colour;
        if (p.state == 3 && p.life <= 0)
            continue;
        anyAlive = true;
        ActorSprite sprite;
        sprite.frame = ball.frame + 2;
        sprite.angle = double(ball.angle);
        sprite.orFlags = 3;
        sprite.size = smallSize;
        neon::vec2d orbit = p.rotation, centre = ball.pos * view;
        if (p.state == 0) {
            if (!paused) {
                p.radius *= p.shrink;
                if (p.radius <= 1) {
                    p.radius = 1;
                    p.state = 2;
                }
            }
            sprite.colour = c.palette[4];
            const float pulse = std::sin(float(double(p.radius) * 3.141592654)) + 2.0f;
            sprite.size = smallSize * double(pulse);
        } else if (p.state == 2) {
            orbit = rotation;
            if (depleted == int(i)) {
                sprite.colour = c.palette[0];
                sprite.size = sprite.size * 2;
            } else
                sprite.colour.unpackRGBA(p.colour);
        }
        if (p.state != 3) {
            const auto v = project(p.direction, double(p.radius), orbit);
            sprite.position = {centre.x + v.x * double(shellRadius),
                               centre.y + v.y * double(shellRadius)};
            sprite.depth = float(double(ball.depth) + v.z * double(shellRadius));
            renderer.emit(sprite);
        }
        if (p.life <= 0)
            continue;
        // Detached particles still exist when a restored point is re-entering.
        // The original draws both until this independent life has decayed.
        centre = p.origin * view;
        const float pulse =
            std::sin(float((double(p.detachedRadius) + double(p.detachedRadius)) * 3.141592654)) +
            1.0f;
        sprite.size = smallSize * double(pulse);
        if (!paused) {
            p.detachedRadius += p.growth;
            p.growth = float(double(p.growth) * .99);
            p.life *= p.fade;
            if (double(p.life) < .01)
                p.life = 0;
        }
        sprite.colour = c.palette[0];
        orbit = p.rotation;
        if (p.detachedRadius < 10)
            sprite.colour =
                c.palette[0].lerp(c.palette[4], float(1.0 + double(p.detachedRadius) / -5.0));
        if (!paused) {
            p.rotation += p.drift;
            p.drift = p.drift * .95;
        }
        const auto v = project(p.direction, double(p.detachedRadius), orbit);
        sprite.position = {centre.x + v.x * double(shellRadius),
                           centre.y + v.y * double(shellRadius)};
        sprite.depth = float(double(ball.depth) + v.z * double(shellRadius));
        renderer.emit(sprite);
        p.history[p.historyCursor & 15] = {sprite.position.x, sprite.position.y,
                                           double(sprite.depth)};
        auto history = p.historyCursor;
        for (int j = 0; j < 10; ++j) {
            history = (history - 1) & 15;
            const auto &old = p.history[history];
            if (old.x >= 1000)
                continue;
            sprite.position = {old.x, old.y};
            sprite.depth = float(old.z);
            renderer.emit(sprite);
            sprite.size = sprite.size * .95;
        }
        if (!paused)
            ++p.historyCursor;
    }
    if (!anyAlive && !demo)
        c.levelFinished = true;
}
} // namespace deflex
