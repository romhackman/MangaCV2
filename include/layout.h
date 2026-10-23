// ==========================================================
// layout.h  -  Position des elements de chaque ecran
// ==========================================================
#pragma once

#include <SDL2/SDL.h>

namespace layout
{
    // ---- communs ----
    inline constexpr SDL_Rect QUIT    = { 850, 640, 110,  40 };
    inline constexpr SDL_Rect BACK    = {  40, 640, 180,  40 };
    inline constexpr SDL_Rect PRIMARY = { 560, 640, 270,  40 };

    // ---- accueil ----
    inline constexpr SDL_Rect SETTINGS_BTN = { 820,  20, 140,  40 };
    inline constexpr SDL_Rect FOLDER_LST   = {  40, 130, 290, 490 };
    inline constexpr SDL_Rect PDF_LST      = { 345, 130, 370, 490 };
    inline constexpr SDL_Rect START_BTN    = { 735, 130, 225,  60 };
    inline constexpr int      LIB_ROW_H    = 44;

    // ---- parametres ----
    inline constexpr SDL_Rect DEST_BOX   = {  40, 150, 700,  50 };
    inline constexpr SDL_Rect DEST_BTN   = { 760, 150, 200,  50 };
    inline constexpr SDL_Rect OPEN_CHECK = {  40, 225,  30,  30 };
    inline constexpr SDL_Rect BASE_BTN   = {  40, 340, 300,  45 };
    inline constexpr SDL_Rect CACHE_BTN  = {  40, 450, 300,  45 };

    // ---- recherche ----
    inline constexpr SDL_Rect MODE_DIRECT  = {  40,  78, 270,  30 };
    inline constexpr SDL_Rect MODE_SITEMAP = { 320,  78, 270,  30 };
    inline constexpr SDL_Rect SEARCH_BOX   = {  40, 120, 700,  50 };
    inline constexpr SDL_Rect SEARCH_BTN   = { 760, 120, 200,  50 };
    inline constexpr SDL_Rect SEARCH_LST   = {  40, 230, 920, 390 };
    inline constexpr int      SEARCH_ROW_H = 65;

    // ---- chapitres ----
    inline constexpr SDL_Rect SEL_ALL   = {  40, 100, 220,  40 };
    inline constexpr SDL_Rect SEL_NONE  = { 275, 100, 260,  40 };
    inline constexpr SDL_Rect CHAP_LST  = {  40, 155, 920, 465 };
    inline constexpr int      CHAP_ROW_H = 48;

    // ---- progression ----
    inline constexpr SDL_Rect BAR_A = { 40, 230, 920, 36 };
    inline constexpr SDL_Rect BAR_B = { 40, 320, 920, 36 };
}
