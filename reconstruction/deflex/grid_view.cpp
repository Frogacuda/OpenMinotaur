#include "neon/proportional_text.h"
#include "grid_view.h"
#include <cmath>
#include <algorithm>
namespace deflex {
namespace {
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
} // namespace
void ProportionalLine::update() {
    neon::text::updateLine(*this, progress, currentScale, currentSpacing,
                           neon::text::HoldAlpha::Preserve, [&] { text.clear(); });
}
void GridView::quick(const std::string &text, neon::vec2d pos, neon::vec2d scale, int hold, int row,
                     int seq, float rate, float step) {
    auto &l = lines.at(size_t(row));
    l = ProportionalLine{};
    l.text = text;
    l.pos = l.from = l.to = l.away = pos;
    l.scale = scale;
    l.hold = hold;
    l.sequence = seq;
    l.phaseRate = rate;
    l.charStep = step;
    // QuickMsg replaces the original line immediately. Reset its published
    // state as well, so a reused row cannot inherit an earlier fade-out.
    grid_.messageLines[size_t(row)] = {l.state, text.empty() ? 0 : 1};
}
void GridView::line(GridRenderer &renderer, const ProportionalLine &l) const {
    const auto scale = l.state == 1 ? l.scale : l.currentScale;
    const float spacing = l.state == 1 ? l.spacing : l.currentSpacing;
    const auto measured = neon::text::measureSignedText(
        l.text.c_str(), [](unsigned c) { return FontX[(c & 0xc0) * 2 + (c & 63) * 2 + 1]; });
    const float total = measured.total, average = measured.average;
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
void GridView::text(GridRenderer &renderer, const std::string &text, neon::vec2d position,
                    neon::vec2d scale, uint32_t rgba, int align) const {
    ProportionalLine l;
    l.text = text;
    l.pos = position;
    l.scale = scale;
    l.state = 1;
    l.alpha = 1;
    l.sequence = -1;
    l.rgba = rgba;
    l.align = align;
    line(renderer, l);
}
void GridView::Star::init(neon::DarwinRandom &random) {
    pos = {random.range(-.5, .5), -.6};
    vel.y = random.range(.003, .0045);
    frame = 28;
    colour.unpackRGBA(StarColours[random.next() & 3]);
    size = .005f;
    phase = float(random.range(0, 7));
    speed = float(random.range(.01, .02));
}
void GridView::Star::run() {
    pos += vel;
    if (phase >= 7) {
        const float wave = float(std::sin(double(phase - 7.0f) * 3.141592654));
        colour.a = double(wave) * .3 + .7;
        size = float(double(wave) * .002 + .005);
    } else
        colour.a = .7;
    phase += speed;
    if (phase > 8)
        phase -= 8;
    if (pos.y > .6)
        dead = true;
}
void GridView::draw(CellGrid &grid, const neon::colour *palette, neon::DarwinRandom &random,
                    bool paused, GridRenderer &renderer, ActorRenderer &sprites, neon::vec2d view) {
    for (auto it = stars_.begin(); it != stars_.end();) {
        if (!paused)
            it->run();
        if (it->dead)
            it = stars_.erase(it);
        else {
            it->drawEntity(sprites, view);
            ++it;
        }
    }
    const double cw = view.x / double(float(grid.cols)), ch = view.y / double(float(grid.rows));
    // Run_Module 4a288 advances the cursor independently of its pause flag.
    // Stars and proportional-line animation use separate pause gates.
    if (--cursorClock < 1) {
        cursorClock = 30;
        cursorVisible = !cursorVisible;
    }
    for (int y = 0; y <= grid.rows; ++y)
        for (int x = 0; x <= grid.cols; ++x) {
            auto &cell = grid.cell(y, x);
            cell.run(palette);
            for (int pass = 0; pass < 3; ++pass) {
                const bool cursor = pass == 2;
                if ((pass == 0 && !cell.bgOn) || (pass == 1 && !cell.fgOn) ||
                    (cursor && (!cursorVisible || x != cursorX || y != cursorY)))
                    continue;
                const bool atlas = !cursor && (cell.attr & (pass == 0 ? 1 : 2)) != 0;
                int icon = pass == 0 ? cell.bgIcon : cell.fgIcon;
                double w = cw, h = ch, u, v, du, dv;
                float spin = 0;
                if (atlas) {
                    if (icon < 0)
                        continue;
                    if (pass == 1)
                        h = w;
                    spin = pass == 0 ? cell.bgAngle : cell.fgAngle;
                    u = (double(float(icon) * 18.0f) + 1.5) / 512;
                    v = .4541015625;
                    du = .029296875;
                    dv = .03125;
                } else {
                    const unsigned c =
                        (cursor || pass == 0) ? 127u : std::max(unsigned(cell.ch & 255), 32u);
                    if (c <= 32)
                        continue;
                    const int index = int(c) - 32;
                    u = double(float(glyphW * (index % 32) - 1)) / 512;
                    v = c < 128 ? double(glyphH * (index / 32) + (index / 32) + fontY - 1) / 512
                                : .869140625;
                    du = double(glyphW) / 512;
                    dv = double(glyphH) / 512;
                }
                const double px = -view.x * .5 + cw * x, py = -view.y * .5 + ch * y;
                std::array<neon::vec3d, 4> points = {{{0, 0, 0}, {0, h, 0}, {w, h, 0}, {w, 0, 0}}};
                if (cell.rot.x != 0 || (pass == 1 && spin != 0)) {
                    // outch 4605c adds the cell's Z rotation only in its 3D
                    // branch. A flat atlas spin uses its own angle alone.
                    neon::matrix4f m;
                    neon::D3DXMatrixRotationZ(
                        &m, float(double(spin) + (cell.rot.x != 0 ? cell.rot.z : 0)));
                    if (cell.rot.x != 0) {
                        neon::matrix4f rx, ry;
                        neon::D3DXMatrixRotationX(&rx, float(cell.rot.x));
                        neon::D3DXMatrixRotationY(&ry, float(cell.rot.y));
                        m = rx * ry * m;
                    }
                    for (auto &p : points) {
                        neon::D3DXVECTOR3 a{float(p.x - w * .5), float(p.y - h * .5), 0};
                        neon::D3DXVECTOR4 b;
                        neon::D3DXVec3Transform(&b, &a, &m);
                        p = {px + double(b.x) + cw * .5 + (cell.rot.x != 0 ? cell.offset.x : 0),
                             py + double(b.y) + ch * .5 + (cell.rot.x != 0 ? cell.offset.y : 0),
                             610 + double(b.z) + (cell.rot.x != 0 ? cell.offset.z : 0)};
                    }
                } else
                    for (auto &p : points) {
                        p.x += px;
                        p.y += py;
                        p.z = 610;
                    }
                for (auto &p : points)
                    grid.doDisplace(p);
                // cellGrid's background glyph is 127, including its font/UV margin;
                // it is not an untextured rectangle or an atlas white-pixel sample.
                quad(renderer, points, u, v, du, dv,
                     cursor ? cursorColour : (pass == 0 ? cell.bg : cell.col).packRGBA());
            }
        }
    for (int i = 0; i < grid.rows; ++i) {
        auto &l = lines[size_t(i)];
        if (l.text.empty()) {
            grid.messageLines[size_t(i)].count = 0;
            continue;
        }
        if (grid.messageLines[size_t(i)].state == 2)
            l.state = 2;
        // rawGlyph 4408c emits no proportional glyphs while paused. The host's
        // own pause/Info labels use text() separately and remain visible.
        if (!paused) {
            line(renderer, l);
            l.update();
        }
        grid.messageLines[size_t(i)] = {l.state, l.text.empty() ? 0 : 1};
    }
    if (stars && !paused && --starClock_ < 0) {
        starClock_ = 3;
        stars_.emplace_front();
        stars_.front().init(random);
    }
    for (auto &displacement : grid.displacement)
        displacement.strength *= .99;
}
} // namespace deflex
