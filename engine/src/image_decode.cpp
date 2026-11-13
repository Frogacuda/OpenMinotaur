// One CPU decoder shared by resource uploads and platform artwork/shader LUTs.
// No GL context, SDL, game state or test-core dependency.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"
