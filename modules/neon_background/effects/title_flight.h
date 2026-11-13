#pragma once
#include "pc_particles.h"
namespace neon::pc {
// Intentional title-screen variation. Atlas cells have equal visible longest
// sides (112px inside 128px cells), preserving each source sprite's aspect.
CubeFace titleFlightBillboards(const ShapeSettings &, const std::array<PobParticle, 1024> &,
                               double seconds, float size, float colourPeriod, float hueOffset,
                               float opacity, int selectedSprite);
} // namespace neon::pc
