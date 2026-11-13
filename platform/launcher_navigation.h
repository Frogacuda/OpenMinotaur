// iOX navigation is independent of rendering, SDL and game lifetimes.
#pragma once
#include <array>
#include <cstddef>
#include <vector>

namespace mino::iox {
enum class Screen {
    Home,
    Options,
    Wallpapers,
    Players,
    Mapping,
    About,
    Game,
    Gridrunner,
    Message,
    Quit,
    Achievements,
    AchievementIndex,
    Count
};

// Screen-local slots. Data rows follow the header without a reserved/hidden item.
namespace home {
constexpr int games = 9, options = games, achievements = games + 1, about = games + 2,
              count = games + 3;
}
namespace options {
constexpr int screenTab = 0, controlsTab = 1, back = 2, firstRow = 3;
}
namespace about {
constexpr int body = 0, donate = 1, back = 2, count = 3;
}

constexpr bool isOptionsScreen(Screen screen) {
    return screen == Screen::Options || screen == Screen::Wallpapers || screen == Screen::Players ||
           screen == Screen::Mapping;
}

// Every entry point goes through this navigator. The route records the originating
// screen and focus; nested screens and messages need no hard-coded parent rules.
// LastChange is a synchronous presentation boundary, not an animation engine.
// The outgoing screen can be captured before rendering the destination.
class Navigation {
  public:
    enum class Change { Open, Back, Home };
    struct Route {
        Screen screen;
        int focus;
    };
    struct LastChange {
        Route from, to;
        Change kind;
        unsigned serial;
    };

    Screen screen() const {
        return screen_;
    }
    int &focus() {
        return focus_[index(screen_)];
    }
    int focus() const {
        return focus_[index(screen_)];
    }
    int &homeFocus() {
        return focus_[index(Screen::Home)];
    }
    int homeFocus() const {
        return focus_[index(Screen::Home)];
    }
    void selectHome(int selected) {
        homeFocus() = selected;
        // The host can select another title while a message is open. Returning
        // through the history must preserve that explicit host selection.
        for (auto &route : history_)
            if (route.screen == Screen::Home)
                route.focus = selected;
    }
    const LastChange &lastChange() const {
        return lastChange_;
    }
    std::size_t depth() const {
        return history_.size();
    }

    void open(Screen destination, int initialFocus = 0) {
        const Route from{screen_, focus()};
        if (destination != screen_)
            history_.push_back(from);
        screen_ = destination;
        focus() = initialFocus;
        changed(from, Change::Open);
    }
    bool back() {
        if (history_.empty())
            return false;
        const Route from{screen_, focus()};
        const Route parent = history_.back();
        history_.pop_back();
        screen_ = parent.screen;
        focus() = parent.focus;
        changed(from, Change::Back);
        return true;
    }
    void home() {
        const Route from{screen_, focus()};
        history_.clear();
        screen_ = Screen::Home;
        changed(from, Change::Home);
    }

  private:
    static constexpr std::size_t index(Screen screen) {
        return static_cast<std::size_t>(screen);
    }
    void changed(Route from, Change kind) {
        lastChange_ = {from, {screen_, focus()}, kind, lastChange_.serial + 1};
    }
    Screen screen_ = Screen::Home;
    std::array<int, static_cast<std::size_t>(Screen::Count)> focus_{};
    std::vector<Route> history_;
    LastChange lastChange_{{Screen::Home, 0}, {Screen::Home, 0}, Change::Home, 0};
};
} // namespace mino::iox
