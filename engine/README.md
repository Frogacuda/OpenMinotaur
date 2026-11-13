# Shared Neon engine

The engine contains mechanisms shared by the game family. Each game keeps its
own simulation state, scheduling, rules and resource lifetime. Shared code uses
explicit parameters or bindings where the original games differ.

| Area | Main interfaces | Boundary |
| --- | --- | --- |
| Resources | `ResourceCatalog`, effect DNA readers | Per-instance catalog and parsing; modules choose required resources and failure handling |
| Rendering | `RenderDevice`, render state, sprite drawing/commands | Shared GPU operations and geometry; modules select texture routing and drawing rules |
| Presentation | `game_presentation.h`, effect runtime, feedback history | Common preparation/execution machinery; modules supply game callbacks and frame order |
| Text and grid | Cell storage/traversal, proportional text, score notices | Shared kernels; row metadata, palettes and layout variants remain explicit |
| Particles and entities | Motion, contacts, list operations, particle helpers | Reusable operations; factories, scheduling, RNG ownership and enemy behavior stay in games |
| Input | `ControllerState`, menu navigation, touch slots | Canonical device-neutral state; game-specific response and actions stay in modules |
| Audio | Sample playback and voice machinery | Modules choose active caps, voice policy, random stream and output gain |
| Backend setup | `backend_setup.h` | Shared setup steps; cores retain ownership, initialization policy and teardown order |

`bindings/common` supplies module-facing resource, audio and host interfaces.
`bindings/early` supplies the matching Minotron/Minos sprite and synth contracts.
`bindings/late` supplies later-engine sprite/effect contracts. Identical mixer,
construction, synth and matching presentation implementations live in
`bindings/common` and are compiled
separately with each target's `NEON_GAME`. Rescue retains its distinct sprite and
presentation adapters. Local forwarding headers keep core include paths stable.
Shared declaration `.inl` files are included through those per-game wrappers so
integration checks can include several namespaces in one translation unit.
Namespace bindings borrow state from a core's context; they do not create a global
game instance.

Baked presentation resources are linked explicitly by their consumers: the CRT,
launcher, collection title, Minos core and GoatUp 2 backend. Compiler warning
policy adds no asset-generation dependencies to unrelated targets.

## Numerical and lifetime contracts

Preserve documented float/double operation boundaries, rounding, RNG draws and
mutation order. A cosmetic draw can consume the same random stream used by
gameplay. Render helpers must not introduce additional simulation draws.

Module-selected policy is separate from available capacity. For example,
supporting more audio storage does not authorize accepting voices beyond a
game's active cap or changing its stealing rule. Similar distinctions apply to
sprite overflow, grid dimensions, callbacks and list traversal.

Resource acquisition, failure recovery and teardown remain with their owners.
Do not add implicit allocation clears or universal rollback to a shared helper;
explicit frame clears and owner-specific recovery are separate operations.

The platform owns graphics-context recovery. Before suspension, the shared
render device checkpoints texture and feedback contents; vertex buffers retain
their uploaded data. A replacement context rebuilds GPU objects in the same
logical slots without recreating game or effect owners. Stale GPU names are
abandoned before new objects are allocated, and temporary texture checkpoints
are released on resume. These lifecycle operations run on the application's
single rendering thread; game saves remain the module's responsibility.

## Testing

This directory can be configured independently. CTest exercises resource and
effect parsing, math, render-state models, sample audio, input and contact/list
contracts. Saved-reference comparisons establish regression behavior; captured
original fixtures and independently derived rules provide separate evidence.
Real graphics and game integration checks live in the collection's optional
test targets. Physical drivers and devices need their own testing.
