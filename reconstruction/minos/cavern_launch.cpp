// cavern_launch.cpp - see cavern_launch.h. Case order and values follow theGame::launch
// 0x5ab00 (switch on the character; unlisted characters do nothing and mark nothing).
#include "cavern_launch.h"

namespace minos::cavern {

namespace {
constexpr float kQuarter = 1.5707964f, kHalf = 3.1415927f;
// cavernGoal/oneUp in map column 9 move right by 0.025f (widened to double).
constexpr double kColumn9Shift = double(0.025f);

SpawnRequest base(char ch, EntityClass cls, EntityList list, bool init, Vec2 pos, double lower,
                  int column, int row) {
    SpawnRequest r;
    r.character = ch;
    r.cls = cls;
    r.list = list;
    r.callsInit = init;
    r.position = {pos.x, pos.y - lower};
    r.column = column;
    r.row = row;
    return r;
}
SpawnRequest scrambleRocket(char ch, int32_t spriteIndex, float angle, Vec2 p, int c, int r) {
    SpawnRequest s = base(ch, EntityClass::ScrambleRocket, EntityList::List2e8, true, p, 0.0, c, r);
    s.spriteIndex = spriteIndex;
    s.rot = angle;
    return s;
}
SpawnRequest turret(char ch, float angle, Vec2 p, int c, int r) {
    SpawnRequest s = base(ch, EntityClass::Turret, EntityList::List2e8, true, p, 0.0, c, r);
    s.rot = angle;
    return s;
}
SpawnRequest rocket(char ch, Vec2 p, int c, int r) {
    return base(ch, EntityClass::Rocket, EntityList::List2e8, true, p, 0.008, c, r);
}
// Shared setup of the classes launch initialises inline (no Class::init).
void inlineSetup(SpawnRequest &s, double scaleX, double scaleY) {
    s.game = true;
    s.size = 0.04f;
    s.scaleX = scaleX;
    s.scaleY = scaleY;
    s.burstOnClear = 0;
    s.hidden = 0;
}
} // namespace

std::optional<SpawnRequest> launch(StripMap &map, const LaunchContext &ctx, char ch, Vec2 p, int c,
                                   int r) {
    std::optional<SpawnRequest> out;
    switch (ch) {
    case '$':
        out = scrambleRocket(ch, 0x37, 0.0f, p, c, r);
        break;
    case '(':
        out = scrambleRocket(ch, 0, kQuarter, p, c, r);
        break;
    case ')':
        out = scrambleRocket(ch, 0, -kQuarter, p, c, r);
        break;
    case '{':
        out = scrambleRocket(ch, 0x37, kQuarter, p, c, r);
        break;
    case '}':
        out = scrambleRocket(ch, 0x37, -kQuarter, p, c, r);
        break;
    case '<':
        out = turret(ch, kQuarter, p, c, r);
        break;
    case '>':
        out = turret(ch, -kQuarter, p, c, r);
        break;
    case '^':
        out = turret(ch, 0.0f, p, c, r);
        break;
    case '|':
        out = turret(ch, -kHalf, p, c, r);
        break;
    case '*':
        out = base(ch, EntityClass::Faller, EntityList::List2e4, true, p, 0.0, c, r);
        break;
    case 'V':
        out = base(ch, EntityClass::Goat, EntityList::List2e8, true, p, 0.0, c, r);
        break;
    case 'S':
        out = base(ch, EntityClass::MotherSheep, EntityList::List2e4, true, p, 0.15, c, r);
        out->hidden = 0;
        break;
    case 's':
        // Saucers only in caverns with flag 0x40; the cell is marked either way.
        if (ctx.cavern->flags & 0x40)
            out = base(ch, EntityClass::Saucer, EntityList::List2e8, true, p, 0.0, c, r);
        break;
    case 'A':
        out = rocket(ch, p, c, r);
        out->hidden = 0;
        out->drawDelay = 0;
        break;
    case 'D':
        out = rocket(ch, p, c, r);
        out->spriteIndexAfterInit = 0x22;
        out->colourWhite = true;
        break;
    case 'U':
        out = rocket(ch, p, c, r);
        out->spriteIndexAfterInit = 5;
        out->size = 0.03f;
        out->colourWhite = true;
        break;
    case 'X':
        out = rocket(ch, p, c, r);
        out->promoteSprite5To6 = true;
        break;
    case 'Y':
        out = rocket(ch, p, c, r);
        break;
    case 'G': {
        out = base(ch, EntityClass::CavernGoal, EntityList::List2e4, false, p, 0.01, c, r);
        out->spriteIndex = ctx.cavern->goalSpriteIndex;
        out->sizeQueryFrame = ctx.cavern->goalSpriteIndex + 2;
        const Vec2 size = ctx.spriteSize(*out->sizeQueryFrame);
        inlineSetup(*out, 1.0, size.y / size.x); // height-to-width aspect
        if (c == 9)
            out->position.x = out->position.x + kColumn9Shift;
        break;
    }
    case 'O':
        out = base(ch, EntityClass::OneUp, EntityList::List2e4, false, p, 0.03, c, r);
        out->spriteIndex = 0x11;
        inlineSetup(*out, 1.0, 1.0);
        if (c == 9)
            out->position.x = out->position.x + kColumn9Shift;
        break;
    case 'M':
        out = base(ch, EntityClass::WavingMinotaur, EntityList::List2e4, false, p, 0.025, c, r);
        out->phase184 = float(ctx.rand->range(0.0, 7.0)); // one global rand() draw
        out->spriteIndex = 0xc;
        out->word188 = 0;
        inlineSetup(*out, 1.2, 1.4);
        out->home1b0 = out->position;
        break;
    default:
        return std::nullopt; // not an entity character: no cell mark
    }
    map.markLaunched(r, c);
    return out;
}

} // namespace minos::cavern
