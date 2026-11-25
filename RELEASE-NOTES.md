# Version 0.9.8

The planned 1.0 feature set is in place. This public beta brings LiveWidgets to
all nine titles, adds the in-game iOX menu, and completes the collection's new
title, launcher, and audio presentation.

## LiveWidgets

LiveWidgets show game-specific records, scoring mechanics, active effects, and
run stats in the letterbox. They are on by default and can be turned off under
**Options → Display**. Layouts adapt to the available space in landscape and
upright portrait.

| Game | Widget highlights |
| --- | --- |
| Gridrunner | Checkpoint records and progress, recent pickups, radial power-up timers, and run stats |
| GoatUp | Records, kisses and collected items, gestation and power-up timers, and tower progress |
| Super Ox Wars | Area bests and targets, polarity, chains, follower timers, and boss approach |
| Five a Day | Day bests, fruit collection, firepower, formation bonus, score penalty, and run stats |
| Caverns of Minos | Cavern records, score multiplier breakdown, rescues, landing grades, and mission and par times |
| GoatUp 2 | Placed objects, minotaurs, tail size, active effects, and the Vindaloo prompt |
| Minotron 2112 | Stage bests, weapon setup, pickups and expiry, extra lives, and beasties |
| Deflex | Level score and best, shell, chain timer, and bats used and fewest |
| Minotaur Rescue | Single-player Solar and Deep Space records, rescues, firepower, and losses. Solar also shows sun stability and rescue efficiency. |

## iOX and LiveSynth

- Press **View / Select** or **F1** on Windows during play to open the in-game
  iOX menu. It pauses the game while you adjust Display or Control settings.
  You can resume where you left off or exit to iOX.
- The Minorola launcher bezel adapts to wide and tall displays. The handset
  animates in from the title screen, and returning to iOX reverses the launch
  animation.
- The title and launcher share procedural music and menu cues. **Title & iOX
  Audio** offers **Full**, **Cues Only**, and **Off** under **Options → Controls**.
- LiveSynth scenes extend into available letterboxing. GoatUp, Super Ox Wars,
  Caverns of Minos, and Minotron use fewer rendering passes.

## Game fixes

- A fresh tap or button press exits the final star field in the Story of Deflex.
- The B button no longer exits active Five a Day gameplay. It still works as
  Back in menus.
- GoatUp 2's Reset option keeps its color cycle while visibly highlighted.

The Windows x64 and Android ARM64 collection packages include all nine games.
Individual-game packages use lean standalone builds. On Android, the collection
uses the same package ID and signing certificate as the previous public beta.

# Version 0.9.5

LiveSynth adds game-responsive backgrounds to the letterboxed regions of the
collection. Each game has its own scene, driven by gameplay state, controls,
motion, audio or the game image. Set the background and its quality under
**Options → Display**. The original animated background remains available as
**Plasma**.

## Game scenes

- **Minotaur Rescue** reacts to ship heading, rescues, level changes and
  black-hole contacts. The radial scene wraps cleanly.
- **Minotron 2112** reacts to the soundtrack, weapon changes, bombs and extra
  lives. The ring animation marks level completion.
- **Gridrunner** uses ship position to tilt the grid. Power-ups affect its
  colours and brightness, and extra lives trigger a hue shift. Gridrunner
  Classic uses a distinct palette and position-driven effects.
- **GoatUp** responds to kids, Billy Goat kisses, climbing zones and poison
  mushrooms.
- **Super Ox Wars** responds to red and blue scoring, followers and tilt.
  The radial scroll keeps its timing across display shapes.
- **Five a Day** incorporates the gameplay image and reacts to ship rotation,
  the fruit trail, rescues and level completion.
- **Caverns of Minos** changes its tunnel with each cavern and reacts to camera
  movement, tilt, rescues and level completion.
- **GoatUp 2** responds to hunger, saturation and invincibility. Collected
  items briefly take over its sprite trail and keep their game colours.
- **Deflex** responds to deflector turns, remaining ball shell and the selected
  level. Shader transitions fade between levels.

## Display, performance and fixes

- LiveSynth uses prebuilt scene data, caches shaders when supported and reuses
  rendered layers to reduce GPU work. NHML describes the links between game
  events and scene effects.
- **CRT 1** uses the lighter Consumer S-Video filter. **CRT 2** uses CRT
  Royale. Existing filter and background choices are retained.
- Menus use title case, and Rotate Controls keeps its explanatory tooltip.
- Fixed a GoatUp 2 collision bug that prevented fart attacks from damaging
  enemies.

Windows and Android standalone editions launch each game directly. LiveSynth
and the iOX launcher are available in the full collection.

[OpenMinotaur 0.9.5 on GitHub](https://github.com/Frogacuda/OpenMinotaur/releases/tag/v0.9.5-livesynth-beta)

# Version 0.9.1

- Fixed Minotaur Rescue firing cadence and projectile speed so they respond
  correctly to distance from the sun and rescue upgrades.
- Corrected rounding in Minotaur Rescue scoring and sun growth and collapse.
- Corrected a small Minotron enemy-steering precision difference.

Existing saves and settings remain compatible.

[OpenMinotaur 0.9.1 on GitHub](https://github.com/Frogacuda/OpenMinotaur/releases/tag/v0.9.1-beta-hotfix)

# Version 0.9

The first public beta brought all nine reconstructed games to Windows and
Android through the iOX collection, with native controller support and menus
adapted for current devices.

[OpenMinotaur 0.9 on GitHub](https://github.com/Frogacuda/OpenMinotaur/releases/tag/v0.9-beta.1)
