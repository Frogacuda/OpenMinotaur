// game_tables.h - data tables of theGame.cpp (see game_tables.cpp).
#pragma once
#include <cstdint>
#include "neon_base.h"

namespace NEON_GAME {
namespace Neon_Internals {

extern const int kTheSprites[24];
extern const float kSizesX[24];
extern const float kSizesY[24];
extern const uint32_t kBannerColoursInfo[6];
extern const uint32_t kBannerColoursTitle[6];
extern const uint32_t kBannerColoursStart[6];
extern const int levelDescriptions[2000];
extern const uint32_t gameFlags[100];
extern const int minGruntDelay[100];
extern const int maxGruntDelay[100];
extern const float gruntSpeeds[100];
extern const float gruntSpeedMultipliers[100];
extern const float stepSpeedAdders[100];
extern const float maxGruntSpeeds[100];
extern const int extraLoveDurations[100];
extern const int spikeNums[100];
extern const int numSwarmers[30];
extern const float swarmerSpread[30];
extern const int frontFireRate[10];
extern const float cakeBullSpeeds[10];
extern const float lickerMineSpeeds[20];
extern const int kDroids[10];
extern const int kGruntSamples[6];
extern const char *const levelNames[100];
extern const char *const achievementMessages[10];
extern const char *const congratulatoryMessages[8];
extern const char *const kStoryText0[53];
extern const char *const kStoryText1[145];
extern const char *const kInfoText0[191];
extern const char *const kInfoText1[229];
extern const char *const kInfoText2[172];

extern const int bombPoints[54];
extern const int kPowerupIcons[9];
extern const int kPowerupProbs[63];
extern const int kBeastGraphics[5];
extern const int gruntBaseGraphic[100];
extern const int gruntAnimationType[100];
extern const float kGruntScaleY[9];
extern const int kGruntScaleTypes[10];
extern const float kObstacleXScales[10];
extern const float kObstacleYScales[10];
extern const int kObstacleFrames[10];
extern const uint32_t rockCols[4];
extern const float runningManSpeeds[10];
extern const int runningManShields[10];
extern const int numBlobs[10];

struct HowToLine {
    int index;
    const char *text;
};
extern const HowToLine kHowToPlay[];
extern const int kHowToPlayCount;

} // namespace Neon_Internals
} // namespace NEON_GAME
