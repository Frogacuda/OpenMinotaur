#include "neon_synth.h"
#include "neon/effect_runtime.h"
namespace NEON_GAME {
namespace Neon_Internals {
NeonModule_Base::NeonModule_Base() {
    neon::effects::constructModule(*this);
}
void NeonModule_Base::runPrep() {
    neon::effects::prepareModule(*this);
}
void NeonModule_Base::floatLimit(int i) {
    neon::effects::limit(*this, i);
}
void NeonModule_Base::floatWrap(int i) {
    neon::effects::wrapParameter(*this, i);
}
void NeonModule_Base::granularAlign(int i, int steps) {
    neon::effects::alignParameter(*this, i, steps);
}
void NeonModule_Base::zeroPair(int a, int b) {
    neon::effects::zeroPair(*this, a, b);
}
int NeonModule_Base::SetModuleOutputRT(NeonBaseRender *r) {
    return neon::effects::outputTarget(*this, r);
}
int NeonModule_Base::SetModuleSourceTextures(NeonBaseRender *r) {
    return neon::effects::sourceTextures(*this, r);
}
void NeonModule_Base::getWaypoint(waypoint *w) {
    neon::effects::applyWaypoint(*this, w);
}
NeonStack::NeonStack() {
    neon::effects::constructStack(*this);
}
NeonStack::~NeonStack() {
    neon::effects::destroyStack(*this);
}
int NeonStack::Init(NeonDevice *dev, int stackId, int vertsKind0, int vertsKind1) {
    return neon::effects::initializeStack(*this, dev, stackId, vertsKind0, vertsKind1);
}
int NeonStack::Prv_AddModule(NeonModule_Base *m, int pos, bool init) {
    return neon::effects::addModule(*this, m, pos, init);
}
bool NeonStack::waypointExists(int bank, int slot) const {
    return neon::effects::waypointExists(*this, bank, slot);
}
void NeonStack::getWaypoint(int bank, int slot) {
    neon::effects::selectWaypoint(*this, bank, slot);
}
int NeonStack::RunStack() {
    return neon::effects::runStack(*this);
}
NeonEffect::~NeonEffect() {
    neon::effects::destroyEffect(*this);
}
int NeonEffect::Init(NeonDevice *dev) {
    return neon::effects::initializeEffect(*this, dev);
}
int NeonEffect::RunEffect() {
    return neon::effects::runEffect(*this);
}
int NeonEffect::AddStack(NeonStack *s, int vertsKind0, int vertsKind1) {
    return neon::effects::addStack(*this, s, vertsKind0, vertsKind1);
}
NeonStack *NeonEffect::getStack(int stackIndex) const {
    return neon::effects::stackByIndex(*this, stackIndex);
}
NeonStack *NeonEffect::getTrueStack(int n) const {
    return neon::effects::stackAt(*this, n);
}
int NeonEffect::getTrueStackIndex(int stackIndex) const {
    return neon::effects::positionOfStack(*this, stackIndex);
}
bool NeonEffect::AssignTextureInput(const _input_info *src, const _input_info *dst) {
    return neon::effects::assignTexture(*this, src, dst);
}
int NeonModule_Base::Init_Module_Resources(NeonDevice *dev, int idx, NeonStack *st) {
    index = idx;
    stack = st;
    device = dev;
    initialised = 1;
    return 0;
}
void NeonModule_Base::Release_Module_Resources() {
    initialised = 0;
}
NeonModule_Base *superSaver::loadModule() {
    return neon::effects::loadModule(*this, createModule);
}
int superSaver::loadWaypoint(waypoint *w) {
    return neon::effects::loadWaypoint(*this, w);
}
NeonEffect *superSaver::createEffectFromDNA(NeonDevice *dev, int id, GamePresentationHost *host) {
    return neon::effects::buildEffect(*this, dev, id, host);
}
bool loadGrid(const std::vector<uint8_t> &bytes, std::vector<superSaver> &cells, int max) {
    return neon::effects::loadGrid(bytes, cells, max);
}
} // namespace Neon_Internals
} // namespace NEON_GAME
