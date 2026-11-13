#include "world.h"
#include "original_tables.h"
#include "records.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace fiveaday;
void check(bool value, const char *why) {
    if (!value)
        throw std::runtime_error(why);
}
int main() {
    try {
        {
            World restart;
            restart.start();
            restart.formationBonusMultiplier = 2;
            restart.start();
            check(restart.formationBonusMultiplier == 1, "New game clears formation bonus");
            restart.formationBonusMultiplier = 2;
            restart.newLevel();
            check(restart.formationBonusMultiplier == 1,
                  "New level clears formation bonus through ship init");
            Attacker carrier(restart);
            carrier.formationMember = true;
            Bullet shot(restart);
            shot.type = 600;
            carrier.hitBy(shot);
            check(restart.score == 100,
                  "First formation carrier after restart awards the base 100 points");
            restart.ship.shieldTicks = 1;
            restart.ship.run();
            restart.ship.run();
            check(restart.formationBonusMultiplier == 2,
                  "Shield expiry retains the earned formation bonus");
            restart.ship.integrity = .8f;
            restart.ship.run();
            check(restart.formationBonusMultiplier == 1,
                  "Damage clears the earned formation bonus");
        }
        World w;
        w.start();
        check(w.level == -1 && w.spawnInterval == 120 && w.populationLimit == 10,
              "First original stage/header");
        check(w.random.draws() == 546, "Opening cloud/header RNG order");
        Wrapped actor(w);
        actor.retained = {double(.8f), -double(.8f)};
        actor.velocity = {};
        actor.run();
        check(actor.wrapped == 0, "Wrap is strict at the original float boundary");
        actor.retained = {double(.8f) + .001, -double(.8f) - .001};
        actor.run();
        check(actor.wrapped == 9, "Wrap signs and bitmask");
        std::vector<ActorDraw> frame;
        int shots = 0, kills = 0;
        bool sawEnemy = false;
        for (int tick = 0; tick < 10000 && !w.gameOver && w.checkpoint.empty(); ++tick) {
            const double a = double(tick) * .01;
            w.tick({{std::sin(a), -std::cos(a)}, true});
            w.prepareForeground(frame);
            check(std::isfinite(w.camera.x) && std::isfinite(w.camera.y), "Finite source camera");
            sawEnemy |= w.enemies != nullptr;
            shots += w.bullets != nullptr;
            kills = std::max(kills, w.score);
        }
        check(w.level >= 0, "Composed route completes first boss and bonus into next script");
        check(shots > 100, "Automatic fire connected");
        check(sawEnemy, "Script actor families connected");
        World timeline;
        timeline.start();
        int firstShot = 0, firstEnemy = 0, firstCarrier = 0;
        for (int tick = 1; tick <= 650; ++tick) {
            timeline.tick({});
            timeline.prepareForeground(frame);
            if (timeline.bullets && !firstShot)
                firstShot = tick;
            if (timeline.enemies && !firstEnemy)
                firstEnemy = tick;
            if (timeline.fruits && !firstCarrier)
                firstCarrier = tick;
        }
        check(firstShot == 10 && firstEnemy == 251 && firstCarrier == 611,
              "Original entry and first-formation schedule");
        timeline.population = timeline.populationLimit;
        timeline.spawnTicks = 2;
        const auto cursor = timeline.scriptCursor;
        timeline.sequencePaused = false;
        timeline.ship.state = Ship::State::Flying;
        timeline.sequence();
        check(timeline.scriptCursor == cursor, "Population equality waits for countdown");
        timeline.sequence();
        check(timeline.scriptCursor == cursor + 1,
              "Population equality spawns when countdown expires");
        for (int n = 0; n < 256; ++n) {
            timeline.ticks = uint64_t(n);
            for (int c = 0; c < 6; ++c) {
                const auto colour = timeline.palette(c);
                check(colour.r >= 0 && colour.r <= 1 && colour.g >= 0 && colour.g <= 1 &&
                          colour.b >= 0 && colour.b <= 1,
                      "Native palette range");
            }
        }
        World contact;
        contact.start();
        contact.ship.state = Ship::State::Flying;
        contact.ship.position = {};
        const auto launch = contact.launch();
        auto *enemy = new Attacker(contact);
        enemy->position = {0, -.05};
        enemy->init(launch);
        enemy->culled = false;
        contact.add(contact.enemies, enemy);
        auto *bullet = new Bullet(contact);
        bullet->position = enemy->position;
        bullet->type = 600;
        contact.add(contact.bullets, bullet);
        contact.touch(*bullet, contact.enemies);
        check(enemy->deathTicks == 2 && bullet->dead, "Native contact death delayed by two runs");
        const int population = contact.population;
        enemy->run();
        check(contact.population == population, "Delayed population accounting");
        enemy->run();
        check(contact.population == population - 1 && contact.score == 10,
              "Retirement score and population");
        const auto p = contact.launch();
        Attacker carrier(contact);
        carrier.position = {.1, .1};
        carrier.init(p);
        carrier.carrier = true;
        Fruit fruit(contact, carrier);
        fruit.state = Fruit::State::Loose;
        fruit.retained = fruit.position = {.1, 0};
        contact.camera = {};
        fruit.run();
        check(fruit.drift.x < -.00001 && fruit.drift.x > -.01,
              "Original 50000-strength fruit attraction");
        fruit.hitBy(contact.ship);
        check(fruit.state == Fruit::State::Collected && contact.ship.collected == 2,
              "Fruit pickup and ordered ship trail slot");
        contact.ship.integrity = 0;
        contact.ship.run();
        check(contact.ship.state == Ship::State::Dying, "Damage starts source death state");
        for (int n = 0; n < 400 && contact.ship.state != Ship::State::Entering; ++n)
            contact.ship.run();
        check(contact.lives == 2 && contact.ship.state == Ship::State::Entering,
              "Death timer/life/respawn path");
        World bossWorld;
        bossWorld.start();
        bossWorld.scriptCursor = bossWorld.script.size();
        bossWorld.ship.state = Ship::State::Flying;
        bossWorld.ship.position = {};
        bossWorld.currentLaunch = bossWorld.launch();
        auto *boss = new Boss(bossWorld, bossWorld.currentLaunch, {false});
        boss->position = boss->retained = {0, -.25};
        boss->culled = false;
        bossWorld.add(bossWorld.enemies, boss);
        bossWorld.bossLaunched = true;
        check(boss->health == 5 && boss->reward == 1000 && !boss->armed,
              "Original first boss initialization");
        const int beforeMissile = bossWorld.population;
        const auto beforeDraws = bossWorld.random.draws();
        HomingMissile missile(bossWorld, *boss);
        check(bossWorld.population == beforeMissile && missile.weight == 0 && missile.frame == 107,
              "Native missile is outside stage population");
        check(bossWorld.random.draws() == beforeDraws + 2,
              "Missile init retains both overridden random draws");
        bossWorld.ship.state = Ship::State::Waiting;
        missile.run();
        check(missile.dead, "Missile dissolves while player is not flying");
        bossWorld.ship.state = Ship::State::Flying;
        bossWorld.ticks = 8;
        Fruit reflected(bossWorld, *boss);
        boss->didHit(reflected);
        check(boss->health == 4, "Reflected fruit damages boss");
        boss->didHit(reflected);
        check(boss->health == 4,
              "Source reflection latch prevents repeat in same eight-frame gate");
        bossWorld.ticks = 9;
        boss->didHit(reflected);
        bossWorld.ticks = 16;
        boss->didHit(reflected);
        check(boss->health == 3, "Source reflection latch rearms outside gate");
        for (int n = 0; n < 3; ++n) {
            Bullet hit(bossWorld);
            hit.type = 600;
            hit.position = boss->position;
            boss->hitBy(hit);
            check(hit.dead, "Boss hit calls player projectile response");
        }
        check(boss->deathTicks == 2, "Boss enters original two-run retirement");
        boss->run();
        check(bossWorld.bossDefeated && bossWorld.ship.state == Ship::State::Leaving,
              "Boss defeat starts native exit flight");
        check((bossWorld.achievements & 1) && bossWorld.goal == 0,
              "Clean-stage FRESH award and feedback event retained");
        boss->run();
        check(bossWorld.score == 1000, "Boss reward applied at retirement");
        for (int n = 0; n < 1000 && bossWorld.ship.state != Ship::State::Bonus; ++n)
            bossWorld.ship.run();
        check(bossWorld.ship.state == Ship::State::Bonus && bossWorld.scenery,
              "Exit flight starts original tally and sun");
        World protectedBoss;
        protectedBoss.start();
        protectedBoss.ship.state = Ship::State::Waiting;
        protectedBoss.currentLaunch = protectedBoss.launch();
        Boss survivor(protectedBoss, protectedBoss.currentLaunch, {false});
        survivor.position = {};
        survivor.health = 1;
        Bullet hit(protectedBoss);
        hit.type = 600;
        survivor.hitBy(hit);
        check(survivor.health == 1 && survivor.deathTicks == 0,
              "Boss cannot finish while ship is unavailable");
        World bank;
        bank.start();
        bank.scriptCursor = bank.script.size();
        bank.ship.state = Ship::State::Leaving;
        bank.ship.collected = 6;
        Attacker bankCarrier(bank);
        bankCarrier.carrier = true;
        for (int n = 0; n < 5; ++n) {
            auto *f = new Fruit(bank, bankCarrier);
            f->state = Fruit::State::Collected;
            bank.add(bank.fruits, f);
        }
        const auto bonusDraws = bank.random.draws();
        bank.beginBonus();
        check(bank.bonusRemaining == 5 && bank.bonusFrames.size() == 5 &&
                  bank.ordinaryFruitTotal == 5,
              "Tally inventories carried fruit before ship trail retires");
        check(bank.random.draws() == bonusDraws + 1,
              "Bonus phrase consumes its unconditional original random draw");
        for (int n = 0; n < 29; ++n)
            bank.sequence();
        check(bank.bonusCursor == 0, "First bonus fruit waits thirty sequence calls");
        bank.sequence();
        check(bank.bonusCursor == 1 && bank.bonusInterval == 29 && bank.bonusTicks == 30,
              "Original tally acceleration and reload order");
        for (int n = 0; n < 1000; ++n) {
            bank.tick({});
            bank.prepareForeground(frame);
        }
        check(bank.score == 550, "Five opening fruits award 110 points each");
        check(bank.fruitCount == 1, "One stage fruit is banked after its fall");
        bank.fruitCount = 5;
        bank.ship.state = Ship::State::Entering;
        bank.ship.position.y = .49;
        bank.ship.run();
        check(bank.lives == 4 && bank.fruitCount == 0 && bank.fruitSets == 1 &&
                  (bank.achievements & 2),
              "Entering converts five-fruit bank into an extra life and source award");
        bank.fruitCount = 2;
        bank.fruitSets = 3;
        bank.ship.collected = 1;
        bank.beginBonus();
        check(bank.fruitCount == 0 && bank.fruitSets == 0,
              "Fruitless finish loses the partial bank and consecutive set count");
        World campaign;
        campaign.start();
        int stages = 0;
        bool bomberSeen = false, minotaurSeen = false, bombSeen = false, burstSeen = false;
        // Controlled campaign integration: preserve normal scripts and timing but
        // refill health, then deliver a real projectile contact to each boss.
        for (int n = 0; n < 250000 && stages < 32 && campaign.checkpoint.empty(); ++n) {
            campaign.ship.integrity = 1;
            for (auto *e = campaign.enemies; e; e = e->next) {
                bomberSeen |= dynamic_cast<Bomber *>(e) != nullptr;
                minotaurSeen |= dynamic_cast<MinotaurKiller *>(e) != nullptr;
                bombSeen |= dynamic_cast<Bomb *>(e) != nullptr;
                if (n % 240 == 0 && !dynamic_cast<Boss *>(e))
                    if (auto *a = dynamic_cast<Attacker *>(e); a && a->deathTicks == 0) {
                        a->position = a->retained = {.1, -.25};
                        Bullet collisionResult(campaign);
                        collisionResult.type = 600;
                        collisionResult.position = a->position;
                        a->hitBy(collisionResult);
                    }
                if (auto *b = dynamic_cast<Boss *>(e);
                    b && b->deathTicks == 0 && campaign.ship.state == Ship::State::Flying) {
                    b->health = 1;
                    b->position = b->retained = {0, -.25};
                    Bullet collisionResult(campaign);
                    collisionResult.type = 600;
                    collisionResult.position = b->position;
                    b->hitBy(collisionResult);
                    ++stages;
                }
            }
            const double a = double(n) * .01;
            campaign.tick({{std::sin(a), -std::cos(a)}, true});
            campaign.prepareForeground(frame);
            for (auto *e = campaign.bullets; e; e = e->next)
                if (auto *b = dynamic_cast<Bullet *>(e))
                    burstSeen |= b->burst;
            check(!campaign.gameOver, "Controlled five-stage route keeps its player");
        }
        std::cout << "Campaign trace: bosses=" << stages << " level=" << campaign.level
                  << " ticks=" << campaign.ticks << " state=" << int(campaign.ship.state)
                  << " population=" << campaign.population << " cursor=" << campaign.scriptCursor
                  << " checkpoint=" << campaign.checkpoint << "\n";
        check(stages == 32 && campaign.level >= 29 && campaign.checkpoint.empty(),
              "Thirty scripts, six eras, year wrap and continuation");
        check(bomberSeen && minotaurSeen && bombSeen,
              "Next-script bomber, bomb and Minotaur births run in the composed campaign");
        World pickup;
        pickup.start();
        pickup.ship.state = Ship::State::Flying;
        const auto pickupLaunch = pickup.launch();
        MinotaurKiller minotaur(pickup, pickupLaunch);
        Fruit power(pickup, minotaur);
        power.frame = 32;
        power.state = Fruit::State::Loose;
        power.hitBy(pickup.ship);
        check(pickup.ship.burstCount == 13 && pickup.ship.collected == 2,
              "Power pickup uses the pre-pickup collected count");
        pickup.ship.fireTicks = 1;
        const auto burstDraws = pickup.random.draws();
        pickup.ship.fire();
        int bursts = 0;
        for (auto *e = pickup.bullets; e; e = e->next)
            if (static_cast<Bullet *>(e)->burst)
                ++bursts;
        check(bursts == 13 && pickup.ship.burstCount == 1 &&
                  pickup.random.draws() == burstDraws + 13,
              "Power burst is one-shot and consumes one speed draw per radial projectile");
        std::cout << "Full campaign: bosses=" << stages << " ticks=" << campaign.ticks
                  << " bomber=" << bomberSeen << " bomb=" << bombSeen
                  << " minotaur=" << minotaurSeen << " natural burst=" << burstSeen << "\n";
        std::cout << "Five a Day opening integration: ticks=" << w.ticks << " score=" << kills
                  << " population=" << w.population << " state=" << int(w.ship.state)
                  << " checkpoint=" << w.checkpoint << "\n";
        World messages;
        messages.message(9, "LEVEL OPEN", 2, 0xffffffffu);
        messages.advanceMessages();
        check(messages.messages[0].colour.a == 1, "Opening text holds before fade");
        messages.advanceMessages();
        const double fadeStart = messages.messages[0].colour.a;
        check(std::abs(fadeStart - .99) < 1e-12,
              "Opening text uses source .99 decay when hold expires");
        World tutorial;
        tutorial.start();
        tutorial.ship.state = Ship::State::Flying;
        Bullet damage(tutorial);
        damage.type = 2;
        for (int n = 0; n < 3; ++n)
            tutorial.ship.didHit(damage);
        bool repairHint = false;
        for (const auto &m : tutorial.messages)
            repairHint |= m.row == 12 && m.text == "COLLECT FRUIT";
        check(repairHint && !tutorial.repairHintPending,
              "Opening damage restores the source one-shot repair prompt");
        tutorial.obey(':');
        bool bossHint = false;
        for (const auto &m : tutorial.messages)
            bossHint |= m.row == 26 && m.text == "COMPLETE LEVEL";
        check(bossHint && !tutorial.bossHintPending,
              "Opening boss retains its three-line source prompt");
        tutorial.paused = true;
        tutorial.message(1, "PAUSED MESSAGE", 2);
        const auto pausedTicks = tutorial.ticks;
        tutorial.tick({});
        bool messageAdvanced = false;
        for (const auto &m : tutorial.messages)
            messageAdvanced |= m.row == 1 && m.ticks == 1;
        check(tutorial.ticks == pausedTicks && messageAdvanced,
              "Pause holds gameplay clock while source row-message owner continues");
        tutorial.paused = false;
        tutorial.tick({});
        const auto phaseClock = tutorial.ticks;
        tutorial.letterFeedbackPending = true;
        tutorial.start();
        check(tutorial.ticks == phaseClock && !tutorial.letterFeedbackPending,
              "Real newGame preserves the phase clock and clears pending old-letter feedback");
        World plop;
        plop.demo = true;
        plop.ship.state = Ship::State::GameOver;
        plop.letterFeedbackPending = true;
        int plopSample = -1;
        plop.sound = [&](int sample, int) { plopSample = sample; };
        plop.tick({});
        check(plopSample == 59 && plop.random.draws() == 2,
              "Letter feedback keeps its original unconditional title sound and two draws");
        for (int n = 0; n < 100; ++n)
            messages.advanceMessages();
        check(!messages.messages[0].text.empty() && messages.messages[0].colour.a < fadeStart,
              "Opening text fades gradually rather than disappearing at timeout");
        World demo;
        demo.start(0, 3, 0, true);
        demo.achieveGoal(0);
        check(demo.achievements == 0, "Attract mode cannot award achievements");
        // Original goFruit::run can enter its departing state only in demo;
        // achieveGoal rejects that mode. High totals must not bypass either gate.
        World letterWorld;
        letterWorld.start();
        letterWorld.lifetimeFruit = 10001;
        letterWorld.lifetimeMinotaurs = 1001;
        GameOverFruit letter(letterWorld, {0, -.2}, int(original::thefruit[0]), 0);
        for (int n = 0; n < 1000; ++n)
            letter.run();
        check(letter.state == 999 && !letter.dead && letterWorld.achievements == 0,
              "Game-over letter settles without awarding lifetime goals");
        letterWorld.demo = true;
        for (int n = 0; n < 1000 && !letter.dead; ++n)
            letter.run();
        check(letter.dead && letter.state == 1 && letterWorld.achievements == 0,
              "Title departure retires letter with demo award gate intact");
        letterWorld.start();
        check(!letterWorld.hud, "Fresh play starts without retained game-over letters");
        World lifetime;
        lifetime.start();
        lifetime.lifetimeFruit = 10000;
        lifetime.lifetimeMinotaurs = 1000;
        lifetime.checkLifetimeGoals();
        check(lifetime.achievements == 0 && lifetime.awardMessages.empty(),
              "Lifetime thresholds remain strictly greater than");
        lifetime.lifetimeFruit = 10001;
        lifetime.checkLifetimeGoals();
        check(lifetime.achievements == (1u << 7) && lifetime.awardMessages.size() == 1 &&
                  lifetime.awardMessages[0] == 7,
              "Fruit lifetime award preserves original identity");
        lifetime.lifetimeMinotaurs = 1001;
        lifetime.checkLifetimeGoals();
        check(lifetime.achievements == ((1u << 7) | (1u << 8)) &&
                  lifetime.awardMessages.size() == 2 && lifetime.awardMessages[1] == 8,
              "Minotaur award independent and repeated check deduplicated");
        World simultaneous;
        simultaneous.start();
        simultaneous.lifetimeFruit = 10001;
        simultaneous.lifetimeMinotaurs = 1001;
        simultaneous.checkLifetimeGoals();
        check(simultaneous.awardMessages == std::vector<int>({7, 8}),
              "One completed run retains both award notifications");
        simultaneous.awardMessages.clear();
        simultaneous.start();
        simultaneous.checkLifetimeGoals();
        check(simultaneous.awardMessages.empty(), "Retry preserves session award gate");
        demo.lifetimeFruit = 10001;
        demo.lifetimeMinotaurs = 1001;
        demo.checkLifetimeGoals();
        check(demo.achievements == 0 && demo.awardMessages.empty(),
              "Lifetime fix does not relax demo eligibility");
        demo.ship.state = Ship::State::Flying;
        demo.ship.integrity = 1;
        Bullet hazard(demo);
        hazard.type = 2;
        demo.ship.didHit(hazard);
        check(demo.ship.integrity == 1, "Demo damage gate");
        World lastLife;
        lastLife.start();
        lastLife.lives = 1;
        lastLife.ship.state = Ship::State::Waiting;
        lastLife.ship.waitTicks = 1;
        lastLife.ship.run();
        check(lastLife.gameOver && lastLife.ship.state == Ship::State::GameOver && lastLife.hud,
              "Final life creates fruit-letter game over");
        for (int n = 0; n < 110; ++n) {
            lastLife.tick({});
            lastLife.prepareForeground(frame);
        }
        check(lastLife.gameOverFade == 1 && lastLife.gameOverTicks == 110,
              "Game-over colour transition continues updating");
        for (int era = 0; era < 6; ++era) {
            World family;
            family.start(era);
            family.scriptCursor = family.script.size();
            family.ship.state = Ship::State::Flying;
            family.month = float(era);
            family.week = .6f;
            auto samplePoint = family.launch();
            samplePoint.position = {.2, -.2};
            family.add(family.enemies, new Miner(family, samplePoint));
            family.add(family.enemies, new Jellyfish(family, samplePoint));
            family.add(family.enemies, new PinkPlane(family, samplePoint));
            family.add(family.enemies, new DecoyCloud(family, samplePoint));
            auto *b = new Boss(family, samplePoint, {false});
            b->position = b->retained = {-.2, -.2};
            family.add(family.enemies, b);
            family.bossLaunched = true;
            for (int n = 0; n < 600; ++n) {
                family.ship.integrity = 1;
                family.tick({});
                family.prepareForeground(frame);
                for (const auto &a : frame)
                    check(std::isfinite(a.position.x) && std::isfinite(a.position.y),
                          "Finite all-era family presentation");
            }
        }
        Records records;
        check(records.unlocked() == 1, "Initial day record");
        check(records.checkpoint(1, 3, 1200), "New day unlock");
        check(!records.checkpoint(1, 2, 99999), "Day records prefer lives before points");
        check(records.checkpoint(1, 3, 1300), "Day records compare equal lives by score");
        const auto file =
            std::filesystem::temp_directory_path() / "fiveaday-native-records-check.txt";
        check(records.save(file.u8string()), "Records save");
        Records loaded;
        check(loaded.load(file.u8string()) && loaded.unlocked() == 2 &&
                  loaded.days[1].score == 1300,
              "Records round trip");
        records.checkpoint(1, 4, 100);
        check(records.save(file.u8string()), "Existing record replacement");
        {
            std::ofstream broken(file);
            broken << "FIVEADAY 99\n";
        }
        Records malformed;
        check(!malformed.load(file.u8string()) && !malformed.writable, "Unknown records preserved");
        malformed.dirty = true;
        check(!malformed.save(file.u8string()), "No overwrite after failed record load");
        std::filesystem::remove(file);
        std::cout << "All-era family, demo, game-over and records checks passed.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
