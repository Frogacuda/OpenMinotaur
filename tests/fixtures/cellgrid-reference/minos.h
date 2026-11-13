// Saved reference port bodies, not an original-execution oracle.
static int legacyVisibleLength(const char *s) {
    int i = 0, n = 0;
    while (s[i]) {
        if (s[i] == '~')
            i += 5;
        else {
            i++;
            n++;
        }
    }
    return n;
}
struct LegacyGrid final : cellGrid {
    void scroll(float dx, float dy);
    void cls(int c);
    void cls();
    void cll(int row, int c);
    void cll(int row);
    void clla(int row);
    void clattr();
    void cllabel();
    void printAt(int x, int y, const char *s);
    void cPrintAt(int y, const char *s);
    void cPrintAt(int y, const char *s, uint32_t rgba);
    void poke(int x, int y, char c);
    void poke(int x, int y, char c, uint32_t rgba);
    void lpoke(int x, int y, int lab);
    int getLabel(vec2d pt);
    void hlin_a(int bgv, uint32_t c0, uint32_t c1, int x0, int x1, int y, float alpha);
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
        if (labelMode)
            g.label = curLabel;
        g.fgOn = c != 0x20;
    }
}

void LegacyGrid::cll(int row) {
    int r = wrapRow(row + rowOffset);
    for (int x = 0; x <= cols; x++) {
        gridCell &g = rows[r][x];
        g.ch = 0x20;
        if (labelMode)
            g.label = curLabel;
        g.fgOn = 0;
    }
}

void LegacyGrid::clla(int row) {
    int r = wrapRow(row + rowOffset);
    for (int x = 0; x <= cols; x++)
        rows[r][x].bgOn = 0;
}

void LegacyGrid::clattr() {
    for (int y = 0; y <= rowsN; y++)
        for (int x = 0; x <= cols; x++)
            rows[y][x].bgOn = 0;
}

void LegacyGrid::cllabel() {
    for (int y = 0; y <= rowsN; y++)
        for (int x = 0; x <= cols; x++)
            rows[y][x].label = 0;
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
        g.ch = int(s[i]);
        g.col = printColour;
        cx++;
        if (labelMode)
            g.label = curLabel;
        g.fgOn = s[i] != ' ';
        i++;
        if (!s[i])
            break;
    }
}

void LegacyGrid::cPrintAt(int y, const char *s) {
    if (!s)
        return;
    printAt(cols / 2 - legacyVisibleLength(s) / 2, y, s);
}

void LegacyGrid::cPrintAt(int y, const char *s, uint32_t rgba) {
    printColour.unpackRGBA(rgba);
    if (!s)
        return;
    printAt(cols / 2 - legacyVisibleLength(s) / 2, y, s);
}

void LegacyGrid::poke(int x, int y, char c) {
    gridCell &g = rows[wrapRow(y + rowOffset)][wrapCol(x + colOffset)];
    g.ch = int(c);
    if (labelMode)
        g.label = curLabel;
    g.fgOn = c != 0x20;
}

void LegacyGrid::poke(int x, int y, char c, uint32_t rgba) {
    gridCell &g = rows[wrapRow(y + rowOffset)][wrapCol(x + colOffset)];
    g.ch = int(c);
    g.scale = 1.0f;
    g.decay = 1.0f;
    g.col.unpackRGBA(rgba);
    if (labelMode)
        g.label = curLabel;
    g.fgOn = c != 0x20;
}

void LegacyGrid::lpoke(int x, int y, int lab) {
    rows[wrapRow(y + rowOffset)][wrapCol(x + colOffset)].label = lab;
}

int LegacyGrid::getLabel(vec2d pt) {
    int x = int((pt.x + 0.5) * double(float(cols))) + colOffset;
    int y = int((pt.y + 0.5) * double(float(rowsN))) + rowOffset;
    if (x > cols)
        x = x - cols - 1;
    if (y > rowsN)
        y = y - rowsN - 1;
    return rows[y][x].label;
}

void LegacyGrid::hlin_a(int bgv, uint32_t c0, uint32_t c1, int x0, int x1, int y, float alpha) {
    int n = x1 - x0;
    float step = 1.0f / float(n);
    int sx = wrapCol(x0 + colOffset);
    int r = wrapRow(y + rowOffset);
    colour a, b;
    a.unpackRGBA(c0);
    b.unpackRGBA(c1);
    float t = 0.0f;
    if (n + 1 <= 0)
        return;
    for (int i = 0;; i++) {
        int x = i + sx;
        if (x > cols)
            x = 0;
        gridCell &g = rows[r][x];
        if (labelMode)
            g.label = curLabel;
        g.bgOn = bgv;
        g.bg = a.lerp(b, t) * alpha;
        if (i == n)
            break;
        t += step;
    }
}
