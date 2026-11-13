// Ship-variant data extracted from the iOS ARMv7 image.
constexpr float kTaxiPositionX[4] = {0.0f, -0.800000011920929f, 0.800000011920929f, 0.4000000059604645f}; // 0x1e6fc8
constexpr float kTaxiPositionY[4] = {0.30000001192092896f, 0.0f, 0.20000000298023224f, -0.800000011920929f}; // 0x1e6fb8
constexpr float kTaxiAimX[4] = {0.0f, -1.0f, 1.0f, 0.0f}; // 0x1e6fa8
constexpr float kTaxiAimY[4] = {1.0f, 0.0f, 0.0f, -1.0f}; // 0x1e6f98
constexpr float kAsteroidsPositionX[5] = {0.0f, -1.0f, 1.0f, -1.0f, 1.0f}; // 0x1e6f7c
constexpr float kAsteroidsPositionY[5] = {-1.0f, 1.0f, 1.0f, 1.0f, 1.0f}; // 0x1e6f60
constexpr float kAsteroidsAimX[5] = {0.0f, -0.30000001192092896f, 0.30000001192092896f, -0.20000000298023224f, 0.20000000298023224f}; // 0x1e6f44
constexpr float kAsteroidsAimY[5] = {-1.0f, -1.0f, -1.0f, 1.0f, 1.0f}; // 0x1e6f28
constexpr bool kAsteroidsDisabled[5] = {false, false, false, false, false}; // 0x1e6f20
constexpr bool kTaxiDisabled[4] = {false, false, false, false}; // 0x1e6fd8
constexpr double kAsteroidsThrustRise = 0.0009999999776482583; // pool 0x76040
constexpr double kThrustFall = 5.555555555555556e-05; // pool 0x7688c
constexpr double kAsteroidsContactPush = 0.04; // pool 0x768ac
