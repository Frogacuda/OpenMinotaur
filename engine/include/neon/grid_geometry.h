#pragma once
#include "neon/math.h"
#include <cstdint>
namespace neon::grid {
enum class BackgroundUV { Original, InsetInterior };
// Five-source screen displacement: distance and its square round to float
// before promotion to double. The caller supplies each generation's centre.
template <class Point, class Sources, class Centre>
void applyDisplacements(Point &position, const Sources &sources, Centre centre) {
    for (const auto &source : sources) {
        const auto origin = centre(source);
        const vec2d delta(position.x - origin.x, position.y - origin.y);
        const float remaining = 128.0f - float(delta.length());
        if (remaining > 0.0f) {
            const float square = remaining * remaining;
            position.z = position.z + double(square) * 0.01 * source.strength;
        }
    }
}

// Animated-grid quad generation. Rescue's simple glyph/background paths have a
// different layout and remain separate. This emits six vertices, preserves
// scratch-matrix mutation, and performs no batching, scheduling or RNG draws.
template <class Cell, class Vertex, class Grid>
int emitAnimatedCell(Cell &cell, Vertex *out, Grid *grid, unsigned chr, uint32_t colour,
                     BackgroundUV backgroundUV) {
    auto &owner = cell.owner;
    auto &z = cell.z;
    auto &baseZ = cell.baseZ;
    auto &pass = cell.pass;
    auto &width = cell.width;
    auto &height = cell.height;
    auto &pos = cell.pos;
    auto &special = cell.special;
    auto &bgIcon = cell.bgIcon;
    auto &bgAngle = cell.bgAngle;
    auto &fgIcon = cell.fgIcon;
    auto &fgAngle = cell.fgAngle;
    auto &rot = cell.rot;
    auto &mrx = cell.mrx;
    auto &mry = cell.mry;
    auto &mrz = cell.mrz;
    auto &offset = cell.offset;

    owner = grid;
    z = double(baseZ);
    const vec2d &ps = grid->passScale[pass < 0 ? 0 : pass];
    double w = width * ps.x, h = height * ps.y;
    double px = pos.x * ps.x, py = pos.y * ps.y;
    double u, v, du, dv;
    float spin;

    if (special) {
        int icon;
        if (pass == 0) {
            icon = bgIcon;
            spin = bgAngle;
        } else if (pass == 1) {
            h = w;
            icon = fgIcon;
            spin = fgAngle;
        } else {
            icon = -2;
            spin = 0.0f;
        }
        if (icon == -2) {
            u = 0.0029296875;
        } else {
            if (icon < 0)
                return 0;
            u = (double(float(icon) * 18.0f) + 1.5) * 0.001953125;
        }
        v = 0.4541015625;
        dv = 0.03125;
        du = 0.029296875;
    } else {
        unsigned c8 = chr & 0xff;
        unsigned c = c8 >= unsigned(grid->firstChar) ? c8 : unsigned(grid->firstChar);
        if (c <= 32)
            return 0;
        int idx = int(c) - grid->firstChar;
        int gw = grid->glyphW, gh = grid->glyphH;
        if (c8 > unsigned(grid->maxChar)) {
            v = 0.869140625;
        } else {
            int row = idx / 32;
            v = (double(gh * row + row + grid->fontY) - 1.0) * 0.001953125;
        }
        u = double(gw * (idx % 32) - 1) * 0.001953125;
        spin = 0.0f;
        du = double(gw) * 0.001953125;
        dv = double(gh) * 0.001953125;
        if (pass == 0 && backgroundUV == BackgroundUV::InsetInterior) {
            // Retained host adaptation: A background block's rectangle starts a texel
            // above and left of the glyph, so filtering blends in the
            // neighbouring row / column and every tile shows a faint seam.
            // Sample only the block's interior instead.
            const double t = 0.001953125;
            u += t * 1.5;
            v += t * 1.5;
            du -= t * 3.0;
            dv -= t * 3.0;
        }
    }

    struct P {
        double x, y, z;
    } q[4] = {{0, 0, 0}, {0, h, 0}, {w, h, 0}, {w, 0, 0}};

    if (rot.x != 0.0) {
        D3DXMatrixRotationX(&mrx, float(rot.x));
        D3DXMatrixRotationY(&mry, float(rot.y));
        D3DXMatrixRotationZ(&mrz, float(double(spin) + rot.z));
        matrix4f m = mrx * mry * mrz;
        for (P &v3 : q) {
            D3DXVECTOR3 in{float(w * -0.5 + v3.x), float(h * -0.5 + v3.y), 0.0f};
            D3DXVECTOR4 o;
            D3DXVec3Transform(&o, &in, &m);
            v3.x = px + double(o.x) + width * 0.5 + offset.x;
            v3.y = py + double(o.y) + height * 0.5 + offset.y;
            v3.z = double(o.z) + z + offset.z;
        }
    } else if (pass == 1 && spin != 0.0f) {
        matrix4f m;
        D3DXMatrixRotationZ(&m, spin);
        for (P &v3 : q) {
            D3DXVECTOR3 in{float(w * -0.5 + v3.x), float(h * -0.5 + v3.y), 0.0f};
            D3DXVECTOR4 o;
            D3DXVec3Transform(&o, &in, &m);
            v3.x = px + double(o.x) + width * 0.5;
            v3.y = py + double(o.y) + height * 0.5;
            v3.z = double(o.z) + z;
        }
    } else {
        for (P &v3 : q) {
            v3.x += px;
            v3.y += py;
            v3.z = z;
        }
    }
    for (auto &vertex : q)
        applyDisplacements(vertex, grid->ripples,
                           [](const auto &ripple) { return vec2d(ripple.x, ripple.y); });

    auto V = [&](const P &a, double uu, double vv) {
        Vertex &n = *out++;
        n.x = float(a.x);
        n.y = -float(a.y);
        n.z = float(a.z);
        n.u0 = n.u1 = n.u2 = float(uu);
        n.v0 = n.v1 = n.v2 = float(vv);
        n.colour = colour;
    };
    V(q[0], u, v);
    V(q[1], u, dv + v);
    V(q[2], du + u, dv + v);
    V(q[2], du + u, dv + v);
    V(q[3], du + u, v);
    V(q[0], u, v);
    return 6;
}
// Row-header displacement is measured in cell widths, after scroll/origin.
inline double displacedRowX(double width, double scroll, double origin, double cellWidth,
                            float displacement) {
    double x = (width * -0.5 - cellWidth * scroll) + origin;
    return x + cellWidth * double(displacement);
}

// Ordered animated-grid traversal. The caller captures cellWidth at its original
// point (Minos captures it before doInput), and retains script/blend/proportional
// preparation outside this kernel. Cursor hooks emit at most one six-vertex quad;
// the caller provides the original buffer of at least 8192 vertices.
template <class Grid, class Vertex, class RowOrigin, class Cursor, class Flush>
void traverseAnimatedCells(Grid &grid, Vertex *vertices, double cellWidth, RowOrigin rowOrigin,
                           Cursor cursor, Flush flush) {
    int count = 0;
    Vertex *out = vertices;
    const double cellHeight = grid.height / double(grid.rowsN);
    double y = (grid.height * -0.5 - cellHeight * grid.scrollY) + grid.origin.y;
    for (int logicalRow = 0; logicalRow <= grid.rowsN; ++logicalRow, y += cellHeight) {
        const int physicalRow = grid.wrapRow(logicalRow + grid.rowOffset);
        const double baseX = (grid.width * -0.5 - cellWidth * grid.scrollX) + grid.origin.x;
        double x = rowOrigin(physicalRow, baseX, cellWidth);
        for (int logicalCol = 0; logicalCol <= grid.cols; ++logicalCol, x += cellWidth) {
            const int physicalCol = grid.wrapCol(logicalCol + grid.colOffset);
            auto &cell = grid.rows[physicalRow][physicalCol];
            cell.pos = vec2d(x, y);
            cell.run();
            if (cell.bgOn) {
                cell.pass = 0;
                cell.special = int8_t(cell.attr & 1);
                const int n = cell.outch(out, &grid, unsigned(grid.blockChar), cell.bg.packARGB());
                out += n;
                count += n;
            }
            if (cell.fgOn) {
                cell.pass = 1;
                cell.special = int8_t(cell.attr & 2);
                int n = 0;
                if (!grid.hashMode || grid.hashToggle == 0) {
                    if (grid.hashMode && cell.ch == '#')
                        grid.hashToggle ^= 1;
                    else
                        n = cell.outch(out, &grid, unsigned(cell.ch), cell.col.packARGB());
                } else if (cell.ch == '#')
                    grid.hashToggle ^= 1;
                else {
                    grid.labelChar[grid.labelCount] = cell.ch - 0x41;
                    grid.labelPos[grid.labelCount] = cell.pos;
                    grid.labelCount = (grid.labelCount + 1) & 63;
                }
                out += n;
                count += n;
            }
            cursor(cell, logicalRow, logicalCol, out, count);
            if (count >= 0x1fe8) {
                flush(vertices, count);
                count = 0;
                out = vertices;
            }
        }
    }
    if (count > 0)
        flush(vertices, count);
}
inline double applyRowDisplacement(double base, double cellWidth, float displacement) {
    return base + cellWidth * double(displacement);
}

} // namespace neon::grid
