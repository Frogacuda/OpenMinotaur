# Game modules

Each core implements `include/core_api.h`: initialize against host services and
a display surface, receive canonical input, advance/render frames, mix audio,
save state and shut down. `platform/core_registry.cpp` registers launchable
cores. Gridrunner and Classic share assets but keep separate game identities,
state and presentation formats.

Modules own game rules, enemy behavior, update order, random-stream selection,
resource composition and persistence. Shared engine mechanisms do not replace
those responsibilities. Minos, Gridrunner and Deflex place their standalone
game rules and tables under `reconstruction/`; their cores bind those rules to
the common renderer and host. Other games keep rules directly in their core
directory; `reconstruction/` identifies independently buildable rule libraries,
not a different fidelity standard. A core-local `engine/` directory contains its
game bindings and policies, not another independent shared-engine implementation.

## Input and interface conventions

- Consume modern controller actions directly; do not simulate touches.
- Use canonical controller state after platform remapping, dead-zone and
  orientation handling. Choose digital or analog response to suit the game.
- Accept stick and D-pad navigation in menus. Retain held state across screen
  changes so a held confirmation cannot activate the destination immediately.
- Pause menus provide separate routes to the title and iOX. The right face
  button exits to iOX from titles. These conventions give controller users a
  consistent route without replacing each game's visual style.
- Original loading artwork remains visible for at least one real second from
  first presentation, including initialization time. Classic bypasses loading
  artwork because it has no independent original splash screen.

Modules retain each game's touch behavior and select motion input only when
available/enabled. The host supplies motion in game axes; modules do not apply
screen rotation themselves.

## Deliberate adaptations and uncertain state

Local scores, name entry and collection achievement storage replace retired
network services while retaining game award criteria and notifications.
Native controller curves, precision movement and consistent menu routes support
modern devices. Audio gain/headroom may be tuned to avoid clipping and maintain
consistent listening levels across the collection.

Rescue uses shortest-path heading interpolation to avoid a turning defect exposed
more readily by direct controllers. Five a Day evaluates lifetime goals after
banking a completed run's totals so those goals can unlock. Deflex uses the actual
cell for a branch whose original code consumes unspecified stack contents.
These are intentional behavior corrections rather than claims about original
execution.

Some original fields remain offset-named where their meaning is not established.
Explicit host allocation defaults are distinguished from original constructor
stores. Preserve those distinctions when renaming or extending a component;
do not invent field meanings or initialization guarantees. Fields with no runtime
readers, serialization role or host contract can be omitted; original offsets do
not require reproducing unused storage in a C++ object.

Tests alongside modules identify their scope: source rules, original-execution
fixtures or regression/integration behavior. Captured cases and scripted opening
gameplay are useful coverage, not exhaustive proof of all campaign/device states.
