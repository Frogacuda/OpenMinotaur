# Resources

`games/` contains the graphics, audio, level/effect data and game-specific menu
artwork used by the nine modules. `GAME-ASSETS.json` records their hashes and
is checked by the build-time resource bundler. Desktop builds embed the resulting
archive; Android includes it in the APK. No runtime extraction is required.

`ui/` contains iOX artwork, wallpapers and font sources. The checked-in C++ UI
arrays can be regenerated with the optional artwork tools. `shaders/royale/`
contains the CRT shader sources, masks, preset and their existing notices.

Wallpaper pairs in `ui/wallpapers/` use 1280 x 960 landscape and 960 x 1280
portrait crops, matching the 4:3 and 3:4 iOX canvases with square pixels. The
wallpaper baker embeds the supplied PNG bytes without resizing or recropping.

Original game resources are included with Llamasoft's permission and remain
the property of their respective rights holders. Their re-use requires explicit
permission from the relevant copyright holder unless already authorized or
permitted by law. They are not covered by the project's 0BSD contribution grant.
Separately licensed fonts, shaders and other third-party resources retain their
own terms. See [LICENSE.txt](../LICENSE.txt) and the third-party notices.

## Generated presentation resources

Baked presentation payloads live in `assets/baked` as binary resources with a
size/hash manifest. The build verifies them and generates C++ byte includes in
the build directory. Game resources are also embedded at build time, through
`tools/bundle_game_assets.py`. See [baked/README.md](baked/README.md) for replacement
and verification commands. These payloads preserve the shipped bytes; they are
not newly recovered authoring originals.
