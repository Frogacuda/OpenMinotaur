# CRT Consumer / CRT 2

The preset and three GLSL files are unchanged upstream sources. The revision and
hashes are recorded in provenance.json. The linear and NTSC module headers
specify GPL version 2 or later; see LICENSES/CRT-CONSUMER.txt at the source root.

The host uses the RetroArch parameter UI defaults, with S-Video enabled and
curvature and vignette set to zero. These values become shader constants so the
compiler can remove the disabled branches. Gamma correction, seven-tap S-Video
processing, sharpness, scanlines and the screen-space mask remain intact.

The common CRT runner keeps the beam/mask axes aligned with the screen's long
edge and applies the game's orientation before filtering. Native upright RGB8
input can bypass the identity input copy. The final RGB8 target and presentation
copy preserve the screen mask's interpolation under rotation and letterboxing.
