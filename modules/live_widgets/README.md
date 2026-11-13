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
sprite, text and rect. Object IDs are unique among siblings. Group transform,
opacity and color apply to descendants; explicit RGB inheritance can be disabled
while retaining ancestor opacity. Visible=false hides drawing but retains logic;
enabled=false suspends the subtree. Repeat groups instantiate a bounded
collection by stable key with row/column/grid or absolute layout.

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

Vector/text objects select a built-in or declared vector font; raster text uses
a declared atlas with cell/advance/line-height metrics. Assets are bundle-relative
resource names. Color is RGBA. Fills reveal/crop local bounds rather than scaling
artwork. Groups support rectangular clipping; rotated clips use transformed
axis-aligned bounds.

Documents are limited to 1 MiB, 256 expanded authored objects, fewer than 12
nested levels, 512 evaluated instances/draws per update, repeat maximum 64 and
16,384 expression steps. The backend limits images to 2048 pixels per axis,
32 cached textures and 32 MiB decoded texture storage. Invalid data is rejected.

Legacy `widgetVersion: 1` canvas documents remain readable by the evaluator, but
cannot mix their reference canvas/normalized anchors with WidgetML coordinates.
