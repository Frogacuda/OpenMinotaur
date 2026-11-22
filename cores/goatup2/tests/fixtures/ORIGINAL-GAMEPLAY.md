# Original gameplay execution witnesses

The `original-*.tsv` files are numeric output from executing the archived iOS
ARMv7 instructions under Unicorn 2.1.4. They are independent of the native
implementation. Original executable SHA-256:
`c17852a2803d533fc73c03511c22946d4991f57328a73f21b5c592ac4334c394`.
An internal analysis harness captured them; no original executable or emulator
is needed to run the native regression checks.

| Fixture | Cases/frames | Original entry | Scope |
| --- | ---: | --- | --- |
| original-goat-contact.tsv | 360 | `0x5b3d8` | Category, sprite, invulnerability and player-state hazard gates |
| original-enemy-damage.tsv | 128 | `0x5e730` | Health, attack power, protected state, death, score and rewards |
| original-pickups.tsv | 180 | `0x60500` | Nine pickup categories, energy/caps, disabled attacks, bonus multipliers and flags |
| original-player-traces.tsv | 880 | `0x59238` | Eleven consecutive 80-frame routes with real terrain queries, falling, horizontal movement, ground/held/midair jumps, water, inverted gravity, grass, checkpoints, springs, crumbling floors, conveyors and a breakable ceiling |
| original-machine-traces.tsv | 1,280 | `0x5c2e0` | Sixteen behavior modes, 80 frames each, including cooldowns, firing, steering, return cycles, splitting health drain and wrapping |
| original-patroller-traces.tsv | 2,240 | `0x5e348` | All fourteen variants in dry/wet and normal/inverted gravity, 40 frames each; target direction/height changes exercise pursuit, charging and firing |
| original-death-exit.tsv | 864 | `0x59238` | Death wait, spare lives, free restart, checkpoint restoration, exit growth/fade, terminal state and visibility |
| original-headbutt-dispatch.tsv | 768 | `0x58e5c` | Carried-object selection/ties, grace gates, disabled attacks, multiplier consumption and reusable pickup restoration |

There are **6,700** comparisons. Inputs and outputs are named in each file's
header. Binary32 fields compare exact bits; motion doubles allow an absolute
tolerance of 1e-12 (1e-13 for player motion). The machine/patroller traces use
explicit synthetic starting states, not a claim of reproducing each placement
constructor. Firing cooldowns are shortened in two machine scenarios to exercise
repeated shots within the trace. Target coordinates are recorded explicitly.

Capture boundaries: synthetic objects and owner state are initialized explicitly.
Allocation, entity insertion, audio, score-row clearing, sprite metadata and
exception bookkeeping are supplied by the harness. Damage/pickup cases isolate
visual explosion and collection-burst factories and base constructors; dispatch
cases isolate explosion. Their health, score, category and state decisions
execute original instructions.
Movement traces execute the real base constructors, geometry calculation,
terrain queries, medium handling, movement and update routines. Player input
acquisition and camera writes are isolated, with movement/jump words supplied as
recorded inputs; decorative fart emission is isolated there and tested through
the real native factory in the live-family checks. Death cases isolate terrain
reparse/copy; native reset tests cover the actual stored terrain restoration.
Original random imports return a fixed value; imported trigonometry uses host
math. These witnesses do not establish full RNG, audio, rendering or whole-game
replay identity.

The separate `goatup2-live-campaign-audit` test runs two controller scripts per
shipped stage, using real placement construction, updates, presentation-state
preparation, contacts and death. Its 144,000 frames cover all 80 stages and 67
placed object variants. It grants no extra energy, invulnerability or completion
states. The scripts do not solve the campaigns: they naturally exercise deaths,
respawns, attacks and fruit, while controlled live-family routes separately
exercise kid collection → fart attack → enemy kill and fruit → exit → level
report. Campaign smoke traces are native integration evidence, not comparisons
with original execution.
