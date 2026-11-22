# Live bezels

The title and gameplay bezels share `mino_neon_background`, its effect
implementations and the presentation resource archive. Game cores do not link the
Light Synth renderer. The host presentation service owns each effect instance.

Fixed player assets are prepared offline as `.neonb`; editable `.neongles`
sources remain in the creator/source distribution. The runtime loads only the
prepared graph and reachable resources, rather than constructing an authoring
catalogue. Rigging is defined in [NHML (Neon Harness)](NHML.md), with named inputs,
units, exported controls, passive mappings and finite cues. See [SDK.md](SDK.md)
for preparation, integration and device-local shader caching. Historical
`.neonrig` source files remain importable; shipped profiles use `.nhml`.

In the game's options, choose **LiveSynth** under **Letterboxing**. All collection
games, including Gridrunner Classic, have live profiles. Black letterboxing
avoids creating or updating the effect. Plasma remains a separate backdrop.
Single-game builds retain their existing lightweight presentation path.

## GoatUp profile

| Read value | Existing state | Units |
| --- | --- | --- |
| `player.x` | Projected goat screen X | Native screen center = 0; boundaries = -0.5 and +0.5 |
| `screen.elevation` | Tower scroll minus its starting -0.2 offset | One native gameplay-screen height per unit; decreases during return |
| `kids` | Current goat kid count | Exact unsigned count; decreases do not fire birth cues |
| `billyGoats.kissed` | Current run Billy Goats kissed counter | Exact unsigned count |

The adapter's const `Core::presentationState()` returns owned, typed values. It
neither advances the game nor changes counters, physics, input or RNG. Other
cores can expose additional named values without changing the renderer or the
snapshot interface. A session number changes when the adapter creates a new
world; the first sample seeds event baselines instead of triggering old gains.

| Destination | Mapping |
| --- | --- |
| Layer 6, node 25, UV Y offset | `wrap(-0.506909966 + 0.5 * screen.elevation, -1, 1)` |
| Camera yaw | `clamp(-1.6 * player.x, -0.8, 0.8)` radians |
| Global hue | Kid increase: one linear 0° → 360° cycle in one second |
| Global hue | Billy Goats kissed increase: smooth 0° → 100° → 0° over two seconds |

Elevation is absolute camera elevation, not player height or highest-ever height.
The saved offset is the starting anchor. The two-unit UV wrap matches mirrored
texture repeat, including negative values and descending crossings. All value
rigs can select wrapping or clamping; destination ranges are checked against the
actual effect parameter descriptors.

The hue cues have their own host presentation clock. Their durations are in gameplay
seconds regardless of the scene's saved 0.54 playback speed. Game pause freezes
both the effect and the envelope. Another increase of the same counter restarts its cue; decrease,
new-session baselines and unavailable gameplay do not produce false gains.

## Minotaur Rescue profile

`MinotaurRescue.neongles` contains the authored layered field, using a curated
preset with the original node numbering. Material holds are stored in that
preset; active controllers remain intact. Only the scene's required texture and
shader resources are included. `MinotaurRescue.nhml` supplies the live mappings.

| Read value | Meaning |
| --- | --- |
| `player.orientation` | First human player's visible heading, radians in [0, 2π); lowest surviving formation slot in Jets |
| `player.fireBoost` | Collected-minotaur firing boost, normalized 0–1; ships saturate at weapon 13, tanks when their 80-tick reload reaches the 20-tick minimum; Jets have no corresponding boost |
| `level` | Current zero-based level; an increase advances hue 50° in one second and holds |
| `minotaurs.rescued` | Existing rescued counter in ship modes; presentation-owned accepted-pickup counter in Tanks |
| `center.contacts` | Confirmed human-player contact with the collapsed central gravity object |
| `player1.hits`, `player2.hits` | Accepted Tank/Jet hits, including Jet rebuilds and lost lives while gameplay continues |

Layer 5/node 17 primary UV rotation follows heading one-to-one, wrapping at 2π.
The radial material uses a complete mirrored source-texture repeat at its angular
join. This changes texture repetition, not the output composition or framing.
Layer 4/node 14's secondary UV width and height share channel 3; its clock runs
at 25–100% of the saved speed. Speed changes integrate into phase instead of
seeking the cycle. Overall scene playback remains at its authored 1.99 multiplier.

The starting hue is the scene's 60° shift. Level advances and one-second rescue
turns add together. Central contacts and player-one hits turn node 14's primary
UV rotation +2π with smoothstep timing; player-two hits turn node 4 by −2π.
Each turn takes one presentation second and ends at its authored baseline.
Repeated counter increments queue separate turns, including several pickups
between presentation frames. Pause freezes queues; a new session clears them.

The core adapter inspects player entities before and after each simulation tick.
An optional collision-wrapper observer identifies exact central contacts and
accepted Tank pickups after the original collision response. It only updates
adapter-owned counters. Game rules, collision results, RNG and saved statistics
are unchanged. Menus and attract mode do not create gameplay cues.

Counter cues extend `NEONRIG 1` with:

```
advance "counter" "target" duration baseline increment low high linear
turn "counter" "target" duration baseline increment low high smooth
```

`advance` retains each increment; `turn` returns after its duration. Both wrap
the resulting destination into `[low, high)`. Curves are `linear` or `smooth`
(smoothstep). Cues sharing a target must share its baseline and wrapping range.
For rotation, increments are radians; global hue uses turns. A missing counter
reseeds its observation when it returns, without synthesizing missed events.

## Profile resources

`assets/presentation/bezels/GoatUp.neongles` is the imported runtime scene.
The source scene's mesh resolution, texture/material choices, UV holds, camera
position, exposure and playback speed are retained. The inactive cube/component
workspaces and unused controller-editor records are omitted. The source document
is not modified. Higher tunnel face counts use the same recovered profile
anchors; the original eight-face path remains available.

`GoatUp.nhml` defines live behavior independently of the scene. Its named exports
cover height offset, camera yaw, global hue and the background colour shader.
Passive mappings follow position and elevation; finite cues handle kids and
Billy Goat kisses; persistent hue advances follow zones; a state selector and
held cycle handle mushrooms. See [NHML.md](NHML.md) for the language and the
profile itself for the exact ranges.

Live values enter the existing response-input path, leaving authored control
baselines intact. Global hue is applied at final presentation, including during
frames without a synth tick. This avoids regenerating geometry or textures for
a color-only cue. WidgetML is not involved in these rigs.

After editing presentation resources, bake the scenes, bundle their reachable
resources and rebuild, following [SDK.md](SDK.md). Bundling checks the prepared
assets against their sources and records the payload checksum in
`assets/baked/MANIFEST.json`; runtime assets stay embedded.

## Composition and lifetime

The display passes its resolved game orientation and exact gameplay rectangle.
The effect rotates with gameplay and composites only outside that rectangle.
Intermediate passes remain complete so blur and feedback dependencies are safe.
Final presentation covers the native destination. LQ uses half-resolution effect
composition; HQ uses full resolution. Android/iOS default to LQ and desktop to HQ.
The gameplay image and widgets retain their independent rendering settings.

The service initializes lazily only when a supported game has visible Neon
letterboxing. GPU resources are released before context destruction, or abandoned
on context loss and recreated in the new context. Cosmetic feedback restarts
after context loss; game state and live rig observations remain independent.
Missing resources or shader errors leave Neon black for the current instance.
The service records the failure instead of retrying initialization every frame.
A fresh instance can retry; Plasma is never substituted for Neon.

Build with `MINO_LIVE_BEZELS=OFF` to omit the gameplay service. It can also be
built with `MINO_LIVE_WIDGETS=OFF`: the shared vector primitives remain available,
while WidgetML/title semantics are not linked into the live-bezel service.

## Super Ox Wars

The read-only adapter publishes `player.followers`, `world.tilt`, raw
`score.red`/`score.blue` and complementary score shares. The shares use
red/(red+blue) and blue/(red+blue), with 0.5 each before either side scores.
The native scene preserves the saved layers and nodes; the selected radial node is presented as a snapshot taken
before subsequent colour processing. Completed feedback stacks remain unchanged.

`clock.20.6.speed` maps 0..5 followers to 1..4 times the authored cycle rate.
`colour.7.red` and `colour.10.blue` map each score share from 0..1 to 1.25..3.
`node.20.f.2` maps resolved tilt -1..1 to -0.2..0.2. Its `smoothvalue` rig uses
clamped smoothstep, t*t*(3-2*t), giving zero slope at both extremes. Like `value`,
`smoothvalue` specifies target, source, input endpoints, output endpoints,
boundary mode and final limits. Ordinary value rigs remain linear.

Node 20's second UV scroll retains its -1..1 range and saved period. Texture-width
scrolling makes the boundary displacement exactly two texture repeats in every
aspect ratio. Rate changes preserve phase; changing power does not restart the cycle.

## GoatUp zone and mushroom cues

`zone` reads the game's zone indicator; increases advance hue by 50 degrees over
0.5 seconds. Descending does not undo a reached zone's hue step. `mushroom.active`
reads the existing poison timer, without changing gameplay. It selects shader 46
(Radial Colourise) on node 22; expiry restores shader 12.

`cycle "mushroom.active" "runtime.hue" 2 1` adds one hue turn every two seconds
while the boolean is true. A cycle specifies source field, destination, period in
seconds and offset per cycle. It starts at zero on activation and drops its offset
on release. Cycles pause with gameplay and reset on a new run. The zone baseline
and existing event cues remain independent, so release returns to the current
zone hue, including any still-playing event cue. Counter and pulse cues may share
a destination when they use the same baseline.

## Gameplay texture input

Selector 202 represents the host's current native gameplay texture. The display
passes its already-rendered game surface to the optional bezel service before
presentation filters and composition. Five a Day's node 8 first texture input (Lab texture input 1) uses this
input; its existing feedback passes retain and transform the game image.

`Runtime::setGameplayTexture(texture, width, height)` borrows an existing GLES
texture for subsequent draws. The host owns and keeps it alive through the draw,
updates the binding after surface replacement and supplies a valid texture from
the current graphics context. Zero selects an opaque black fallback. The renderer
never deletes the supplied texture and uses its own sampler, preserving the host's
filter/wrap settings. Sampling flips the framebuffer's vertical axis to the synth's
image convention; the effect and game are rotated together at final composition.

This adds no gameplay render, CPU readback, full-size texture copy or composition
pass. The game surface already exists. GPU resource transitions and sampling
bandwidth still need device measurements. Feedback cost remains scene-dependent.
The source excludes CRT/scanlines, the bezel itself and later widget overlays.
The gameplay module does not depend on the bezel API.

Five a Day's `player.rotation` snapshot reads the ship's displayed angle in radians
and maps it directly to `node.8.f.9` (second UV rotation). Feedback samplers retain
their own previous stack by default; an explicit `input.<node>.0` source can override
that first sampler. The game's display texture is refreshed by the host each frame.

For shader 68, `sampling.<node>.0.scrollspace` moves horizontal scroll after the
nonlinear distortion. The mode is opt-in; the original shader remains unchanged
when disabled. The mirrored range -1..1 spans two texture widths and is canonicalized
at the boundary to avoid feedback amplification of floating-point endpoint differences.

## Five a Day event and trail cues

The presentation observer counts the original `bossDefeated` rising transition as
`levels.completed`. It detects minotaur fruit changing into Collected state as
`minotaurs.rescued`. These counters belong to the presentation session; they do
not wait for lifetime records to be banked at game over. `fruit.trail` counts
currently attached Collected fruit, including minotaurs, and excludes dead or
scattered fruit. The brightness harness maps 0..20 items to 1..2 and clamps there.

`oscillate "levels.completed" "node.12.i.4" 1.5 174 0 255 smooth` runs one finite
full-range slider cycle, starting and ending at its authored baseline 174. It
visits the upper and lower limits continuously, using a sine wave with smooth
phase progression. Its arguments are event counter, target, seconds, baseline,
lower limit, upper limit and linear/smooth phase curve. Unlike a hue turn, this
scalar excursion does not jump from its upper limit to its lower limit.

Each rescue turns `colour.8.hue` through 360 degrees over one second and returns
to its baseline 120 degrees. Rapid rescues queue complete turns. Pause freezes
cues and a new run clears the observer/cue state. Existing image input and player
rotation bindings remain independent.

## Gridrunner profile

The enhanced game's adapter samples existing ship coordinates, power timers and
lives. It adds no simulation callbacks. Classic retains its separate presentation.
The Gridrunner plane blends its cached beam-derived texture with the animated
base each frame. The component scene retains its two editable saved-layer
sources; live values do not replace their authored definitions.

| Destination | Source and behavior |
| --- | --- |
| Base source / Node 8 scrolling clock | Slow power timer active multiplies the saved 0.75 speed by 0.5, yielding 0.375; expiry restores 0.75 without seeking |
| Grid plane yaw | Ship X -0.4..0.43 maps to -0.2..0.2 radians |
| Grid plane pitch | Ship Y -0.228..0.428 maps to +0.3..0 radians (top to bottom); unavailable position uses 0. Zero pitch with centred yaw/roll gives a screen-aligned plane |
| Base source / Node 18 shader | RGB-BGR (13) while three/four-way shot timers are active; restores saved shader 14 |
| Base source / Node 18 brightness | All-around state rises smoothly 1..2 in 0.5 seconds, holds at 2 while active, then returns in 0.5 seconds on expiry |
| Global hue | Each increase in lives turns hue once over one second, then returns to baseline |

`layer.<component-id>/` prefixes address values inside a saved source. Prefixes
can nest within the existing four-source budget. The host installs live-input
routes and removes their overrides when unavailable; it never rewrites the saved
layer document. Source clocks integrate speed changes continuously. Unrelated
inputs do not invalidate a cached static source.

`statepulse` is the boolean rising-state form of `pulse`; it uses an EnterState
read rule, not a new game event. Numeric `pulse` continues to observe increases.

`stateenvelope "field" "target" attack decay baseline peak` follows a boolean
read state. Smooth attack and release times are in seconds; the peak is held
while the state is true. Interrupted transitions start from their current value,
pause freezes the envelope, and a new session clears it.

## Minotron audio brightness

The application analyzes the float stereo mix already supplied to SDL. Each audio
block publishes its normalized RMS (0..1) through an atomic value, sampled as
`audio.level` each presentation frame. It precedes device volume adjustment and
requires neither audio capture nor a game callback. Silence maps to brightness 0.7,
full-scale RMS to 1.9. Bomb flashes take the maximum over this baseline and can
reach 3. Pause/session handling follows the existing rig rules.

Game-state observers read existing fields through const references. Scene
destinations and audio analysis remain outside the game engine.

## Layered Gridrunner quality and Classic

HQ Gridrunner profiles compose at native destination resolution with a
1024-pixel base producer. LQ profiles use half-resolution
composition and a 512-pixel base producer; the grid producer remains at its
authored 1024-pixel bound. These bounds limit mobile fill rate and memory use.
Copy elision remains enabled at both quality levels. Larger source targets
improve sharpness but increase memory and source shading cost.

Non-feedback mixer layers render directly to their own targets when no sampler
reads the destination. Feedback, spatial, legacy-framing and node-inspection
paths retain scratch copies. Component compositions without stack feedback use
the completed scratch image directly. Static vector rasterization remains
cached independently of animated blends. The copyElision parameter provides a
conservative comparison path for diagnostics.

Gridrunner Classic shares the scene, with a 300-degree global hue baseline.
Ship cell coordinates X 1..21.9 drive yaw -0.2..0.2 and Y 8..18.9 drive pitch
0.3..0. The host reads the existing marked floating-ship VIC screen record only
while Playing; no simulation reporting hooks are added. Unavailable positions
restore the authored neutral pose. Classic has no enhanced power-up or hue-event
rigs.

## Caverns of Minos

The absolute cavern number selects wall roots 22, 25, 27, 15, 11 and 5, then
repeats with an additional 50 degrees of hue per completed group. This also
handles direct starts and continues. Map-camera scroll drives second UV X in
both directions. Rescues turn the material's UV Y once over one second; level
completion rotates the barrel axis through 360 degrees over two seconds.
Already capped game gravity eases the pipe centre between 0.4 and 0.6.

## GoatUp 2

The read-only inventory observer publishes newly collected sprite identities.
Each pickup pre-empts eight scheduled star emissions at twice ordinary flight
speed. Inserted particles can follow the game's live palette; ordinary star and
rare-object sequences resume afterward. No additional particle population or
per-pickup texture upload is required.

Normalized hunger drives Node 11 C25.g across 0..1. Saturation falls from 1 to
0.25 below half hunger. Invincibility selects 0.4 for Node 32 C25.r and Node 31
green and cycles global hue once per second; expiry restores authored values.

## Deflex

The host observes velocity and deflector cells before and after each logical
update. Perpendicular changes become ordered signed steps; ordinary wall
reversals do not. Flipping deflectors are resolved from their pre-flip orientation.
Horizontal motion on a slash turns counterclockwise and on a backslash clockwise;
vertical motion uses the opposite directions. Both motion signs use the same
rule. Node 11 UV rotation takes 0.5 seconds per quarter turn and retains the result.

Remaining ball shell, including replenishment, maps 0..128 points to Node 18
C25.r 0.15..0.305299997. Ball-free menu/result scenery is not sampled as a ball.
The absolute level selects Node 20 shaders 12 through 16 cyclically. Later level
changes fade C25.r to zero over 0.15 seconds, switch the shader, then restore the
current shell-driven value over 0.15 seconds. First availability resolves the
correct shader immediately without replaying earlier levels.

These profiles use existing game state and host-side observations. They do not
add reporting counters or change the reconstructed game rules.
