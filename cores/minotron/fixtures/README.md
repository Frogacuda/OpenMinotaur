# Minotron gameplay witnesses

These numeric fixtures were captured by executing the original Minotron 2112
1.01 ARMv7 instructions under Unicorn 2.1.4. The executable matches the ARMv7
slice in the archived IPA; its SHA-256 is
`d2fab2a43dc8b477122c827c987175e5f98ace1d05251778de4ea386a5054d60`.
They are independent original-execution witnesses, not saved output from this port.

`mino_minotron_gameplay_checks` runs 4,224 cases against the native implementation:

| Fixture | Cases | Behavior |
| --- | ---: | --- |
| player-shots-ios.tsv | 60 | Aim directions, projectile speeds, single/three/five-way patterns |
| bullet-motion-ios.tsv | 240 | Arena edges, reflection, lifetime and level ending |
| rescues-ios.tsv | 1,728 | Chain bonuses, player/droid pickups, speed upgrades, caps and practice mode |
| powerups-ios.tsv | 432 | All nine pickup types and combinations of existing upgrades |
| damage-ios.tsv | 432 | Health, score multipliers, hot shots, explosive kills and hit callbacks |
| player-death-ios.tsv | 48 | State, demo, invulnerability and delayed-draw death gating |
| progression-ios.tsv | 960 | Completion, herd rescue requirements, final level and all four modes |
| movement-ios.tsv | 324 | Inertial seeking, turning, direct seeking and three edge behaviors |

`level-tables-ios.tsv` additionally checks 11 original tables (3,000 four-byte
words), including all 100 level descriptions, flags and grunt tuning values.
Rows describe their input/output columns in a header. Float bit columns preserve
binary32 values; movement doubles allow an absolute tolerance of 1e-12 for host
math-library differences.

The capture harness is an internal analysis tool. It supplies zeroed synthetic
objects with explicit field values, intercepts allocation and entity insertion,
and isolates audio, logging, pixel shatter and display callbacks. Constructors
and the selected gameplay methods execute original instructions. Imported math
functions use host math; random calls return a fixed value. Progression uses a
stationary synthetic herd member. Pickup checks use empty enemy lists, so bomb
target selection is outside their scope; rescue checks isolate the pickup from
the last-animal completion path. These are focused checks, not full-game replay
equivalence or coverage of every enemy, spawn sequence, collision or input path.

The movement witnesses caught a heading-wrap precision error: the original
compares and wraps using double constants before storing a float. Rounding the
circle constant to float before wrapping changed steering at the angle seam.
