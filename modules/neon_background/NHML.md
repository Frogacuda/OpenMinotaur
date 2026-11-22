# NHML 1 — Neon Harness

NHML is the human-readable contract between read-only project state and exported Neon parameters. It describes rigging, not scene construction or widget behavior. WidgetML and NHML can consume the same snapshot while remaining independently enabled and separately evaluated.

Creator `.neongles` files retain their editable graphs. An NHML file assigns stable names to the parameters the game may drive. The scene baker resolves those names and validates their destinations. Shipped `.neonb` files contain resolved playback data, exported descriptors and reachable dependencies; they do not initialize the editor catalogue.

## Three separate responsibilities

1. A project adapter publishes named values, their declared units, a game identity, a session identity and monotonically increasing snapshot sequence. It reads existing state; it does not call the renderer or alter simulation logic.

2. NHML maps those inputs to named scene parameters or derives presentation cues from their changes.

3. The renderer applies transient overrides over the baked authored scene. Shader, controller and material implementation details remain behind the export map.

The adapter does not know layer/node addresses. For example, replacing a grid node changes the `parameter` declaration below; the adapter and its `player.x` input remain unchanged.

## Complete example

```text
NHML 1
input "player.x" number "game-widths"
input "power.allAround" boolean "boolean"
input "lives" integer "count"
parameter "grid.yaw" "component.4.4" "radians"
parameter "background.brightness" "layer.3/colour.18.brightness" "gain"
parameter "scene.hue" "runtime.hue" "turns"
value "grid.yaw" "player.x" -0.4 0.43 -0.2 0.2 clamp -0.2 0.2
stateenvelope "power.allAround" "background.brightness" 0.5 0.5 1 2
turn "lives" "scene.hue" 1 0 1 0 1 linear
```

Statements occupy one line. Double-quoted names are recommended; `#` begins a comment. Names and unit identifiers must not contain `#` or newlines. Declarations can appear in any order, but every referenced input and parameter must be declared. Duplicate input names, parameter names and physical parameter destinations are errors. Existing `NEONRIG 1/2` documents remain supported for importing older work; NHML is the creator format for new profiles.

## Inputs, parameters and units

```text
input NAME TYPE UNIT
parameter NAME BACKEND_TARGET UNIT
```

| Input type | Accepted snapshot values |

| --- | --- |

| `number` | Finite real or integer values; booleans are not numeric inputs |

| `integer` | Signed/unsigned integer observations; whole real values must fit ±(2^53−1) |

| `boolean` | A boolean value, not a numeric 0 or 1 |

| `state` | A named string state, up to 128 bytes; use event entry/exit rules to turn it into cues |

Inputs are optional observations, not mandatory game callbacks. A missing, non-finite or incorrectly typed value is unavailable. A value binding then removes its override and reveals the authored scene baseline. Selectors use their declared baseline; state envelopes release; state cycles remove their offset. Counter/event readers re-seed when the value becomes available again, avoiding a false event for the previously unseen total. Already running finite cues retain their normal timeline.

Unit identifiers are explicit documentation contracts; NHML does not silently convert them. `turns` means one full revolution is 1, `degrees` means 360, and `radians` means 2Ï€. `gain`, `multiplier`, `ratio`, `byte`, `texture-widths`, `cells` and project-specific units describe other parameters. A provider and its harness must agree about input units. The bake manifest records parameter units, physical targets, ranges and authored baselines for SDK inspection.

A physical target is a renderer export key, not a public game-state field. Examples include `camera.yaw`, `runtime.hue`, `node.13.f.3`, `clock.8.6.speed`, and `layer.3/node.18.i.2`. Nested `layer.ID/` prefixes address source graphs. These implementation keys are confined to the scene's parameter declarations.

## Passive value mapping

```text
value PARAM INPUT IN_A IN_B OUT_A OUT_B BOUNDARY LOW HIGH
smoothvalue PARAM INPUT IN_A IN_B OUT_A OUT_B BOUNDARY LOW HIGH
```

`IN_A` and `IN_B` are distinct input anchors. The output anchors may be reversed or off-centre. `value` linearly maps and can extrapolate before applying the boundary. `smoothvalue` clamps its normalized input to 0..1 and applies smoothstep t²(3−2t). This is range easing, not a temporal smoothing filter.

`clamp` saturates to `[LOW,HIGH]`. `wrap` repeats over `[LOW,HIGH)`, preserving continuous phase for cyclic UV/hue controls. The wrap period is `HIGH−LOW`; a shader's texture addressing determines the appropriate UV period. No universal slider range is assumed.

## State selection and sustained behavior

```text
select PARAM BASELINE COUNT INPUT MIN VALUE [INPUT MIN VALUE ...]
stateenvelope INPUT PARAM ATTACK RELEASE BASELINE PEAK
cycle INPUT PARAM PERIOD INCREMENT BASELINE
```

`select` uses the first condition whose available value is at least `MIN`; otherwise it uses `BASELINE`. Order is explicit priority, for example invincibility before multi-shot. Boolean states compare as 0/1 after type validation.

`stateenvelope` smoothly attacks to `PEAK`, holds while the input is true/positive, then smoothly releases to `BASELINE`. All durations are seconds. An interrupted transition starts from its current value, without a jump. A missing state behaves as inactive.

`cycle` adds a repeating linear phase offset while the state is active. `PERIOD` is positive seconds per cycle; `INCREMENT` is in destination units. Release removes the temporary offset and reactivation starts a fresh phase. `BASELINE` is explicit for a standalone cycle; when it shares a destination with another baseline-bearing cue, it contributes its phase offset to that result. Existing baked preset oscillators remain part of the scene; their exported clock multipliers can also be mapped with `value`.

## Indexed mappings

```text
indexed PARAM INPUT FIRST BASELINE LOOP_STEP LOW HIGH COUNT VALUE [VALUE ...]
```

An indexed mapping reads an absolute integer input, rather than counting events.

`FIRST` identifies the first entry. It selects `(INPUT - FIRST) modulo COUNT` and

adds `LOOP_STEP` for each complete traversal, then wraps the result into

`[LOW,HIGH)`. There are 1–64 entries. Missing/invalid inputs and values before

`FIRST` select `BASELINE`. A direct jump, saved-game continuation or return to an

earlier level produces the same result as reaching that level normally; no past

events are replayed. This mapping owns its parameter, like `value` and `select`.

For example, Minos rotates six root textures from level 1; each following block

of six levels shifts global hue by 50 degrees. Texture parameters declare

`texture-selector` units, use an integer baseline/list and a zero loop step.

The baker includes every listed texture in the shipped resource closure.

Root labels are zero-based: Root 22 is selector 23.

```text
indexed "wall.texture" "level.number" 1 23 0 1 31 6 23 26 28 16 12 6
indexed "scene.hue" "level.number" 1 0 0.1388888888888889 0 1 6 0 0 0 0 0 0
```

## Derived events

```text
event NAME INPUT CONDITION VALUE HYSTERESIS COOLDOWN
```

An event declaration creates an internal cumulative integer input named `NAME`. It reads an existing declared external input and increments once for each qualifying observation. Feed that internal counter to a pulse, turn, advance or other cue. It never calls or modifies the game. Event declarations cannot chain into one another, and cannot replace an external input name.

Conditions are `increase`, `decrease`, `above`, `below`, `changed`, `enter` and `leave`. `above`/`below` are threshold crossings rather than continuously retriggering predicates. `VALUE` uses a typed token: `n:0.25`, `i:1`, `u:1`, `b:true` or `s:attack`. For increases/decreases it specifies the minimum change; for crossings it specifies the threshold; for entry/exit it specifies the state. `changed` accepts a placeholder value, which is ignored.

Hysteresis is in input units and applies to threshold crossings. Cooldown is in presentation seconds. First observations and recovered missing inputs seed baselines without firing. Event observation preserves signed/unsigned counter comparisons; direct continuous mappings and queued turn arithmetic eventually convert values to floating point, so very large cumulative counters should be adapted through event rules.

```text
input "health" number "ratio"
input "phase" state "enum"
event "hurt" "health" decrease "n:0.1" 0 0
event "danger.entered" "health" below "n:0.25" 0.02 0.2
event "attack.entered" "phase" enter "s:attack" 0 0
pulse "hurt" "background.brightness" 0.1 0.4 1 2
```

Boolean combinations or project-specific derived values can be published by a reusable presentation `ReadSource` adapter. NHML 1 does not embed arbitrary expressions or a general-purpose scripting language.

## Finite cues

```text
pulse INPUT PARAM ATTACK RELEASE BASELINE PEAK
pulsemax INPUT PARAM ATTACK RELEASE BASELINE PEAK
envelope INPUT PARAM ATTACK HOLD RELEASE BASELINE PEAK
statepulse INPUT PARAM ATTACK RELEASE BASELINE PEAK
sweep INPUT PARAM DURATION BASELINE PEAK
easesweep INPUT PARAM DURATION BASELINE PEAK
```

Numeric `pulse`, `pulsemax`, `envelope` and sweeps trigger on an observed increase. One observed increase triggers one cue, even if a sample skips several units. `statepulse` triggers on entry into a positive state; it remains a finite cue and does not hold for the state's duration. Use `stateenvelope` for that.

Attack/release envelopes use smoothstep, with an optional plateau for `envelope`. Sweeps rise linearly, or with an ease-in quadratic ramp for `easesweep`, and reset to their baseline on completion. Sweeps are useful for temporary speed acceleration; cyclic parameters should use a full-turn cue for a visually continuous reset.

Ordinary pulse/sweep triggers restart their timeline. `pulsemax` instead tracks overlapping cues and takes the strongest peak, allowing a bomb flash to dominate a continuously mapped audio brightness value. Pulse offsets sharing a destination add relative to their common baseline; incompatible baselines are rejected.

```text
advance INPUT PARAM DURATION BASELINE INCREMENT LOW HIGH CURVE
turn INPUT PARAM DURATION BASELINE INCREMENT LOW HIGH CURVE
oscillate INPUT PARAM DURATION BASELINE LOW HIGH CURVE
```

These commands observe cumulative integer increases and queue one sequence for each newly observed unit. Queued sequences run serially. Decreases re-seed the observation without producing a cue.

`advance` retains the increment after each sequence and wraps its settled value into `[LOW,HIGH)`. `turn` makes a temporary excursion, then removes that increment; use a full period for seamless hue/rotation turns. `oscillate` follows a full sine cycle within its limits, starting and ending at `BASELINE`. `CURVE` is `linear` or `smooth` (smoothstep progression through the sequence).

Persistent advances, temporary turns and state-cycle offsets can compose on one destination. That destination has a single authored baseline; contributions are evaluated deterministically, then the declared periodic boundary is applied. Conflicting direct value/selector writers are rejected. A `pulsemax` can explicitly layer over a direct value mapping.

## Lifecycle and timing

`RigPlayer::sample(snapshot, elapsed, active, paused)` uses presentation time in seconds, independent of the scene's authored playback speed. Pausing freezes harness time and preserves outputs. Inactive presentation, a changed game identity or a changed session identity clears transient timelines, observations and accumulated presentation offsets. The first observation seeds counters instead of replaying historical totals.

A session identifies a play run, not a frame or level. The snapshot sequence increases for observations within a session. Game-state snapshots are read-only and do not become save data. Persistent presentation advances are scoped to the session; the scene's original authored state is never rewritten.

The current evaluator bounds a program to 32 behavior statements, 32 derived event rules, 64 total input names and 64 parameter declarations. Counter queues allow 4096 entries; overlapping peak cues keep at most 128 recent starts. Providers should publish current observations regularly rather than replaying unbounded histories.

## Creator and runtime workflow

1. Create and save the scene in Effect Lab.

2. Declare the exported controls and their behavior in NHML.

3. Run the baker. An unknown target or missing resource fails preparation; shader selector alternatives are included in the resource closure.

4. Ship the generated playback asset, NHML, manifest and reachable resources. Keep the editable scene and catalogue in creator tools/source packages.

5. Publish `ReadSnapshot` values from a project adapter. Use an independent WidgetML consumer for widgets.

The baker is an offline creator tool. The game loads prepared playback and a small NHML program; it does not discover presets, build editor panels, validate unrelated graphs or expose authoring controls to players. See [SDK.md](SDK.md) for commands and integration contracts.

## Ordered collections and sprite insertions

```text
collection NAME
emitter NAME NODE
insert COLLECTION EMITTER SPAWN_COUNT MAPPING_COUNT SOURCE_VALUE ATLAS_FRAME [SOURCE_VALUE ATLAS_FRAME ...]
```

Host collection items have positive, monotonically ordered identities and integer values. First availability seeds a high-water mark without replaying existing inventory. New identities produce insertion actions in order; repeated snapshots and disappearing old items do not retrigger them. A session change reseeds. An omitted collection becomes unavailable and reseeds on recovery. Pause defers observation. Limits are 4096 observed items, 16 insertion rules and 256 unique value/frame mappings per rule.

Insertions replace scheduled emissions rather than adding particles. Existing particles keep their frames. Queued runs pause the ordinary base/rare cadence, which resumes afterward. The emitter queue holds at most 4096 spawns; further insertions are rejected without growing memory. Baking validates mapped frames against the emitter. Atlas artwork is compiled from explicit game resource entries; input values remain original game sprite identifiers, independent of packing.

### Pickup-specific speed and live colour

`emitter NAME NODE [INSERT_SPEED COLOUR_MODE]` optionally styles inserted emissions. Speed is a 0.25–8 multiplier, default 1. `COLOUR_MODE` is `none` (default) or `source`. Ordinary base and rare emissions keep their original style. The multiplier follows the particle for its lifetime; it changes motion, not the emission rate or game clock.

With `source`, a collection item's optional colour-source ID is retained by the queued run and each emitted particle. The host publishes RGB values for these IDs separately from inventory membership. Consequently a particle keeps following the game's colour cycle after the collected actor leaves the inventory. An absent colour source falls back to the artwork's ordinary tint. Colour changes update existing vertex attributes in the same batch; no texture uploads, extra passes or replacement particles are needed.

GoatUp2 uses `emitter "items.flight" 24 2 source`. Fruit follows its existing palette index; rainbow pickups follow the game's existing rainbow palette. Both streams are observed read-only. The eight-emission count is unchanged.

## Ordered signed steps

```text
steps COLLECTION PARAM DURATION BASELINE INCREMENT LOW HIGH CURVE
```

Each new collection identity carries -1, 0 or +1. Nonzero values enqueue a signed
increment, preserving order even when opposing turns arrive in the same sample.
Each step lasts DURATION seconds and retains its result; output wraps into
[LOW,HIGH). CURVE is linear or smooth. First availability seeds without replay;
pause defers observation. The queue is bounded to 4096 steps. Like other
presentation cues, these offsets reset on an inactive or new session.

Deflex observes perpendicular velocity changes after each logical update and
reads the deflector cell. Its adapter publishes clockwise as +1 and
counterclockwise as -1, without adding reporting to the game simulation.

## Fade-gated absolute shader selection

```text
fadeindex PARAM INPUT FIRST BASELINE COUNT CHOICE... MASK DOWN UP
```

PARAM must declare unit shader-id; INPUT is an integer. The absolute input
selects CHOICE[(INPUT-FIRST) modulo COUNT]; invalid or earlier values use
BASELINE. On first availability the choice resolves immediately, so continued
games need no historical replay. All shader alternatives are baked and prepared
before playback.

MASK must already have a direct value mapping. Later choice changes fade its
last displayed value to zero over DOWN seconds, switch the shader at zero, then
fade to the currently mapped value over UP seconds. Both ramps use smoothstep.
A changed destination during a transition restarts the fade from its current
value. Pause freezes the transition. This gate applies after ordinary mappings;
it does not rewrite their underlying value or the saved scene.

Deflex uses five choices (12 through 16), with 0.15-second ramps. Its mask maps
ball shell 0..128 to C25.r 0.15..0.305299997, including replenishment.
