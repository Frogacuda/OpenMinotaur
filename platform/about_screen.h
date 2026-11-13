#pragma once
#include "launcher_layout.h"
#include "build_info.h"
namespace mino {
inline constexpr const char *kDonationUrl = "http://www.minotaurproject.co.uk/Minotaur/donate.php";
struct AboutSection {
    const char *text;
    bool bold;
    float size;
};
inline constexpr AboutSection kAboutSections[] = {
    {"Minotaur Project: Llamasoft iOS Archive", true, 21},
    {"Version " MINO_RELEASE_VERSION " (public beta)", false, 17},
    {"About The Minotaur Project:", true, 17},
    {"Between 2011 and 2013, Llamasoft released nine classic titles for iOS (iPhone/iPad/iPod "
     "Touch), with each title serving as an homage to a different platform or era of classic "
     "gaming. Though beloved by fans, they were de-listed in 2016 as they were no longer "
     "considered commercially viable to maintain.",
     false, 17},
    {"About OpenMinotaur:", true, 17},
    {"OpenMinotaur is a decompilation and documentation project that rebuilds the nine original "
     "Minotaur Project titles as portable, native C++, and updates them to support a wider range "
     "of modern inputs and displays, alongside minor bugfixes, and UX adjustments to accommodate "
     "end-to-end play on a wider range of configurations. It aims to preserve and future-proof "
     "these titles so they can be enjoyed as hardware and operating systems evolve.",
     false, 17},
    {"About iOX:", true, 17},
    {"iOX unifies the Minotaur Collection as a single, polished compilation, with dynamic menus, "
     "advanced display options, screen filters, control options, and achievement tracking, with "
     "a cheeky retro-mobile interface packed with nostalgic skeuomorphisms.",
     false, 17},
    {"About Llamasoft:", true, 17},
    {"Llamasoft has been developing games since 1982, first as a solo dev and later as a two-man "
     "unit. All titles are copyright Jeff Minter and Ivan Zorzin. The author of iOX, Travis Fahs, "
     "releases his contribution free to any who wish to use or modify it.",
     false, 17},
    {"The titles included are no longer supported or sold by Llamasoft, but if you enjoyed them, "
     "they would appreciate a donation using the button below:",
     false, 17},
};
struct AboutLayout {
    MenuRect panel, header, back, body, donate;
    explicit AboutLayout(const LauncherLayout &l) {
        panel = {0, LauncherLayout::statusBarHeight, l.width,
                 l.height - LauncherLayout::statusBarHeight - 28};
        header = {panel.x, panel.y, panel.w, 43};
        back = {18, panel.y + 8, 84, 27};
        body = {panel.x + 16, panel.y + 60, panel.w - 38, panel.h - 132};
        donate = {panel.cx() - 100, panel.y + panel.h - 49, 200, 32};
    }
};
} // namespace mino
