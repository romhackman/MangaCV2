// ==========================================================
// screens.h  -  Affichage et clics de chaque ecran
// ==========================================================
#pragma once

#include <SDL2/SDL.h>

#include "app.h"

namespace screens
{
    // Point d'entree : dessine l'ecran courant / traite un clic gauche
    void render(App& app, int mx, int my, Uint32 ticks);
    void click(App& app, int x, int y, bool& running);

    // Titre commun a tous les ecrans
    void header(const std::string& subtitle);

    // ---- accueil + parametres (screen_home.cpp) ----
    void renderHome(App& app, int mx, int my, Uint32 ticks);
    void clickHome(App& app, int x, int y);
    void renderSettings(App& app, int mx, int my, Uint32 ticks);
    void clickSettings(App& app, int x, int y);

    // ---- recherche + chapitres (screen_search.cpp) ----
    void renderSearch(App& app, int mx, int my, Uint32 ticks);
    void clickSearch(App& app, int x, int y);
    void renderChapters(App& app, int mx, int my, Uint32 ticks);
    void clickChapters(App& app, int x, int y);

    // ---- progression, fin, erreur (screen_progress.cpp) ----
    void renderPrepare(App& app, int mx, int my, Uint32 ticks);
    void renderScraping(App& app, int mx, int my, Uint32 ticks);
    void renderDownload(App& app, int mx, int my, Uint32 ticks);
    void renderDone(App& app, int mx, int my, Uint32 ticks);
    void renderError(App& app, int mx, int my, Uint32 ticks);
    void clickProgress(App& app, int x, int y);   // Prepare, Scraping, Download, Done, Error
}
