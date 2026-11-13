// cavern_data.cpp - binds the generated original data (cavern_data.inc) to cavern_data.h.
#include "cavern_data.h"

namespace minos::cavern {

#include "cavern_data.inc"

RowTable rowTable(int symbolNumber) {
    switch (symbolNumber) {
    case 254:
        return {kRowTable254, int(sizeof(kRowTable254) / sizeof(kRowTable254[0]))};
    case 255:
        return {kRowTable255, int(sizeof(kRowTable255) / sizeof(kRowTable255[0]))};
    case 256:
        return {kRowTable256, int(sizeof(kRowTable256) / sizeof(kRowTable256[0]))};
    case 257:
        return {kRowTable257, int(sizeof(kRowTable257) / sizeof(kRowTable257[0]))};
    case 258:
        return {kRowTable258, int(sizeof(kRowTable258) / sizeof(kRowTable258[0]))};
    case 259:
        return {kRowTable259, int(sizeof(kRowTable259) / sizeof(kRowTable259[0]))};
    case 260:
        return {kRowTable260, int(sizeof(kRowTable260) / sizeof(kRowTable260[0]))};
    case 261:
        return {kRowTable261, int(sizeof(kRowTable261) / sizeof(kRowTable261[0]))};
    case 262:
        return {kRowTable262, int(sizeof(kRowTable262) / sizeof(kRowTable262[0]))};
    case 263:
        return {kRowTable263, int(sizeof(kRowTable263) / sizeof(kRowTable263[0]))};
    case 264:
        return {kRowTable264, int(sizeof(kRowTable264) / sizeof(kRowTable264[0]))};
    case 265:
        return {kRowTable265, int(sizeof(kRowTable265) / sizeof(kRowTable265[0]))};
    case 266:
        return {kRowTable266, int(sizeof(kRowTable266) / sizeof(kRowTable266[0]))};
    case 267:
        return {kRowTable267, int(sizeof(kRowTable267) / sizeof(kRowTable267[0]))};
    case 268:
        return {kRowTable268, int(sizeof(kRowTable268) / sizeof(kRowTable268[0]))};
    case 269:
        return {kRowTable269, int(sizeof(kRowTable269) / sizeof(kRowTable269[0]))};
    case 270:
        return {kRowTable270, int(sizeof(kRowTable270) / sizeof(kRowTable270[0]))};
    case 271:
        return {kRowTable271, int(sizeof(kRowTable271) / sizeof(kRowTable271[0]))};
    case 272:
        return {kRowTable272, int(sizeof(kRowTable272) / sizeof(kRowTable272[0]))};
    case 273:
        return {kRowTable273, int(sizeof(kRowTable273) / sizeof(kRowTable273[0]))};
    case 274:
        return {kRowTable274, int(sizeof(kRowTable274) / sizeof(kRowTable274[0]))};
    case 275:
        return {kRowTable275, int(sizeof(kRowTable275) / sizeof(kRowTable275[0]))};
    case 276:
        return {kRowTable276, int(sizeof(kRowTable276) / sizeof(kRowTable276[0]))};
    case 277:
        return {kRowTable277, int(sizeof(kRowTable277) / sizeof(kRowTable277[0]))};
    case 278:
        return {kRowTable278, int(sizeof(kRowTable278) / sizeof(kRowTable278[0]))};
    case 279:
        return {kRowTable279, int(sizeof(kRowTable279) / sizeof(kRowTable279[0]))};
    case 280:
        return {kRowTable280, int(sizeof(kRowTable280) / sizeof(kRowTable280[0]))};
    case 281:
        return {kRowTable281, int(sizeof(kRowTable281) / sizeof(kRowTable281[0]))};
    case 282:
        return {kRowTable282, int(sizeof(kRowTable282) / sizeof(kRowTable282[0]))};
    case 283:
        return {kRowTable283, int(sizeof(kRowTable283) / sizeof(kRowTable283[0]))};
    case 284:
        return {kRowTable284, int(sizeof(kRowTable284) / sizeof(kRowTable284[0]))};
    case 285:
        return {kRowTable285, int(sizeof(kRowTable285) / sizeof(kRowTable285[0]))};
    case 286:
        return {kRowTable286, int(sizeof(kRowTable286) / sizeof(kRowTable286[0]))};
    case 287:
        return {kRowTable287, int(sizeof(kRowTable287) / sizeof(kRowTable287[0]))};
    case 288:
        return {kRowTable288, int(sizeof(kRowTable288) / sizeof(kRowTable288[0]))};
    case 289:
        return {kRowTable289, int(sizeof(kRowTable289) / sizeof(kRowTable289[0]))};
    case 290:
        return {kRowTable290, int(sizeof(kRowTable290) / sizeof(kRowTable290[0]))};
    case 291:
        return {kRowTable291, int(sizeof(kRowTable291) / sizeof(kRowTable291[0]))};
    case 292:
        return {kRowTable292, int(sizeof(kRowTable292) / sizeof(kRowTable292[0]))};
    case 293:
        return {kRowTable293, int(sizeof(kRowTable293) / sizeof(kRowTable293[0]))};
    case 294:
        return {kRowTable294, int(sizeof(kRowTable294) / sizeof(kRowTable294[0]))};
    case 295:
        return {kRowTable295, int(sizeof(kRowTable295) / sizeof(kRowTable295[0]))};
    case 296:
        return {kRowTable296, int(sizeof(kRowTable296) / sizeof(kRowTable296[0]))};
    case 297:
        return {kRowTable297, int(sizeof(kRowTable297) / sizeof(kRowTable297[0]))};
    case 298:
        return {kRowTable298, int(sizeof(kRowTable298) / sizeof(kRowTable298[0]))};
    case 299:
        return {kRowTable299, int(sizeof(kRowTable299) / sizeof(kRowTable299[0]))};
    case 300:
        return {kRowTable300, int(sizeof(kRowTable300) / sizeof(kRowTable300[0]))};
    case 301:
        return {kRowTable301, int(sizeof(kRowTable301) / sizeof(kRowTable301[0]))};
    case 302:
        return {kRowTable302, int(sizeof(kRowTable302) / sizeof(kRowTable302[0]))};
    case 303:
        return {kRowTable303, int(sizeof(kRowTable303) / sizeof(kRowTable303[0]))};
    case 304:
        return {kRowTable304, int(sizeof(kRowTable304) / sizeof(kRowTable304[0]))};
    case 305:
        return {kRowTable305, int(sizeof(kRowTable305) / sizeof(kRowTable305[0]))};
    case 306:
        return {kRowTable306, int(sizeof(kRowTable306) / sizeof(kRowTable306[0]))};
    case 307:
        return {kRowTable307, int(sizeof(kRowTable307) / sizeof(kRowTable307[0]))};
    case 308:
        return {kRowTable308, int(sizeof(kRowTable308) / sizeof(kRowTable308[0]))};
    case 309:
        return {kRowTable309, int(sizeof(kRowTable309) / sizeof(kRowTable309[0]))};
    default:
        return {nullptr, 0};
    }
}

} // namespace minos::cavern
