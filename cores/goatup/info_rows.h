// Static rows from iOS charScreen 56d98; positions are grid coordinates.
#pragma once
namespace goatup {
struct InfoRow {
    int page, x, y, palette;
    const char *text;
};
inline constexpr InfoRow InfoRows[] = {
    {1, -1, 2, -1, "HOW TO PLAY"},
    {1, -1, 3, -1, "==========="},
    {1, 2, 5, -1, "~99991: ~2929GRAZE ~9999on platforms to"},
    {1, 2, 6, -1, "find bonus items."},
    {1, 2, 10, -1, "~99992: ~2929KISS ~9999billy goats to"},
    {1, 2, 11, -1, "initiate fecundity."},
    {1, 2, 15, -1, "~99993: ~2929GRAZE ~9999some more to"},
    {1, 2, 16, -1, "actualize fecundity."},
    {1, 2, 20, -1, "~99994: ~2929LEAD ~9999your family as"},
    {1, 2, 21, -1, "high as you can go!"},
    {1, 2, 25, 0, "HINT: ~2929USE ~9999your \'tail\' of"},
    {1, 2, 26, -1, "kids to knock off enemies!"},
    {3, -1, 2, 0, "GOAT UP"},
    {3, -1, 3, 0, "======="},
    {3, 2, 6, 8, "created by LLAMASOFT:"},
    {3, 4, 8, 4, "JEFF MINOTAUR"},
    {3, 2, 10, 8, "and"},
    {3, 4, 12, 4, "GILESGOAT"},
    {3, 2, 15, -2, "Rely on ~6399LLAMASOFT ~9999for"},
    {3, 2, 17, -2, "excellent iOS games."},
    {3, 2, 19, 4, "Always excellent controls"},
    {3, 2, 21, 4, "Always excellent design"},
    {3, 2, 24, 5, "Designing games since 1982"},
    {3, 2, 26, 5, "Experience"},
    {3, 9, 27, -1, "~9999makes for"},
    {3, 15, 28, 5, "Excellence"},
    {4, -1, 2, 0, "ALSO BY LLAMASOFT"},
    {4, -1, 3, 0, "================="},
    {4, -1, 10, 4, "MINOTAUR RESCUE"},
    {4, -1, 26, -2, "~9999The distilled essence"},
    {4, -1, 28, -2, "of arcade fun"},
    {4, -1, 30, -2, "For 1 to 4 players"},
    {5, -1, 2, 0, "ALSO BY LLAMASOFT"},
    {5, -1, 3, 0, "================="},
    {5, -1, 10, 4, "MINOTRON: 2112"},
    {5, -1, 26, -2, "~9999The best arena shooter"},
    {5, -1, 28, -2, "on iOS. 100 levels of"},
    {5, -1, 30, -2, "arcade action!"},
    {6, -1, 2, 0, "ALSO BY LLAMASOFT"},
    {6, -1, 3, 0, "================="},
    {6, -1, 10, 4, "DEFLEX"},
    {6, -1, 26, -2, "~9999An elegant and"},
    {6, -1, 28, -2, "challenging dexterity"},
    {6, -1, 30, -2, "puzzle game."},
};
} // namespace goatup
