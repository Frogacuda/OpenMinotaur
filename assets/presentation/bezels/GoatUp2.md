# GoatUp2 live bezel — candidate 3

Uses the supplied flat candidate 3. The existing emitter stays at Layer 6 / Node 24. Four base stars and the six rare sprites retain their original atlas cells and artwork; the same atlas adds the possible collected objects from GoatUp2's own grunt.png resource catalog. No layers or render passes are added. All images are prepared offline; pickups do not load or upload textures.

Each newly observed collected object replaces the next eight scheduled emissions with its own sprite. Several pickups queue in collection order. Existing particles retain their sprite; the base and rare sequence cursors resume afterward. The observer reads existing follower actors, their collection serial and sprite. Kids are included. Grazing grass is outside this inventory. The immediate-use invulnerability pickup has its own active-state effect and is not queued.

Hunger maps 0–1 directly to Layer 2 / Node 11 C25.g. Layer 2 mixer Node 10 saturation maps hunger 0–0.5 to 0.25–1 and stays at 1 above half hunger.

While the existing invulnerability timer is positive, Layer 8 mixer Node 31's C25.r is 0.4 and its colour green gain is 0.4. C25 is stored on the layer's constant node, Node 32. Global hue makes one full revolution per real second while active. These temporary overrides return to the saved baseline when invulnerability ends. Pausing freezes cue time. Saved scene playback speed remains 0.48 and does not slow the hue cue.

The NHML defines all mappings and the eight-spawn count. Its insert statement maps original game artwork IDs to atlas frames; the JSON records the underlying game texture rectangles. The first inventory sample seeds without replaying existing items. Identities are namespaced by presentation session; world/stage changes or a follower-count reset establish a new namespace. The game adapter observes existing state. Simulation, resource identifiers and save behavior are unchanged.

The .neongles scene is self-contained and editable in Lab v0.38. It shows the authored baseline there; the .nhml is applied by the game, rather than automatically by the Lab. The game ships the baked scene, reachable resources and NHML only. The reference atlas and binding JSON are creator assets.

EF182: Inserted pickup particles travel at 2x normal motion speed while base stars and rare objects remain at 1x. Palette-driven pickups retain their existing game palette-source ID; live colours continue across the queued run and the full particle lifetime. The adapter publishes the game's current palette without changing its phase or consuming randomness. The source stream remains available after the original actor disappears, including on menus while the backdrop continues. No render passes or texture uploads are added.
