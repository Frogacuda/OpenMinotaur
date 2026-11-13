// core_registry.cpp - the cores compiled into this build (toggled by CMake).
#include "core_api.h"

#include <cstring>

namespace mino {
#ifdef MINO_STANDALONE_CORE
extern const CoreEntry MINO_STANDALONE_CORE;
const CoreEntry *coreRegistry(int *count) {
    *count = 1;
    return &MINO_STANDALONE_CORE;
}
const CoreEntry *findCore(const std::string &id) {
    return id == MINO_STANDALONE_CORE.info->id ? &MINO_STANDALONE_CORE : nullptr;
}
#else
#ifdef MINO_CORE_SUPEROXWARS
extern const CoreEntry kSuperOxWarsCore;
#endif
#ifdef MINO_CORE_FIVEADAY
extern const CoreEntry kFiveADayCore;
#endif
#ifdef MINO_CORE_GOATUP
extern const CoreEntry kGoatUpCore;
#endif
#ifdef MINO_CORE_MINOTRON
extern const CoreEntry kMinotronCore;
#endif
#ifdef MINO_CORE_MINORESCUE
extern const CoreEntry kMinoRescueCore;
#endif
#ifdef MINO_CORE_MINOS
extern const CoreEntry kMinosCore;
#endif
extern const CoreEntry kGridrunnerCore;
extern const CoreEntry kGridrunnerClassicCore;
#ifdef MINO_CORE_DEFLEX
extern const CoreEntry kDeflexCore;
#endif
#ifdef MINO_CORE_GOATUP2
extern const CoreEntry kGoatUp2Core;
#endif

static const CoreEntry *const kCores[] = {
#ifdef MINO_CORE_SUPEROXWARS
    &kSuperOxWarsCore,
#endif
#ifdef MINO_CORE_GOATUP
    &kGoatUpCore,
#endif
#ifdef MINO_CORE_MINOTRON
    &kMinotronCore,
#endif
#ifdef MINO_CORE_MINORESCUE
    &kMinoRescueCore,
#endif
#ifdef MINO_CORE_MINOS
    &kMinosCore,
#endif
    &kGridrunnerCore,  &kGridrunnerClassicCore,
#ifdef MINO_CORE_DEFLEX
    &kDeflexCore,
#endif
#ifdef MINO_CORE_GOATUP2
    &kGoatUp2Core,
#endif
#ifdef MINO_CORE_FIVEADAY
    &kFiveADayCore,
#endif
};

static CoreEntry gCores[sizeof kCores / sizeof kCores[0]];

const CoreEntry *coreRegistry(int *count) {
    constexpr int n = int(sizeof kCores / sizeof kCores[0]);
    for (int i = 0; i < n; i++)
        gCores[i] = *kCores[i];
    *count = n;
    return gCores;
}

const CoreEntry *findCore(const std::string &id) {
    int n = 0;
    const CoreEntry *c = coreRegistry(&n);
    for (int i = 0; i < n; i++)
        if (id == c[i].info->id)
            return &c[i];
    return nullptr;
}

#endif
} // namespace mino
