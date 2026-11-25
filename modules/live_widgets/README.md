# Live widgets and title presentation

This optional module supplies WidgetML evaluation, vector/raster drawing and the
collection title sequence. It owns no game state, SDL window, graphics context or
iOX navigation. The light synth remains a separate background provider.

`mino_widget_model` is CPU-only. `mino_widget_renderer` adds GLES3 drawing and
`TitlePresenter`, sharing the application's GL loader. Neither library links a
game module or the light synth. Builds that do not use widgets incur no runtime
dependency.

## Building

Collection builds enable `MINO_LIVE_WIDGETS` and its renderer by default.
Disable the option for a shell without the collection title.
Link `minotaur::widgets` for semantics or `minotaur::widget_renderer` for the
presentation service.

For a standalone CPU build:

```sh
cmake -S modules/live_widgets -B build-widgets -DMINO_WIDGET_TESTS=ON
cmake --build build-widgets --config Release
build-widgets/Release/mino_title_sequence_checks modules/live_widgets/assets/title
```

`MINO_WIDGET_RENDERER=ON` also builds the GLES backend using the repository's
glad/stb dependencies. No SDL or font installation is required by the module.

## Startup contract

Load `assets/title/Minotaur-Title.widgetml` from the host's bundled resources and
construct a `TitleSequence`. It begins the intro automatically. Pass the same
resolved orientation to its `Viewport` and the presenter's `RenderTarget`.
The document's `presentation` block sets the supported width-to-height ratios
and the maximum frame rate. Resolve `presentationViewport()` in native pixels,
clear any space outside that rectangle to black, and use the same rectangle for
the background and widgets. Pace the title loop using `maxFrameRate()`.

1. At application startup, call `begin(buttonsHeld, pointerHeld)` with the current
   aggregate input state. Held launch controls must return to neutral before a
   new press can dismiss the title.
2. While the title owns the screen, pass button/key and pointer state exclusively
   to `input()`. A fresh button/key press or pointer-down requests the exit, even
   during the intro. Repeated/held input cannot restart the fade.
3. Call `update(elapsedSeconds, viewport)` once per active frame. Use zero elapsed
   time when paused and exclude time spent suspended or loading resources.
4. Draw the host's Neon background and widgets through `TitlePresenter::render`.
   The callback draws into the supplied target; the presenter applies the authored
   opacity once to the complete composition. Do not also multiply widget alpha.
5. When `takeLaunchRequest()` returns true, stop the title and activate iOX. It
   returns true exactly once, after the exit reaches black. Clear touch ownership
   and require neutral controller input before routing controls to iOX. The press
   used to dismiss the title must not also activate an iOX icon.

The title fades in over 0.8 seconds while its logo zooms from 75% to full size.
The launch prompt cycles WRGBCMY. Exit takes 0.5 seconds. Curves, durations and
composition opacity are authored in WidgetML; the host does not duplicate those
timelines. Dismissing during the intro starts from the currently visible opacity
and does not jump to full brightness.

Keep startup presentation separate from the game's `Core`: direct single-game
builds need not include this module, and returning from a game normally goes
directly to iOX rather than replaying the collection title.

```cpp
using namespace neon::widgets;
TitleSequence title(Value::parse(titleDocumentBytes));
TitlePresenter presenter;
presenter.assets(title, readTitleResource);
title.begin(anyButtonHeld, pointerHeld);

// During the title-owned portion of the application loop:
title.input(anyButtonHeld, pointerHeld);
title.update(elapsedSeconds, viewport);
if (title.takeLaunchRequest()) {
    // Cancel the title's touch capture, gate held controls, then enter iOX.
    enterIox();
} else {
    presenter.render(title, target, drawNeonBackground);
}
```

The shell owns the one-time startup route and `enterIox` action. This module
provides the presentation service and handoff boundary. The application
installs this route only for collection startup.

## Resources and ownership

`ResourceReader` has the same boolean/byte-buffer shape as `Host::readAsset`.
Bind it to the title bundle prefix so names such as
`assets/minotaur-face.nvector` resolve inside that bundle. Vector fonts, images
and vector objects load through this callback; no APK filesystem paths or
temporary extraction are necessary. A filesystem overload remains available to
desktop authoring tools. The reader and any borrowed host must outlive rendering.

The `MINO_TITLE_ASSETS` property on `mino_widget_model` is the exact resource list
for the host bundler. Only the
definition, four logo vector layers and HAMMRF raster atlas are included. Source
SVGs, TTFs, editor fixtures, alternative fonts and unused textures are excluded.
The background callback retains responsibility for its own scene/assets.

Create/render/release the presenter with the owning GLES3 context current.
Call `Renderer::loadGlesEntryPoints` with the application's resolver before
rendering. Before destroying a live context, call `release()`. If a context was
already lost, call `abandonGraphics()` instead; it forgets handles without
deleting unrelated objects in the replacement context. Keep the CPU sequence
alive across graphics recreation to retain its timeline and one-time handoff.

The renderer preserves host graphics state and destination alpha. Widget image
uploads and vector/text layouts are cached; changing a transform does not rebuild
the underlying assets. Render-target/context ownership remains with the shell.

See [font notices](FONT-NOTICES.txt) for font sources.

## Gameplay widget appearance

Gameplay LiveWidgets share these beam settings. All values are percentages.

| Element | Brightness | Halo strength | Halo spread | White-hot core | Beam width | Opacity |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Vectrex headings and labels | 70 | 30 | 120 | 10 | 70 | 100 |
| Readout text | 65 | 30 | 150 | 10 | 60 | 100 |
| Panel borders | 130 | 50 | 180 | 35 | 60 | 60 |
| Other vector marks | 55 | 30 | 175 | 10 | 70 | 100 |

Use the `bezelFrame` template in the
[Gridrunner WidgetML layout](assets/examples/gridrunner-landscape.widgetml)
as the starting point for new game panels. Its four edges use the panel border
beam settings and its black shadow is 35% opaque. Meters and sprites use their
authored opacity and color with 100% gain.

The collection shell loads the game's WidgetML document and samples its
read-only widget presentation state on the LiveSynth refresh cadence.
The renderer warms vector and text raster tiles at the display resolution
during loading. Meter fills and changing values reuse the cached graphics
where possible. Gameplay widgets stay in the letterboxing and inside a centered
16:9 or 9:16 boundary on wider or taller screens.

The in-game iOX menu uses the same renderer and has its own input and graphics
lifecycle. The shell suspends the game while the menu owns input. The
LiveWidgets display option controls the gameplay overlay.

## Gridrunner letterbox widgets

Enable **LiveWidgets** under **Options → Display** and choose horizontal
presentation in modern Gridrunner to keep the 3:4 playfield upright. The overlay fits the usable side
rails inside a centered 16:9 boundary. The checkpoint cards, stats, pickup log
and cooldown stack share the editable [WidgetML layout](assets/examples/gridrunner-landscape.widgetml).
The sample inputs exercise the full layout, including six concurrent timers.
The app supplies live inputs through the core's read-only presentation snapshot.

In upright portrait, the shared `portraitLayout` template places the checkpoint
stack and run stats above the playfield. It reserves five pickup rows below the
playfield and aligns two cooldown rows to that panel. The grid fills one column
at a time from the right, with no drawing for empty cells. The overlay uses the
actual gameplay bounds and stays inside a centered 9:16 region on taller screens.

The pickup ring has its halo baked into the sprite
alpha so the live color tint also colors the halo in one draw.

The loading screen is presented before the cached renderer prewarms vector beams,
glyphs and the radial meter mask at the final widget pixel size. A resize or
graphics context recreation can rebuild the artwork cache. Gridrunner samples
widget inputs and evaluates its layout on the same frames LiveSynth refreshes.
When LiveSynth reuses a frame at 30 Hz, the overlay composites its previous
widget draw list over it. The collection title uses its existing rendering path.

## GoatUp letterbox widgets

In landscape, the [GoatUp layout](assets/examples/goatup-landscape.widgetml)
scales to the side rails. High Score and Run Stats anchor the left side.
Flight, Mushroom, Protection and Gestation use independent cards that stack
only while active. The Progress panel tracks the current camera elevation in
both directions, with ticks at the tower's terrain changes.

Slow Down uses a draining bar based on the remaining climb-speed recovery.
When five cards overlap, both orientations show the four latest activations.

In portrait, records and run stats sit above the playfield. The horizontal
progress meter below it uses the same terrain ticks and a goat marker that
faces the direction of travel. Active cards share the landscape templates and
pack into a row beneath the meter. The portrait layout stays within a centered
9:16 region on taller screens.

GoatUp uses the same loading-time cache warm-up and LiveSynth refresh cadence
as Gridrunner.

## Super Ox Wars letterbox widgets

The [landscape layout](assets/examples/superoxwars-landscape.widgetml) combines
saved records, baked area target totals with live clears, a polarity readout,
active combo cards, area progress, and five follower slots. The empty portions
of the bars use a translucent track. Polarity glyph tints are warmed at load
and the widgets refresh with LiveSynth.

In portrait, High Score and Area Targets stack above the playfield next to
Area Bests. Polarity, the radial boss meter and Followers sit below it.
Active chain cards pack into their own row underneath. The layout stays
within a centered 9:16 region on taller screens.

## Five a Day letterbox widgets

The [landscape layout](assets/examples/fiveaday-landscape.widgetml) places
records and fruit tracking on the left, and firepower, formation bonuses, and
damage stats on the right. Fruit Trail uses the current stage's scripted fruit
total. The Five a Day slots use the game's own fruit sprites as each clear is
counted. Lifetime totals cap and turn green at their achievement thresholds.
Widget caches warm at load, and updates follow the LiveSynth refresh cadence.

In portrait, scores and run stats sit above the playfield. Formation Bonus,
radial Power and Fruit Trail form the first row below it, with Five a Day and
Lifetime in the second row. The 2x Points badge fits inside Formation Bonus
while both are active and has its own card when only the multiplier is active.

## Caverns of Minos letterbox widgets

The [landscape layout](assets/examples/minos-landscape.widgetml) shows records by
ship, the score multiplier breakdown, cavern and game rescues, the last awarded
landing grade and current S streak, and clean-run status. The level bonus uses
the game's rescue, fuel and mission time rules. The current depth marker uses
the map's surface and objective rows, and the par meter turns red at expiry.
Widget caches warm at load and updates follow the LiveSynth refresh cadence.

In portrait, records sit above gameplay on the left, with the multiplier and
rescues on the right. Clean Run and Level Bonus sit below gameplay, next to
Landing, Mission Time and Par Time. Level Bonus stays in place when Clean Run
expires.

## GoatUp 2 and Minotron letterbox widgets

GoatUp 2 shows placed objects, minotaurs, tail size and active effects.
Its counters move to the lower left in portrait. The cooldowns remain on
the right. See the [GoatUp 2 layout](assets/examples/goatup2.widgetml).

Minotron shows stage records, next extra life, weapon, beasties, pickups and
effect timers. Landscape uses narrow score readouts with compact millions
above six digits. Portrait shows full scores and uses vertical pickup expiry
bars. See the [Minotron layout](assets/examples/minotron-landscape.widgetml).

## Deflex and Minotaur Rescue letterbox widgets

Deflex places level scores on the left and bats, shell and the active chain
timer on the right. In upright portrait these two groups sit above the
playfield. See the [Deflex layout](assets/examples/deflex.widgetml).

Minotaur Rescue uses the current single-player Solar or Deep Space leaderboard,
including the Survival variants. Solar adds sun stability, minotaur losses and
rescue efficiency. Efficiency compares actual points from rescued minotaurs with
the sum of their theoretical center values. In portrait, the bottom cards form a
centered row that adjusts to the visible cards.
See the [Minotaur Rescue layout](assets/examples/minorescue.widgetml).

## WidgetML

WidgetML is formatted JSON with `widgetML: 1`, optional resource/font/template
definitions and a root group. Its bounded expressions cannot execute scripts or
provide shaders. It describes widgets independently of light-synth oscillators
and event envelopes.

## Coordinates

The root origin is screen center, after the host's resolved presentation turn.
Positive X is right and positive Y is down. `coordinates.shortEdge` defaults to
1000: that many units span the shorter oriented screen dimension. Both axes use
`min(width,height)/shortEdge` pixels per unit. Aspect changes reveal or crop more
of the longer axis; they do not stretch or contain-fit the layout.

`x`/`y` are offsets from the parent's origin. `size` allocates a local box, `pivot`
selects a fractional point in that box and `scale`/rotation transform its contents.
Children without explicit placement remain local to their group.

Explicit placement uses `placement.origin` for a shared origin and optional
`placement.landscape` / `placement.portrait` overrides. Screen origins available
in either mode are `screen-center`, `screen-left`, `screen-right`, `screen-top`,
`screen-bottom` and the four `screen-top-left` / `screen-top-right` /
`screen-bottom-left` / `screen-bottom-right` corners. `gameplay-left` and
`gameplay-right` are landscape-only overrides, using the actual gameplay
rectangle's horizontal edges and screen-center Y. They require host gameplay
bounds. Square screens choose the landscape branch.

An explicit nested placement reanchors its origin in screen space; its offsets
still follow its parent's scale and rotation. Omitting placement preserves the
local origin. An explicit placement with no matching override or default uses
screen center. Offscreen content is clipped at the viewport rather than moved or
shrunk. `region: "bezel"` additionally excludes gameplay from widget drawing.

For a bottom-locked label:

```json
{
  "id": "legal",
  "kind": "text",
  "placement": { "origin": "screen-bottom" },
  "y": -18,
  "size": [900, 40],
  "pivot": [0.5, 1],
  "align": "center",
  "fontSize": 24,
  "text": "Legal text"
}
```

Hosts supply display-local gameplay bounds, without pre-rotating them. The
runtime maps them into oriented coordinates using the same turn as rendering.

## Objects and behavior

Groups contain groups, visuals, logic and sequences. Visual kinds are vector,
sprite, text, rect and radial. Object IDs are unique among siblings. Group transform,
opacity and color apply to descendants; explicit RGB inheritance can be disabled
while retaining ancestor opacity. Visible=false hides drawing but retains logic;
enabled=false suspends the subtree. Repeat groups instantiate a bounded
collection by stable key with row/column/grid or absolute layout.

A `radial` visual draws a segmented ring as one object. It uses `size` and
`segments` (1–64, default 16) to select a cached mask. The `fill` expression
sets the clockwise portion lit from twelve o'clock, `color` sets the lit
color, and `trackColor` sets the unlit RGBA color. Changing the fraction or
colors reuses the mask.

Groups declare inputs, settings/parameters, local logic and outputs. Inputs are
evaluated in the parent's scope. Local `get` references do not silently access
another group's internals. Shared game observations enter through `read` and
never mutate game state. The reserved local values are `time`, `state` and
`stateTime`. Group outputs cross the parent boundary on the next completed
update; the host can inspect the current frame's published outputs directly.

Expressions are prefix arrays: `get`, `read`, `output`, `event`, `entered`,
arithmetic, comparisons, boolean operations, `if`, `coalesce`, `clamp`, `lerp`,
`normalize`, `count`, `smooth`, `increased`, `decreased`, `changed`, `crossAbove`
and `crossBelow`. Missing observations remain unavailable rather than becoming
zero. First reads establish edge baselines. Exact integer counters are retained
until an operation requires floating point.

Ordered group transitions select at most one destination per update. Animation
tracks target x/y, scale, rotation, color, opacity or fill with ordered keys and
linear/smooth/in/out interpolation. Finite tracks return to their current baseline
unless completion is hold. Loops repeat explicit tracks. Sequences emit timed
local events; sprite flipbooks support loop, ping-pong and finite playback.
Local emissions arrive next update, avoiding recursive evaluation.

The title publishes its composition opacity through the root output. Its host
applies that value once after background/widget composition, rather than
reimplementing the fade curve or scheduling iOX inside the widget model.

## Rendering and limits

Text objects use `"kind": "text"` and select a font by name with `font`. The
built-in IDs are `neon-hud` (the default) and `vectrex-menu`. A text object can
also select a font declared in the document's `fonts` object. Use a literal
such as `"font": "vectrex-menu"` or bind it to a template input with
`"font": ["get", "headingFont"]`. The cached renderer keys its raster glyphs
by the resolved font and pixel style, so changing only the text reuses its
glyphs. Raster fonts use a declared atlas with cell/advance/line-height metrics.
Assets are bundle-relative
resource names. Color is RGBA. Fills reveal/crop local bounds rather than scaling
artwork. Groups support rectangular clipping; rotated clips use transformed
axis-aligned bounds.

Documents are limited to 1 MiB, 256 expanded authored objects, fewer than 12
nested levels, 512 evaluated instances/draws per update, repeat maximum 64 and
16,384 expression steps. The backend limits images to 2048 pixels per axis,
32 cached textures and 32 MiB decoded texture storage. Invalid data is rejected.

Legacy `widgetVersion: 1` canvas documents remain readable by the evaluator, but
cannot mix their reference canvas/normalized anchors with WidgetML coordinates.
