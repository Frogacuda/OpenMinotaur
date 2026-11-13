# OpenMinotaur

Version 0.9 — public beta, progressing toward feature-complete 1.0.

OpenMinotaur seeks to document and preserve the nine classic iOS titles released
by Llamasoft between 2011 and 2013. Dubbed "The Minotaur Project," these titles
were loved by fans, but enjoyed an unusually short period of official support, as 
the need for constant updates on the platform was considered too high of a burden. 
This project seeks to offload the burden of ongoing support to the community with
three initiatives:

###Games
OpenMinotaur provides an open-source, portable C++ reconstruction of all nine titles, 
along with native controller support, support for a broader range of display 
configurations, updated menus to allow all features to be accessed end-to-end with 
controller or touch input, as well as minor bugfixes and UX improvements. These 
updates serve the projects goal of allowing these games to be supported natively on 
a wide range of platforms, now and in the future. Presently, Windows, Android, and 
Linux  builds will be provided officially, but the source is built to accommodate 
porting to a wide range of platforms, with OpenGL ES 3.0 and SDL3 being the primary
dependencies.

Because these titles used the now-discontinued OpenFeint service  for high scores
and achievements, these features have been replaced with local high score tables
and achievement tracking through the provided iOX launcher experience.

The nine supported titles are:
Minotaur Rescue
Minotron: 2112
Deflex
GoatUp 
Caverns of Minos
Gridrunner
Five a Day
Super Ox Wars
GoatUp 2. 

Gridrunner Classic, once accessible by changing orientations in Gridrunner is now a
separate module. 

###Engine
OpenMinotaur also attempts to reconstruct and document the lightweight iOS  version
of the Neon Engine that drives  these games with a unified and reusable architecture,
potentially enabling community development.

Over the course of these games, various changes were made to the shared engine services.
These are documented and made explicit where they occur.

A future goal of this project is to develop an SDK supporting homebrew development with
this engine.

###Unified presentation and enhanced features.
In addition to build files for individual games, OpenMinotaur also provides a unified
launcher experience called iOX. This launcher compiles all titles into a single package,
and aims to provide users with the kind of polished presentation and robust feature set
found in a premium retro compilation. 

iOX presents users with a clean graphical interface in the form of a cheeky parody of
the early version of iOS. This serves as a hub for launching games, tracking achievement
progress, and configuring advanced options and features not available in the base titles.

Additional features available through iOX:
*Original title sequence demonstrating light synth and vector modules
*Screen settings for resolution, fullscreen/windowed, aspect ratio and orientation
*Screen modes and filters for enhanced resolution, native res, scanlines, and CRT. 
*Tilt control calibration/recentering
*Controller remapping
*Control orientation rotation

In addition, an ambitious Live Bezel system is being built for the full release, which
will fill letterboxed regions with responsive light synth graphics in the style of 
classic Llamasoft titles, and realtime gameplay information widgets. The engine powering
these features is implemented and powers the title sequence, but the scenes, rigs, and
widgets are still being built and tested.

## Build

Install CMake 3.20+, Python 3 and a C++17 compiler. Supply SDL3 through an
installed CMake package, a source checkout at `third_party/SDL3`, or
`-DSDL3_SOURCE_DIR=/path/to/SDL3`.

Windows with Visual Studio:

```sh
cmake -S . -B build -DSDL3_SOURCE_DIR=/path/to/SDL3
cmake --build build --config Release
```

Run `build/Release/minotaur.exe`. CMake copies the SDL3 shared library beside
the executable when SDL is built from source.

Linux or macOS with Ninja:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSDL3_SOURCE_DIR=/path/to/SDL3
cmake --build build
./build/minotaur
```

Python 3.10 or newer generates verified presentation byte includes and creates
the deterministic game-resource archive for desktop executables. All native
builds require the Python interpreter; no imaging packages are needed to embed
the committed resources. Set `-DPython3_EXECUTABLE=/path/to/python` if discovery fails.
Editable assets remain in the source tree; players do not need a loose asset
folder. Optional artwork-generation tools under `tools/` are not build
dependencies; the UI baker additionally requires Pillow, NumPy and SciPy.

For Android, supply SDL3 at `third_party/SDL3`, then open `android/` in Android
Studio. The Gradle project specifies SDK/NDK requirements and builds the same
native CMake project. Release APKs and bundles require a private signing key;
there is no fallback to the development key. `assembleDebug` remains available
for local development.

Direct-download APKs default to **ARM64 (`arm64-v8a`) only**. From `android/`,
use `gradlew.bat assembleRelease` on Windows or `./gradlew assembleRelease` on
Linux/macOS. For the future Play Store distribution, include all supported ABIs
in the App Bundle:

```sh
./gradlew bundleRelease -PtargetAbis=arm64-v8a,armeabi-v7a,x86_64
```

Google Play delivers the matching architecture to each device. Custom builds
can select any supported subset with `-PtargetAbis=...`; the default does not
remove ARM32 or x86-64 build support.

Use `-PpythonExecutable=PATH` when Python is not available as `python`. This selects
the host interpreter for both game archives and CMake's baked-resource generation.
Android application builds disable desktop check targets.

Release signing reads `~/AndroidSigning/openminotaur-signing.json`, or the file
selected by `-PandroidSigningFile=PATH` / `OPENMINOTAUR_SIGNING_FILE`. The private
JSON contains `storeFile` (absolute keystore path), `storePassword`, `keyAlias`,
`keyPassword`, and optional `storeType` (default `PKCS12`). Keep this file and the
keystore outside the checkout. Never commit them or include them in packages.
CI may instead supply `OPENMINOTAUR_SIGNING_STORE_FILE`,
`OPENMINOTAUR_SIGNING_STORE_PASSWORD`, `OPENMINOTAUR_SIGNING_KEY_ALIAS`,
`OPENMINOTAUR_SIGNING_KEY_PASSWORD` and optional `OPENMINOTAUR_SIGNING_STORE_TYPE`.
The key password defaults to the store password. `:app:checkReleaseSigning`
checks that required settings and the key file are present without printing
credentials; the actual signing task verifies the key and password.

The project release certificate SHA-256 fingerprint is:

```text
2723044d8c20a0e7bac935471407c10c238841811bb9e1b7721bb16f7fc0feb1
```

Use `apksigner verify --print-certs APK` to inspect a downloaded APK. Builds signed
with your own key will have a different fingerprint.

Platform and physical-device qualification is ongoing.

### Standalone games

Set `MINO_STANDALONE_GAME` to build one game without iOX, collection title/
Light Synth services, CRT/scanline filters or other games' assets:

```sh
cmake -S . -B build-minotron -DMINO_STANDALONE_GAME=minotron -DSDL3_SOURCE_DIR=/path/to/SDL3
cmake --build build-minotron --config Release --target minotron
```

Valid IDs are `minotron`, `minorescue`, `goatup`, `minos`, `gridrunner`,
`gridrunner-classic`, `fiveaday`, `superoxwars`, `deflex` and `goatup2`.
On desktop, `MINO_STANDALONE_GAME=all` and target `standalone-games` build
all ten separate executables. Each executable embeds only its own game assets;
Classic shares Gridrunner's asset set and engine family.

In `android/`, add `-PstandaloneGame=minotron` to `assembleRelease`. Each game
has a distinct application ID and title, its original icon, and an isolated
output under `app/build-standalone/<game>/outputs/apk/`. A per-game Gradle
variant keeps collection and standalone package outputs independent.
Standalone APKs use the same ARM64 default and `targetAbis` override.

Standalone presentation uses fixed collection defaults: 4:3/3:4, Hi-res,
black letterboxing, default controller mapping/dead zone and enabled touch.
Android uses Native orientation, locked to the active game's axis; desktop
uses Horizontal. Collection settings files and `--set` overrides are not used.
Original in-game effects, local scores/achievements, game settings and editor
file access remain available. Exit and title-screen Back close the standalone
application; the in-game Title action still returns to
the game's title. Saves use an independent per-game application folder;
`--data-dir PATH` can select a different folder.

## Play and settings

Launch the application to open iOX. Use the D-pad or left stick to navigate,
the bottom face button to confirm and the right face button to go back.
Mouse and touch activate the on-screen controls. Keyboard arrows navigate;
Enter or Space confirms and Escape goes back or pauses.
On Windows, Alt+Enter toggles windowed/borderless fullscreen in the collection
and standalone games.

Each pause menu offers separate **Title** and **Exit** actions. Exit returns to
iOX; the right face button also returns to iOX from a game's title. Game-specific
control choices remain available where appropriate. Gridrunner's default Mode 1
pairs normal touch with fast controller movement; holding the bottom face button
halves controller speed. Mode 2 pairs accelerated touch with slower controller
movement; holding that button restores full speed. Super Ox Wars also offers
precision movement while holding the bottom face button. Its right stick leans
the world left/right, affecting layered scenery and enemy bullets. Centering the
stick returns control to physical tilt on devices with an accelerometer.
With both an accelerometer and controller connected, its pause menu also offers
**TILT ON / R STICK** to disable physical tilt while retaining right-stick input.

Minotron remembers the last mode used to start a game. Without a saved choice,
it defaults to Normal with a controller, or Simplified without one.

iOX Options controls aspect, display orientation, filtering, wallpapers and
controller mapping. Menu focus highlighting appears only with a controller or a
desktop keyboard; touch-only devices retain normal menu artwork.
Android phone/tablet-class touchscreen devices default to
Native orientation: title and iOX follow the device orientation. On first launch,
landscape with a connected controller instead defaults to Horizontal. The initial
choice is saved. Explicit saved
orientation settings take precedence; desktop and TV-class devices retain the
Horizontal default. The platform applies device mapping and orientation before
modules receive input, so games work in their own coordinate systems.
CRT 1 uses CRT Royale; CRT 2 offers a lighter CRT Consumer S-Video filter.
Both use flat screens, with scanlines aligned to the screen rather than the game.

Settings, scores, achievements and saves use SDL's per-user application folder.
`minotaur --list` prints the data location and available games. Use
`--data-dir /path/to/data` to select another location, or `--game ID` to launch
directly. Game IDs are `minorescue`, `minotron`, `goatup`, `minos`, `gridrunner`,
`gridrunner-classic`, `fiveaday`, `superoxwars`, `deflex` and `goatup2`.

## Source layout

| Location | Responsibility |
| --- | --- |
| `engine/include/neon`, `engine/src`, `engine/foundation` | Shared math, resources, drawing, effects, audio, input and reusable mechanisms |
| `engine/bindings` | Module-facing names and adapters, with explicit early/late-engine differences |
| `cores` | Game hosts, game-specific behavior, input response, menus, persistence and backend composition |
| `reconstruction/{minos,gridrunner,deflex}` | Game rules and data used by the corresponding cores |
| `platform` | SDL application, devices, iOX, display transforms, filters and bundled-resource access |
| `shared` | Local score and collection achievement storage |
| `tests` and component test directories | Source-rule, captured-execution and regression checks |
| `assets`, `tools` | Editable resources and build/artwork generation |

See [the engine guide](engine/README.md) and [module guide](cores/README.md).
Game rules remain in modules; sharing a mechanism does not imply identical
timing, random streams, allocation policies or voice limits in every game.

## Evidence and limits

The code reconstructs iOS behavior, using Android symbols and implementations
as supporting evidence. Comments retain numerical constraints, source
correspondence, intentional adaptations and unresolved meanings where useful.
Source review, captured-original comparisons and port regression tests establish
different kinds of evidence; passing tests does not prove every original state
or every device. Physical motion-control checks and wider platform testing remain.

The standalone engine checks need no SDL window or GPU:

```sh
cmake -S engine -B build-engine
cmake --build build-engine --config Release
ctest --test-dir build-engine -C Release --output-on-failure
```

Game and real-graphics checks are optional CMake targets, excluded from normal
application builds. They require their supplied fixtures or bundled game assets
as indicated by the test's command-line arguments. Fixtures distinguish captured
original execution from independently calculated rules and saved port references.

## Copyright and licenses

Project contributors offer their own original contributions under
[0BSD](LICENSES/0BSD.txt), to the extent they hold the necessary rights. Credit
is appreciated, not required by that grant.

Jeff Minter has authorized publication of this project as open source.
Llamasoft and the other original rights holders retain copyright and other IP
rights in their games, reconstructed original material and assets. Re-use of
original game assets requires explicit permission from the relevant copyright
holder unless already authorized or permitted by law. Inclusion here is not a
general asset-reuse license.

Read [the licensing scope and retained rights](LICENSE.txt) and
[third-party notices](THIRD-PARTY-NOTICES.txt). Existing library, shader and font
licenses remain applicable; this is not a blanket 0BSD license for every file.

## Collection startup

The collection opens with a WidgetML title over the Vaporwave Neon scene. Tap or
press a button to fade into iOX. Direct `--game ID` launches bypass the title.
`MINO_LIVE_WIDGETS=OFF` builds the shell without the title presentation services.
See [live widgets](modules/live_widgets/README.md) for the resource and host API.

## Motion and controller options

Tilt Calibration in iOX Controls saves a comfortable neutral position for every
game with tilt input. Hold the device still and tap or press a button to confirm.
Controller sticks and triggers are independent of this calibration.

Minos and Super Ox Wars accept right-stick horizontal movement and RT minus LT
for world influence. Equal trigger pressure cancels. Minos keeps its face-button
thrust control. Its pause menu offers phone, tablet and mirrored-tablet touch
layouts, touch disabling when a controller is connected, and a tilt toggle when
both tilt and a controller are available. A single visible toggle is centered.
Controller remapping includes both triggers, Start and Select; existing mappings
retain their assignments. GoatUp 2 touch controls retain their movement/jump
roles until release and adjust their floating origin only for real return strokes,
preventing a stationary or jittering hold from fading to zero movement.

## Validation

With `BUILD_TESTING=ON`, build and run the registered suite with:

```sh
cmake --build build --config Release --target check --parallel
```

Set `MINO_GRAPHICS_CHECKS=ON` to include desktop graphics integration checks.
See [tests/README.md](tests/README.md) for fixture evidence, scratch outputs and
manual developer tools. Checks retain assertions in Release builds. Project-owned
targets enable compiler warnings; vendor targets keep their own warning policy.
Numeric source behavior uses the shared `neon_source_numeric` build contract.
