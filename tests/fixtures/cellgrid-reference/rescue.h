// Saved reference port bodies, not an original-execution oracle.
struct LegacyGrid final : cellGrid {
    void scroll(float dx, float dy);
    void cls(int c);
    void cls();
    void cll(int row, int c);
    void cll(int row);
    void clla(int row);
    void clattr();
    void ccol(colour c);
    void printAt(int x, int y, const char *s);
    void cPrintAt(int y, const char *s);
    void poke(int x, int y, char c);
    int getLabel(vec2d pt);
    void hlin_a(int a, uint32_t c0, uint32_t c1, int x0, int x1, int y, float alpha);
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

void LegacyGrid::ccol(colour c) {
    for (int y = 0; y <= rowsN; y++)
        for (int x = 0; x <= cols; x++) {
            gridCell &g = rows[y][x];
            g.col = c;
            setLabel(g);
        }
}

void LegacyGrid::printAt(int x, int y, const char *s) {
    if (!s)
        return;
    int cx = wrapCol(x + colOffset);
    int r = wrapRow(y + rowOffset);
    int i = 0;
    while (s[i]) {
        int c = cx > cols ? cx - cols - 1 : cx;
        if (s[i] == '~') {
            printColour.r = double(float(s[i + 1] - '0') / 9.0f);
            printColour.g = double(float(s[i + 2] - '0') / 9.0f);
            printColour.b = double(float(s[i + 3] - '0') / 9.0f);
            printColour.a = double(float(s[i + 4] - '0') / 9.0f);
            i += 5;
            // (A trailing colour code: the original went on to print the
            // terminator and read past it.)
            if (!s[i])
                break;
        }
        gridCell &g = rows[r][c];
        if (s[i] == '&') {
            g.ch = 0x20;
            g.attr = 4;
        } else {
            g.ch = int(s[i]);
        }
        g.col = printColour;
        setLabel(g);
        g.fgOn = s[i] != ' ';
        cx++;
        i++;
    }
}

void LegacyGrid::cPrintAt(int y, const char *s) {
    if (!s)
        return;
    int i = 0, n = 0;
    while (s[i]) {
        if (s[i] == '~')
            i += 5;
        else {
            i++;
            n++;
        }
    }
    printAt(cols / 2 - n / 2, y, s);
}

void LegacyGrid::poke(int x, int y, char c) {
    gridCell &g = rows[wrapRow(y + rowOffset)][wrapCol(x + colOffset)];
    g.ch = int(c);
    setLabel(g);
    g.fgOn = c != 0x20;
}

int LegacyGrid::getLabel(vec2d pt) {
    int x = int((pt.x + 0.5) * double(float(cols))) + colOffset;
    int y = int((pt.y + 0.5) * double(float(rowsN))) + rowOffset;
    if (x > cols)
        x = x - cols - 1;
    if (y > rowsN)
        y = y - rowsN - 1;
    // (Unchecked in the original; touches here can also come from the
    // letterbox, outside -0.5..0.5.)
    if (x < 0 || y < 0 || x > cols || y > rowsN)
        return 0;
    return rows[y][x].label;
}

void LegacyGrid::hlin_a(int a, uint32_t c0, uint32_t c1, int x0, int x1, int y, float alpha) {
    int n = x1 - x0;
    float step = 1.0f / float(n);
    int sx = wrapCol(x0 + colOffset);
    int r = wrapRow(y + rowOffset);
    colour ca, cb;
    ca.unpackRGBA(c0);
    cb.unpackRGBA(c1);
    float t = 0.0f;
    if (n + 1 <= 0)
        return;
    for (int i = 0;; i++) {
        int x = i + sx;
        if (x > cols)
            x = 0;
        gridCell &g = rows[r][x];
        setLabel(g);
        g.attr = a;
        g.bg = ca.lerp(cb, t) * alpha;
        if (i == n)
            break;
        t += step;
    }
}
