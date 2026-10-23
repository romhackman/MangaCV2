// ==========================================================
// gfx.h  -  Primitives de dessin SDL2 / SDL_ttf
// ==========================================================
#pragma once

#include <string>
#include <vector>

#include <SDL2/SDL.h>

namespace gfx
{
    // Cree le renderer et charge les polices. false + message si echec.
    bool init(SDL_Window* window, std::string& error);
    void shutdown();

    void beginFrame();
    void endFrame();

    void setColor(SDL_Color c);
    void fillRect(const SDL_Rect& r, SDL_Color c);
    void strokeRect(const SDL_Rect& r, SDL_Color c);
    void clip(const SDL_Rect* r);                 // nullptr = pas de clip

    void text(const std::string& s, int x, int y, SDL_Color c, bool big = false);
    int  textWidth(const std::string& s);
    int  lineHeight();
    std::string fitHead(std::string s, int maxW);   // "Un titre tr..."
    std::string fitTail(std::string s, int maxW);   // "...dossier\fichier"

    bool inRect(int x, int y, const SDL_Rect& r);

    // ---- composants ----
    void button(const SDL_Rect& r, const std::string& label, int mx, int my,
                bool enabled = true, bool danger = false);
    void checkbox(const SDL_Rect& r, bool checked);
    void radio(const SDL_Rect& r, const std::string& label, bool selected);
    void progress(const SDL_Rect& r, double fraction, SDL_Color fill, Uint32 ticks);   // < 0 = indetermine
    void logBox(const std::vector<std::string>& lines, int x, int y, int w, int rows);
    void scrollbar(const SDL_Rect& area, int contentH, int offset);
    int  clampScroll(int offset, int contentH, int viewH);
}
