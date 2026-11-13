# Checks and evidence

Configure the collection with `BUILD_TESTING=ON` (the default), then run:

```sh
cmake --build build --config Release --target check --parallel
```

`check` builds its registered dependencies and runs CTest with failure output.
`check-build` only builds them. To rerun built checks, use
`ctest --test-dir build -C Release --output-on-failure`.
Ordinary application builds exclude check executables; use these explicit targets
to build and run them.

Desktop standalone configurations use the same `check` target. Each selected game
gets a byte-for-byte bundled-asset check. With `MINO_GRAPHICS_CHECKS=ON`, each also
gets a context-loss resume check and, on Windows, a window lifecycle check.
`MINO_STANDALONE_GAME=all` validates all ten standalone bundles and shells.
CPU checks run by default. Set `MINO_GRAPHICS_CHECKS=ON` to register desktop
window/context, launcher, CRT and resume checks; these require a working GL
driver. Generated output stays under `build/check-output`, never source assets.
The shared engine's suite is included through `NEON_BUILD_CHECKS`; the engine
can also be configured independently. For existing build caches, enable `GRR_BUILD_CHECKS`, `MINOS_BUILD_CHECKS` and
`DEFLEX_BUILD_CHECKS` to include all rule-library suites; cached opt-outs are
respected. Game rule libraries retain their explicit
fixture-backed CTest registrations. Some developer trace/inspection executables
remain manual tools, rather than argument-free tests.

Checks distinguish three kinds of evidence: independently captured original
execution, source-level behavioral rules, and regression comparisons against a
saved implementation. Passing one does not establish the other two.

Files named `legacy_*` and targets named `*-reference` are frozen implementations
used as regression controls for shared-engine extraction. They are test inputs,
not alternate production engines or public compatibility APIs. Keep their bodies
independent of the helpers they exercise; changing both sides together defeats
the comparison. Original-execution fixtures carry their own provenance in the
rule library's fixture README. An unpublished capture harness is not required to
run a committed fixture.

Gridrunner's `head_entry.cpp` calibration model belongs only to its replay check;
the production library uses `native_head`. Both observed-entry and negative-control
checks remain available when `GRR_BUILD_CHECKS=ON`.

Light Synth is a presentation service, not an original-execution conformance
target. Its validation concerns scene behavior, rendering and resource lifecycle.

## Platform validation

Use the build commands in the root README. Validate a Release collection build
on each supported desktop platform and an ARM64 APK on a physical Android device.
Build one standalone separately as well; `MINO_STANDALONE_GAME=all` provides the
full desktop set. Android selects one game with `-PstandaloneGame=GAME`.

- Launch from a clean directory without loose assets. Confirm that the bundled
  resources load and the launcher contains only the supported game entries.
- Open every game, including Gridrunner Classic. Check loading artwork, gameplay,
  sound, pause/resume, return to the title and return to iOX. Exercise controller
  navigation and touch or mouse input where applicable.
- In Minotron, check level/player/mode selection, death/restart, rescues, spread
  and reflective shots, and normal level completion. In Rescue, check solar,
  deep-space and tank/jet modes.
- Change window size/fullscreen on desktop. On Android, background and restore
  during both gameplay and the collection title; lock/unlock and rotate where
  supported. Check graphics, feedback effects, controls and audio after recovery.
- Close and relaunch. Confirm settings, scores and existing progress survive,
  including GoatUp 2 saves using the retained `goatup2-stage` storage identity.
- For a standalone build, confirm only its game is available and title-screen
  Back exits the app. For the direct Android APK, inspect that native libraries
  are under `lib/arm64-v8a` only.

Record the build configuration, OS/device, graphics driver and any failing route.
Automated checks establish bundled-resource integrity and covered behavior;
physical-device graphics, audio, input and operating-system lifecycle checks
remain a separate release qualification step.

## Formatting and continuous integration

Project-owned C/C++ sources use `.clang-format` with clang-format 22.1.3:

```sh
python -m pip install clang-format==22.1.3
python tools/format_sources.py --check
# Apply the same formatting locally:
python tools/format_sources.py --fix
```

The formatter excludes third-party code, generated build files and data-only
`.inc` tables. Text checkout uses LF, with CRLF retained for Windows batch files.
Fixture readers accept either line ending.

The GitHub Actions workflow builds the collection and runs `check` on Windows
with MSVC and Linux with GCC and Clang. It also configures and tests the independent
engine, Gridrunner, Minos, Deflex and GoatUp 2 rule suites. Graphics/device checks
remain separate from these headless jobs.
