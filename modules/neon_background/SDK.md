# Neon creator and playback integration

Neon has two asset contracts: editable `.neongles` scenes plus [NHML](NHML.md), and fixed `.neonb` playback assets. WidgetML is a separate optional presentation consumer; sharing a read-only snapshot does not couple their semantics or enablement.

## Preparing a scene

Build the excluded creator target `neon_scene_bake` with the optional Neon module enabled. It does not require a graphics context and is not part of the player executable package.

```text
neon_scene_bake DATA_DIRECTORY INPUT.neongles OUTPUT.neonb [INPUT.nhml]
```

The tool resolves the graph and original controllers, preserves the authored animation baselines, validates NHML destinations, exports only the requested parameter registry, and recursively prepares reachable source layers. Its adjacent JSON manifest lists required shaders/textures and symbolic parameter exports. The playback reader does not need source presets, editor descriptors or routing TSVs.

For this collection, run `tools/bake_presentation_scenes.py --baker PATH_TO_BAKER`, then `tools/bundle_presentation_assets.py`. The bundle step validates source, harness, prepared-data and resource SHA-256 hashes. Stale or missing prepared data fails bundling instead of silently shipping an outdated scene. Editable source assets remain in the repository and Lab/SDK content; the embedded player archive contains the prepared closure only.

## Playback

`Runtime::loadBaked(stream, dataPath, assetReader, &rate)` creates independent mutable playback state from prepared definitions. It requires no GL context or preset catalogue. `responseParameters()` returns the prepared export registry. The host applies an NHML `RigPlayer` to read-only snapshots and supplies the resulting values through the renderer's response bindings. The game integration is in `platform/live_bezel.cpp`; the host adapter does not issue renderer commands from simulation code.

Create/load CPU state on the owning presentation thread. Call `frame`, release GPU resources and manage borrowed gameplay textures only with the renderer's GLES 3 context current. `abandonGraphics()` handles context loss without issuing GL deletion calls. A subsequent frame reconstructs GPU resources from prepared CPU state. Nested layers retain their prepared documents, so reconstruction does not rebuild the authoring catalogue.

The prepared format is versioned, fixed-endian and bounded. It contains portable playback data, not C++ object memory or device-specific GL handles. Unknown versions, truncated data, invalid numeric values and nesting over budget are errors. Renderer/compiler changes that alter the binary layout require a format revision and rebake.

## Device-local program cache

`setShaderCacheDirectory(path)` enables a disposable cache of GLES program binaries. The renderer checks binary support, keys entries by complete shader sources and GPU/driver identity, and verifies link status after loading. Missing, unsupported or rejected binaries fall back to source compilation. The cache is not packaged or distributed between devices; it may be deleted at any time. Scene baking remains portable and independent of that cache.

The application places cache entries under its writable data directory. No writable filesystem is required by the renderer when caching is disabled or storage is unavailable. GPU program objects remain owned by their contexts; only binary data persists. Driver/OS updates or shader changes may cause a new first-use compile.

## Quality and separation

Scene preparation does not raise render-target resolutions or change clocks, feedback, geometry, sampler addressing, hue or rigging. Mobile retains the approved half-resolution final composition and 512-pixel visible base for Gridrunner; desktop retains its sharper profile. Runtime quality policy is independent of the baked source definition.

The game can build without live bezels. LiveSynth is the default letterboxing
choice when no preference is saved. The compatible INI token remains `neon`;
explicit saved choices are retained. Plasma is an explicit separate backdrop choice. Neon uses black while unavailable or preparing and never displays Plasma as a loading substitute.

## Presentation quality and cadence

The collection host’s [performance policy](../../platform/PERFORMANCE.md) describes Android submission pacing and its separation from effect update cadence.

`neon_quality=hq|lq` selects the player's resolution profile; Android/iOS default to LQ, desktop to HQ. It is independent of cadence. The launcher enables LiveSynth Quality only for LiveSynth letterboxing. An initial desktop title-screen assessment can choose LQ before a quality choice exists; it never changes an explicit preference. An early exit leaves HQ as the desktop default and does not repeat the test.

Cadence is assessed once during the existing loading presentation, using CPU frame-work duration and observed presentation intervals. No GPU timing queries are issued. The shared startup presentation notifies the host when the hold ends; Classic uses its existing launch transition. The host finalizes at that boundary, or at a one-second deadline, without extending loading. Four warmup samples are discarded; at least twelve valid observations are required. A 90th-percentile CPU work time above 14.5 ms or more than 20% of intervals above 22 ms selects 30 Hz; otherwise the initial 60 Hz assumption is retained. An inconclusive window retains 60 Hz. This is a coarse bezel assessment against the loading workload, not a complete gameplay benchmark.

The result is locked for the run. Ordinary gameplay collects no performance samples, and orientation/viewport changes do not restart assessment. Temporarily having no visible letterboxing preserves the choice and performs no bezel rendering. HQ/LQ remains exclusively player controlled after the initial desktop title assessment. A new game launch starts a new loading assessment. Direct launches with no loading presentation retain 60 Hz.

The pacer counts game presentation frames. Reduced cadence updates exactly every other frame, irrespective of elapsed time or display refresh rate. A late frame never changes that schedule or adds GPU catch-up work. On a 60 Hz host this produces 30 Hz; if the host slows, the bezel remains at half its rate. Both cadence choices use `frameSampled` to advance clocks and render each graph once per update.

Skipped updates use `presentCached` to recompose existing completed targets. No additional full-screen history allocation is required. On a subsequent update, `frameSampled` advances authored clocks and renders each nested graph once. It does not replay missed GPU feedback steps. Thus motion clocks retain their rate, but feedback trails/accumulation may look different at reduced cadence; arbitrary shader feedback is not automatically converted to a continuous-time equation. Widgets and game simulation do not share this cadence control.

## Authored shader additions

Shader IDs 92 and 93 are the concave and convex one-axis barrel variants. C25 R controls strength, G rotates the axis from horizontal to vertical, and B/A set the UV centre. They sample input 1 only and add no render passes. Prepared playback revision 2 carries their metadata; the reader retains revision-1 compatibility. Existing fixed scenes do not use them, so their shader assets are excluded from this collection's player bundle until a scene requires them.

## Exclusive presentation handoff

`Destination::preserveHostState` defaults to true. General embeddings retain their existing state-save/restore contract. An exclusive presentation host can opt out for a top-level pass, provided it establishes the next pass's graphics state explicitly. This skips the top-level query-based snapshot and restoration; it is not a cached snapshot of earlier game state. Nested layer passes still preserve their parent state. Pixel-unpack state is normalized before any uploads in either mode.

The collection uses this opt-in only in the host's live-background callback. Display composition then restores its VAO/attribute enables, scissor, depth/stencil/raster/sample enables, colour/depth masks, blend functions/equations, sampler/texture bindings, renderbuffer binding and active texture unit explicitly before its game-image/filter pass. Context replacement recreates programs and their cached uniform locations. No game simulation changes are involved.

An exclusive host which draws the opaque game image after Neon can also set `Destination::clearDestination`. This clears the output viewport once, including alpha, after preparing the offscreen effect graph. The subsequent bezel strips skip their individual clears. The default is false: SDK hosts which have already composed the game retain its excluded pixels. State preservation and whole-output clearing are independent opt-ins.

Single-surface barrel shaders 94/95 separate shape coordinates from material UVs.
Their strength/axis/centre use C25; width/height use C26 R/G (log2 scale: 0=1x,
1=2x, -1=half). They require prepared playback version 3, which extends the shader
metadata from 94 to 96 entries. The reader retains versions 1 and 2. Existing
barrel shaders 92/93 and their saved appearance remain unchanged. Applying a
barrel before a later tiling pass still repeats its completed image; place the
barrel after material tiling when one continuous final surface is required.

Native layer material addressing uses the authored reference tile before mapping
into an expanded target. This preserves rotated repeat/mirror/clamp phase across
aspect changes. `sampling.NODE.INPUT.3` chooses automatic (0), reference material
(1) or extended scene surface (2). Automatic retains full-scene behavior for copy
shader 26. This is a renderer correction; no additional render passes or targets
are allocated. Root/imported texture addressing and host display rotation remain
separate. A whole-scene surface still has finite rendered coverage; a later pass
can repeat that completed image if requested by its sampler.

Surface-filter geometry uses unwrapped reference coordinates in the vertex stage,
independently of transformed material UVs. Aspect expansion therefore reveals
more of the same warp instead of scaling its curvature to the new viewport.

## Reference units and scene coverage

The authored reference frame defines scale, not a repeated image boundary.
Automatic flat composites consume the extended scene surface. Mesh materials
and projected tunnel shaders retain reference-material sampling. Explicit input
domains override that policy: 1 is a reference tile, 2 an extended surface,
3 extended U/reference V, and 4 reference U/extended V.

Native producers render the bounds requested by their consumers, including
rotated/scaled UVs and single-surface barrel displacement. Coverage changes
coordinates within the existing render targets; it adds neither targets nor
passes. Wider hidden coverage can reduce that producer's sampling density.
Final composition crops to the viewport. Retained feedback stays finite and
transform coverage is bounded to 64 reference widths/heights. Intentional
material repeat/mirror/clamp remains separate from scene-surface coverage.
