// Exercise the shared Windows shortcut in both collection and standalone shells.
#define MINOTAUR_SHELL_TEST
#include "../platform/main.cpp"
#include <stdexcept>
#include <iostream>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (false)
int main(int, char **) {
    try {
        CHECK(SDL_Init(SDL_INIT_VIDEO));
        App app;
        app.host.app = &app;
        app.window = SDL_CreateWindow("Fullscreen check", 640, 480,
                                      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
        CHECK(app.window);
        CHECK(createContext(app, false));
        CHECK(app.display.init(app.glES));
        int width = 0, height = 0;
        SDL_GetWindowSize(app.window, &width, &height);
        SDL_KeyboardEvent key{};
        key.type = SDL_EVENT_KEY_DOWN;
        key.key = SDLK_RETURN;
        CHECK(!fullscreenShortcut(app, key));
        key.mod = SDL_KMOD_LALT;
        CHECK(fullscreenShortcut(app, key));
        CHECK(SDL_GetWindowFlags(app.window) & SDL_WINDOW_FULLSCREEN);
        key.repeat = true;
        CHECK(fullscreenShortcut(app, key));
        CHECK(SDL_GetWindowFlags(app.window) & SDL_WINDOW_FULLSCREEN);
        key.repeat = false;
        key.key = SDLK_KP_ENTER;
        key.mod = SDL_KMOD_RALT;
        CHECK(fullscreenShortcut(app, key));
        CHECK(!(SDL_GetWindowFlags(app.window) & SDL_WINDOW_FULLSCREEN));
        int restoredW = 0, restoredH = 0;
        SDL_GetWindowSize(app.window, &restoredW, &restoredH);
        CHECK(width == restoredW && height == restoredH);
        CHECK(app.gatePads);
#ifdef MINO_STANDALONE_GAME
        CHECK(standaloneDisplaySettings().orientation == Orientation::Landscape);
        CHECK(gameInputSettings(app).deadzone == .20f && !gameInputSettings(app).disableTouch);
#else
        CHECK(app.settings.get("global", "window_mode", "") == "windowed");
#endif
        CHECK(glGetError() == GL_NO_ERROR);
        app.display.shutdown();
        SDL_GL_DestroyContext(app.gl);
        SDL_DestroyWindow(app.window);
        SDL_Quit();
        std::cout
            << "Alt+Enter/keypad Enter, both Alt keys, repeat suppression and window restoration passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
