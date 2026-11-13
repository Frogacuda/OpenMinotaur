// Saved reference reader logic; renderer/game allocation replaced by record stubs.
#include "legacy_effect_dna.h"
#include <cstring>
namespace frozen {
LegacyModule *LegacyCell::loadModule() {
    if (s(sc) != 1)
        return nullptr;
    sc++;
    int type = s(sc++);
    LegacyModule *m = createModule(type);
    m->setBasics();
    for (int i = 0; i < 64; i++)
        m->params[i].value = fc < int(F.size()) ? double(F[size_t(fc)]) : 0.0, fc++;
    for (int i = 0; i < 64; i++)
        m->params[i].ival = pc < int(P.size()) ? P[size_t(pc)] : 0, pc++;
    return m;
}

int LegacyCell::loadWaypoint(neon::WaypointData *w) {
    if (!w || s(sc) != 2)
        return 0;
    sc++;
    w->count = s(sc++);
    if (w->count < 0 || w->count > 64)
        w->count = 0;
    w->index.resize(size_t(w->count));
    w->value.resize(size_t(w->count));
    for (int i = 0; i < w->count; i++)
        w->index[size_t(i)] = pc < int(P.size()) ? P[size_t(pc)] : 0, pc++;
    for (int i = 0; i < w->count; i++)
        w->value[size_t(i)] = fc < int(F.size()) ? F[size_t(fc)] : 0.0f, fc++;
    int f[16];
    for (int &v : f)
        v = pc < int(P.size()) ? P[size_t(pc)] : 0, pc++;
    w->mode = f[0];
    for (int k = 0; k < 3; k++)
        w->inputSrc[k] = f[1 + k];
    w->inputFlags = f[4];
    for (int k = 0; k < 6; k++)
        w->wrap[k] = f[5 + k];
    for (int k = 0; k < 5; k++)
        w->extra[k] = f[11 + k];
    return 0;
}

bool loadGrid(const std::vector<uint8_t> &file, std::vector<LegacyCell> &cells, int maxCells) {
    cells.assign(size_t(maxCells), LegacyCell{});
    if (file.size() < 12)
        return false;
    auto rd = [&](size_t off) {
        int32_t v;
        std::memcpy(&v, file.data() + off, 4);
        return v;
    };
    int nS = rd(0), nP = rd(4), nF = rd(8);
    if (nS < 1 || nP < 1024 || nF < 0 || 12 + 4 * size_t(nS + nP + nF) > file.size())
        return false;
    std::vector<int> S(static_cast<size_t>(nS)), P(static_cast<size_t>(nP));
    std::vector<float> F(static_cast<size_t>(nF));
    std::memcpy(S.data(), file.data() + 12, 4 * size_t(nS));
    std::memcpy(P.data(), file.data() + 12 + 4 * size_t(nS), 4 * size_t(nP));
    if (nF)
        std::memcpy(F.data(), file.data() + 12 + 4 * size_t(nS + nP), 4 * size_t(nF));

    int count = S[0];
    int sOff = 1, pOff = 1024, fOff = 0;
    for (int i = 0; i < count && i < 256; i++) {
        int cell = P[size_t(i)], ns = P[size_t(256 + i)], np = P[size_t(512 + i)],
            nf = P[size_t(768 + i)];
        if (ns < 0 || np < 0 || nf < 0 || sOff + ns > nS || pOff + np > nP || fOff + nf > nF)
            return false;
        if (cell >= 0 && cell < maxCells) {
            LegacyCell &c = cells[size_t(cell)];
            c.S.assign(S.begin() + sOff, S.begin() + sOff + ns);
            c.P.assign(P.begin() + pOff, P.begin() + pOff + np);
            c.F.assign(F.begin() + fOff, F.begin() + fOff + nf);
            c.hasDNA = true;
        }
        sOff += ns;
        pOff += np;
        fOff += nf;
    }
    return true;
}

} // namespace frozen
