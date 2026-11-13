
#include "view_data.inc"
void quad(GridRenderer &out, const std::array<neon::vec3d, 4> &p, double u, double v, double du,
          double dv, uint32_t rgba, bool additive = true, int texture = 1) {
    neon::colour c;
    c.unpackRGBA(rgba);
    const uint32_t packed = c.packARGB();
    std::array<GridVertex, 6> q;
    constexpr int order[] = {0, 1, 2, 2, 3, 0};
    for (int i = 0; i < 6; ++i) {
        const int n = order[i];
        q[i] = {float(p[n].x),
                -float(p[n].y),
                float(p[n].z),
                float(u + (n >= 2 ? du : 0)),
                float(v + (n == 1 || n == 2 ? dv : 0)),
                packed};
    }
    out.quad(q, additive, texture);
}
void rect(GridRenderer &out, neon::vec2d p, neon::vec2d size, double u, double v, double du,
          double dv, uint32_t rgba) {
    quad(out,
         {{{p.x, p.y, 610},
           {p.x, p.y + size.y, 610},
           {p.x + size.x, p.y + size.y, 610},
           {p.x + size.x, p.y, 610}}},
         u, v, du, dv, rgba, false);
}
void oldLine(GridRenderer &renderer, const ProportionalLine &l) {
    const auto scale = l.state == 1 ? l.scale : l.currentScale;
    const float spacing = l.state == 1 ? l.spacing : l.currentSpacing;
    int sum = 0, count = 0;
    for (unsigned char c : l.text) {
        if (int8_t(c) <= 0)
            break;
        sum += FontX[(c & 0xc0) * 2 + (c & 63) * 2 + 1];
        ++count;
    }
    const float total = float(sum), average = count ? total / float(count) : 0;
    const float lineWidth = float(scale.x * double(spacing + total));
    auto pos = l.pos * 512;
    if (l.align == 1)
        pos.x -= double(lineWidth * .5f + average * .5f);
    else if (l.align == 3)
        pos.x -= double(lineWidth);
    const double height = scale.y * 18;
    pos.y -= height * .5;
    float phase = l.phase;
    for (unsigned char c : l.text) {
        if (int8_t(c) <= 0)
            break;
        const int k = ((c >> 6) & 3) * 128 + (c & 63) * 2;
        const int gx = FontX[k], gw = FontX[k + 1], gy = FontY[(c >> 6) & 3];
        const double width = scale.x * double(float(gw));
        if (gx >= 0) {
            uint32_t colour = l.rgba;
            if (l.sequence >= 0)
                colour = Sequences[l.sequence % 6][int(phase) % SequenceLengths[l.sequence % 6]];
            const auto alpha = uint32_t(int(float(colour & 255) * l.alpha));
            rect(renderer, pos, {width, height}, double(float(gx)) / 512, double(float(gy)) / 512,
                 double(float(gw)) / 512, double(18) / 512 * .95, (colour & 0xffffff00) | alpha);
        }
        pos.x += scale.x * double(spacing) + width;
        phase += l.charStep;
    }
}
