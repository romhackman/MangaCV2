// ==========================================================
// config.h  -  Constantes globales : fenetre, fichiers, couleurs
// ==========================================================
#pragma once

#include <SDL2/SDL.h>

namespace cfg
{
    constexpr int WINDOW_W = 1000;
    constexpr int WINDOW_H = 700;

    constexpr const char* APP_NAME    = "Manga Downloader";
    constexpr const char* APP_VERSION = "v0.2.0";

    // Tous les chemins sont relatifs au dossier de l'exe
    // (le programme s'y place au demarrage).
    constexpr const char* FONT_PATH     = "data/font/yoruka.ttf";
    constexpr const char* SETTINGS_FILE = "settings.json";
    constexpr const char* DOMAIN_FILE   = "data/domaine.json";
    constexpr const char* HERITAGE_FILE = "data/heritage.json";
    constexpr const char* LOG_FILE      = "last_run.log";

    constexpr const char* TEMP_DIR      = "temp";
    constexpr const char* SCAN_DIR      = "scan";
    constexpr const char* DATA_TEMP_DIR = "data/temp";

    // Page d'entree qui redirige vers le domaine courant du site
    constexpr const char* PORTAL_URL    = "https://anime-sama.pw";
    // Domaine utilise tant que data/domaine.json n'existe pas
    constexpr const char* DEFAULT_BASE  = "https://anime-sama.to";
}

namespace color
{
    constexpr SDL_Color BG        = { 18,  18,  24, 255 };
    constexpr SDL_Color PANEL     = { 28,  28,  38, 255 };
    constexpr SDL_Color INPUT     = { 40,  40,  52, 255 };
    constexpr SDL_Color BUTTON    = { 70,  90, 180, 255 };
    constexpr SDL_Color HOVER     = { 90, 115, 220, 255 };
    constexpr SDL_Color TEXT      = {235, 235, 240, 255 };
    constexpr SDL_Color MUTED     = {150, 150, 160, 255 };
    constexpr SDL_Color SEARCHING = {190,  55,  55, 255 };
    constexpr SDL_Color FINISHED  = { 50, 180, 100, 255 };
    constexpr SDL_Color CHECKBOX  = { 55,  55,  70, 255 };
    constexpr SDL_Color BORDER    = { 90,  90, 110, 255 };
    constexpr SDL_Color DANGER    = {160,  50,  50, 255 };
    constexpr SDL_Color DANGER_HI = {200,  70,  70, 255 };
}
