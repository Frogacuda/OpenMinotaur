// Module-facing resource names; neon owns the data and parsing.
#include "neon/resource_catalog.h"
namespace NEON_GAME {
namespace Neon_Internals {
using SpriteEntry = neon::SpriteEntry;
using SpritePage = neon::SpritePage;
using SampleEntry = neon::SampleEntry;
using FontEntry = neon::FontEntry;
// Preserve the class identity required by module forward declarations.
class NeonResManager : public neon::ResourceCatalog {};
} // namespace Neon_Internals
} // namespace NEON_GAME
