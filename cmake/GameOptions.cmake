# Collection registration and independently buildable rule libraries.
# Minotron/Rescue combine rules and adapters; later rule libraries can also be
# built for checks when their collection entry is disabled.
option(MINO_CORE_MINOTRON   "Build the Minotron core"   ON)
option(MINO_CORE_MINORESCUE "Build the Minotaur Rescue core" ON)
option(MINO_CORE_SUPEROXWARS "Register the Super Ox Wars core" ON)
option(MINO_CORE_FIVEADAY "Register the Five a Day core" ON)
option(MINO_CORE_GOATUP "Register the GoatUp core" ON)
option(MINO_CORE_GOATUP2 "Register the GoatUp 2 core" ON)
option(MINO_MINOS_RECONSTRUCTION "Build the Caverns of Minos reconstruction library and checks" ON)
option(MINO_CORE_MINOS "Register the Caverns of Minos core" ON)
option(MINO_DEFLEX_RECONSTRUCTION "Build the Deflex reconstruction library and checks" ON)
option(MINO_CORE_DEFLEX "Register the Deflex core" ON)
