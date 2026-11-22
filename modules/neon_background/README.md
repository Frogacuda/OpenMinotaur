# Neon background runtime

A host-owned Light Synth runtime shared by the collection title and live bezels.
The title plays Vaporwave. All games in the collection have state-driven
live-bezel profiles, including the separate Gridrunner Classic entry.
The runtime evaluates scenes, submits ordered effects, and draws into the host's
framebuffer. It owns neither a window nor input navigation.

The host supplies a bundle reader to `Runtime`. Nested scenes use the same
reader, so assets can remain in an embedded archive. The scene resource closures are in `assets/presentation`; no editor or preview
images are needed. See [LIVE-BEZELS.md](LIVE-BEZELS.md) for value rigs and host integration.

Advance the scene with `frame(elapsedSeconds, destination)` on the graphics
thread. Paused hosts pass zero elapsed time. Release graphics before destroying
a live context, or abandon stale names after loss. Scene time and authored values
are retained; intermediate feedback targets are rebuilt on the next draw.

The implementation is organized by responsibility: `runtime/` owns scene loading,
composition and graphics lifetime; `effects/` contains the effect algorithms;
`authoring/` contains scene controls, geometry studies, study/demo content and the
preset catalogue. Editable scene loading uses the authoring model; prepared
playback initializes only its active controls and resolved graph. See [SDK.md](SDK.md)
for offline preparation and [NHML.md](NHML.md) for the Neon Harness language.
Study presets are authoring choices,
not entries in the collection launcher. The `pc_` prefix records the PC Neon
origin of an effect algorithm; it does not select a platform backend.

Light Synth is maintained as an evolving presentation service. Describe geometry,
parameter behavior, composition and host ownership directly in code comments;
reconstruction comparisons are not its acceptance criterion. Validate scene
loading, rendering, destination handling and graphics-resource lifecycle.

## Scene compatibility

The runtime accepts NEONGLES scene versions 1 through 15 and component documents
through version 9. Current scenes may contain graph recipes, embedded PNG image
assets, per-input filtering/addressing, routed layer sources and projective layer
surfaces. Scene loading retains the compressed image data on the CPU; each GLES
context owns its uploads and sampler objects. Static beam textures and imported
images are rebuilt when their relevant inputs change, or after context loss.

Older game-specific scene IDs 22 through 25 are interpreted using their original
graphs when loading scene versions through 10. Newly authored scenes use the Lab
catalogue and can carry their own graph recipes. Existing game rigs retain their
UV ranges, shader-switch targets and continuous controller-speed aliases.

For bundled scenes, embed imported images in the document or include their
resources when preparing the presentation bundle. Do not depend on an editor's
absolute image paths on the player's device. Editor windows and tree controls
are not dependencies of game playback.
