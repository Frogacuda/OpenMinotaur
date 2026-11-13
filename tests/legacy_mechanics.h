// Saved reference functions for differential tests only. No production callers.
#pragma once
#include <cmath>
namespace frozen_mechanics {
template <class Self> void mtBaseDown(Self &self, unsigned id, typename Self::PointType p) {
    using vec2d = typename Self::PointType;
    if (self.inputFlipped)
        p = p * -1.0;
    int i = 0;
    while (i < 4 && self.touchActive[i])
        i++;
    if (i < 4) {
        self.touchFlag[i] = false;
        self.touchActive[i] = true;
        self.touchIdleFrames[i] = 0;
        self.touchPos[i] = p;
        self.touchId[i] = id;
        self.touchStartFrame[i] = 0;
        self.touchStartPos[i] = p;
        self.touchPrevPos[i] = p;
    }
    if (!self.joyActive[0]) {
        self.joyActive[0] = true;
        self.joyOrigin[0] = p;
        self.joyDelta[0] = vec2d(0, 0);
        self.joyTouchId[0] = id;
    }
}
template <class Self> void mtBaseMove(Self &self, unsigned id, typename Self::PointType p) {
    using vec2d = typename Self::PointType;
    if (self.inputFlipped)
        p = p * -1.0;
    for (int i = 0; i < 4; i++) {
        if (!self.touchActive[i] || self.touchId[i] != id)
            continue;
        vec2d last = self.touchLast[i];
        self.touchIdleFrames[i] = 0;
        int n = ++self.touchStartFrame[i];
        self.touchPos[i] = p;
        self.touchLast[i] = p;
        self.touchMove[i] = vec2d(last.x - p.x, last.y - p.y);
        if (n > 1)
            self.touchFlag[i] = true;
    }
    for (int i = 0; i < 4; i++)
        if (self.joyTouchId[i] == id) {
            self.joyDelta[i] = p - self.joyOrigin[i];
            return;
        }
}
template <class Self> void mtGameDown(Self &self, unsigned id, typename Self::PointType p) {
    using vec2d = typename Self::PointType;
    if (self.inputFlipped)
        p = p * -1.0;
    if (!self.pauseArmed) {
        self.paused = false;
        self.f2d4 = 0;
    } else if (p.y < -0.4) {
        self.paused = true; // a touch in the score bar
    }
    int i = 0;
    while (i < 4 && self.touchActive[i])
        i++;
    if (i < 4) {
        self.touchActive[i] = true;
        self.touchIdleFrames[i] = 0;
        self.touchId[i] = id;
        self.touchPos[i] = p;
        self.touchStartFrame[i] = self.frameCount;
        self.touchStartPos[i] = p;
    }
    if (!self.joyActive[0]) {
        self.joyActive[0] = true;
        self.joyTouchId[0] = id;
        self.joyOrigin[0] = p;
        self.joyDelta[0] = vec2d(0, 0);
    }
}
template <class Self> void mtGameMove(Self &self, unsigned id, typename Self::PointType p) {
    using vec2d = typename Self::PointType;
    if (self.inputFlipped)
        p = p * -1.0;
    for (int i = 0; i < 4; i++)
        if (self.touchActive[i] && self.touchId[i] == id) {
            self.touchPos[i] = p;
            self.touchIdleFrames[i] = 0;
        }
    for (int i = 0; i < 4; i++)
        if (self.joyTouchId[i] == id) {
            self.joyDelta[i] = p - self.joyOrigin[i];
            return;
        }
}
template <class Self> void rescueDown(Self &self, unsigned id, typename Self::PointType p) {
    using vec2d = typename Self::PointType;
    if (!self.pauseArmed) {
        self.paused = false;
        self.pauseTapTimer = 0;
    } else if (std::sqrt(p.y * p.y + p.x * p.x) < 0.05) {
        if (self.pauseTapTimer < 1) {
            self.pauseTapTimer = 30;
            self.flash = 1.0f;
        } else {
            self.paused = true;
            self.flash = 0;
        }
    }
    if (self.inputFlipped)
        p = p * -1.0;
    int i = 0;
    while (i < 4 && self.touchActive[i])
        i++;
    if (i < 4) {
        self.touchActive[i] = true;
        self.touchPos[i] = p;
        self.touchId[i] = id;
    }
    int zone;
    if (self.joyZones == 2)
        zone = p.x > 0.0;
    else if (self.joyZones < 2 || self.joyZones > 4)
        zone = 0;
    else
        zone = ((p.y > 0.0) != (p.x > 0.0)) * 2 + (p.x > 0.0);
    if (self.demo)
        zone = 0;
    if (!self.joyActive[zone] && !self.controllers.motion(zone).active) {
        self.joyActive[zone] = true;
        self.joyOrigin[zone] = p;
        self.joyDelta[zone] = vec2d(0, 0);
        self.joyTouchId[zone] = id;
    }
}
template <class Self> void rescueMove(Self &self, unsigned id, typename Self::PointType p) {
    using vec2d = typename Self::PointType;
    if (self.inputFlipped)
        p = p * -1.0;
    for (int i = 0; i < 4; i++)
        if (self.touchActive[i] && self.touchId[i] == id)
            self.touchPos[i] = p;
    for (int i = 0; i < 4; i++)
        if (self.joyTouchId[i] == id) {
            self.joyDelta[i] = p - self.joyOrigin[i];
            return;
        }
}
template <class Self> void release(Self &self, unsigned id) {
    using vec2d = typename Self::PointType;
    for (int i = 0; i < 4; i++)
        if (self.touchActive[i] && self.touchId[i] == id)
            self.touchActive[i] = false;
    for (int i = 0; i < 4; i++)
        if (self.joyTouchId[i] == id) {
            self.joyActive[i] = false;
            self.joyTouchId[i] = 0x3d0dead;
            return;
        }
}
template <class Self> void minotronMotion(Self &self) {
    self.pos = self.pos + self.vel;
    self.vel = self.vel * self.drag;
    self.age++;
    if (self.noWrap)
        return;
    double w = self.wrapWidth, h = self.wrapHeight;
    self.wrapped = false;
    double x = self.pos.x, y = self.pos.y;
    if (double(float(w * 0.5)) < x) {
        x -= w;
        self.wrapped = true;
        self.pos.x = x;
    }
    if (double(float(h * 0.5)) < y) {
        y -= h;
        self.wrapped = true;
        self.pos.y = y;
    }
    if (x < double(-float(w * 0.5))) {
        self.wrapped = true;
        self.pos.x = x + w;
    }
    if (y < double(-float(h * 0.5))) {
        self.wrapped = true;
        self.pos.y = y + h;
    }
}
template <class Node> int minotronCollide(Node *a, Node *b, bool bounce) {
    if (a->drawDelay != 0 || a->dead || !b)
        return 0;
    float ra = a->size;
    double ax = a->pos.x, ay = a->pos.y;
    float ma = a->mass;
    int result = 0;
    bool sameGroup = false;
    for (; b; b = b->next) {
        if (b == a || b->dead)
            continue;
        float sumR = ra + b->size;
        double dx = ax - b->pos.x, dy = ay - b->pos.y;
        float dist = float(std::sqrt(dy * dy + dx * dx));
        if (sumR <= dist) {
            a->touching = false;
            continue;
        }
        if (a->group > 0 && a->group == b->group)
            sameGroup = true;
        if (a->touching || b->drawDelay != 0) {
            sameGroup = true;
            continue;
        }
        if (sameGroup)
            continue;
        a->hit = true;
        if (a->flags & 3)
            b->wasShot(a);
        b->hitBy(a);
        result = 1;
        if (!bounce)
            continue;
        float ang = float(std::atan2(b->pos.y - ay, b->pos.x - ax));
        float mt = ma + b->mass;
        float overlap = sumR - dist;
        float pushA = overlap * (b->mass / mt);
        float pushB = overlap * (ma / mt);
        a->pos.x += double(std::cos(ang + 3.1415927f) * pushA);
        a->pos.y += double(std::sin(ang + 3.1415927f) * pushA);
        b->pos.x += double(std::cos(ang) * pushB);
        b->pos.y += double(std::sin(ang) * pushB);
        float c = std::cos(ang), s = std::sin(ang);
        double dc = double(c);
        float va = float(double(s) * a->vel.y + a->vel.x * dc);
        float vb = float(double(s) * b->vel.y + dc * b->vel.x);
        float k = (va - vb + (va - vb)) / mt;
        float ka = b->mass * k;
        a->vel.x -= double(c * ka * a->elasticity);
        a->vel.y -= double(s * ka * a->elasticity);
        float kb = k * ma;
        b->vel.x += double(c * kb * b->elasticity);
        b->vel.y += double(s * kb * b->elasticity);
    }
    return result;
}
template <class Self> void minorescueMotion(Self &self) {
    self.pos = self.pos + self.vel;
    self.vel = self.vel * self.drag;
    self.age++;
    if (self.noWrap)
        return;
    double w = self.wrapWidth, h = self.wrapHeight;
    self.wrapped = false;
    double x = self.pos.x, y = self.pos.y;
    if (double(float(w * 0.5)) < x) {
        x -= w;
        self.wrapped = true;
        self.pos.x = x;
    }
    if (double(float(h * 0.5)) < y) {
        y -= h;
        self.wrapped = true;
        self.pos.y = y;
    }
    if (x < double(-float(w * 0.5))) {
        self.wrapped = true;
        self.pos.x = x + w;
    }
    if (y < double(-float(h * 0.5))) {
        self.wrapped = true;
        self.pos.y = y + h;
    }
}
template <class Node> int minorescueCollide(Node *a, Node *b, bool bounce) {
    if (!b)
        return 0;
    float ra = a->size;
    double ax = a->pos.x, ay = a->pos.y;
    float ma = a->mass;
    int result = 0;
    bool sameGroup = false;
    for (; b; b = b->next) {
        if (b == a || b->dead)
            continue;
        float sumR = ra + b->size;
        double dx = ax - b->pos.x, dy = ay - b->pos.y;
        float dist = float(std::sqrt(dy * dy + dx * dx));
        if (sumR <= dist) {
            a->touching = false;
            continue;
        }
        if (a->group > 0 && a->group == b->group)
            sameGroup = true;
        if (a->touching) {
            sameGroup = true;
            continue;
        }
        if (sameGroup)
            continue;
        a->hit = true;
        if (a->flags & 3)
            b->wasShot(a);
        b->hitBy(a);
        result = 1;
        if (!bounce)
            continue;
        float ang = float(std::atan2(b->pos.y - ay, b->pos.x - ax));
        float mt = ma + b->mass;
        float overlap = sumR - dist;
        float pushA = overlap * (b->mass / mt);
        float pushB = overlap * (ma / mt);
        a->pos.x += double(std::cos(ang + 3.1415927f) * pushA);
        a->pos.y += double(std::sin(ang + 3.1415927f) * pushA);
        b->pos.x += double(std::cos(ang) * pushB);
        b->pos.y += double(std::sin(ang) * pushB);
        float c = std::cos(ang), s = std::sin(ang);
        double dc = double(c);
        float va = float(double(s) * a->vel.y + a->vel.x * dc);
        float vb = float(double(s) * b->vel.y + dc * b->vel.x);
        float k = (va - vb + (va - vb)) / mt;
        float ka = b->mass * k;
        a->vel.x -= double(c * ka * a->elasticity);
        a->vel.y -= double(s * ka * a->elasticity);
        float kb = k * ma;
        b->vel.x += double(c * kb * b->elasticity);
        b->vel.y += double(s * kb * b->elasticity);
    }
    return result;
}
} // namespace frozen_mechanics
