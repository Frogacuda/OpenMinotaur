// Saved reference port baseline for differential checks; not an original oracle.
// Bodies retained verbatim apart from the owning class name.
struct LegacyGrid final : cellGrid {
    void scroll(float dx, float dy);
    void cls(int c);
    void cls();
    void cll(int row, int c);
    void cll(int row);
    void clla(int row);
    void clattr();
};

void LegacyGrid::scroll(float dx, float dy) {
    scrollX += double(dx);
    scrollY += double(dy);
    while (scrollX < 0.0) {
        scrollCount[0]++;
        scrollX += 1.0;
        if (--colOffset < 0)
            colOffset = cols;
    }
    while (scrollX >= 1.0) {
        if (++colOffset > cols)
            colOffset = 0;
        scrollCount[1]++;
        scrollX -= 1.0;
    }
    while (scrollY < 0.0) {
        scrollCount[2]++;
        if (--rowOffset < 0)
            rowOffset = rowsN;
        scrollY += 1.0;
    }
    while (scrollY >= 1.0) {
        if (++rowOffset > rowsN)
            rowOffset = 0;
        scrollCount[3]++;
        scrollY -= 1.0;
    }
}

void LegacyGrid::cls(int c) {
    for (int y = 0; y <= rowsN; y++)
        for (int x = 0; x <= cols; x++) {
            gridCell &g = rows[y][x];
            g.ch = c;
            g.label = labelMode ? curLabel : 0;
            g.fgOn = c != 0x20;
        }
    printColour.white(1.0);
    rowOffset = colOffset = 0;
    scrollX = scrollY = 0;
}

void LegacyGrid::cls() {
    cls(0x20);
}

void LegacyGrid::cll(int row, int c) {
    int r = wrapRow(row + rowOffset);
    for (int x = 0; x <= cols; x++) {
        gridCell &g = rows[r][x];
        g.ch = c;
        setLabel(g);
        g.fgOn = c != 0x20;
    }
}

void LegacyGrid::cll(int row) {
    cll(row, 0x20);
}

void LegacyGrid::clla(int row) {
    int r = wrapRow(row + rowOffset);
    for (int x = 0; x <= cols; x++)
        rows[r][x].attr = 0;
}

void LegacyGrid::clattr() {
    for (int y = 0; y <= rowsN; y++)
        for (int x = 0; x <= cols; x++)
            rows[y][x].attr = 0;
}
