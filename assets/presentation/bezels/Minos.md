# Caverns of Minos live bezel

Uses the approved **Minos - final single surface concave** scene, retained as a
flat editable graph. The final warp is Layer 8 / Node 24, with C25/C26 constants
on Node 25. Material tiling occurs before that final shape.

In-game presentation submits the letterbox strips together and leaves the
gameplay rectangle outside the geometry. This saves one draw per frame while
keeping the scene graph and its animation controls intact.

Existing level and scroll mappings are retained. Level number is absolute:
levels 1-6 use Root 22, 25, 27, 15, 11, 5; each further block of six adds 50 degrees
of global hue. A later-level start or continued game resolves the same values.
Node 18 second UV X follows cameraY/25, one texture-width per gameplay screen,
positive downward and reversed upward, wrapping at one.

`minotaurs.rescued` reads the existing run-total `minosSavedTotal`, increased when
a minotaur reaches the ship. It drives Node 8 first UV Y through one complete
mirrored-texture period (two units) in one second, then returns to its authored
baseline, 1/1024. Rescues during a running cue queue subsequent complete cycles.

`game.phase` reads the existing run state. Entry into state 5 marks successful
completion and entry into the bonus summary, before confirmation of the next
cavern. This includes the final cavern.

`gravity.influence` reads existing `shotDrift`: the game applies its physical-tilt
dead zone and clamps this influence to +/-0.3; controller influence uses the same
cap. Smoothstep maps it to Node 25 C25.b from 0.4 to 0.6, neutral 0.5. The visual
range therefore saturates exactly when the game does, rather than assuming an
independent raw tilt angle. Physical motion reaches the cap around 0.38 g after
the 0.08 dead zone. All observations are read-only in the host adapter; original
simulation and saves are unchanged.

On successful completion, Node 25 C25.g makes a linear 360-degree turn over two
seconds, returning seamlessly to zero. Its units are quarter-turns: 1 remains
90 degrees, while 4 completes the circle. Single-surface barrel shaders preserve
all existing 0-1 angle settings and now accept the full periodic 0-4 range. Other
shader constant ranges remain unchanged.
