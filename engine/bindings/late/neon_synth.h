// neon_synth.h - the light-synth framework the game runs inside:
// NeonEffect -> NeonStack -> NeonModule_Base (+ waypoints), and the DNA
// loader (superSaver) that builds effects from testGrid.ox.
//
// Offsets record the source layout for provenance. C++ consumers access named
// members; these classes do not promise binary-compatible object layout.
//
// Execution, DNA construction and routing use neon::effects. The binding retains
// the module-facing member names; the module owns game and render state.
// Editor transitions, geometry/camera/replicator modules and DNA saving remain
// untranslated. Recognized unavailable module IDs fail explicitly.
#pragma once

#include "neon_base.h"
#include "neon/effect_runtime.h"
#include "game_presentation.h"
#include "neon/effect_dna.h"
#include "neon_device.h"

#include <cstdint>
#include <vector>

namespace NEON_GAME {
namespace Neon_Internals {

class NeonStack;
class NeonEffect;
class theGame;

using waypoint = neon::WaypointData;

// Identifies a texture source or a module input slot (NeonEffect::AssignTextureInput).
using _input_info = neon::effects::TextureRoute;

enum InputType { INPUT_NONE = -1, INPUT_STACK = 3, INPUT_LITERAL = 4, INPUT_FEEDBACK = 5 };

class NeonModule_Base {
  public:
    using ModuleType = NeonModule_Base;
    using StackType = NeonStack;
    using EffectType = NeonEffect;
    using DeviceType = NeonDevice;
    using RenderType = NeonBaseRender;
    using Vector3Type = D3DXVECTOR3;
    struct Param { // 0x24 bytes at 0x9c + i * 0x24
        int src[3] = {-1, -1, -1};
        int a = 0;
        int ival = 0;     // 0xac
        double value = 0; // 0xb0
    };
    struct Range { // 0x24 bytes at 0xd00 + i * 0x24
        int pad[3] = {0, 0, 0};
        double min = 0;   // 0xd0c
        double max = 0;   // 0xd14
        double saved = 0; // 0xd1c
    };
    struct Input { // 0x10 bytes at 0xc9c + k * 0x10
        int stack = -1;
        int num = -1;
        int type = -1;
        int *rtPtr = nullptr; // points at the source stack's output RT
    };

    NeonModule_Base();
    virtual ~NeonModule_Base() = default;

    // vtable order of the original
    virtual void runPrep();                                                     // 0
    virtual int Init_Module_Resources(NeonDevice *dev, int idx, NeonStack *st); // 1
    virtual void Release_Module_Resources();                                    // 2
    virtual int Run_Module() {
        return 0;
    } // 3
    virtual int Reset_Module() {
        return 0;
    } // 4
    virtual void setDefaults() {} // 5
    virtual void setBasics() {}   // 6

    void floatLimit(int i);
    void floatWrap(int i);
    void granularAlign(int i, int steps);
    void zeroPair(int a, int b);
    int SetModuleOutputRT(NeonBaseRender *r);
    int SetModuleSourceTextures(NeonBaseRender *r);
    void getWaypoint(waypoint *w);

    double &p(int i) {
        return params[i].value;
    }

    uint8_t prepped = 0;              // 0x04
    uint8_t running = 0;              // 0x05
    int kind = 0;                     // 0x08 3 = loop start, -1 = loop end
    int initialised = 0;              // 0x90
    NeonDevice *device = nullptr;     // 0x94
    NeonModule_Base *next = nullptr;  // 0x98
    Param params[64];                 // 0x9c
    Input inputs[6];                  // 0xc9c
    int index = -1;                   // 0xcfc
    Range ranges[64];                 // 0xd00
    NeonStack *stack = nullptr;       // 0x1600
    int mode = 0;                     // 0x1604
    int iconIndex = 0;                // 0x160c (editor icon, set by setBasics)
    unsigned inputFlags = 0;          // 0x1610
    int inputSrc[3] = {0, 0, 0};      // 0x1614
    int wrap[6] = {1, 1, 1, 1, 1, 1}; // 0x1620 S0, S1, S2, T0, T1, T2
    float fade = 1.0f;                // 0x1648
    int paramEnabled[64];             // 0x1654
    int transitionValue = 0;          // 0x1754 (always 0: transitions are editor-only)
    int typeId = 0;                   // 0x65c of the subclass layout (DNA module type)
};

class NeonStack {
  public:
    using ModuleType = NeonModule_Base;
    using StackType = NeonStack;
    using EffectType = NeonEffect;
    using DeviceType = NeonDevice;
    using RenderType = NeonBaseRender;
    using Vector3Type = D3DXVECTOR3;
    static constexpr int kMaxModules = 32;
    // This late binding provides 32K vertices to its six game consumers. The
    // capacity and flush threshold below are shared binding policy; offsets
    // on the following members describe the Gridrunner source layout.
    static constexpr int kMaxVerts = 32768;
    static constexpr int kSpriteFlushAfter = 0x7fc4;

    NeonStack();
    ~NeonStack();

    int Init(NeonDevice *dev, int id, int vertsKind0, int vertsKind1);
    int RunStack();
    int AddModule(NeonModule_Base *m, int pos) {
        return Prv_AddModule(m, pos, true);
    }
    int Prv_AddModule(NeonModule_Base *m, int pos, bool init);
    bool waypointExists(int bank, int slot) const;
    void getWaypoint(int bank, int slot);
    waypoint *&waypointAt(int module, int bank, int slot) {
        return waypoints[module][bank][slot];
    }

    int initialised = 0;                     // 0x0
    NeonDevice *device = nullptr;            // 0x4
    int rtW = 0, rtH = 0;                    // 0x8, 0xc
    int id = -1;                             // 0x10
    waypoint *waypoints[kMaxModules][3][64]; // 0x14
    NeonVertex verts[kMaxVerts];             // 0x6014 sprite batch store (iOS: 0x140000 bytes)
    NeonStack *nextStack = nullptr;          // 0x146014
    NeonBaseRender render;                   // 0x146018
    NeonModule_Base *modules = nullptr;      // 0x14656c
    int moduleCount = 0;                     // 0x146570
    int outputRT = -1;                       // 0x146574
    int prevRT = -1;                         // 0x146578
    int loopCount[16] = {};                  // 0x14657c
    matrix4f matrices[16];                   // 0x1465bc
    matrix4f view;                           // 0x1469bc
    NeonModule_Base *loopStart[16] = {};     // 0x1469fc
    int loopDepth = 0;                       // 0x146a3c
    int vb0 = -1, vb1 = -1;                  // 0x146a40, 0x146a44
    int stackIndex = 0;                      // 0x146a48
    NeonEffect *effect = nullptr;            // 0x146a8c
};

class NeonEffect {
  public:
    using ModuleType = NeonModule_Base;
    using StackType = NeonStack;
    using EffectType = NeonEffect;
    using DeviceType = NeonDevice;
    using RenderType = NeonBaseRender;
    using Vector3Type = D3DXVECTOR3;
    NeonEffect() = default;
    ~NeonEffect();

    int Init(NeonDevice *dev);
    int RunEffect();
    int AddStack(NeonStack *s, int vertsKind0, int vertsKind1);
    NeonStack *getStack(int stackIndex) const; // by NeonStack::stackIndex
    NeonStack *getTrueStack(int n) const;      // n-th stack in the list
    int getTrueStackIndex(int stackIndex) const;
    bool AssignTextureInput(const _input_info *src, const _input_info *dst);

    int initialised = 0;                      // 0x00
    NeonDevice *device = nullptr;             // 0x04
    NeonStack *stacks = nullptr;              // 0x08
    GamePresentationHost *gameHost = nullptr; // borrowed; module owns its game
    int stackCount = 0;                       // 0x10
    NeonEffect *nextEffect = nullptr;         // 0x14 (CDrawingObject effect list)
    int id = 0;                               // 0x18
    float aspect = 1.0f;                      // 0x1c
};

// Creates the module for a DNA type id (superSaver::createModule).
NeonModule_Base *createModule(int type);

// DNA streams for one effect plus the reader (superSaver). The grid file
// (testGrid.ox) holds the DNA of several effects, one per grid cell.
class superSaver : public neon::EffectDna {
  public:
    using ModuleType = NeonModule_Base;
    using StackType = NeonStack;
    using EffectType = NeonEffect;
    using DeviceType = NeonDevice;
    using RenderType = NeonBaseRender;
    using Vector3Type = D3DXVECTOR3;
    NeonModule_Base *loadModule();
    int loadWaypoint(waypoint *w);
    // Builds the effect; returns null if the DNA is malformed.
    NeonEffect *createEffectFromDNA(NeonDevice *dev, int effectId,
                                    GamePresentationHost *host = nullptr);
};

// Splits testGrid.ox into per-cell DNA (superSaver::loadGrid). cells[i] is
// empty unless cell i has DNA.
bool loadGrid(const std::vector<uint8_t> &file, std::vector<superSaver> &cells, int maxCells = 256);

} // namespace Neon_Internals
} // namespace NEON_GAME
