# Neon background runtime

A host-owned Light Synth runtime for authored animated backgrounds and future
live bezels. The collection title currently plays the saved Vaporwave scene.
The runtime evaluates scenes, submits ordered effects, and draws into the host's
framebuffer. It owns neither a window nor input navigation.

The host supplies a bundle reader to `Runtime`. Nested scenes use the same
reader, so assets can remain in an embedded archive. The title's Vaporwave resource
closure is in `assets/presentation`; no editor or preview images are needed.

Advance the scene with `frame(elapsedSeconds, destination)` on the graphics
thread. Paused hosts pass zero elapsed time. Release graphics before destroying
a live context, or abandon stale names after loss. Scene time and authored values
are retained; intermediate feedback targets are rebuilt on the next draw.

The implementation is organized by responsibility: `runtime/` owns scene loading,
composition and graphics lifetime; `effects/` contains the effect algorithms;
`authoring/` contains scene controls, geometry studies, study/demo content and the
preset catalogue. The runtime uses the authoring model during playback and scene
loading, so it remains part of the module. Study presets are authoring choices,
not entries in the collection launcher. The `pc_` prefix records the PC Neon
origin of an effect algorithm; it does not select a platform backend.

Light Synth is maintained as an evolving presentation service. Describe geometry,
parameter behavior, composition and host ownership directly in code comments;
reconstruction comparisons are not its acceptance criterion. Validate scene
loading, rendering, destination handling and graphics-resource lifecycle.
