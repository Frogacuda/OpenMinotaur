# Version 0.9.6

## EF190 — Collection audio level

- Raise title and iOX audio by 25%, with another 10% on iOX menu cues.

## EF189 — Collection audio balance

- Lower the harmonic bed, give the settings kick more weight, and let the
  music fade in after the collection title reaches full opacity.

## EF188 — Collection sound design

- The collection title and iOX share a procedural harmonic bed with slowly
  shifting resonance. Short phrases from the bundled games pass through a
  stereo reverb, with irregular timing, occasional overlap and movement.
- Icon moves accent the current harmony. Icon selection and Back have
  distinct melodic cues. Settings use light snare ticks and a short kick.
- Set **Title & iOX Audio** under **Options → Controls** to **Full**,
  **Cues Only** or **Off**. The setting is saved for the next startup.

## EF187 — Extended scene coverage

- Composited scene layers extend with the viewport instead of mirroring at the
  authored reference boundary. Sprite Flight on the title retains its direction
  and sprite orientation in portrait.
- Native layer producers cover the UV bounds requested by downstream affine and
  single-surface barrel passes. Existing target allocations and pass counts are
  retained; the reference frame continues to define scale and proportions.
- Independent input-axis domains preserve intentional material tiling. Minos
  extends along the pipe while retaining its cross-pipe material symmetry.
- Effect Lab and game playback share the same coverage rules.

# Version 0.9.5

LiveSynth game-responsive bezels are available throughout the collection, with
HQ/LQ quality settings and the completed game-specific harnesses. Menu labels
use title case; CRT 1 is the lighter filter and CRT 2 uses Royale. Existing saved
choices are retained. Windows and Android standalone editions launch each game
directly without iOX or collection presentation services.

## EF185

- Menu labels use title case. Rotate Controls keeps its explanation in the tooltip.
- Neon letterboxing is now named LiveSynth, with LiveSynth Quality beneath it.
  LiveSynth is the default for settings without a saved letterboxing choice.
- CRT 1 is the lighter Consumer filter and CRT 2 is the heavier Royale filter,
  in that order. Existing saved filters and letterboxing choices are retained.

## EF184

- Public-source cleanup for Live Bezels: consolidate harness timeline reset,
  remove temporary test output and clarify the current profile, loading, quality
  and lifecycle documentation. Scene assets and gameplay behavior are unchanged.

## EF183

- Deflex Neon uses the supplied candidate scene with retained half-second
  deflector quarter turns, shell-driven C25.r and absolute five-shader level
  selection. Shader changes fade through zero over 0.15 seconds each way.
- NHML supports ordered signed steps and fade-gated indexed shader selection.
  Deflex observations stay in the host adapter; game rules are unchanged.

# EF182

GoatUp2 pickup particles travel at twice the normal flight speed. Palette-coloured pickups retain a live reference to their original game colour stream while queued and in flight. Base stars, rare objects, the eight-spawn count and previous harness mappings remain unchanged. No render passes or pickup-time texture uploads are added.

# EF181

GoatUp2 uses candidate 3 with live hunger, saturation and invincibility controls. Each newly observed collected item replaces the next eight scheduled sprite emissions with that object's game sprite. Base and rare sequences resume afterward. Ordered collection observation and insertion mapping are declared in NHML; original simulation and save behavior are unchanged.

# EF180

Corrects Super Ox Wars Neon scroll-cycle resets across screen aspects. Reference-tile sampling no longer applies the older whole-surface scroll conversion a second time. Animation ranges, speed rigging and game logic are unchanged.

# EF179

Caverns of Minos now uses the approved final single-surface concave scene. Rescues cycle its material vertically over one second; level completion rotates the pipe axis through a full turn over two seconds. Effective game tilt moves the pipe centre over 0.4–0.6 with eased limits. Existing cavern texture/hue selection and scrolling remain intact. All hooks read existing state; simulation and save formats are unchanged.

# EF178

Correct layer addressing when effects extend into different display aspects.
Native layer materials repeat/mirror/clamp in their authored reference domain,
then map into the expanded intermediate, keeping rotated inputs centred in
portrait. Whole-scene copy passes retain their expanded coverage. Explicit layer
input domains are available in the creator tools. HQ/LQ, update cadence and
existing gameplay rigs are retained; no game simulation or save changes.

The renderer also supports single-surface barrel variants 94/95 and prepared
playback version 3, while retaining readers for versions 1/2. Approved game scene
assets remain in place. Creator-only final-barrel Minos examples are separate.

## EF177

Add the Caverns of Minos candidate as a live bezel. Current cavern number selects
six root textures in order, with a 50-degree hue shift for each subsequent group
of six caverns. The camera scroll drives second UV X by one texture-width per
gameplay screen, reversing when the camera scrolls up. Continuing or selecting a
later cavern immediately resolves the same presentation as reaching it in play.

NHML adds indexed absolute-value mappings. The scene baker includes all declared
texture alternatives. The Minos adapter reads existing cavern/camera state;
original gameplay and save formats are unchanged. Existing Neon quality, cadence
and loading optimizations are retained.

## EF176

Android now uses the accepted 60-Hz submission schedule whenever an available Neon bezel is visible. The fixed schedule preserves display synchronization, waits before input polling and avoids catch-up submissions. Desktop pacing, manual HQ/LQ selection and the one-time loading assessment remain unchanged.

Retains cached presentation uniforms, exclusive host-state handoff, one complete output clear after offscreen work, single-render animation updates and cached half-rate output. Removes the temporary diagnostics menu, frame trace history/export, EGL timestamp probes, cadence overrides and local profiling hooks. Runtime and SDK performance contracts are documented in `platform/PERFORMANCE.md` and the Neon SDK. The validated GoatUp 2 attack correction is retained.

The accepted presentation work also includes player-selectable Neon Quality, authored one-axis barrel shaders, and Minotron audio-driven brightness from 0.7 to 1.9.

## EF165

- Prepare the eight fixed Neon scenes offline. Gameplay and the title load resolved playback graphs and only reachable shaders/textures, without constructing the editor catalogue. Preserve the approved mobile resolutions and existing rigging.
- Add NHML (Neon Harness): named, typed inputs and scene parameters, explicit units, passive mappings, state selectors and finite cues, with threshold/change/state event rules. The creator baker validates destinations and includes all declared shader alternatives. Game simulation remains unchanged.
- Cache shader program binaries locally when the graphics driver supports them. Shader/driver changes or rejected entries fall back to source compilation. Cache data is disposable and never shipped between devices.
- Keep Neon letterboxing black while unavailable or loading. Plasma remains a separate explicit option.

## EF164

- Restore Gridrunner and Classic to half-resolution mobile composition and a 512-pixel base producer. Retain the copy-elision savings, all scene edits, rigging and Classic hue. Desktop keeps native composition and its sharper 1024-pixel base.

## EF163

- Eliminate redundant copies for non-feedback mixer layers and component compositions while retaining conservative paths for feedback, legacy framing and node inspection.
- Gridrunner uses a native-resolution final composition and a sharper 1024-pixel base producer, preserving its authored effects and rigging.
- Add the same scene to Gridrunner Classic with a 300-degree hue baseline and position-only rigging, read from the existing VIC display records.

## EF162

- Use the revised Gridrunner candidate 2 live bezel scene. Remove its muted, unused external-input placeholder while preserving the active plane blend with the animated base. Existing player-position and power-up rigging remains attached to the original targets.

## EF161

- Enlarge only the grid plane from 1.08 to 1.44, compensating beam density from 12x9 to 16x12 to preserve visible spacing and keep the tunnel unchanged.

- Gridrunner plane pitch now runs from neutral (0) at the bottom to +0.3 at the top. Unavailable player position uses the neutral plane. Horizontal yaw remains independently driven. Clear the grid source's additional saved camera pitch so neutral is screen-aligned.

## EF160

- Gridrunner plane pitch uses -0.3 at the bottom and 0 at the top, with -0.15 when player position is unavailable.
- All-around-shot brightness eases to 2, holds while active, and eases back on expiry.

# Version 0.9 public beta - EF159

- Added the editable Gridrunner candidate with slow-motion scroll, independent grid-plane position tilt, multi-shot RGB-BGR, an all-around activation brightness pulse, and an extra-life hue turn.
- Minotron source brightness follows mixed-audio RMS from 1 to 1.6. Bomb flashes retain their separate peak of 3. Removed the enemy-retirement callback and destruction-reporting state from the game engine and adapter.

- Updated Light Synth scene playback to the current Effect Lab format, including saved graph recipes, embedded images, individual sampler settings and routed layer inputs.
- Added projective layer surfaces and corrected perspective interpolation for image-plane camera controls.
- Matched the Lab's full eight-bit alpha decoding for original root textures.
- Fixed the RoundTunnel portrait angular-wrap seam and added the masked-light floor used by the Gridrunner beam scene.
- Cached static vector sources and imported-image uploads until their content or rendering configuration changes.
- Retained existing game rigs, legacy scene identities, borrowed gameplay textures and graphics-context recovery. Game rules and the selected bezel scenes are unchanged.

# Version 0.9 public beta - EF158

- Five a Day level completion triggers a 1.5-second smooth full-range excursion of Layer 2 / Node 12's shader effect parameter, returning to its authored value 174.
- Rescuing a minotaur turns Layer 1 / Node 8 hue once over one second, returning to its authored hue.
- Node 8 brightness maps the current attached fruit trail from 0..20 items to 1..2, clamped at 2. Lost or banked items leave the trail count.
- Gameplay imagery, player rotation and the clean radial scroll remain as in the accepted Candidate 3 build.

# Version 0.9 public beta - EF157

- Replaced the Five a Day bezel with Candidate 3. Gameplay now replaces the load-screen source in Layer 1 / Node 8's first texture input (labelled texture input 1 in the Lab).
- The player's current rotation drives Node 8's second UV rotation, one-to-one in radians.
- Layer 4 / Node 19 now scrolls in texture widths after its nonlinear distortion, closing the mirrored -1..1 loop without changing the saved cycle timing.
- Fixed texture-choice bounds for higher artwork selectors.

# Version 0.9 public beta - EF156

- Added the Five a Day Candidate 2 live bezel experiment. Layer 1 / Node 9 texture 0 samples the current gameplay surface directly on the GPU.
- The source is the native gameplay image before CRT, scanlines or other display filters. Bezel output never feeds back into the gameplay surface.
- No screen readback, duplicate gameplay render or additional full-size copy target is required. Mobile intermediates remain half resolution.
- Preserved the scene's saved global hue, feedback routing and authored controls.

# Version 0.9 public beta - EF155

- GoatUp's global hue advances by 50 degrees over half a second when reaching a higher zone. The baseline remains shifted, wrapping through the hue cycle.
- While the poison mushroom is active, Layer 5 / Node 22 uses Radial Colourise and global hue rotates once every two seconds. Expiry restores the original shader and current zone hue.
- Existing kid and Billy Goat cues remain additive to the zone baseline. Gameplay pause freezes all cues.

# Version 0.9 public beta - EF154

- Super Ox Wars now presents the radial stage selected in the saved Candidate preview, preserving its intended colour response. The graph's original default output added a further colour-altering pass.
- Powerup, score balance and tilt rigging are unchanged.

# Version 0.9 public beta - EF153

- Added the Super Ox Wars Candidate live bezel under Neon letterboxing.
- Radial scrolling wraps in texture widths across screen shapes, preserving the authored cycle timing.
- Ox followers increase radial scroll speed from 100% to 400%, reaching the maximum at five followers.
- Red and blue colour gains follow their respective shares of the combined score, from 1.25 to 3. Equal or zero scores give both 2.125.
- Resolved game tilt (sensor or controller) drives horizontal UV offset from -0.2 to 0.2 with easing at the extremes.

# Version 0.9 public beta - EF152

- Live bezels render during module loading and animate before gameplay starts.
- Minotron level-end ring speed now eases gradually toward 8x, then resets without a phase jump.
- Enemy destruction brightness now peaks at 1.6.
- Bombs flash brightness to 3 over a 0.5-second cue. Overlapping kill flashes use the stronger brightness rather than adding together.
- Extra lives trigger a one-second full hue rotation, returning to the current level colour.

# Version 0.9 public beta - EF151

- Preserved the generic animated bezel as Plasma, with its own Letterboxing option.
- Added the Minotron Candidate live bezel. Choose Neon under Letterboxing.
- Completing a level fades the ring texture to opacity 200 over 0.5 seconds,
  holds for 1.5 seconds, and fades out over 0.25 seconds. Ring scrolling ramps
  from 1x to 8x over the same cue and returns to 1x without a phase jump.
- The colour-layer shader follows invincibility (46), then triple/five-way shots
  (61), otherwise its baseline (59). Losing invincibility restores the current
  weapon state.
- Each level adds a smooth, persistent 25-degree source hue step. Each enemy
  destruction pulses source brightness from 1 to 1.3. Pause freezes the cues;
  a new run clears them.
- Updated the integrated renderer for this scene's independent node colour,
  displacement and texture-width radial scrolling. The imported baseline
  matches the Lab in all four tested aspect ratios.

# Version 0.9 public beta â€” EF150

- Updated Minotaur Rescue to the source-mirrored scene: the radial mapping completes a mirrored texture repeat at the wrap boundary.
- Preserved the original screen framing and existing gameplay rigging.

## Previous scene synchronization

- Synchronized the Minotaur Rescue scene with the saved Effect Lab v0.23 document, including exposure and the disabled outer-layer rotation hold.
- Player-two rotation cues return to the synchronized scene's zero-rotation baseline.

## Minotaur Rescue live bezel

- Added the authored Minotaur Rescue live bezel. Choose Neon under Letterboxing.
- Player heading drives one UV turn per full rotation. Collected-minotaur firing boost scales the shared UV animation clock from 25% to 100% speed.
- Level increases move the hue baseline by 50 degrees over one second. Each rescue adds one complete one-second hue cycle.
- Collapsed-center contacts and player-one Tank/Jet hits turn the inner layer forward; player-two hits turn the outer layer backward, using one-second smooth curves.
- Repeated Rescue cues queue separately. Pause freezes playback and a new game clears the cues.

## Previous GoatUp update

- Gaining a GoatUp kid now cycles global hue through one complete turn in one second.
- Kissing a Billy Goat triggers the previous two-second 0 to 100 to 0 degree hue pulse.
- Overlapping cues combine their hue offsets. Game pause freezes both cues.

