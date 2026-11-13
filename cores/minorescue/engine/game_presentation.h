#pragma once
#include "neon/game_presentation.h"
namespace NEON_GAME {
namespace Neon_Internals {
class sprite;
class cellGrid;
using GamePresentationHost = neon::GamePresentationHost<sprite, cellGrid>;
} // namespace Neon_Internals
} // namespace NEON_GAME
