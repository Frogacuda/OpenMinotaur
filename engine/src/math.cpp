#include "neon/math.h"
namespace neon {
// ------------------------------------------------------------ matrices -----

matrix4f operator*(const matrix4f &a, const matrix4f &b) {
    matrix4f r;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j] +
                        a.m[i][3] * b.m[3][j];
    return r;
}

static void zero(matrix4f *o) {
    for (auto &row : o->m)
        for (float &v : row)
            v = 0.0f;
}

void D3DXMatrixIdentity(matrix4f *o) {
    zero(o);
    o->m[0][0] = o->m[1][1] = o->m[2][2] = o->m[3][3] = 1.0f;
}

void D3DXMatrixTranspose(matrix4f *o, const matrix4f *in) {
    matrix4f t;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            t.m[i][j] = in->m[j][i];
    *o = t;
}

void D3DXMatrixOrthoLH(matrix4f *o, float w, float h, float zn, float zf) {
    zero(o);
    o->m[0][0] = 2.0f / w;
    o->m[1][1] = 2.0f / h;
    o->m[2][2] = 1.0f / (zf - zn);
    o->m[3][2] = -zn / (zf - zn);
    o->m[3][3] = 1.0f;
}

void D3DXMatrixPerspectiveFovLH(matrix4f *o, float fovy, float aspect, float zn, float zf) {
    float ys = 1.0f / std::tan(fovy * 0.5f);
    zero(o);
    o->m[0][0] = ys / aspect;
    o->m[1][1] = ys;
    o->m[2][2] = zf / (zf - zn);
    o->m[2][3] = 1.0f;
    o->m[3][2] = -(zn * zf) / (zf - zn);
}

static D3DXVECTOR3 norm3(D3DXVECTOR3 v) {
    float l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (l > 0.0f) {
        float k = 1.0f / l;
        v.x *= k;
        v.y *= k;
        v.z *= k;
    }
    return v;
}
static D3DXVECTOR3 cross3(const D3DXVECTOR3 &a, const D3DXVECTOR3 &b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
static float dot3(const D3DXVECTOR3 &a, const D3DXVECTOR3 &b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

void D3DXMatrixLookAtLH(matrix4f *o, const D3DXVECTOR3 *eye, const D3DXVECTOR3 *at,
                        const D3DXVECTOR3 *up) {
    D3DXVECTOR3 z = norm3({at->x - eye->x, at->y - eye->y, at->z - eye->z});
    D3DXVECTOR3 x = norm3(cross3(*up, z));
    D3DXVECTOR3 y = cross3(z, x);
    zero(o);
    o->m[0][0] = x.x;
    o->m[0][1] = y.x;
    o->m[0][2] = z.x;
    o->m[1][0] = x.y;
    o->m[1][1] = y.y;
    o->m[1][2] = z.y;
    o->m[2][0] = x.z;
    o->m[2][1] = y.z;
    o->m[2][2] = z.z;
    o->m[3][0] = -dot3(x, *eye);
    o->m[3][1] = -dot3(y, *eye);
    o->m[3][2] = -dot3(z, *eye);
    o->m[3][3] = 1.0f;
}

void D3DXMatrixRotationX(matrix4f *o, float a) {
    float c = std::cos(a), s = std::sin(a);
    D3DXMatrixIdentity(o);
    o->m[1][1] = c;
    o->m[1][2] = s;
    o->m[2][1] = -s;
    o->m[2][2] = c;
}

void D3DXMatrixRotationY(matrix4f *o, float a) {
    float c = std::cos(a), s = std::sin(a);
    D3DXMatrixIdentity(o);
    o->m[0][0] = c;
    o->m[0][2] = -s;
    o->m[2][0] = s;
    o->m[2][2] = c;
}

void D3DXMatrixRotationZ(matrix4f *o, float a) {
    float c = std::cos(a), s = std::sin(a);
    D3DXMatrixIdentity(o);
    o->m[0][0] = c;
    o->m[0][1] = s;
    o->m[1][0] = -s;
    o->m[1][1] = c;
}

void D3DXMatrixRotationAxis(matrix4f *o, const D3DXVECTOR3 *axis, float a) {
    D3DXVECTOR3 v = norm3(*axis);
    double c = std::cos(double(a)), s = std::sin(double(a)), t = 1.0 - c;
    double x = v.x, y = v.y, z = v.z;
    D3DXMatrixIdentity(o);
    o->m[0][0] = float(c + x * t * x);
    o->m[1][0] = float(x * t * y - s * z);
    o->m[2][0] = float(z * x * t + y * s);
    o->m[0][1] = float(x * y * t + s * z);
    o->m[1][1] = float(c + y * t * y);
    o->m[2][1] = float(y * z * t - s * x);
    o->m[0][2] = float(x * z * t - y * s);
    o->m[1][2] = float(y * z * t + x * s);
    o->m[2][2] = float(c + z * t * z);
}

void D3DXMatrixScaling(matrix4f *o, float sx, float sy, float sz) {
    zero(o);
    o->m[0][0] = sx;
    o->m[1][1] = sy;
    o->m[2][2] = sz;
    o->m[3][3] = 1.0f;
}

void D3DXMatrixTranslation(matrix4f *o, float tx, float ty, float tz) {
    D3DXMatrixIdentity(o);
    o->m[3][0] = tx;
    o->m[3][1] = ty;
    o->m[3][2] = tz;
}

void D3DXVec3Transform(D3DXVECTOR4 *o, const D3DXVECTOR3 *v, const matrix4f *m) {
    D3DXVECTOR4 r;
    r.x = v->x * m->m[0][0] + v->y * m->m[1][0] + v->z * m->m[2][0] + m->m[3][0];
    r.y = v->x * m->m[0][1] + v->y * m->m[1][1] + v->z * m->m[2][1] + m->m[3][1];
    r.z = v->x * m->m[0][2] + v->y * m->m[1][2] + v->z * m->m[2][2] + m->m[3][2];
    r.w = v->x * m->m[0][3] + v->y * m->m[1][3] + v->z * m->m[2][3] + m->m[3][3];
    *o = r;
}

// --------------------------------------------------------------- colour ----

static uint32_t to8(double v) {
    return uint32_t(int(v * 255.0)) & 0xff;
}

uint32_t colour::packRGBA() const {
    return (to8(r) << 24) | (to8(g) << 16) | (to8(b) << 8) | to8(a);
}

uint32_t colour::packARGB() const {
    return (to8(a) << 24) | (to8(b) << 16) | (to8(g) << 8) | to8(r);
}

void colour::unpackRGBA(uint32_t v) {
    r = (v >> 24) / 255.0;
    g = ((v >> 16) & 0xff) / 255.0;
    b = ((v >> 8) & 0xff) / 255.0;
    a = (v & 0xff) / 255.0;
}

void colour::unpackARGB(uint32_t v) {
    a = (v >> 24) / 255.0;
    b = ((v >> 16) & 0xff) / 255.0;
    g = ((v >> 8) & 0xff) / 255.0;
    r = (v & 0xff) / 255.0;
}

void colour::generate() {
    generate(7);
}
void colour::generate(unsigned phaseMask) {
    for (int i = 0; i < 3; i++)
        if (phaseMask & (1u << i))
            phase[i] += speed[i];
    r = base[0] + (std::sin(phase[0]) * 0.5 + 0.5) * amp[0];
    g = base[1] + (0.5 + std::sin(phase[1]) * 0.5) * amp[1];
    b = base[2] + (0.5 + std::sin(phase[2]) * 0.5) * amp[2];
}

colour colour::lerp(const colour &o, float t) const {
    colour c = *this;
    double k = t;
    c.r = r + (o.r - r) * k;
    c.g = g + k * (o.g - g);
    c.b = b + k * (o.b - b);
    c.a = a + k * (o.a - a);
    return c;
}

colour colour::operator*(float s) const {
    colour c = *this;
    double k = s;
    c.r = r * k;
    c.g = k * g;
    c.b = k * b;
    c.a = k * a;
    return c;
}

colour clerp(const colour &a, const colour &b, float t) {
    return a.lerp(b, t);
}

} // namespace neon
