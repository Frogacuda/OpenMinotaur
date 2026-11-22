# Rescue gameplay witnesses

These tables contain observations from executing functions in the original iOS
ARMv7 game, not output saved from OpenMinotaur. The binary is the ARMv7 slice of
`Payload/Mino Rescue.app/Mino Rescue`, bundle version 1.0. Its SHA-256 is
`323ec68af0b103911426106aec78386634c77c16ab10456749e70c418729e5a7`.

The source executable is not distributed here. A local analysis harness uses
Unicorn 2.1.4 to execute its ARM/VFP instructions. Original double-precision square
roots use a host square-root shim; allocation, audio, logging, exception-frame
registration and presentation callbacks are isolated where they are outside the
behavior being observed. No original addresses are used by the production game.
The capture harness is not required to run these committed tables.

| Table | Original entry | Cases | Observations |
| --- | --- | ---: | --- |
| `ship-fire-ios.tsv` | `spaceCraftGun::run`, 0x481b8 | 896 | Timer, relative shot-speed bits and shoot dispatch; vary position independently of velocity, rescues, solar/deep-space and black-hole state |
| `tank-fire-ios.tsv` | `tankGun::run`, 0x4f9e4 | 480 | Timer, burst count and shoot dispatch across player/CPU, power, spin and demo states; sample selection is recorded but not asserted |
| `rescue-pickup-ios.tsv` | `collectible::hitBy`, 0x515a0 | 128 | Score callback, weapon increment, rescue counters, death flag and no-rescue streak reset; score and achievement callbacks are isolated |
| `score-ios.tsv` | `spaceWars::score`, 0x50d1c | 288 | Score and life counters around extra-life thresholds, multi-threshold awards, survival and the ten-life cap; achievement/presentation callbacks are isolated |
| `ship-damage-ios.tsv` | `spaceCraft::damage`, 0x4f450 | 144 | Health, shields, lives, weapon, invulnerability, spin and respawn position; explosion rendering is isolated |
| `entity-motion-ios.tsv` | `entity::run` | 128 | Movement, damping, age and wrapping at both arena sizes, including no-wrap flag retention |
| `sun-reset-ios.tsv` | `theSun::run`, 0x50c40 | 40 | Radius, mass, health, position and collapsed/black-hole reset at level completion |
| `gravity-ios.tsv` | `controlBlock::attract` | 96 | Attraction/repulsion, mass selection, invulnerability and nearest distance; force comparisons allow 1e-14 absolute error for host square-root rounding |
| `sun-growth-ios.tsv` | `theSun::hitBy`, 0x4fb6c | 64 | Radius/mass growth, collapse and capped black-hole mass after swallowing a planet fragment; explosion/allocation/presentation callbacks are isolated |

Total: 2,264 cases. Floating-point output columns named `*_bits` are unsigned
binary32 bit patterns. Other columns describe inputs and observed state; the
first line of each table names its columns.

`minorescue_gameplay_original` runs through CTest and the root `check` target.
These are bounded mechanics witnesses, not full-game emulation or evidence that
every combination of enemies, inputs and campaign states has been exercised.
