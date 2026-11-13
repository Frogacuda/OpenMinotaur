#include "world.h"
#include "original_tables.h"
#include <stdexcept>
#include <cmath>

namespace goatup {
World::World(CollectionRecord &record, uint32_t seed, bool initializeStars)
    : random(seed), game(tower), effects(random), actors{game, goat, random, effects, record},
      denizens(actors), kids(actors, kidState), generator(random, game) {
    if (initializeStars)
        effects.initializeStars();
    feedback.scaleX = feedback.scaleY = .99f;
    generator.create = [this](const auto &birth) {
        if (birth.kind == PlatformBirth::Kind::ConveyorCap)
            effects.conveyorCap(*birth.platform, birth.frame, birth.angle, birth.tile);
        else
            denizens.create(birth);
    };
    generator.enterZone = [this] {
        if (game.returning && game.lastTheme < 7)
            return;
        zoneTicks = 120;
        constexpr int sounds[] = {0, 22, 27, 31, 33, 0, 38, 41, 0};
        if (sounds[game.lastTheme] && !game.demo)
            actors.play(sounds[game.lastTheme]);
    };
    tower.generate = [this](auto &platform) { generator.generate(platform); };
    game.sound = [this](int sample) { actors.play(sample); };
    game.splash = [this](int n, float speed, float spread, Vec2 p, uint32_t from, uint32_t to) {
        effects.splash(n, speed, spread, p, from, to);
    };
    game.tread = [this](auto &platform, int tile) { treadOnTile(platform, tile, actors, kids); };
    game.smash = [this](auto &platform, int tile) { smash(platform, tile); };
    actors.checkBridge = [this](auto &platform) { return checkBridge(platform); };
    tower.firstOffscreen = [this](auto &platform) {
        if (platform.special == 1 && !game.returning)
            denizens.create({PlatformBirth::Kind::CeDude});
    };
    game.settleSummit = [this] { settleSummit(); };
    game.homecoming = [this] {
        actors.award(3);
        denizens.homecoming();
        kidState.tally = false;
        summitCaption = "Welcome home!";
        summitTicks = 600;
    };
    game.trailStar = [this] { effects.playerTrail(goat.position, game.flightTicks); };
    game.contacts = [this] { denizens.touch(goat.position, goat.radius, 1, touching_); };
    kids.touch = [this](Vec2 p, float radius, int type, bool &touching) {
        denizens.touch(p, radius, type, touching);
    };
    actors.popup = [this](int points, Vec2 at, double vx, double vy, float radius) {
        effects.floatingScore(points, at, vx, vy, radius);
    };
    actors.bonusCaption = [this](int frame) {
        for (size_t n = 1; n < original::BonusCaptions.size(); ++n) {
            const auto &item = original::BonusCaptions[n];
            if (item.frame < 0 || item.frame == 0)
                break;
            if (item.frame == frame) {
                caption = item.text;
                captionTicks = 120;
                break;
            }
        }
    };
    actors.achievement = [this](int id) {
        goal = id;
        goalTicks = 120;
    };
    actors.status = [this](int kind, int ticks) {
        kidState.statusKind = kind;
        kidState.statusTicks = ticks;
    };
}
bool World::checkBridge(Platform &p) {
    // platform::checkBridge 5b9fc: forward scan, skip the duplicate left cap.
    int left = -1, right = -1;
    for (int n = 0; n < 40; ++n) {
        int tile = p.tiles[size_t(n)];
        if (left < 0 && (tile == 106 || (game.returning && tile == 101))) {
            left = n;
            if (++n >= 40)
                break;
            tile = p.tiles[size_t(n)];
        }
        if (tile == 107 || (game.returning && tile == 105)) {
            right = n;
            break;
        }
    }
    if (left < 0 || right < 0)
        return false;
    bridgeFeedback_ = .99f;
    if (!game.demo)
        actors.play(28);
    for (int n = left; n <= right + 1 && n < 40; ++n) {
        p.tiles[size_t(n)] = 0;
        effects.bridgeBit(p.centres[size_t(n)]);
    }
    return true;
}
void World::smash(Platform &p, int tile) {
    if (p.tiles.at(size_t(tile)) != 188)
        return;
    p.tiles[size_t(tile)] = 0;
    if (tile > 0)
        p.tiles[size_t(tile - 1)] = 0;
    if (tile < 39)
        p.tiles[size_t(tile + 1)] = 0;
    for (int n = 0; n < 38; ++n) {
        if (p.tiles[size_t(n + 1)] != 0 && p.tiles[size_t(n)] == 0 && p.tiles[size_t(n + 2)] == 0)
            p.tiles[size_t(n + 1)] = 0;
        if (p.tiles[size_t(n + 1)] == 0 && p.tiles[size_t(n)] != 0 && p.tiles[size_t(n + 2)] != 0)
            p.tiles[size_t(n + 1)] = 188;
    }
    constexpr int noises[] = {0, 1, 2, 3, 4, 4};
    const auto first = random.next(), second = random.next();
    actors.play(noises[(first & 3) + (second & 1)]);
    actors.flash = 1;
    if (!game.demo)
        actors.play(37);
    double vx = double(-.003f);
    for (int n = 0; n < 4; ++n) {
        PlatformBirth birth{PlatformBirth::Kind::PlatformBit};
        birth.frame = 188;
        birth.velocity = {vx, -.025};
        birth.position = p.centres[size_t(tile)] + birth.velocity;
        denizens.create(birth);
        vx = double(float(vx + .0029));
    }
}
void World::settleSummit() {
    if (!game.tallyBegun) {
        game.tallyBegun = true;
        kidState.tally = true;
        kidState.tallyMultiplier = 1;
        game.tallyComplete = goat.kids < 1;
        kidReleaseTicks_ = 30;
        summitCaption = goat.kids < 1    ? "You arrived all alone."
                        : goat.kids == 1 ? "You raised a kid!"
                                         : "You raised " + std::to_string(goat.kids) + " kids!";
        summitTicks = 300;
    } else if (!game.tallyComplete) {
        if (--kidReleaseTicks_ < 1) {
            kidReleaseTicks_ = 30;
            kidState.releaseOrdinal = goat.kids;
            const auto voice = random.next();
            if (!game.demo)
                actors.play(int((voice & 1023) % 6) + 6, 13);
            if (--goat.kids < 1)
                game.tallyComplete = true;
        }
    } else if (game.ticks > 600 && !game.summitBirdSent) {
        effects.summitBird(game, goat);
        game.summitBirdSent = true;
        if (!game.demo)
            actors.play(47);
    }
}
void World::updatePowerups() {
    // theGame::run 61ab4: warning flashes precede decrement/threshold tests.
    if (!paused && !game.demo) {
        if (actors.hazardFlash > 0)
            --actors.hazardFlash;
        if (actors.hazardGrace != 0 && --actors.hazardGrace == actors.hazardWarning) {
            actors.play(46, 10);
            actors.hazardFlash = 5;
            actors.hazardWarning = (actors.hazardGrace >> 1) + (actors.hazardGrace >> 2);
        }
        if (game.flightFlash > 0)
            --game.flightFlash;
        if (game.flightTicks != 0 && --game.flightTicks == game.flightWarning) {
            if (game.flightTicks < 300) {
                game.flightFlash = 5;
                actors.play(45, 10);
            }
            game.flightWarning = (game.flightTicks >> 1) + (game.flightTicks >> 2);
        }
    }
    if (actors.poison <= 0)
        return;
    if (!paused) {
        actors.poison = float(double(actors.poison) - .002);
        if (actors.poison <= 0) {
            feedback = {};
            feedback.scaleX = feedback.scaleY = .99f;
            return;
        }
    }
    const float amplitude = double(actors.poison) < .1 ? float(actors.poison * 10.f) : 1.f;
    // The original retains this product in f64 until the final colour store.
    const double blueAmplitude = double(actors.poison) < .1 ? double(amplitude) * .9 : .9;
    feedback.colour[0] =
        float(double(amplitude) * (double(std::sin(feedbackPhase)) * .5 + .5) * .85);
    feedback.colour[1] = float((double(std::sin(float(double(feedbackPhase) / 3.))) * .5 + .5) *
                               .7 * double(actors.poison));
    feedback.colour[2] =
        float(double(blueAmplitude) * (double(std::sin(float(feedbackPhase * 1.5f))) * .5 + .5));
    feedback.colour[3] = actors.poison;
    feedback.scaleX = feedback.scaleY = float(double(actors.poison) * -.2 + .99);
}
void World::tick(const ClimbInput &input) {
    feedbackPhase = float(double(feedbackPhase) + .1);
    if (!paused) {
        ++game.ticks;
        const auto colour = neon::paletteColour(0, 1, uint32_t(game.ticks),
                                                {neon::WashRule::TriangularRainbow, 12, 4});
        actors.cyclingBonusColour = {colour.r, colour.g, colour.b, colour.a};
    }
    updatePowerups();
    prepareHud(); // theGame::run prints/ages HUD before runUnpausedEntities.
    updateBridgeFeedback();
    if (!paused) {
        const auto colour = neon::paletteColour(5, 1, uint32_t(game.ticks),
                                                {neon::WashRule::TriangularRainbow, 12, 4});
        actors.vulnerableColour = {colour.r, colour.g, colour.b, colour.a};
        const auto protectedColour = neon::paletteColour(
            8, 1, uint32_t(game.ticks), {neon::WashRule::TriangularRainbow, 12, 4});
        actors.protectionColour = {protectedColour.r, protectedColour.g, protectedColour.b,
                                   protectedColour.a};
        // runUnpausedEntities 544e4: player, particles, stars, kids, NPCs,
        // then reserved/bits lists. UI follows in the separate always-run domain.
        if (!game.gameOver)
            actors.rewardKids = goat.kids;
        if (!game.gameOver) {
            if (actors.hazardGrace != 0)
                goat.drawColour = actors.protectionColour;
            else
                goat.drawColour.white(1);
        }
        goat.step(game, input);
        effects.tickParticles();
        effects.tickStars();
        kids.tick();
        denizens.tick();
        denizens.tickBits();
        actors.records.highestPlatform =
            std::max(actors.records.highestPlatform, game.highestPlatform);
    }
    actors.paused = paused;
    effects.tickUi(paused, tower.angle, actors.cyclingBonusColour, game.returning, game.gameOver,
                   actors.tablet);
}
void World::updateBridgeFeedback() {
    if (bridgeFeedback_ > .01f) {
        const float remaining = float(1.f - bridgeFeedback_);
        feedback.colour[0] = float(double(remaining) * -.7 + .9);
        feedback.colour[1] = float(double(remaining) * -.6 + .6);
        feedback.scaleX = feedback.scaleY = float(double(remaining) * .09 + .9);
        bridgeFeedback_ = float(double(bridgeFeedback_) * .9);
        if (bridgeFeedback_ <= .01f) {
            feedback.colour[0] = feedback.colour[1] = 0;
            feedback.scaleX = feedback.scaleY = .99f;
        }
    }
}
void World::prepareHud() {
    hudRows.clear();
    hudGameOver = game.gameOver;
    const auto palette = [&](int type) {
        auto c = neon::paletteColour(type, 1, uint32_t(game.ticks),
                                     {neon::WashRule::TriangularRainbow, 12, 4});
        return neon::colour{c.r, c.g, c.b, c.a};
    };
    const auto row = [&](int y, std::string text, int type, int x = -1) {
        hudRows.push_back({x, y, std::move(text), palette(type)});
    };
    row(3, "Score: " + std::to_string(actors.score), 5, 4);
    if (game.gameOver) {
        for (int y = 6; y <= 8; ++y)
            row(y, "*** GOAT OVER ***", 5);
        row(14, "THIS GAME", 8);
        row(16, "Unique items found: " + std::to_string(actors.uniqueRunBonuses), 5);
        row(18, "Highest platform: " + std::to_string(game.highestPlatform), 5);
        row(20, "Goats kissed: " + std::to_string(actors.runBillyGoats), 5);
        row(22, "Most kids: " + std::to_string(kidState.mostInRun), 5);
        row(24, "LIFETIME", 8);
        row(26, "Unique items found: " + std::to_string(actors.records.uniqueBonuses), 4);
        row(28, "Highest platform: " + std::to_string(actors.records.highestPlatform), 4);
        row(30, "Goats kissed: " + std::to_string(actors.records.billyGoats), 4);
        row(32, "Most kids: " + std::to_string(actors.records.mostKids), 4);
        return;
    }
    // These message slots age while paused too. They are distinct source slots:
    // +43c bonus caption, +440 zone, +448 summit/home, +44c/+450 award.
    for (auto *timer : {&zoneTicks, &captionTicks, &summitTicks, &goalTicks, &kidState.statusTicks})
        if (*timer > 0)
            --*timer;
    constexpr const char *zones[] = {
        "GREEN GRASS ZONE",    "BOUNTY BOB'S DONKEY'S ZONE", "MANIC MINING ZONE",
        "GOAT'S REVENGE ZONE", "CITY CONNECTION ZONE",       "THE PINKIE PIE FACTORY",
        "ROLLING SKULL ZONE",  "CHUCKIE EGGAGOG ZONE",       "ARRIVING AT THE TOP!"};
    constexpr const char *goals[] = {
        "'FECUNDITY'",    "'ULTRA-FECUNDITY'", "'MOUNTAIN GOAT'", "'HOMECOMING'", "'BOP THE OX'",
        "'SUPER MOTHER'", "'REVENGE'",         "'COLLECTOR'",     "'HOARDER'",    "'COMPULSIVE'"};
    if (kidState.statusTicks > 0)
        row(18, original::KidMessages.at(size_t(kidState.statusKind)), 5);
    if (captionTicks > 0)
        row(16, caption, 4);
    if (zoneTicks > 0 && game.lastTheme >= 0)
        row(10, zones[game.lastTheme], 4);
    if (summitTicks > 0)
        row(12, summitCaption, 4);
    if (goalTicks > 0 && unsigned(goal) < 10) {
        row(22, goals[goal], 2);
        row(20, "HOORAY! YOU GET DISTINCTION", 0);
    }
}
void World::prepareBackground(std::vector<ActorDraw> &out) {
    effects.drawStars(out);
    tower.dim = game.gameOver;
    tower.prepare();
}
void World::prepareForeground(std::vector<ActorDraw> &out) {
    // theGame::_draw 59cd8: kids, particles, bits, NPC, UI, main goat.
    kids.draw(out);
    effects.drawParticles(out);
    denizens.drawBits(out);
    denizens.prepareDraw(out);
    effects.prepareUiDraw(out);
    if (game.gameOver)
        return;
    goat.prepareDraw(game.diveRequested);
    ActorDraw player;
    player.position = goat.position;
    player.scale = goat.projectedScale;
    player.frame = goat.frame;
    player.angle = goat.angle;
    player.radius = goat.radius;
    player.colour = goat.drawColour;
    out.push_back(player);
    if (game.flightTicks > 0 && game.flightFlash == 0) {
        ActorDraw wings = player;
        wings.frame = 93;
        wings.colour.white(1);
        if (goat.platform < 0 && goat.velocity.y <= 0)
            wings.frame += (game.flightTicks >> 2) & 1;
        out.push_back(wings);
    }
}
} // namespace goatup
