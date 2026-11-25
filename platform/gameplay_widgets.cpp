#include "gameplay_widgets.h"
#include "bundled_assets.h"
#include "gameplay_widget_documents.h"
#include "widget_renderer.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace mino {
namespace {
using namespace neon::widgets;
constexpr const char *pickupNames[] = {"Rear Fire", "Spread Fire",     "Four-Way Fire",
                                       "Pod Sweep", "All-Around Fire", "Shield",
                                       "Time Warp", "Disease"};

int64_t integer(const PresentationState &state, const std::string &key) {
    const auto it = state.values.find(key);
    if (it != state.values.end())
        if (const auto *value = std::get_if<int64_t>(&it->second))
            return *value;
    return 0;
}

Value::Object achievementRow(const PresentationState &state, const char *id, const char *label) {
    const auto key = std::string("widgets.") + id;
    const auto count = integer(state, key), target = integer(state, key + ".target");
    const auto completed = state.values.find(key + ".completed");
    return {{"id", id},
            {"label", label},
            {"value", std::to_string(count) + "/" + std::to_string(target)},
            {"completed", completed != state.values.end() && std::get<bool>(completed->second)}};
}

std::string scoreText(int64_t score) {
    std::string text = std::to_string(score);
    for (int index = int(text.size()) - 3; index > int(score < 0); index -= 3)
        text.insert(size_t(index), 1, ',');
    return text;
}

Value colour(const PresentationState &state, int source) {
    auto tint = state.colours.find(source);
    if (tint == state.colours.end())
        return Value::Array{1, 1, 1, 1};
    const auto &rgb = tint->second;
    return Value::Array{rgb[0], rgb[1], rgb[2], 1};
}

Value::Object item(const PresentationState &state, const PresentationState::CollectionItem &data) {
    return {{"id", data.id},
            {"label", pickupNames[data.colourSource]},
            {"tint", colour(state, data.colourSource)}};
}

void arrangePortraitEffects(Value::Array &effects) {
    for (size_t index = 0; index < effects.size(); ++index) {
        auto &effect = effects[index];
        effect["column"] = int(index / 2);
        effect["row"] = int(index % 2);
        const auto label = effect.at("label").string();
        if (label == "All-Around Fire")
            effect["label"] = "All Around";
        else if (label == "Four-Way Fire")
            effect["label"] = "Four-Way";
    }
}

const char *widgetGameName(WidgetGame game) {
    if (game == WidgetGame::GoatUp)
        return "goatup";
    if (game == WidgetGame::SuperOxWars)
        return "superoxwars";
    if (game == WidgetGame::FiveADay)
        return "fiveaday";
    if (game == WidgetGame::Minos)
        return "minos";
    if (game == WidgetGame::GoatUp2)
        return "goatup2";
    if (game == WidgetGame::Minotron)
        return "minotron";
    if (game == WidgetGame::Deflex)
        return "deflex";
    if (game == WidgetGame::MinotaurRescue)
        return "minorescue";
    return "gridrunner";
}

std::string missionClock(int64_t ticks) {
    const auto seconds = ticks / 60;
    const auto remainder = seconds % 60;
    return std::to_string(seconds / 60) + (remainder < 10 ? ":0" : ":") + std::to_string(remainder);
}

Value::Object minosValues(const PresentationState &state, Value::Object values) {
    values["highScore"] = scoreText(integer(state, "highScore"));
    const auto best = integer(state, "cavernBest"), lives = integer(state, "cavernBestLives");
    values["cavernBest"] = best < 0 ? "--" : scoreText(best);
    values["cavernBestLives"] = lives > 0 ? scoreText(lives) : "--";
    const auto minos = integer(state, "rescuedForMultiplier"),
               ship = integer(state, "shipKind") + 1;
    values["multiplier"] = std::to_string(minos + ship) + "x";
    values["multiplierSize"] = minos + ship < 10 ? 30 : 26;
    values["minoBonus"] = minos;
    values["shipBonus"] = ship;
    values["shipName"] = std::get<std::string>(state.values.at("shipName"));
    values["cavernRescues"] = std::to_string(integer(state, "cavernRescued")) + " / " +
                              std::to_string(integer(state, "cavernTotal"));
    values["gameRescues"] = scoreText(integer(state, "gameRescues"));
    constexpr const char *grades[] = {"--", "D", "C", "B", "A", "S"};
    values["landingGrade"] = grades[std::clamp<int64_t>(integer(state, "landingRating"), 0, 5)];
    values["landingStreak"] = integer(state, "landingStreak");
    values["cleanRun"] = std::get<bool>(state.values.at("cleanRun"));
    const auto rescue = integer(state, "rescueBonus"), fuel = integer(state, "fuelBonus"),
               time = integer(state, "timeBonus");
    values["levelBonus"] = scoreText(rescue + fuel + time);
    values["rescueBonus"] = scoreText(rescue);
    values["fuelBonus"] = scoreText(fuel);
    values["timeBonus"] = scoreText(time);
    values["cavernDepth"] = std::get<double>(state.values.at("cavernDepth"));
    values["missionLeg"] = std::get<bool>(state.values.at("returning")) ? "Return" : "Descend";
    const auto elapsed = integer(state, "missionTime"), par = integer(state, "parTime");
    values["missionTime"] = missionClock(elapsed);
    values["parTime"] = missionClock(par);
    values["pastPar"] = elapsed >= par;
    values["parRemaining"] = par > 0 ? std::clamp(double(par - elapsed) / double(par), 0., 1.) : 0.;
    return values;
}

Value::Object fiveADayValues(const PresentationState &state, Value::Object values) {
    values["highScore"] = scoreText(integer(state, "highScore"));
    const auto dayBest = integer(state, "dayBest"), dayLives = integer(state, "dayLives");
    values["dayBest"] = dayBest < 0 ? "--" : scoreText(dayBest);
    values["dayLives"] = dayLives < 0 ? "--" : scoreText(dayLives);
    values["fruitTrail"] = std::to_string(integer(state, "trail")) + " / " +
                           std::to_string(integer(state, "trailMax"));
    constexpr int64_t minotaurGoal = 1001, fruitGoal = 10001;
    const auto minotaurs = integer(state, "lifetimeMinotaurs"),
               fruit = integer(state, "lifetimeFruit");
    values["lifetimeMinotaurs"] = scoreText(std::min(minotaurs, minotaurGoal));
    values["lifetimeFruit"] = scoreText(std::min(fruit, fruitGoal));
    values["minotaurGoalComplete"] = minotaurs >= minotaurGoal;
    values["fruitGoalComplete"] = fruit >= fruitGoal;
    const auto count = integer(state, "fruitSetCount");
    values["fruitSetCount"] = count;
    for (int64_t slot = 0; slot < count; ++slot) {
        const auto key = "fruit" + std::to_string(slot);
        values[key + "Frame"] = Value::Array{integer(state, key + "X"), integer(state, key + "Y"),
                                             integer(state, key + "W"), integer(state, key + "H")};
    }
    values["firepower"] = std::get<double>(state.values.at("firepower"));
    values["formationActive"] = std::get<bool>(state.values.at("formationActive"));
    const auto formationKills = integer(state, "formationKills");
    values["formationKills"] = formationKills;
    values["formationCount"] = std::to_string(formationKills) + " / 5";
    values["nextBonus"] = scoreText(integer(state, "nextBonus"));
    values["doublePoints"] = std::get<bool>(state.values.at("doublePoints"));
    values["scorePenalty"] = std::to_string(integer(state, "scorePenalty")) + "%";
    values["minotaursLost"] = scoreText(integer(state, "minotaursLost"));
    const auto hits = integer(state, "damageTaken");
    values["damageTaken"] = scoreText(hits) + (hits == 1 ? " hit" : " hits");
    return values;
}

Value::Object goatUpValues(const PresentationState &state, Value::Object values) {
    values["recordScore"] = scoreText(integer(state, "recordScore"));
    for (const auto *field : {"recordPlatform", "totalKisses", "totalItems", "kids", "mostKids",
                              "kisses", "uniqueItems"})
        values[field] = integer(state, field);
    for (const auto *field : {"progress", "gestation", "slowDown"})
        values[field] = std::get<double>(state.values.at(field));
    values["gestationActive"] = std::get<bool>(state.values.at("gestationActive"));
    values["returning"] = std::get<bool>(state.values.at("returning"));
    values["gestationText"] =
        std::to_string(int(std::floor(values["gestation"].number() * 100))) + "%";
    values["slowDownText"] =
        std::to_string(int(std::ceil(values["slowDown"].number() * 100))) + "%";
    values["protectionDuration"] = std::max(integer(state, "protectionDuration"), int64_t(1));
    for (const auto *effect : {"flight", "mushroom", "protection"}) {
        const std::string key = effect;
        const int64_t ticks = integer(state, key + "Ticks");
        values[key + "Ticks"] = ticks;
        values[key + "Time"] = std::to_string((ticks + 59) / 60) + "s";
    }
    constexpr std::array<const char *, 5> effects{"flight", "mushroom", "protection", "gestation",
                                                  "slowDown"};
    std::array active{integer(state, "flightTicks") > 0, integer(state, "mushroomTicks") > 0,
                      integer(state, "protectionTicks") > 0, values["gestationActive"].boolean(),
                      values["slowDown"].number() > 0};
    if (std::all_of(active.begin(), active.end(), [](bool visible) { return visible; })) {
        const auto oldest =
            std::min_element(effects.begin(), effects.end(), [&](const char *a, const char *b) {
                return integer(state, std::string(a) + "Order") <
                       integer(state, std::string(b) + "Order");
            });
        active[size_t(oldest - effects.begin())] = false;
    }
    for (size_t index = 0; index < effects.size(); ++index)
        values[std::string(effects[index]) + "Visible"] = active[index];
    return values;
}

Value::Object goatUp2Values(const PresentationState &state, Value::Object values) {
    values["objects"] = std::to_string(integer(state, "objectsFound")) + "/" +
                        std::to_string(integer(state, "objectsTotal"));
    values["minotaurs"] = std::to_string(integer(state, "minotaursFound")) + "/" +
                          std::to_string(integer(state, "minotaursTotal"));
    values["tail"] = std::to_string(integer(state, "tailSize"));
    values["countSize"] = values["objects"].string().size() > 5 ? 18 : 27;
    values["blocked"] = std::get<bool>(state.values.at("fartDisabled"));
    const auto invincible = integer(state, "invincibleTicks");
    const auto unlimited = integer(state, "unlimitedTicks");
    values["invincibleFill"] =
        double(invincible) / std::max<int64_t>(1, integer(state, "invincibleDuration"));
    values["unlimitedFill"] =
        double(unlimited) / std::max<int64_t>(1, integer(state, "unlimitedDuration"));
    const auto tenths = (invincible + 5) / 6;
    values["invincibleTime"] =
        std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + "s";
    values["unlimitedTime"] = std::to_string((unlimited + 59) / 60) + "s";
    return values;
}

constexpr const char *minotronItems[] = {"Beast Love", "Hot Bullets", "Bomb",
                                         "3-Way Shot", "Reflective",  "Floyd Bonus",
                                         "Extra Life", "Explosive",   "Invincible"};
constexpr int minotronFrames[][4] = {{250, 73, 18, 18},  {496, 0, 8, 8},     {271, 73, 32, 32},
                                     {332, 73, 16, 16},  {333, 90, 26, 18},  {401, 72, 32, 13},
                                     {307, 205, 53, 49}, {363, 230, 26, 26}, {389, 203, 48, 39}};
constexpr size_t minotronPrimaryExpiryRows = 8;

Value::Object compactScore(int64_t score, int maxSize = 29, int width = 174) {
    const bool millions = score >= 1000000;
    const auto roundedThousands = (std::max<int64_t>(score, 0) + 500) / 1000;
    auto lead = score < 0  ? "--"
                : millions ? std::to_string(roundedThousands / 1000)
                           : scoreText(score);
    auto fraction = std::to_string(roundedThousands % 1000);
    fraction.insert(0, 3 - fraction.size(), '0');
    const int size = std::min(maxSize, width / int(lead.size() + (millions ? 5 : 0)));
    return {{"lead", lead},
            {"leadLength", int(lead.size())},
            {"fraction", fraction},
            {"size", size},
            {"millions", millions}};
}

Value::Object narrowRailScore(int64_t score, bool portrait) {
    if (!portrait && score >= 1000000) {
        const auto compact = compactScore(score, 23, 118);
        const auto &lead = compact.at("lead").string();
        return {{"text", lead + " " + compact.at("fraction").string() + "m"},
                {"size", compact.at("size")},
                {"decimal", true},
                {"decimalColumn", lead.size() + .22}};
    }
    auto text = scoreText(score);
    const int size = std::min(portrait ? 44 : 23, (portrait ? 288 : 118) / int(text.size()));
    return {{"text", std::move(text)}, {"size", size}, {"decimal", false}};
}

Value::Object deflexValues(const PresentationState &state, Value::Object values, bool portrait) {
    auto score = narrowRailScore(integer(state, "levelScore"), portrait);
    auto best = narrowRailScore(integer(state, "levelBest"), portrait);
    const auto scoreSize = std::min(score.at("size").number(), best.at("size").number());
    score["size"] = best["size"] = scoreSize;
    values["levelScore"] = std::move(score);
    values["levelBest"] = std::move(best);
    const auto shell = std::get<double>(state.values.at("shell"));
    const auto chain = integer(state, "chain");
    const auto fewest = integer(state, "batsFewest");
    values["shellFill"] = shell;
    values["shellText"] = std::to_string(std::lround(shell * 100)) + "%";
    values["chainFill"] = std::get<double>(state.values.at("chainFill"));
    values["chainText"] = std::to_string(chain) + "x";
    values["chainActive"] = chain > 1;
    values["batsUsed"] = integer(state, "batsUsed");
    values["batsFewest"] = fewest < 0 ? "--" : std::to_string(fewest);
    return values;
}

Value::Object minotaurRescueValues(const PresentationState &state, Value::Object values,
                                   bool portrait) {
    const auto flag = [&](const char *key) { return std::get<bool>(state.values.at(key)); };
    const auto number = [&](const char *key) { return std::get<double>(state.values.at(key)); };
    const auto percent = [](double value) {
        return value >= 0 ? std::to_string(std::lround(value * 100)) + "%" : std::string("--");
    };
    const bool solar = flag("solar"), mines = flag("minesVisible");
    values["solar"] = solar;
    values["minesVisible"] = mines;
    values["nextExtraVisible"] = flag("nextExtraVisible");
    values["highScore"] = narrowRailScore(integer(state, "highScore"), portrait);
    values["nextExtra"] =
        flag("maxLives")
            ? Value::Object{{"text", "Max Lives"}, {"size", portrait ? 30 : 12}, {"decimal", false}}
            : narrowRailScore(integer(state, "nextExtra"), portrait);
    const bool blackHole = flag("blackHole");
    values["firepower"] = number("firepower");
    values["blackHole"] = blackHole;
    values["sunStability"] = number("sunStability");
    values["sunText"] = percent(number("sunStability"));
    for (auto key : {"minesShot", "rescuesLevel", "rescuesGame", "livesLost", "minotaursLost"})
        values[key] = scoreText(integer(state, key));
    values["efficiencyLevel"] = percent(number("efficiencyLevel"));
    values["efficiencyGame"] = percent(number("efficiencyGame"));
    if (!portrait)
        return values;
    // Portrait cards keep their approved widths and the shorter row stays centered.
    constexpr std::array solarWidths{108, 137, 165, 107, 142};
    const std::array widths = solar ? solarWidths : std::array{175, 185, 0, 175, 0};
    const int gap = 9;
    int rowWidth = -gap;
    for (size_t i = mines ? 0 : 1; i < widths.size(); ++i)
        if (widths[i] > 0)
            rowWidth += widths[i] + gap;
    int x = (720 - rowWidth) / 2;
    constexpr std::array names{"mines", "rescues", "efficiency", "lives", "lost"};
    for (size_t i = 0; i < widths.size(); ++i) {
        values[std::string(names[i]) + "X"] = x;
        values[std::string(names[i]) + "Width"] = widths[i] > 0 ? widths[i] : solarWidths[i];
        if (widths[i] > 0 && (i != 0 || mines))
            x += widths[i] + gap;
    }
    return values;
}

Value::Object minotronItem(const PresentationState::CollectionItem &item, size_t row) {
    const auto &frame = minotronFrames[item.colourSource];
    return {{"id", item.id},
            {"row", int(row)},
            {"label", minotronItems[item.colourSource]},
            {"frame", Value::Array{frame[0], frame[1], frame[2], frame[3]}},
            {"remaining", item.value / 10000.}};
}

void minotronPortraitExpiry(Value::Object &values, Value::Array &items) {
    const size_t columns = items.size() > 15 ? 7 : 5;
    const auto rows = (items.size() + columns - 1) / columns;
    const double maxHeight = std::min(471., values["portraitRail"].number(514.) - 24.);
    const double rowHeight = std::min(140., (maxHeight - 51.) / std::max<size_t>(1, rows));
    const double cellWidth = 280. / columns;
    const double tileScale = rowHeight / 140.;
    for (size_t i = 0; i < items.size(); ++i) {
        items[i]["x"] = (i % columns + .5) * cellWidth;
        items[i]["y"] = (i / columns) * rowHeight;
    }
    values["portraitExpiryHeight"] = rows ? 51. + rows * rowHeight : 0.;
    values["portraitExpiryScale"] = tileScale;
}

Value::Object minotronValues(const PresentationState &state, Value::Object values, bool portrait) {
    const auto highScore = integer(state, "highScore");
    const auto stageBest = integer(state, "stageBestScore");
    if (portrait) {
        const auto highText = scoreText(highScore);
        const auto bestText = stageBest < 0 ? std::string("--") : scoreText(stageBest);
        values["highScoreFull"] = highText;
        values["stageBestFull"] = bestText;
        values["highFont"] = std::min(44, 396 / int(highText.size()));
        values["stageFont"] = std::min(44, 396 / int(bestText.size()));
    } else {
        values["highScore"] = compactScore(highScore);
        values["stageBest"] = compactScore(stageBest);
    }
    values["hasStageBest"] = stageBest >= 0;
    values["savedLives"] = std::to_string(integer(state, "stageBestLives")) + " Lives";
    const auto pattern = integer(state, "shotPattern");
    values["pattern"] = std::to_string(pattern) + "-Way";
    values["multiplier"] = pattern == 5 ? "x6" : pattern == 3 ? "x3" : "x1";
    const auto reflect = std::get<bool>(state.values.at("reflective"));
    const auto explode = std::get<bool>(state.values.at("explosive"));
    values["behavior"] = reflect ? "Reflect" : explode ? "Explode" : "Normal";
    values["both"] = reflect && explode;
    values["following"] = integer(state, "beasties");
    values["killed"] = integer(state, "beastiesKilled");
    values["collected"] = integer(state, "pickupsThisLevel");
    const bool pending = std::get<bool>(state.values.at("lifeDropPending"));
    values["lifePending"] = pending;
    values["lifeAvailable"] = std::get<bool>(state.values.at("lifeDropAvailable"));
    values["nextLife"] =
        pending ? "Drop Pending" : scoreText(integer(state, "nextExtraLife")) + " pts";

    const auto loveFrames = integer(state, "loveFrames");
    values["loveFill"] = double(loveFrames) / std::max<int64_t>(1, integer(state, "loveDuration"));
    values["loveText"] = std::to_string((loveFrames + 59) / 60) + "s";
    const auto invincible = std::get<double>(state.values.at("invincibleFraction"));
    values["invincibleFill"] = invincible;
    const auto tenths = int(std::ceil(invincible * (500. / 6)));
    values["invincibleText"] =
        std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + "s";
    const int effectsHeight = 82 * (int(loveFrames > 0) + int(invincible > 0));
    values["effectsHeight"] = effectsHeight;

    auto &history = values["history"].array();
    if (const auto items = state.collections.find("pickupHistory");
        items != state.collections.end())
        for (const auto &item : items->second)
            history.emplace_back(minotronItem(item, history.size()));
    auto &expiry = values["expiry"].array();
    auto &overflow = values["overflow"].array();
    if (const auto items = state.collections.find("expiringPickups");
        items != state.collections.end())
        for (const auto &item : items->second) {
            auto &list = portrait || expiry.size() < minotronPrimaryExpiryRows ? expiry : overflow;
            list.emplace_back(minotronItem(item, list.size()));
        }
    if (portrait) {
        minotronPortraitExpiry(values, expiry);
        return values;
    }
    const int expiryHeight = expiry.empty() ? 0 : 42 + int(expiry.size()) * 42;
    values["expiryHeight"] = expiryHeight;
    values["bottomHeight"] = expiryHeight + effectsHeight;
    // Keep a full-width row whenever possible. Spare space between the left cards handles larger
    // bursts.
    const double overflowRow = std::min(42., 445. / std::max<size_t>(1, overflow.size()));
    values["overflowHeight"] = overflow.empty() ? 0. : 42. + overflow.size() * overflowRow;
    values["overflowRow"] = overflowRow;
    values["overflowIcon"] = std::min(32., overflowRow - 2.);
    values["overflowBar"] = std::min(13., overflowRow / 2.);
    return values;
}

const char *polarityName(double polarity) {
    return polarity > 0 ? "Garantido" : polarity < 0 ? "Caprichoso" : "Neutral";
}

Value::Object superOxWarsValues(const PresentationState &state, Value::Object values) {
    values["highScore"] = scoreText(integer(state, "highScore"));
    const auto blue = integer(state, "blueBest"), red = integer(state, "redBest");
    const auto lives = integer(state, "savedLives");
    values["blueBest"] = blue >= 0 ? scoreText(blue) : "--";
    values["redBest"] = red >= 0 ? scoreText(red) : "--";
    values["combined"] = blue >= 0 && red >= 0 ? scoreText(blue + red) : "--";
    values["savedLives"] = lives >= 0 ? std::to_string(lives) : "--";
    for (const auto *side : {"blue", "red"}) {
        const std::string key = side;
        values[key + "Targets"] = std::to_string(integer(state, key + "Cleared")) + " / " +
                                  std::to_string(integer(state, key + "Total"));
    }
    for (const auto *key : {"formationChain", "oxCombo"})
        values[key] = integer(state, key);
    for (const auto *key : {"areaProgress", "oxWindow"})
        values[key] = std::get<double>(state.values.at(key));
    for (const auto *key : {"area", "areaPhase"})
        values[key] = std::get<std::string>(state.values.at(key));
    const auto polarity = std::get<double>(state.values.at("polarity"));
    const auto dominance = std::lround(std::abs(polarity) * 50);
    values["polarityText"] = std::to_string(50 + dominance) + "%";
    values["leadingPolarity"] = polarityName(dominance ? polarity : 0.);
    // A finite palette lets loading warm every polarity glyph tint.
    values["polarityStrength"] = std::lround(polarity * 5) / 5.;
    for (int slot = 1; slot <= 5; ++slot)
        values["follower" + std::to_string(slot) + "Fill"] = 0;
    const auto followers = state.collections.find("followers");
    if (followers != state.collections.end())
        for (const auto &follower : followers->second) {
            const auto key = "follower" + std::to_string(follower.id + 1);
            values[key + "Fill"] = follower.value / 10000.;
            values[key + "Tint"] = follower.colourSource == 0 ? Value::Array{.36, .69, 1, 1}
                                                              : Value::Array{1, .39, .43, 1};
        }
    return values;
}

bool loadWidgetGl() {
    return Renderer::loadGlesEntryPoints(
        [](const char *name) { return reinterpret_cast<void *>(SDL_GL_GetProcAddress(name)); });
}

struct OverlayLayout {
    Viewport viewport;
    RenderTarget target;
    WidgetGame game;
    double scale;
    bool portrait = false;
    double portraitInset = 0;
    double portraitRail = 0;
    double landscapeInset = 0;

    Value::Object inputs() const {
        if (game == WidgetGame::Deflex || game == WidgetGame::MinotaurRescue)
            return {{"railScale", scale}, {"inset", portrait ? portraitInset : landscapeInset}};
        if (game == WidgetGame::Minotron && portrait)
            return {{"railScale", scale},
                    {"portraitInset", portraitInset},
                    {"portraitRail", portraitRail}};
        if (game == WidgetGame::GoatUp2 || game == WidgetGame::Minotron)
            return {{"railScale", scale},
                    {"portraitInset", portraitInset},
                    {"landscapeInset", landscapeInset}};
        if (portrait)
            return {{"railScale", scale}, {"portraitInset", portraitInset}};
        return {{"railScale", scale}};
    }
};

std::optional<OverlayLayout> overlayLayout(WidgetGame game, unsigned framebuffer, int width,
                                           int height, int x, int y, int gameplayWidth,
                                           int gameplayHeight, int turns) {
    const bool narrowRail = game == WidgetGame::Deflex || game == WidgetGame::MinotaurRescue;
    const bool landscapeGame = game == WidgetGame::Minotron || narrowRail;
    if (turns != 0 || width <= 0 || height <= 0 ||
        (landscapeGame ? gameplayWidth <= gameplayHeight : gameplayWidth >= gameplayHeight))
        return {};
    const Rect gameplay{double(x), double(y), double(gameplayWidth), double(gameplayHeight)};
    OverlayLayout layout{
        {width, height, 0, gameplay},
        {framebuffer, width, height, {0, 0, double(width), double(height)}, gameplay},
        game,
        0};
    if (narrowRail) {
        layout.portrait = width < height;
        const bool portrait = layout.portrait;
        const double units = std::min(width, height) / 720.;
        const double edge = std::max(0., (std::max(width, height) - units * 1280.) / 2.);
        layout.portraitInset = layout.landscapeInset = edge / units;
        const double rail = portrait ? y : std::min(x, width - x - gameplayWidth);
        const double portraitHeight = game == WidgetGame::Deflex ? 278. : 240.;
        layout.scale = std::min(1., (rail - edge) / (units * (portrait ? portraitHeight : 160.)));
        return layout.scale >= .7 ? std::optional<OverlayLayout>{layout} : std::nullopt;
    }
    if (width < height) {
        const double units = width / 1000.;
        const double edge = std::max(0., (height - width * 16. / 9.) / 2.);
        const double rail = std::min(y - edge, height - edge - y - gameplayHeight);
        const double requiredRail = game == WidgetGame::Minotron     ? 398.
                                    : game == WidgetGame::Gridrunner ? 219.
                                    : game == WidgetGame::GoatUp2    ? 160.
                                                                     : 220.;
        layout.scale = std::min(1., rail / (units * requiredRail));
        if (layout.scale < (game == WidgetGame::Minotron ? .70 : .8))
            return {};
        layout.portrait = true;
        layout.portraitInset = edge / units;
        layout.portraitRail = rail / (units * layout.scale);
        if (game == WidgetGame::Gridrunner)
            layout.portraitInset += 15 * layout.scale;
        return layout;
    }
    const double units = height / 1000.;
    const double edge = std::max(0., (width - height * 16. / 9.) / 2.);
    const double rail = std::min(x - edge, width - edge - x - gameplayWidth);
    layout.landscapeInset = edge / units;
    if (game == WidgetGame::Minotron) {
        layout.scale = std::min(1., rail / (units * 222.));
        return layout.scale >= .60 ? std::optional<OverlayLayout>{layout} : std::nullopt;
    }
    layout.scale = game == WidgetGame::Gridrunner ? std::min(1.16, (rail / units - 44) / 304)
                                                  : std::min(1., (rail / units - 44) / 410);
    if (layout.scale < .55)
        return {};
    return layout;
}
} // namespace

struct GameplayWidgets::Impl {
    Runtime runtime;
    Renderer renderer{BeamAppearance{}, BeamProfile::Gameplay};
    bool loaded = false, portrait = false, failed = false;
    WidgetGame game = WidgetGame::Gridrunner;
    int warmWidth = 0, warmHeight = 0;
    double warmScale = 0;
    uint64_t session = 0, sequence = 0;
    std::array<int64_t, 8> peak{};
    bool sampled = false, visible = false;
    std::array<int, 6> lastLayout{};

    void prepare(const OverlayLayout &layout);
};

GameplayWidgets::GameplayWidgets() : impl_(std::make_unique<Impl>()) {}
GameplayWidgets::~GameplayWidgets() = default;

void GameplayWidgets::Impl::prepare(const OverlayLayout &layout) {
    if (!loaded || game != layout.game || portrait != layout.portrait) {
        if (!loadWidgetGl())
            throw std::runtime_error("GLES sampler entry point unavailable");
        const auto *source = layout.game == WidgetGame::MinotaurRescue
                                 ? minotaurRescueWidgetDocument
                             : layout.game == WidgetGame::Deflex      ? deflexWidgetDocument
                             : layout.game == WidgetGame::Minotron    ? minotronWidgetDocument
                             : layout.game == WidgetGame::GoatUp2     ? goatUp2WidgetDocument
                             : layout.game == WidgetGame::Minos       ? minosWidgetDocument
                             : layout.game == WidgetGame::FiveADay    ? fiveADayWidgetDocument
                             : layout.game == WidgetGame::SuperOxWars ? superOxWarsWidgetDocument
                             : layout.game == WidgetGame::GoatUp      ? goatUpWidgetDocument
                                                                      : gridrunnerWidgetDocument;
        auto document = Value::parse(source);
        if (layout.portrait)
            document["root"]["children"] =
                document.at("templates").at("portraitLayout").at("children");
        runtime.document(std::move(document));
        renderer.assets(runtime.document(), [game = layout.game](const auto &path, auto &bytes) {
            return readBundledAsset(widgetGameName(game), path, bytes);
        });
        loaded = true;
        game = layout.game;
        portrait = layout.portrait;
        warmWidth = 0;
    }
    const auto &viewport = layout.viewport;
    const auto &target = layout.target;
    const double scale = layout.scale;
    if (warmWidth == viewport.width && warmHeight == viewport.height && warmScale == scale)
        return;
    if (warmWidth == viewport.width && warmHeight == viewport.height)
        renderer.release();
    Runtime warm;
    warm.document(runtime.document());
    auto sample = layout.inputs();
    if (game == WidgetGame::MinotaurRescue) {
        PresentationState state;
        state.values = {{"solar", true},
                        {"blackHole", false},
                        {"highScore", int64_t(1247360)},
                        {"nextExtraVisible", true},
                        {"maxLives", false},
                        {"nextExtra", int64_t(24860)},
                        {"firepower", .71},
                        {"sunStability", .62},
                        {"minesVisible", true},
                        {"minesShot", int64_t(5)},
                        {"rescuesLevel", int64_t(8)},
                        {"rescuesGame", int64_t(26)},
                        {"efficiencyLevel", .75},
                        {"efficiencyGame", .68},
                        {"livesLost", int64_t(2)},
                        {"minotaursLost", int64_t(3)}};
        sample = minotaurRescueValues(state, sample, portrait);
        uint64_t warmSequence = 0;
        warm.observe({"minorescue", 0, ++warmSequence, sample});
        renderer.warmCached(warm.update(0, viewport), target);
        constexpr std::array narrowSizes{23, 19, 16, 14, 13, 11, 10, 9, 8};
        constexpr std::array portraitSizes{44, 41, 36, 32, 28, 26, 24, 22, 20};
        for (int size : portrait ? portraitSizes : narrowSizes) {
            for (const auto *key : {"highScore", "nextExtra"})
                sample[key] =
                    Value::Object{{"text", "0123456789,m"}, {"size", size}, {"decimal", false}};
            warm.observe({"minorescue", 0, ++warmSequence, sample});
            renderer.warmCached(warm.update(0, viewport), target);
        }
        state.values["blackHole"] = true;
        state.values["maxLives"] = true;
        warm.observe({"minorescue", 0, ++warmSequence,
                      minotaurRescueValues(state, layout.inputs(), portrait)});
        renderer.warmCached(warm.update(0, viewport), target);
        state.values["solar"] = false;
        warm.observe({"minorescue", 0, ++warmSequence,
                      minotaurRescueValues(state, layout.inputs(), portrait)});
        renderer.warmCached(warm.update(0, viewport), target);
        warmWidth = viewport.width;
        warmHeight = viewport.height;
        warmScale = scale;
        return;
    }
    if (game == WidgetGame::Deflex) {
        sample["shellFill"] = .94;
        sample["shellText"] = "100%";
        sample["chainFill"] = .61;
        sample["chainText"] = "2x";
        sample["chainActive"] = true;
        sample["batsUsed"] = 2;
        sample["batsFewest"] = "--";
        for (int length = 1; length <= 14; ++length) {
            Value::Object score{
                {"text", "0123456789,-"},
                {"decimal", false},
                {"size", std::min(portrait ? 44 : 23, (portrait ? 288 : 118) / length)}};
            sample["levelScore"] = score;
            sample["levelBest"] = std::move(score);
            warm.observe({"deflex", 0, uint64_t(length), sample});
            renderer.warmCached(warm.update(0, viewport), target);
        }
        if (!portrait) {
            sample["levelScore"] = narrowRailScore(1234567, false);
            sample["levelBest"] = narrowRailScore(1234567, false);
            warm.observe({"deflex", 0, 15, std::move(sample)});
            renderer.warmCached(warm.update(0, viewport), target);
        }
        warmWidth = viewport.width;
        warmHeight = viewport.height;
        warmScale = scale;
        return;
    }
    if (game == WidgetGame::Minotron) {
        sample["hasStageBest"] = true;
        sample["savedLives"] = "0123456789 Lives";
        sample["pattern"] = "135-Way";
        sample["multiplier"] = "x136";
        sample["behavior"] = "Normal Reflect Explode";
        sample["both"] = true;
        sample["following"] = 0;
        sample["killed"] = 0;
        sample["collected"] = 0;
        sample["lifeAvailable"] = true;
        sample["lifePending"] = true;
        sample["nextLife"] = "Drop Pending";
        sample["loveFill"] = .6;
        sample["loveText"] = "0123456789s";
        sample["invincibleFill"] = .15;
        sample["invincibleText"] = "0123456789.s";
        sample["effectsHeight"] = 164;
        sample["expiryHeight"] = 84;
        sample["bottomHeight"] = 248;
        sample["expiry"] = Value::Array{minotronItem({0, 1000, 6}, 0)};
        sample["overflowHeight"] = 0;
        if (portrait) {
            sample["highScoreFull"] = "0123456789,";
            sample["stageBestFull"] = "0123456789,-";
            minotronPortraitExpiry(sample, sample["expiry"].array());
        }
        constexpr int scoreSizes[] = {29, 24, 21, 19, 19};
        constexpr int portraitScoreSizes[] = {44, 39, 36, 33, 30};
        for (size_t pass = 0; pass < std::size(scoreSizes); ++pass) {
            auto score = compactScore(3234567);
            score["size"] = scoreSizes[pass];
            sample["highScore"] = score;
            sample["stageBest"] = std::move(score);
            if (portrait) {
                sample["highFont"] = portraitScoreSizes[pass];
                sample["stageFont"] = portraitScoreSizes[pass];
            }
            auto &history = sample["history"].array();
            history.clear();
            for (size_t i = 0; i < 5; ++i)
                history.emplace_back(minotronItem({i, 0, int((pass * 5 + i) % 9)}, i));
            if (pass == 1) {
                sample["lifePending"] = false;
                sample["nextLife"] = "0123456789, pts";
                sample["killed"] = 1;
            }
            warm.observe({"minotron", 0, pass + 1, sample});
            renderer.warmCached(warm.update(0, viewport), target);
        }
        warmWidth = viewport.width;
        warmHeight = viewport.height;
        warmScale = scale;
        return;
    }
    if (game == WidgetGame::GoatUp2) {
        sample["objects"] = "0123456789/";
        sample["minotaurs"] = "0123456789/";
        sample["tail"] = "0123456789";
        sample["countSize"] = 27;
        sample["invincibleFill"] = .62;
        sample["unlimitedFill"] = .55;
        sample["invincibleTime"] = "0123456789.s";
        sample["unlimitedTime"] = "0123456789s";
        warm.observe({"goatup2", 0, 1, sample});
        renderer.warmCached(warm.update(0, viewport), target);
        sample["countSize"] = 18;
        sample["unlimitedFill"] = 0;
        sample["blocked"] = true;
        warm.observe({"goatup2", 0, 2, std::move(sample)});
        renderer.warmCached(warm.update(0, viewport), target);
        warmWidth = viewport.width;
        warmHeight = viewport.height;
        warmScale = scale;
        return;
    }
    if (game == WidgetGame::Minos) {
        warm.observe({"minos", 0, 1, sample});
        renderer.warmCached(warm.update(0, viewport), target);
        sample["pastPar"] = true;
        sample["missionLeg"] = "Descend";
        sample["landingGrade"] = "ABCD-";
        sample["multiplier"] = "10x";
        sample["multiplierSize"] = 26;
        constexpr const char *ships[] = {"Pilot", "Warrior", "Commander"};
        for (size_t index = 0; index < std::size(ships); ++index) {
            sample["shipName"] = ships[index];
            warm.observe({"minos", 0, uint64_t(index + 2), sample});
            renderer.warmCached(warm.update(0, viewport), target);
        }
        warmWidth = viewport.width;
        warmHeight = viewport.height;
        warmScale = scale;
        return;
    }
    if (game == WidgetGame::FiveADay) {
        warm.observe({"fiveaday", 0, 1, sample});
        renderer.warmCached(warm.update(0, viewport), target);
        sample["minotaurGoalComplete"] = false;
        sample["fruitGoalComplete"] = true;
        if (portrait)
            sample["formationActive"] = false; // Warm the standalone 2x Points badge.
        warm.observe({"fiveaday", 0, 2, std::move(sample)});
        renderer.warmCached(warm.update(0, viewport), target);
        warmWidth = viewport.width;
        warmHeight = viewport.height;
        warmScale = scale;
        return;
    }
    if (game == WidgetGame::SuperOxWars) {
        sample["area"] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        constexpr const char *phases[] = {"To Boss", "Boss", "Bonus"};
        for (int level = -5; level <= 5; ++level) {
            sample["areaPhase"] = phases[(level + 5) % 3];
            sample["polarityStrength"] = level / 5.;
            sample["leadingPolarity"] = polarityName(level);
            warm.observe({"superoxwars", 0, uint64_t(level + 6), sample});
            renderer.warmCached(warm.update(0, viewport), target);
        }
        sample["polarityStrength"] = 0.;
        for (int side : {-1, 1}) {
            sample["leadingPolarity"] = polarityName(side);
            warm.observe({"superoxwars", 0, side < 0 ? uint64_t(12) : uint64_t(13), sample});
            renderer.warmCached(warm.update(0, viewport), target);
        }
        warmWidth = viewport.width;
        warmHeight = viewport.height;
        warmScale = scale;
        return;
    }
    if (game == WidgetGame::GoatUp) {
        warm.observe({"goatup", 0, 1, sample});
        renderer.warmCached(warm.update(0, viewport), target);
        sample["slowDown"] = .64;
        sample["slowDownVisible"] = true;
        sample["gestationVisible"] = false;
        warm.observe({"goatup", 0, 2, std::move(sample)});
        renderer.warmCached(warm.update(0, viewport), target);
        warmWidth = viewport.width;
        warmHeight = viewport.height;
        warmScale = scale;
        return;
    }
    if (portrait) {
        auto effects = warm.document().at("root").at("inputs").at("effects").at(2).array();
        arrangePortraitEffects(effects);
        sample["effects"] = std::move(effects);
    }
    warm.observe({"gridrunner", 0, 1, sample});
    renderer.warmCached(warm.update(0, viewport), target);
    const Value::Array completedStats{
        Value::Object{
            {"id", "swarmers"}, {"label", "Swarmers Shot"}, {"value", "0/0"}, {"completed", true}},
        Value::Object{{"id", "deflections"},
                      {"label", "Deflections"},
                      {"value", "0/0"},
                      {"completed", true}}};
    sample["stats"] = completedStats;
    warm.observe({"gridrunner", 0, 2, sample});
    renderer.warmCached(warm.update(0, viewport), target);
    // Warm every log height and the names absent from the full sample.
    Value::Array otherPickups;
    for (int count = 0; count < 5; ++count) {
        if (count > 0) {
            const int kind = count - 1;
            otherPickups.emplace_back(item({}, {uint64_t(kind), kind, kind}));
        }
        auto inputs = layout.inputs();
        inputs["pickups"] = otherPickups;
        inputs["effects"] = Value::Array{};
        warm.observe({"gridrunner", 0, uint64_t(count + 3), std::move(inputs)});
        renderer.warmCached(warm.update(0, viewport), target);
    }
    warmWidth = viewport.width;
    warmHeight = viewport.height;
    warmScale = scale;
}

void GameplayWidgets::prepare(WidgetGame game, int width, int height, int x, int y,
                              int gameplayWidth, int gameplayHeight, int turns) {
    auto layout = overlayLayout(game, 0, width, height, x, y, gameplayWidth, gameplayHeight, turns);
    if (!layout || impl_->failed)
        return;
    try {
        impl_->prepare(*layout);
    } catch (const std::exception &error) {
        SDL_Log("%s widget warm-up failed: %s", widgetGameName(game), error.what());
        impl_->failed = true;
    }
}

bool GameplayWidgets::draw(WidgetGame game, const Core &core, bool refresh, unsigned framebuffer,
                           int width, int height, int x, int y, int gameplayWidth,
                           int gameplayHeight, int turns) {
    auto &i = *impl_;
    auto layout =
        overlayLayout(game, framebuffer, width, height, x, y, gameplayWidth, gameplayHeight, turns);
    if (i.failed || !layout) {
        i.sampled = false;
        return false;
    }
    const auto &viewport = layout->viewport;
    const auto &target = layout->target;
    const std::array<int, 6> geometry{width, height, x, y, gameplayWidth, gameplayHeight};
    try {
        if (!refresh && i.sampled && i.game == game && i.lastLayout == geometry) {
            if (i.visible)
                i.renderer.renderCached(i.runtime.frame(), target);
            return i.visible;
        }
        const auto state = core.widgetPresentationState();
        i.sampled = true;
        i.visible = state.gameplay;
        i.lastLayout = geometry;
        if (!i.visible)
            return false;
        i.prepare(*layout);
        if (game != WidgetGame::Gridrunner) {
            auto values = game == WidgetGame::MinotaurRescue
                              ? minotaurRescueValues(state, layout->inputs(), layout->portrait)
                          : game == WidgetGame::Deflex
                              ? deflexValues(state, layout->inputs(), layout->portrait)
                          : game == WidgetGame::Minotron
                              ? minotronValues(state, layout->inputs(), layout->portrait)
                          : game == WidgetGame::GoatUp2  ? goatUp2Values(state, layout->inputs())
                          : game == WidgetGame::Minos    ? minosValues(state, layout->inputs())
                          : game == WidgetGame::FiveADay ? fiveADayValues(state, layout->inputs())
                          : game == WidgetGame::GoatUp ? goatUpValues(state, layout->inputs())
                                                       : superOxWarsValues(state, layout->inputs());
            i.runtime.observe(
                {widgetGameName(game), state.session, ++i.sequence, std::move(values)});
            i.renderer.renderCached(i.runtime.update(0, viewport), target);
            return true;
        }
        if (i.session != state.session) {
            i.session = state.session;
            i.peak.fill(0);
        }
        const auto checkpoint = integer(state, "widgets.nextCheckpoint");
        auto values = layout->inputs();
        values["checkpointVisible"] = checkpoint != 0;
        values["nextCheckpoint"] = "Grid " + std::to_string(checkpoint);
        values["checkpointScore"] = state.values.count("widgets.checkpointScore")
                                        ? scoreText(integer(state, "widgets.checkpointScore"))
                                        : "--";
        values["stats"] = Value::Array{
            Value::Object{{"id", "ships-lost"},
                          {"label", "Ships Lost"},
                          {"value", integer(state, "widgets.shipsLost")}},
            Value::Object{{"id", "deathless"},
                          {"label", "Deathless Grids"},
                          {"value", std::to_string(integer(state, "widgets.deathless")) + "/5"}},
            achievementRow(state, "swarmers", "Swarmers Shot"),
            achievementRow(state, "deflections", "Deflections")};
        auto &pickups = values["pickups"].array();
        auto log = state.collections.find("widgets.pickups");
        if (log != state.collections.end())
            for (const auto &pickup : log->second)
                pickups.emplace_back(item(state, pickup));
        auto &effects = values["effects"].array();
        auto active = state.collections.find("widgets.effects");
        std::array<bool, 8> seen{};
        if (active != state.collections.end())
            for (const auto &effect : active->second) {
                const auto kind = size_t(effect.colourSource);
                seen[kind] = true;
                i.peak[kind] =
                    std::max({i.peak[kind], effect.value,
                              integer(state, "widgets.duration." + std::to_string(kind))});
                auto row = item(state, effect);
                row["remaining"] = effect.value;
                row["duration"] = i.peak[kind];
                // The simulation subtracts two units at 60 ticks per second.
                const auto tenths = (effect.value + 11) / 12;
                row["timeText"] =
                    std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + "s";
                effects.emplace_back(std::move(row));
            }
        for (size_t kind = 0; kind < seen.size(); ++kind)
            if (!seen[kind])
                i.peak[kind] = 0;
        if (layout->portrait)
            arrangePortraitEffects(effects);
        i.runtime.observe({"gridrunner", state.session, ++i.sequence, std::move(values)});
        i.renderer.renderCached(i.runtime.update(0, viewport), target);
        return true;
    } catch (const std::exception &error) {
        SDL_Log("%s LiveWidgets unavailable: %s", widgetGameName(game), error.what());
        i.failed = true;
        return false;
    }
}

void GameplayWidgets::release() {
    impl_->renderer.release();
    impl_ = std::make_unique<Impl>();
}

void GameplayWidgets::abandonGraphics() {
    impl_->renderer.abandonGraphics();
    impl_->warmWidth = impl_->warmHeight = 0;
    impl_->sampled = false;
}

} // namespace mino
