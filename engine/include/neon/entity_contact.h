#pragma once
#include "neon/entity_motion.h"
namespace neon::entities {
// Early circle/contact/bounce operation. The module supplies source eligibility;
// callbacks and their consequences are the actual node's virtual operations.
// afterHit(a, b) runs after each hit's callbacks and bounce (Minos records the contact
// position and normal there: controlBlock::collide 0xedc8). Opt-in; the 4-argument form below
// is unchanged for the other titles.
template <class Node, class RejectTarget, class AfterHit>
int collideEarly(Node *a, Node *b, bool bounce, RejectTarget rejectTarget, AfterHit afterHit) {
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
        float dist = roundedDistance(dy, dx);
        if (sumR <= dist) {
            a->touching = false;
            continue;
        }
        if (a->group > 0 && a->group == b->group)
            sameGroup = true;
        if (a->touching || rejectTarget(*b)) {
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
        if (!bounce) {
            afterHit(*a, *b);
            continue;
        }
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
        afterHit(*a, *b);
    }
    return result;
}
template <class Node, class RejectTarget>
int collideEarly(Node *a, Node *b, bool bounce, RejectTarget rejectTarget) {
    return collideEarly(a, b, bounce, rejectTarget, [](Node &, Node &) {});
}
} // namespace neon::entities
