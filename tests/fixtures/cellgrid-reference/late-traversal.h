// Saved reference traversal, with only the GPU sink replaced for observation.
// Primitive cell operations are checked separately against frozen cell bodies.
template <class Grid, class Vertex, class Sink>
void legacyTraversal(Grid &grid, Vertex *vertices, double cw, Sink sink) {
    auto &height = grid.height;
    auto &rowsN = grid.rowsN;
    auto &scrollY = grid.scrollY;
    auto &scrollX = grid.scrollX;
    auto &origin = grid.origin;
    auto &rowOffset = grid.rowOffset;
    auto &cols = grid.cols;
    auto &colOffset = grid.colOffset;
    auto &rows = grid.rows;
    auto &blockChar = grid.blockChar;
    auto &hashMode = grid.hashMode;
    auto &hashToggle = grid.hashToggle;
    auto &labelChar = grid.labelChar;
    auto &labelPos = grid.labelPos;
    auto &labelCount = grid.labelCount;
    auto &width = grid.width;
#ifdef NEON_CELL_ROW_OFFSETS
    auto &rowX = grid.rowX;
#endif
    Vertex *vp = vertices;
    int count = 0;
    auto flush = [&]() {
        sink(vertices, count);
        count = 0;
        vp = vertices;
    };
    double chH = height / double(rowsN);
    double y = (height * -0.5 - chH * scrollY) + origin.y;
    for (int ry = 0; ry <= rowsN; ry++, y += chH) {
        int ri = grid.wrapRow(ry + rowOffset);
        double x = (width * -0.5 - cw * scrollX) + origin.x;
#ifdef NEON_CELL_ROW_OFFSETS
        x += double(rowX[ri]) * cw;
#endif
        for (int cx = 0; cx <= cols; cx++, x += cw) {
            int ci = grid.wrapCol(cx + colOffset);
            auto &g = rows[ri][ci];
            g.pos = vec2d(x, y);
            g.run();
            if (g.bgOn) {
                g.pass = 0;
                g.special = int8_t(g.attr & 1);
                int n = g.outch(vp, &grid, unsigned(blockChar), g.bg.packARGB());
                vp += n;
                count += n;
            }
            if (g.fgOn) {
                g.pass = 1;
                g.special = int8_t(g.attr & 2);
                int n = 0;
                if (!hashMode || hashToggle == 0) {
                    if (hashMode && g.ch == '#')
                        hashToggle ^= 1;
                    else
                        n = g.outch(vp, &grid, unsigned(g.ch), g.col.packARGB());
                } else if (g.ch == '#') {
                    hashToggle ^= 1;
                } else {
                    // Text between '#'s is not drawn: it marks where the game
                    // should draw sprite labelChar[i] (letter - 'A') afterwards.
                    labelChar[labelCount] = g.ch - 0x41;
                    labelPos[labelCount] = g.pos;
                    labelCount = (labelCount + 1) & 63;
                }
                vp += n;
                count += n;
            }
            if (count >= 0x1fe8)
                flush();
        }
    }
    if (count > 0)
        flush();
}
