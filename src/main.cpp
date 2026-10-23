// ==========================================================
// main.cpp  -  Point d'entree : fenetre SDL2 et boucle principale
//
// Un seul executable, sans fenetre de terminal. Si l'application est
// lancee depuis un terminal, les logs y sont affiches.
// ==========================================================
#include <algorithm>
#include <string>

#include <SDL2/SDL.h>

#include "http.h"        // avant windows.h (winsock2)

#include <windows.h>
#include <objbase.h>

#include "app.h"
#include "config.h"
#include "gfx.h"
#include "platform.h"
#include "screens.h"
#include "utils.h"

// Ctrl+V : colle le texte du presse-papiers (sans retour a la ligne)
static void pasteInto(std::string& target)
{
    char* clip = SDL_GetClipboardText();
    if (!clip) return;

    std::string c = clip;
    c.erase(std::remove(c.begin(), c.end(), '\n'), c.end());
    c.erase(std::remove(c.begin(), c.end(), '\r'), c.end());
    target += c;
    SDL_free(clip);
}

static void handleKey(App& app, const SDL_KeyboardEvent& key)
{
    const SDL_Keycode k = key.keysym.sym;

    if (app.activeInput)
    {
        if (k == SDLK_BACKSPACE) utils::eraseLastChar(*app.activeInput);
        if (k == SDLK_v && (SDL_GetModState() & KMOD_CTRL)) pasteInto(*app.activeInput);
    }

    if (k == SDLK_RETURN || k == SDLK_KP_ENTER)
    {
        if (app.screen == Screen::Search) app.doSearch();
        else if (app.screen == Screen::Settings && app.activeInput == &app.settings.destination)
            saveSettings(app.settings);
    }

    if (k == SDLK_ESCAPE) app.activeInput = nullptr;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    platform::attachParentConsole();     // logs visibles seulement si lance depuis un terminal
    platform::enterExeDir();             // "data/", "temp/"... relatifs a l'exe

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    http::globalInit();

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        platform::showError("Erreur", std::string("SDL : ") + SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(cfg::APP_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          cfg::WINDOW_W, cfg::WINDOW_H, SDL_WINDOW_SHOWN);
    std::string error;
    if (!window || !gfx::init(window, error))
    {
        platform::showError("Erreur", window ? error : std::string("Creation de la fenetre impossible."));
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    {
        App app;
        app.settings = loadSettings();
        app.refreshIndex();
        app.refreshLibrary();

        SDL_StartTextInput();
        bool running = true;

        while (running)
        {
            SDL_Event e;
            while (SDL_PollEvent(&e))
            {
                switch (e.type)
                {
                    case SDL_QUIT:
                        running = false;
                        break;

                    case SDL_MOUSEBUTTONDOWN:
                        if (e.button.button == SDL_BUTTON_LEFT)
                            screens::click(app, e.button.x, e.button.y, running);
                        break;

                    case SDL_MOUSEWHEEL:
                    {
                        int mx = 0, my = 0;
                        SDL_GetMouseState(&mx, &my);
                        app.onWheel(mx, my, e.wheel.y);
                        break;
                    }

                    case SDL_KEYDOWN:
                        handleKey(app, e.key);
                        break;

                    case SDL_TEXTINPUT:
                        if (app.activeInput) *app.activeInput += e.text.text;
                        break;
                }
            }

            app.update();

            int mx = 0, my = 0;
            SDL_GetMouseState(&mx, &my);
            screens::render(app, mx, my, SDL_GetTicks());
        }

        app.stopAll();
        saveSettings(app.settings);
        SDL_StopTextInput();
    }   // l'App est detruite ici, avant l'arret de SDL

    gfx::shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    http::globalCleanup();
    CoUninitialize();
    return 0;
}
