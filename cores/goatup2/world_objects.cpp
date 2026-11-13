#include "stage_runtime.h"
#include "campaign_actors.h"
#include "actor_tables.h"
#include "neon/entity_list.h"
#include "neon/integer.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace goatup2 {
namespace {
// Original iOS tables 0x7be84, 0x7b218, 0x7e4ec and 0x7b1d8.
constexpr int32_t pickupCategories[] = {3, 9, 11, 12, 13, 13, 13, 13, 14, 15, 16, 17};
constexpr int32_t pickupSprites[] = {0, 120, 152, 153, 22, 23, 24, 163, 165, 175, 180, 0};
constexpr int32_t fruitSprites[] = {71, 73, 74,  75, 76, 77, 78, 79, 80, 81,
                                    82, 83, 107, 51, 52, 53, 54, 71, 71, 71};
constexpr int32_t grassSprites[] = {63,  64,  65,  66,  67,  68,  69,  70,
                                    121, 122, 123, 124, 125, 126, 127, 129};
constexpr double sourcePi = 0x1.921fb54524550p+1;
int32_t add(int32_t a, int32_t b) {
    return neon::integers::add32(a, b);
}
int32_t product(int32_t a, int32_t b) {
    return neon::integers::signedBits(uint32_t(a) * uint32_t(b));
}
float sign(float g) {
    return g < 0 ? -1.f : 1.f;
}
void sound(StageRuntime &world, int32_t sample, unsigned shift = 12) {
    if (!world.medium.splashSoundSuppressed)
        world.playSample(sample, uint32_t(world.medium.sampleVolume) << shift, 0);
}
} // namespace

int32_t fruitSpriteForStage(int32_t index) {
    if (index < 0)
        throw std::out_of_range("negative source stage index");
    return fruitSprites[index % 20];
}
int32_t pickupSpriteForStage(int32_t index, int variant) {
    if (variant < 0 || variant >= 12)
        throw std::out_of_range("pickup artwork variant");
    return variant == 0 || variant == 11 ? fruitSpriteForStage(index) : pickupSprites[variant];
}
PickupActor::PickupActor(StageRuntime &world, StageAllocationInputs a, neon::vec2d spawn,
                         uint32_t flags)
    : StageActor(world, a, 0, false) {
    const unsigned variant = flags & 255U, transform = flags >> 8;
    if (variant >= 12)
        throw std::invalid_argument("pickup variant outside original descriptor");
    category = pickupCategories[variant];
    sprite = pickupSprites[variant];
    motionFlags = 0;
    attackPower = 100;
    stagePosition = savedStagePosition = spawn;
    if (variant == 0 || variant == 11) {
        if (owner().stageIndex < 0)
            throw std::invalid_argument("negative source stage index");
        sprite = fruitSpriteForStage(owner().stageIndex);
        paletteIndex = uint32_t(owner().fruitPaletteIndex) < 32U ? owner().fruitPaletteIndex : -1;
        owner().requiredFruit = add(owner().requiredFruit, 1);
        if (variant == 11)
            gravity = .00035f;
    }
    if (category == 11) {
        buoyancy = -.1f;
        gravity = .00035f;
    }
    drawScale = owner().spriteScale(sprite);
    if (category == 12) {
        colour.a = 0;
        drawScale = drawScale * 2.;
    }
    if (category == 14)
        colour = owner().rainbow;
    if (category == 15) {
        heading = float(owner().random.range(-sourcePi, sourcePi));
        // Source consumes this radius draw, then its descriptor assignment
        // replaces the result. Keep the draw without scaling the final radius.
        const float unused = float(owner().random.range(.8, 1.2) * double(radius));
        (void)unused;
    }
    radius = .03f;
    savedDrawScale = drawScale;
    if (transform & 1U) {
        velocity.x = -velocity.x;
        drawScale.x = -drawScale.x;
    }
    if (transform & 2U) {
        gravity = -gravity;
        drawScale.y = -drawScale.y;
    }
}
void PickupActor::run() {
    // 0x60100: these category-specific operations precede EVERY state branch.
    if (category == 11 && resting)
        heading = float(owner().random.range(-.5235987756, .5235987756) *
                        double(std::sin(owner().sourceTime)));
    else if (category == 12 && owner().goat) {
        const float distance = float((stagePosition - owner().goat->stagePosition).length());
        const double near = .3 - double(distance);
        if (near > 0)
            colour.a = near / .3; // no far-away alpha reset in the original
    } else if (category == 14)
        colour = owner().rainbow;
    if (headbuttState == 0) {
        if (gravity != 0 && readyForContacts)
            fall();
        prepareWorldPosition();
    } else if (headbuttState == 1) {
        followGoat();
        colour.a = 1;
        owner().activeFollowers = add(owner().activeFollowers, 1);
        if (followedState == 1) {
            owner().emitRadialBurst(*this, 10, .01f, 7.f, 5.f);
            if (category == 15) {
                gravity = 0;
                stagePosition = savedStagePosition;
                headbuttState = 0;
                colour.a = 1;
            } else
                dead = true;
        }
        if (followedState == 2) {
            owner().explode(*this, 20);
            dead = true;
            static constexpr int32_t exitSounds[] = {71, 44, 44, 45, 45, 46, 46, 47, 47, 6, 6, 6};
            if (!owner().medium.splashSoundSuppressed) {
                if (owner().exitOrdinal < 0 || owner().exitOrdinal >= 12)
                    throw std::out_of_range("original pickup exit sound table index");
                owner().playSample(exitSounds[owner().exitOrdinal],
                                   uint32_t(owner().medium.sampleVolume) << 12, 0);
            }
            rewardValue = product(owner().exitOrdinal, 1000);
            owner().addScore(rewardValue);
            owner().exitBonus = add(owner().exitBonus, rewardValue);
            owner().exitBonusProgress = owner().exitBonusProgress + owner().exitBonusStep;
        }
    } else if (headbuttState == 2) {
        colour.a = 0;
        if (owner().goat && owner().goat->state == 1) {
            gravity = 0;
            stagePosition = savedStagePosition;
            headbuttState = 0;
            colour.a = 1;
        }
    }
}
void PickupActor::preparePresentation() {
    drawMarker = false;
    drawMain = true;
    if (category == 15 && headbuttState != 0) {
        const auto position = stagePosition;
        const float objectSize = radius;
        stagePosition = savedStagePosition;
        sprite = 2;
        colour.r = 0;
        colour.g = colour.b = colour.a = 1;
        radius = .01f;
        StageActor::preparePresentation();
        markerWorldPosition = worldPosition;
        drawMarker = true;
        stagePosition = position;
        radius = objectSize;
        sprite = 175;
        colour.r = colour.g = colour.b = colour.a = 1;
        if (headbuttState == 2) {
            drawMain = false;
            return;
        }
    }
    StageActor::preparePresentation();
}
void PickupActor::hitBy(StageActor *other) {
    // 0x60500, before factoring out repeated inline floating-score construction.
    if (headbuttState != 0)
        return;
    other->onContact(this);
    if (other != owner().goat.get() || other->category != 1 || owner().goat->state != 0)
        return;
    auto &goat = *owner().goat;
    if (category != 16) {
        goat.followingCount = add(goat.followingCount, 1);
        serial = goat.followingCount;
        followDelay = 0;
        targetDelay = product(goat.followingCount, 6);
        goat.hunger = std::min(1.f, float(double(goat.hunger) + .05));
    }
    headbuttState = 1;
    owner().emitCollectionBurst(*this);
    const int32_t bonus = owner().headbuttDisabled ? 5 : 1;
    const int32_t factor = add(bonus, owner().multiplier);
    float labelScale = 5;
    int32_t sample = -1;
    switch (category) {
    case 3:
    case 17:
        rewardValue = product(factor, 100);
        owner().addScore(rewardValue);
        sound(owner(), owner().multiplier % 8 + 28);
        owner().requiredFruit = add(owner().requiredFruit, -1);
        if (owner().requiredFruit < 1)
            sound(owner(), 36, 11);
        labelScale = 3;
        break;
    case 9:
        rewardValue = product(factor, 1000);
        owner().addScore(rewardValue);
        owner().bonusPickups = add(owner().bonusPickups, 1);
        // Both choices AND the play call are unconditional in this branch.
        {
            const auto first = owner().random.next(), second = owner().random.next();
            owner().playSample(int32_t((first & 3U) + (second & 1U)),
                               uint32_t(owner().medium.sampleVolume) << 12, 0);
        }
        break;
    case 11:
        rewardValue = product(factor, 500);
        owner().addScore(rewardValue);
        goat.hunger = std::min(.98f, float(double(goat.hunger) + .1));
        sample = 62;
        break;
    case 12:
        rewardValue = product(factor, 5000);
        owner().addScore(rewardValue);
        labelScale = 7;
        if (!owner().headbuttDisabled) {
            owner().sharedGrace = 1000;
            sample = 63;
        } else {
            owner().headbuttDisabled = false;
            sample = 74;
            owner().messages[0] = {"  YOU LOYALLY GIVE YOUR CURRY TO HER MAJ!", 150, 6, 2};
            owner().messages[1] = {"    ** FARTING MAY RESUME FORTHWITH **", 150, 8, 2};
        }
        break;
    case 13:
    case 15:
        rewardValue = product(factor, 500);
        owner().addScore(rewardValue);
        sample = 67;
        break;
    case 14:
        rewardValue = product(owner().multiplier, 10000);
        owner().sharedGrace = 0;
        owner().sounds.push_back({63, 0, 0, RuntimeSound::Kind::Stop});
        owner().textRowsToClear.push_back(6);
        owner().textRowsToClear.push_back(8);
        owner().addScore(rewardValue);
        sample = 73;
        break;
    case 16:
        rewardValue = product(factor, 500);
        owner().addScore(rewardValue);
        sample = 77;
        break;
    default:
        throw std::logic_error("untranslated source pickup category");
    }
    if (sample >= 0)
        sound(owner(), sample);
    auto &label = owner().showScore(*this, rewardValue, labelScale);
    if (category == 14) {
        label.fade = .018f;
        owner().headbuttDisabled = true;
        owner().messages[0] = {"  STRICTLY NO FARTING IN FRONT OF HER MAJ!", 250, 6, 3};
        owner().messages[1] = {"    ** CANNOT FART TILL CURRY FOUND **", 350, 8, 3};
    } else if (category == 16) {
        const double hunger = double(goat.hunger) + .2;
        goat.hunger = hunger > .98 ? .98f : float(hunger);
        goat.invulnerability = 100;
        dead = true;
    }
}

GrassBonusActor::GrassBonusActor(StageRuntime &world, StageAllocationInputs a, int32_t column,
                                 int32_t row, uint8_t grass)
    : StageActor(world, a, grass == 4 ? .00035f : -.00035f, false) {
    sprite = grassSprites[owner().random.next() & 15U];
    radius = .03f;
    drawScale = savedDrawScale = owner().spriteScale(sprite);
    colour.a = 0;
    const auto cell = owner().layout.cellSize;
    stagePosition = {
        (cell.x * .5) + (double(float(column)) * cell.x - double(owner().bounds.halfWidth)),
        (cell.y * .5) + (double(float(row)) * cell.y - double(owner().bounds.halfHeight))};
    stagePosition.y += gravity >= 0 ? double(radius) : -double(radius);
}
void GrassBonusActor::award() {
    velocity.y = double(sign(gravity)) * (-.008);
    bonusState = 2;
    rewardValue = product(product(owner().multiplier, owner().grassSequence),
                          owner().headbuttDisabled ? 10 : 1);
    owner().grassSequence = add(owner().grassSequence, 1);
    owner().addScore(rewardValue);
    owner().showScore(*this, rewardValue);
    sound(owner(), 48);
}
void GrassBonusActor::run() {
    if (bonusState == 2) {
        stagePosition.y = velocity.y + stagePosition.y;
        velocity.y += double(gravity);
        radius = float(double(radius) * 1.02);
        colour.a -= .01;
        if (colour.a < 0) {
            colour.a = 0;
            dead = true;
        }
    } else if (bonusState == 1) {
        fall();
        if (!resting)
            heading = float(double(heading) + .16);
        else
            award();
        colour.a = std::min(1., colour.a + .1);
    }
}
void GrassBonusActor::hitBy(StageActor *other) {
    if (bonusState == 1) {
        if (other->category == 1 && resting)
            award();
    } else if (bonusState == 0 && owner().goat && sign(owner().goat->gravity) == sign(gravity)) {
        velocity.y = double(sign(gravity)) * (-.008);
        resting = false;
        stagePosition.y = velocity.y + stagePosition.y;
        bonusState = 1;
        sound(owner(), 26);
    }
}

ExitActor::ExitActor(StageRuntime &world, StageAllocationInputs a, neon::vec2d spawn)
    : StageActor(world, a, 0, false) {
    category = 10;
    sprite = 49;
    radius = .05f;
    stagePosition = spawn;
    drawScale = savedDrawScale = owner().spriteScale(49);
    colour = owner().rainbow;
}
void ExitActor::run() {
    if ((owner().unlockExit || owner().requiredFruit < 1) && headbuttState == 0) {
        headbuttState = 1;
        colour.r = colour.g = colour.b = colour.a = 1;
        sprite = 50;
    }
    if (headbuttState == 2) {
        radius = float(double(radius) * 1.04);
        colour.a -= .01;
        if (colour.a < 0) {
            colour.a = 0;
            dead = true;
        }
    } else if (headbuttState == 0) {
        colour = owner().terrainPalette[2];
        prepareWorldPosition();
    }
}
void ExitActor::hitBy(StageActor *other) {
    if (headbuttState != 1 || other != owner().goat.get() || other->category != 1 ||
        owner().goat->state != 0)
        return;
    auto &goat = *owner().goat;
    goat.state = 2;
    const float count = float(goat.followingCount);
    const float start = std::max(-.3f, float(double(count) * -.05));
    owner().exitBonusProgress = start;
    owner().exitBonusStep = (start * (-2.f)) / count; // retain source IEEE zero-count result
    owner().exitOrdinal = 1;
    sound(owner(), 37);
    sound(owner(), 40);
    if (owner().stageTicks < owner().bestStageTime) {
        owner().bestStageTime = owner().stageTicks;
        owner().newBestStageTime = true;
        owner().timeRecordCounter = add(owner().timeRecordCounter, 1);
    }
    headbuttState = 2;
    owner().emitCollectionBurst(*this);
    auto total = std::make_unique<ExitTotalizer>(*this);
    neon::entities::prepend<StageActor>(owner().overlayEffects, *total);
    total.release(); // source 0x5f710 -> +3b4
}
ExitTotalizer::ExitTotalizer(StageActor &target) : FloatingScoreActor(target, 100) {
    screenSpace = true;
    worldPosition = {0, -.1};
    velocity = {0, 0};
    fade = .01f;
    growth = 1.01f;
    radius = .03f;
    paletteIndex = 0;
    iconSprite = 0;
    owner().exitBonus = 100;
}
void ExitTotalizer::run() {
    if (owner().originalPauseFlag)
        return;
    life = life - fade;
    worldPosition = worldPosition + velocity;
    if (life < 0) {
        life = 0;
        dead = true;
    }
    radius = radius * growth;
    colour = owner().terrainPalette.at(size_t(paletteIndex));
    colour.a = double(life);
    if (owner().exitBonus != observedTotal) {
        observedTotal = owner().exitBonus;
        radius = float(double(float(owner().exitOrdinal)) * .005 + .05);
        paletteIndex = owner().exitOrdinal;
        if (observedTotal != value) {
            setNumber(observedTotal);
            life = 1;
        }
    }
}

void StageRuntime::spawnPickup(neon::vec2d p, uint32_t flags) {
    auto child = std::make_unique<PickupActor>(*this, allocation, p, flags);
    neon::entities::prepend<StageActor>(contactObjects, *child);
    child.release();
}
void StageRuntime::spawnExit(neon::vec2d p) {
    auto child = std::make_unique<ExitActor>(*this, allocation, p);
    neon::entities::prepend<StageActor>(contactObjects, *child);
    child.release();
}
void StageRuntime::spawnGrassBonuses() {
    for (int32_t column = 0; column < layout.columns; ++column)
        for (int32_t row = 0; row < layout.rows; ++row) {
            const uint8_t grass = uint8_t(working.at(size_t(row) * 100 + column));
            if ((grass == 4 || grass == 5) && (random.next() & 0xe0U) < 0x20U) {
                auto child =
                    std::make_unique<GrassBonusActor>(*this, allocation, column, row, grass);
                neon::entities::prepend<StageActor>(grassBonuses, *child);
                child.release();
            }
        }
}
void StageRuntime::spawnOriginalObjects(const PlacementTable &placements) {
    if (goat || contactObjects || enemies || grassBonuses)
        throw std::logic_error("stage objects already constructed");
    for (const auto &p : placements.entries()) {
        if (p.type > 9)
            throw std::logic_error("stage requires an untranslated enemy/device/platform family");
        if (p.type == 2 && (p.flags & 255U) >= 12U)
            throw std::logic_error("untranslated pickup variant");
        if (p.type == 3 && p.flags >= 6U)
            throw std::logic_error("untranslated device variant");
        if (p.type >= 4 && p.type <= 8 && !actor_data::find(p.type, int(p.flags & 255U)))
            throw std::logic_error("actor record outside reconstructed campaigns");
    }
    requiredFruit = 0;
    int slot = 0;
    for (const auto &p : placements.entries()) {
        switch (p.type) {
        case 0:
            spawnGoat({p.x, p.y}, p.flags != 0, spriteScale(107));
            break;
        case 1:
            spawnKid({p.x, p.y}, p.flags != 0);
            break;
        case 2:
            spawnPickup({p.x, p.y}, p.flags);
            break;
        case 3: {
            auto child =
                std::make_unique<DeviceActor>(*this, neon::vec2d{p.x, p.y}, int(p.flags), slot);
            neon::entities::prepend<StageActor>(contactObjects, *child);
            child.release();
            break;
        }
        case 4:
        case 5:
        case 6:
        case 7:
        case 8: {
            auto child =
                std::make_unique<EnemyActor>(*this, neon::vec2d{p.x, p.y}, p.type, p.flags);
            neon::entities::prepend<StageActor>(enemies, *child);
            child.release();
            break;
        }
        case 9:
            spawnExit({p.x, p.y});
            break;
        }
        ++slot;
    }
    spawnGrassBonuses();
    initialFruit = requiredFruit;
}
} // namespace goatup2
