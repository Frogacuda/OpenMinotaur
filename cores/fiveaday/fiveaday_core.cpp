#include "startup_screen.h"
#include "backend.h"
#include "records.h"
#include "original_tables.h"
#include "highscores.h"
#include <filesystem>
#include "neon/controller_session.h"
#include "neon/sample_audio.h"
#include <algorithm>
#include <atomic>
#include <cstdio>

namespace mino {
namespace {
const CoreInfo Info = {"fiveaday", "Five a Day", 320, 480, .75f, .75f, "tp.png"};
class FiveADayCore final : public Core {
    mino::StartupScreen startup_;
    Host *host_ = nullptr;
    SurfaceInfo surface_;
    fiveaday::Backend backend_;
    std::unique_ptr<fiveaday::World> world_;
    neon::input::ControllerFrame controllers_;
    neon::audio::SampleMixer audio_{{32, neon::audio::MixArithmetic::ReverseQ16Saturating,
                                     neon::audio::RetriggerRule::RewindFirst}};
    std::atomic<bool> suspended_{false};
    struct Finger {
        uint32_t id = 0;
        bool active = false;
        fiveaday::Vec2 origin, position;
    };
    std::array<Finger, 4> fingers_{};
    // Menu contacts outlive screen changes; clearing gameplay fingers must not
    // turn an already-held touch into another menu activation.
    std::vector<uint32_t> contacts_;
    bool touchAwaitRelease_ = false;
    std::array<uint32_t, neon::input::MaxControllers> menuButtons_{};
    std::vector<fiveaday::ActorDraw> cache_;
    enum class Screen { Title, Days, Starting, Playing, Scores, Name };
    Screen screen_ = Screen::Title, returnScreen_ = Screen::Title;
    fiveaday::Records records_;
    HighScores scores_;
    std::string recordsPath_, scoresPath_, name_;
    std::array<std::string, 2> awardText_;
    bool clearLines_ = true;
    int clock_ = 0, day_ = 0, key_ = 0;
    bool recorded_ = false, motionAvailable_ = false;
    float previousX_ = 0, previousY_ = 0;
    std::array<float, 3> buttonPulse_{{1, 1, 1}};
    struct PauseGeometry {
        double y, rx, ry;
    };
    inline static constexpr std::array<PauseGeometry, 5> PauseLayout{{{-.35, .08, .08},
                                                                      {-.13, .22, .055},
                                                                      {.035, .22, .055},
                                                                      {.20, .132, .055},
                                                                      {.365, .22, .055}}};
    PauseGeometry pauseGeometry(int action) const {
        auto g = PauseLayout[size_t(action)];
        if (motionAvailable_ && action == 3)
            g.y = PauseLayout[4].y;
        if (motionAvailable_ && action == 4)
            g.y = PauseLayout[3].y;
        return g;
    }
    static constexpr const char *Keys = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 <#";
    void persist() {
        if (world_ && !world_->demo && records_.planes != world_->lifetimePlanes) {
            records_.planes = world_->lifetimePlanes;
            records_.dirty = true;
        }
        if (!recordsPath_.empty() && !records_.save(recordsPath_))
            host_->log("Five a Day records could not be saved");
        if (!scoresPath_.empty() && scores_.dirty && !scores_.save(scoresPath_))
            host_->log("Five a Day scores could not be saved");
    }
    void click() {
        audio_.play(67, 8 << 12, 0);
    }
    void title() {
        persist();
        screen_ = Screen::Title;
        clock_ = 0;
        restart();
    }
    void chooseDay(int step) {
        day_ = (day_ + records_.unlocked() + step) % records_.unlocked();
        clock_ = -1500;
        buttonPulse_[step > 0 ? 1 : 2] = 2;
        click();
    }
    void begin() {
        screen_ = Screen::Starting;
        clock_ = 10;
        buttonPulse_[0] = 2;
        fingers_ = {};
        click();
    }
    void showScores() {
        returnScreen_ = screen_;
        screen_ = Screen::Scores;
        fingers_ = {};
        click();
    }
    void submitName() {
        scores_.insert(0, name_.empty() ? "PLAYER" : name_, world_->score, world_->level + 1);
        scores_.lastName = name_;
        persist();
        screen_ = Screen::Scores;
        returnScreen_ = Screen::Title;
    }
    void enterKey() {
        char c = Keys[key_];
        click();
        if (c == '#')
            submitName();
        else if (c == '<') {
            if (!name_.empty())
                name_.pop_back();
        } else if (name_.size() < 8) {
            name_ += c;
            if (name_.size() == 8)
                key_ = 38;
        }
    }
    void finishRun() {
        if (world_->gameOverFade < 1)
            return;
        if (scores_.qualifies(0, world_->score)) {
            screen_ = Screen::Name;
            name_.clear();
            key_ = 0;
            fingers_ = {};
        } else
            title();
    }
    void titleClock() {
        auto msg = [&](int row, const char *text, int time, uint32_t colour = 0xffffffffu) {
            world_->message(row, text, time, colour);
        };
        switch (clock_) {
        case 0: {
            world_->messages = {};
            msg(7, "{FIVE A DAY}", 1000, 0xffffff00u);
            msg(26, "TOUCH TO PLAY", 1000, 0xffffff00u);
            char text[64];
            std::snprintf(text, sizeof text, "LOCAL HISCORE: %d", records_.highScore);
            msg(23, text, 1000, 0xff20ff00u);
            break;
        }
        case 300:
            msg(28, "(C) 2012 LAMBCO", 500, 0xffffff00u);
            break;
        case 500:
            msg(10, "SHOOT BADDIES", 500);
            break;
        case 560:
            msg(12, "COLLECT FRUIT", 500);
            break;
        case 620:
            msg(14, "FRUIT REPAIRS SHIP", 500);
            break;
        case 800:
            msg(18, "SHOOT/FRUIT MOTHERSHIP", 400);
            break;
        case 860:
            msg(20, "TO FINISH LEVEL", 400);
            break;
        case 1400:
            world_->messages = {};
            break;
        case 2000:
            world_->messages = {};
            msg(7, "{FIVE A DAY}", 1000, 0xffffff00u);
            msg(10, "CREATED BY LLAMASOFT", 300);
            msg(12, "JEFF MINOTAUR", 300, 0xff00ff00u);
            msg(14, "AND", 300);
            msg(16, "GILESGOAT", 300, 0xff00ff00u);
            break;
        case 2400:
            msg(20, "AUDIO SOURCED FROM", 300);
            msg(22, "FREESOUND.ORG", 300, 0x00ffff00u);
            msg(24, "AND ARRANGED BY", 300);
            msg(26, "THE OX", 300, 0x00ffff00u);
            break;
        }
        if (++clock_ > 2900)
            clock_ = 0;
    }
    void scoresOverlay(fiveaday::Neon_Internals::cellGrid &grid) {
        grid.cPrintAt(7, "HEALTHY SCORES", 0xffffffffu);
        for (int n = 0; n < 10; ++n) {
            const auto &e = scores_.entry(0, n);
            char text[64];
            std::snprintf(text, sizeof text, "%9d %-8.8s", e.score, e.name.c_str());
            grid.cPrintAt(10 + n * 2, text);
        }
    }
    uint32_t highlight() const {
        return menuFocus_ && !(uiTicks_ & 16) ? 0xffff00ffu : 0xffffffffu;
    }
    void advance() {
        ++uiTicks_;
        if (screen_ == Screen::Title)
            titleClock();
        if (screen_ == Screen::Days && ++clock_ >= -10)
            begin();
        if (screen_ == Screen::Starting && --clock_ < 1) {
            screen_ = Screen::Playing;
            restart();
        }
        if (screen_ != Screen::Scores && screen_ != Screen::Name)
            world_->tick(input());
        if (!world_->demo) {
            if (records_.planes != world_->lifetimePlanes) {
                records_.planes = world_->lifetimePlanes;
                records_.dirty = true;
            }
            if (world_->gameOver && !recorded_) {
                recorded_ = true;
                records_.fruit += world_->ordinaryFruitTotal;
                records_.minotaurs += world_->powerFruitTotal;
                records_.highScore = std::max(records_.highScore, world_->score);
                records_.dirty = true;
                world_->lifetimeFruit = records_.fruit;
                world_->lifetimeMinotaurs = records_.minotaurs;
                // Evaluate after banking totals so the completed run can earn lifetime goals,
                // while this completed run is still eligible (not title/demo).
                world_->checkLifetimeGoals();
                char text[64];
                std::snprintf(text, sizeof text, "ALL FRUIT: %d", records_.fruit);
                world_->message(22, text, 1900);
                std::snprintf(text, sizeof text, "ALL MINOTAURS: %d", records_.minotaurs);
                world_->message(24, text, 1900);
                persist();
            }
        }
        if (screen_ == Screen::Playing && world_->gameOver && world_->gameOverTicks > 2000)
            finishRun();
        for (auto &pulse : buttonPulse_)
            if (pulse > 1)
                pulse = std::max(1.f, pulse * .9f);
        advancePauseButton();
        host_->observeOriginalAchievements(world_->achievements);
    }
    float pauseOpacity_ = 1;
    double quitOpacity_ = 0, scoresOpacity_ = 0, controlOpacity_ = 0;
    uint32_t uiTicks_ = 0;
    double accumulator_ = 0;
    bool rendered_ = false, menuFocus_ = false;
    int selection_ = 0;
    uint32_t menuDirections_ = 0;
    void restart() {
        audio_.stopAll();
        fingers_ = {};
        clearLines_ = true;
        if (!world_)
            world_ = std::make_unique<fiveaday::World>(1);
        world_->sound = [this](int id, int gain) { audio_.play(id, gain, 0); };
        world_->spriteSize = [this](int frame) {
            const auto *s = backend_.resources().sprite(frame + 2);
            if (!s)
                throw std::runtime_error("Missing Five a Day sprite");
            return fiveaday::Vec2{double(s->w), double(s->h)};
        };
        world_->dayCheckpoint = [this](int day, int lives, int score) {
            if (records_.checkpoint(day, lives, score)) {
                world_->noise(38);
                persist();
            }
        };
        // Preserve falling game-over letters on returning to attract mode.
        fiveaday::Entity *letters = nullptr;
        if (screen_ == Screen::Title)
            for (auto *e = world_->hud; e;) {
                auto *next = e->next;
                if (dynamic_cast<fiveaday::GameOverFruit *>(e)) {
                    if (e->prev)
                        e->prev->next = e->next;
                    else
                        world_->hud = e->next;
                    if (e->next)
                        e->next->prev = e->prev;
                    e->next = e->prev = nullptr;
                    world_->add(letters, e);
                }
                e = next;
            }
        const auto &d = records_.days[size_t(day_)];
        world_->start(day_, d.lives, d.score, screen_ != Screen::Playing);
        while (letters) {
            auto *e = letters;
            letters = e->next;
            if (letters)
                letters->prev = nullptr;
            e->next = e->prev = nullptr;
            world_->add(world_->hud, e);
        }
        world_->lifetimePlanes = records_.planes;
        world_->tilt = records_.tilt && motionAvailable_;
        world_->lifetimeFruit = records_.fruit;
        world_->lifetimeMinotaurs = records_.minotaurs;
        if (screen_ == Screen::Playing)
            world_->message(14, "PLAY PLAYER ONE", 200);
        host_->enableMotion(world_->tilt);
        previousX_ = previousY_ = 0;
        recorded_ = false;
        pauseOpacity_ = world_->demo ? 0.f : 1.f;
        quitOpacity_ = scoresOpacity_ = controlOpacity_ = 0;
        rendered_ = false;
        selection_ = 0;
    }
    bool menu() const {
        return screen_ == Screen::Playing && world_ &&
               (world_->paused || !world_->checkpoint.empty());
    }
    void activate() {
        click();
        fingers_ = {};
        if (selection_ == 0)
            world_->paused = false;
        else if (selection_ == 1)
            showScores();
        else if (selection_ == 2)
            title();
        else if (selection_ == 3) {
            persist();
            host_->requestQuit();
        } else if (motionAvailable_) {
            records_.tilt = !records_.tilt;
            records_.dirty = true;
            world_->tilt = records_.tilt;
            host_->enableMotion(world_->tilt);
            persist();
        }
    }
    void commands(uint32_t edges) {
        if (!world_ || suspended_)
            return;
        const bool confirm = bool(edges & (PAD_A | PAD_START)),
                   back = bool(edges & (PAD_B | PAD_BACK));
        if (screen_ == Screen::Title) {
            if (back) {
                persist();
                host_->requestQuit();
            } else if (confirm) {
                day_ = 0;
                if (records_.unlocked() > 1) {
                    screen_ = Screen::Days;
                    clock_ = -1500;
                    world_->messages = {};
                    click();
                } else
                    begin();
            }
            return;
        }
        if (screen_ == Screen::Days) {
            if (edges & (PAD_UP | PAD_RIGHT))
                chooseDay(1);
            if (edges & (PAD_DOWN | PAD_LEFT))
                chooseDay(-1);
            if (confirm)
                begin();
            else if (back)
                title();
            return;
        }
        if (screen_ == Screen::Starting)
            return;
        if (screen_ == Screen::Scores) {
            if (confirm || back) {
                if (returnScreen_ == Screen::Title)
                    title();
                else
                    screen_ = returnScreen_;
            }
            return;
        }
        if (screen_ == Screen::Name) {
            if (edges & PAD_LEFT)
                key_ = (key_ / 8) * 8 + (key_ % 8 + 7) % 8;
            if (edges & PAD_RIGHT)
                key_ = (key_ / 8) * 8 + (key_ % 8 + 1) % 8;
            if (edges & PAD_UP)
                key_ = (key_ + 32) % 40;
            if (edges & PAD_DOWN)
                key_ = (key_ + 8) % 40;
            key_ = std::min(key_, 38);
            if (confirm)
                enterKey();
            else if (back) {
                if (!name_.empty())
                    name_.pop_back();
                else
                    key_ = 38;
            }
            return;
        }
        if (world_->gameOver) {
            if (confirm || back)
                finishRun();
            return;
        }
        if (menu()) {
            const int count = motionAvailable_ ? 5 : 4;
            const std::array<int, 5> order = motionAvailable_ ? std::array<int, 5>{0, 1, 2, 4, 3}
                                                              : std::array<int, 5>{0, 1, 2, 3, 4};
            auto move = [&](int step) {
                const auto at =
                    std::find(order.begin(), order.begin() + count, selection_) - order.begin();
                selection_ = order[size_t((at + count + step) % count)];
            };
            if (edges & (PAD_UP | PAD_LEFT))
                move(-1);
            if (edges & (PAD_DOWN | PAD_RIGHT))
                move(1);
            if (confirm)
                activate();
            else if (back || edges & PAD_START) {
                world_->paused = false;
                fingers_ = {};
            }
        } else if (edges & (PAD_START | PAD_B | PAD_BACK)) {
            world_->paused = true;
            selection_ = 0;
            fingers_ = {};
        }
    }
    fiveaday::Input input() {
        fiveaday::Input result;
        if (world_->tilt) {
            const auto m = host_->motion();
            if (m.available) {
                result.direction = fiveaday::Input::directionFromGravity(
                    {double(float((m.x + previousX_) * .5f)),
                     double(float((m.y + previousY_) * .5f))});
                previousX_ = float(m.x);
                previousY_ = float(m.y);
            }
        }
        for (const auto &p : controllers_.slots())
            if (p.connected) {
                const auto a = neon::input::movement(p);
                if (a.x || a.y)
                    result.direction = {double(a.x), double(a.y)};
                result.held |= bool(a.x || a.y || p.buttons & (PAD_A | PAD_X | PAD_Y));
            }
        for (const auto &f : fingers_)
            if (f.active) {
                if (!world_->tilt)
                    result.direction = f.position - f.origin;
                result.held = true;
                break;
            }
        return result;
    }
    void advancePauseButton() {
        // Source fade rates; matching opacity keeps the three
        // lower pause actions visually consistent (the original caps were .6/1/.5).
        auto fade = [&](double &alpha, double cap) {
            alpha = world_->paused ? std::min(cap, alpha + .05) : alpha * .95;
        };
        fade(quitOpacity_, .6);
        fade(scoresOpacity_, .6);
        fade(controlOpacity_, .6);
        // pauseButton::run 0x57f68; gameButton::draw 0x54ef8 hides below .1.
        bool nearby = false;
        for (const auto &finger : fingers_)
            if (finger.active) {
                nearby = std::abs(finger.position.x) < double(.08f) * 1.5 &&
                         std::abs(finger.position.y + .35) < double(.08f) * 1.5;
                break;
            }
        if (world_->gameOver || screen_ != Screen::Playing)
            pauseOpacity_ = 0;
        else if (world_->paused)
            pauseOpacity_ = 1;
        else if (nearby)
            pauseOpacity_ = std::min(1.f, float(pauseOpacity_ + .1f));
        else if (pauseOpacity_ > 0)
            pauseOpacity_ *= .98f;
    }
    void overlay(fiveaday::Neon_Internals::cellGrid &grid) {
        if (grid.cols != 29 || grid.rowsN != 32)
            grid.setSize(29, 32);
        grid.cls();
        grid.clattr();
        if (!world_)
            return;
        if (clearLines_) {
            grid.clm();
            clearLines_ = false;
        }
        static constexpr const char *goals[] = {
            "FRESH!",       "5 A DAY!",         "BOANTHROPE BONANZA!", "YOU SO FRUITY!",
            "PERFECT DAY",  "TILT-TASTIC!",     "AMEN BREAKER",        "GALACTIC GREENGROCER!",
            "TAURY PARTY!", "ULTRA FRUITERER!", "WELL DONE!"};
        for (int goal : world_->awardMessages) {
            const int slot = grid.lines[11]->text ? 1 : 0;
            const int row = 11 + slot;
            awardText_[size_t(slot)] = goals[goal];
            grid.QuickMsg(awardText_[size_t(slot)].c_str(), {0, slot ? .1 : 0}, {1.2, 2.4}, 4, row,
                          3, 1.f, .5f);
            auto *line = grid.lines[row];
            line->inScale = {4, 4};
            line->away.y = slot ? double(.35f) : double(.4f);
            line->from.y = slot ? double(-.55f) : -.5;
        }
        world_->awardMessages.clear();
        for (const auto &m : world_->messages)
            if (!m.text.empty()) {
                const auto byte = [](double v) { return uint32_t(std::clamp(v, 0., 1.) * 255.); };
                const uint32_t c = (byte(m.colour.r) << 24) | (byte(m.colour.g) << 16) |
                                   (byte(m.colour.b) << 8) | byte(m.colour.a);
                grid.cPrintAt(m.row, m.text.c_str(), c);
            }
        if (screen_ == Screen::Scores) {
            grid.clm();
            grid.cls();
            scoresOverlay(grid);
            grid.cPrintAt(31, "BACK", highlight());
            return;
        }
        if (screen_ == Screen::Name) {
            grid.clm();
            grid.cls();
            grid.cPrintAt(5, "A HEALTHY SCORE!", 0xffffffffu);
            const auto field = name_ + std::string(8 - name_.size(), '_');
            grid.cPrintAt(9, field.c_str());
            for (int k = 0; k < 39; ++k)
                grid.poke(3 + k % 8 * 3, 14 + k / 8 * 3, Keys[k],
                          k == key_ ? highlight() : 0xffffffffu);
            grid.cPrintAt(30,
                          menuFocus_ && key_ == 37   ? "DELETE"
                          : menuFocus_ && key_ == 38 ? "DONE"
                                                     : "ENTER YOUR NAME",
                          highlight());
            return;
        }
        if (screen_ == Screen::Days || screen_ == Screen::Starting) {
            grid.cls();
            const auto &d = records_.days[size_t(day_)];
            char text[96];
            grid.cPrintAt(10, "START PLAY ON");
            grid.cPrintAt(12, fiveaday::original::weekdays[size_t(day_ % 6)], highlight());
            std::snprintf(text, sizeof text, "%d %s %d", day_ % 30 + 1,
                          fiveaday::original::months[size_t((day_ / 30) % 6)], day_ / 180 + 2012);
            grid.cPrintAt(14, text);
            std::snprintf(text, sizeof text, "START WITH %d LIVES", d.lives);
            grid.cPrintAt(16, text);
            std::snprintf(text, sizeof text, "AND %d POINTS", d.score);
            grid.cPrintAt(18, text);
            return;
        }
        if (screen_ == Screen::Title && clock_ > 1400 && clock_ < 2000)
            scoresOverlay(grid);
        if (!world_->checkpoint.empty())
            grid.cPrintAt(10, "CAMPAIGN ERROR");
    }

  public:
    const CoreInfo &info() const override {
        return Info;
    }
    bool init(Host &host, const SurfaceInfo &surface) override {
        if (!startup_.begin(host, surface))
            return false;
        host_ = &host;
        surface_ = surface;
        controllers_.clear();
        menuDirections_ = 0;
        menuButtons_ = {};
        contacts_.clear();
        touchAwaitRelease_ = false;
        uiTicks_ = 0;
        records_ = fiveaday::Records{};
        recordsPath_.clear();
        scoresPath_.clear();
        if (!backend_.init(host, surface))
            return false;
        std::vector<neon::audio::Sample> samples;
        std::vector<uint8_t> bytes;
        for (const auto &entry : backend_.resources().samples()) {
            neon::audio::Sample sample;
            sample.maxInstances = entry.maxInstances;
            sample.priority = entry.priority;
            if (!host.readAsset(entry.file, bytes) || !neon::audio::parseWav(bytes, sample))
                return false;
            samples.push_back(std::move(sample));
        }
        audio_.setSamples(std::move(samples));
        scores_ = HighScores{};
        scores_.init(1, {"STINKYOX"}, 1000, 100);
        if (!host.saveDir().empty()) {
            recordsPath_ =
                (std::filesystem::u8path(host.saveDir()) / "fiveaday-records.txt").u8string();
            scoresPath_ =
                (std::filesystem::u8path(host.saveDir()) / "fiveaday-scores.txt").u8string();
            if (!records_.load(recordsPath_))
                host.log("Five a Day records unreadable; preserving existing file");
            scores_.load(scoresPath_);
        }
        motionAvailable_ = host.enableMotion(true);
        host.enableMotion(false);
        backend_.background = [](auto &, double, double) {};
        backend_.foreground = [this](auto &target, double width, double height) {
            cache_.clear();
            if (!world_)
                return;
            // theGame::_draw: sky; player shots; hostile shots; attackers;
            // fruit; effects; ship; clouds. Keep the native overlay order.
            cache_.push_back({{0, 0}, {1, 1}, .5f, 0, 0, fiveaday::unpack(world_->sky)});
            std::vector<fiveaday::ActorDraw> actors;
            world_->prepareForeground(actors, screen_ != Screen::Scores && screen_ != Screen::Name);
            cache_.insert(cache_.end(), actors.begin(), actors.end());
            if (double(pauseOpacity_) > .1) {
                fiveaday::ActorDraw pause;
                pause.position = {0, -.35};
                pause.frame = 1;
                pause.radius = .08f;
                pause.colour.a = double(pauseOpacity_);
                if (menuFocus_ && menu() && selection_ == 0)
                    pause.colour = fiveaday::unpack(highlight());
                cache_.push_back(pause);
            }
            auto button = [&](int frame, fiveaday::Vec2 pos, float radius, double scale,
                              float alpha) {
                fiveaday::ActorDraw b;
                b.frame = frame;
                b.position = pos;
                b.radius = radius;
                b.scale.x = scale;
                b.colour.a = alpha;
                cache_.push_back(b);
            };
            if (screen_ == Screen::Days || screen_ == Screen::Starting) {
                constexpr int frames[] = {85, 86, 87};
                constexpr double x[] = {0, .42, -.42}, y[] = {.22, -.08, -.08};
                for (int n = 0; n < 3; ++n) {
                    auto size = world_->dimensions(frames[n]);
                    button(frames[n], {x[n], y[n]}, .07f * buttonPulse_[n], size.x / size.y,
                           screen_ == Screen::Starting ? float(clock_) * .1f : 1.f);
                }
            }
            // theGame::newGame 0x5ab88: retain original positions, sizes
            // and scales. Controller focus tints artwork instead of overprinting it.
            auto pauseAction = [&](int id, int frame, double alpha) {
                if (alpha <= .1)
                    return;
                const auto g = pauseGeometry(id);
                button(frame, {0, g.y}, float(g.ry), g.rx / g.ry, float(alpha));
                if (menuFocus_ && menu() && selection_ == id) {
                    cache_.back().colour = fiveaday::unpack(highlight());
                    cache_.back().colour.a = alpha;
                }
            };
            pauseAction(1, 109, scoresOpacity_);
            pauseAction(2, 110, quitOpacity_);
            pauseAction(3, 88, quitOpacity_);
            if (motionAvailable_)
                pauseAction(4, world_->tilt ? 90 : 89, controlOpacity_);
            backend_.drawActors(target, width, height, cache_);
        };
        backend_.text = [this](auto &grid) { overlay(grid); };
        for (auto &channel : backend_.feedbackState.colour)
            channel = .2f;
        title();
        host.log("Five a Day: six-era campaign, attract, day selection and local records");
        return true;
    }
    void resize(const SurfaceInfo &s) override {
        startup_.resize(s);
        surface_ = s;
        backend_.resize(s);
        rendered_ = false;
    }
    void pads(const PadState *p, int count) override {
        if (startup_.pads(p, count))
            return;
        controllers_.capture(p, count);
        menuFocus_ = controllers_.connected() || host_->keyboardNavigationAvailable();
        uint32_t edges = 0, directions = 0;
        const auto snapshot = controllers_.consume();
        for (size_t slot = 0; slot < snapshot.size(); ++slot) {
            const auto &pad = snapshot[slot];
            const uint32_t held = pad.connected ? (pad.buttons | pad.pressed) : 0;
            edges |= held & ~menuButtons_[slot];
            menuButtons_[slot] = held;
            const auto a = neon::input::movement(pad);
            if (a.y < -.5f)
                directions |= PAD_UP;
            if (a.y > .5f)
                directions |= PAD_DOWN;
            if (a.x < -.5f)
                directions |= PAD_LEFT;
            if (a.x > .5f)
                directions |= PAD_RIGHT;
        }
        edges |= directions & ~menuDirections_;
        menuDirections_ = directions;
        commands(edges);
    }
    void touch(const TouchEvent &e) override {
        if (startup_.touch(e))
            return;
        if (suspended_ || !world_)
            return;
        const fiveaday::Vec2 p{double(e.x) / surface_.width - .5,
                               double(e.y) / surface_.height - .5};
        const auto contact = std::find(contacts_.begin(), contacts_.end(), e.id);
        if (e.phase == TouchPhase::Up || e.phase == TouchPhase::Cancel) {
            if (contact != contacts_.end())
                contacts_.erase(contact);
            for (auto &f : fingers_)
                if (f.active && f.id == e.id)
                    f.active = false;
            if (contacts_.empty())
                touchAwaitRelease_ = false;
            return;
        }
        if (e.phase == TouchPhase::Down) {
            if (contact != contacts_.end())
                return;
            contacts_.push_back(e.id);
        }
        if (touchAwaitRelease_)
            return;
        if (screen_ != Screen::Playing) {
            if (e.phase != TouchPhase::Down)
                return;
            touchAwaitRelease_ = true;
            if (screen_ == Screen::Days) {
                if (std::abs(p.x - .42) < .1 && std::abs(p.y + .08) < .1)
                    chooseDay(1);
                else if (std::abs(p.x + .42) < .1 && std::abs(p.y + .08) < .1)
                    chooseDay(-1);
                else if (std::abs(p.x) < .1 && std::abs(p.y - .22) < .1)
                    begin();
            } else if (screen_ == Screen::Name) {
                const int row = int((p.y + .5) * 32), col = int((p.x + .5) * 29);
                if (row >= 13 && row <= 27 && col >= 2 && col <= 26) {
                    const int k =
                        std::clamp((row - 13) / 3, 0, 4) * 8 + std::clamp((col - 2) / 3, 0, 7);
                    if (k < 39) {
                        key_ = k;
                        enterKey();
                    }
                } else if (row >= 29)
                    submitName();
            } else
                commands(PAD_A);
            return;
        }
        if (world_->gameOver) {
            if (e.phase == TouchPhase::Down) {
                touchAwaitRelease_ = true;
                finishRun();
            }
            return;
        }
        if (menu()) {
            if (e.phase == TouchPhase::Down) {
                for (int n = 0; n < (motionAvailable_ ? 5 : 4); ++n) {
                    const auto g = pauseGeometry(n);
                    if (std::abs(p.x) < g.rx * 1.2 && std::abs(p.y - g.y) < g.ry * 1.2) {
                        selection_ = n;
                        touchAwaitRelease_ = true;
                        activate();
                        break;
                    }
                }
            }
            return;
        }
        if (e.phase == TouchPhase::Down) {
            if (std::abs(p.x) < .08 && std::abs(p.y + .35) < .08) {
                world_->paused = true;
                selection_ = 0;
                fingers_ = {};
                touchAwaitRelease_ = true;
                return;
            }
            for (auto &f : fingers_)
                if (!f.active) {
                    f = {e.id, true, p, p};
                    break;
                }
        } else
            for (auto &f : fingers_)
                if (f.active && f.id == e.id) {
                    f.position = p;
                    if (e.phase == TouchPhase::Up)
                        f.active = false;
                    break;
                }
    }
    void frame(double dt) override {
        if (startup_.frame(dt))
            return;
        if (suspended_ || !world_)
            return;
        accumulator_ += std::clamp(dt, 0., .1);
        bool stepped = false;
        try {
            while (accumulator_ >= 1. / 60) {
                accumulator_ -= 1. / 60;
                advance();
                backend_.execute(world_->random);
                stepped = true;
            }
            if (!rendered_ && !stepped)
                backend_.execute(world_->random);
            backend_.present();
            rendered_ = true;
        } catch (const std::exception &e) {
            host_->log("Five a Day checkpoint error: %s", e.what());
            world_->checkpoint = e.what();
        }
    }
    void mixAudio(float *out, int frames) override {
        if (suspended_)
            std::fill(out, out + frames * 2, 0.f);
        else
            audio_.mix(out, frames);
    }
    void pause(bool paused) override {
        suspended_ = paused;
        accumulator_ = 0;
        fingers_ = {};
        if (host_)
            host_->enableMotion(!paused && world_ && world_->tilt);
        if (paused)
            save();
    }
    bool back() override {
        if (startup_.blocking())
            return true;
        if (!world_)
            return false;
        commands(PAD_B);
        return true;
    }
    void save() override {
        if (world_ && !world_->demo && records_.planes != world_->lifetimePlanes) {
            records_.planes = world_->lifetimePlanes;
            records_.dirty = true;
        }
        persist();
    }
    void shutdown() override {
        startup_.release();
        if (host_) {
            save();
            host_->enableMotion(false);
        }
        audio_.stopAll();
        backend_.release();
        world_.reset();
        host_ = nullptr;
    }
};
Core *create() {
    return new FiveADayCore;
}
} // namespace
extern const CoreEntry kFiveADayCore = {&Info, create};
} // namespace mino
