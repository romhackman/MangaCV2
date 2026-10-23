#include "gfx.h"

#include <algorithm>
#include <cstdlib>

#include <SDL2/SDL_ttf.h>

#include "config.h"
#include "platform.h"
#include "utils.h"

namespace gfx
{
    namespace
    {
        SDL_Renderer* gRenderer = nullptr;
        TTF_Font* gFont  = nullptr;
        TTF_Font* gTitle = nullptr;

        TTF_Font* openFirst(int size, const std::vector<std::string>& candidates)
        {
            for (const auto& path : candidates)
                if (TTF_Font* f = TTF_OpenFont(path.c_str(), size)) return f;
            return nullptr;
        }
    }

    bool init(SDL_Window* window, std::string& error)
    {
        if (TTF_Init() != 0) { error = "Initialisation de SDL_ttf impossible."; return false; }

        // La police du projet, sinon une police Windows : l'exe marche meme seul.
        const char* win = std::getenv("WINDIR");
        const std::string fonts = std::string(win ? win : "C:/Windows") + "/Fonts/";
        const std::vector<std::string> candidates = {
            cfg::FONT_PATH, fonts + "segoeui.ttf", fonts + "arial.ttf", fonts + "tahoma.ttf"
        };

        gFont = openFirst(20, candidates);
        if (!gFont) { error = "Aucune police trouvee (data/font/yoruka.ttf)."; return false; }
        gTitle = openFirst(28, candidates);
        if (!gTitle) gTitle = gFont;

        gRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!gRenderer) gRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        if (!gRenderer) { error = "Creation du renderer impossible."; return false; }
        return true;
    }

    void shutdown()
    {
        if (gTitle && gTitle != gFont) TTF_CloseFont(gTitle);
        if (gFont) TTF_CloseFont(gFont);
        gTitle = gFont = nullptr;
        if (gRenderer) SDL_DestroyRenderer(gRenderer);
        gRenderer = nullptr;
        TTF_Quit();
    }

    void beginFrame() { setColor(color::BG); SDL_RenderClear(gRenderer); }
    void endFrame()   { SDL_RenderPresent(gRenderer); }

    void setColor(SDL_Color c) { SDL_SetRenderDrawColor(gRenderer, c.r, c.g, c.b, c.a); }
    void fillRect(const SDL_Rect& r, SDL_Color c)   { setColor(c); SDL_RenderFillRect(gRenderer, &r); }
    void strokeRect(const SDL_Rect& r, SDL_Color c) { setColor(c); SDL_RenderDrawRect(gRenderer, &r); }
    void clip(const SDL_Rect* r) { SDL_RenderSetClipRect(gRenderer, r); }

    void text(const std::string& s, int x, int y, SDL_Color c, bool big)
    {
        if (s.empty()) return;
        TTF_Font* font = big ? gTitle : gFont;

        SDL_Surface* surf = TTF_RenderUTF8_Blended(font, s.c_str(), c);
        if (!surf) return;

        if (SDL_Texture* tex = SDL_CreateTextureFromSurface(gRenderer, surf))
        {
            SDL_Rect dst = { x, y, surf->w, surf->h };
            SDL_RenderCopy(gRenderer, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
        }
        SDL_FreeSurface(surf);
    }

    int textWidth(const std::string& s)
    {
        int w = 0, h = 0;
        TTF_SizeUTF8(gFont, s.c_str(), &w, &h);
        return w;
    }

    int lineHeight() { return TTF_FontHeight(gFont); }

    std::string fitHead(std::string s, int maxW)
    {
        if (textWidth(s) <= maxW) return s;
        while (!s.empty() && textWidth(s + "...") > maxW) utils::eraseLastChar(s);
        return s + "...";
    }

    std::string fitTail(std::string s, int maxW)
    {
        if (textWidth(s) <= maxW) return s;
        while (!s.empty() && textWidth("..." + s) > maxW)
        {
            size_t i = 1;
            while (i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) ++i;
            s.erase(0, i);
        }
        return "..." + s;
    }

    bool inRect(int x, int y, const SDL_Rect& r)
    {
        return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
    }

    void button(const SDL_Rect& r, const std::string& label, int mx, int my, bool enabled, bool danger)
    {
        SDL_Color base  = danger ? color::DANGER : color::BUTTON;
        SDL_Color hover = danger ? color::DANGER_HI : color::HOVER;
        fillRect(r, !enabled ? color::INPUT : (inRect(mx, my, r) ? hover : base));

        const std::string shown = fitHead(label, r.w - 16);
        text(shown, r.x + (r.w - textWidth(shown)) / 2, r.y + (r.h - lineHeight()) / 2,
             enabled ? color::TEXT : color::MUTED);
    }

    void checkbox(const SDL_Rect& r, bool checked)
    {
        fillRect(r, color::CHECKBOX);
        strokeRect(r, color::BORDER);
        if (checked)
        {
            const int m = r.w / 5;
            SDL_Rect in = { r.x + m, r.y + m, r.w - 2 * m, r.h - 2 * m };
            fillRect(in, color::FINISHED);
        }
    }

    void radio(const SDL_Rect& r, const std::string& label, bool selected)
    {
        fillRect(r, selected ? color::BUTTON : color::PANEL);
        strokeRect(r, selected ? color::HOVER : color::BORDER);

        SDL_Rect box = { r.x + 8, r.y + 6, 18, 18 };
        fillRect(box, color::INPUT);
        strokeRect(box, color::BORDER);
        if (selected)
        {
            SDL_Rect dot = { box.x + 4, box.y + 4, 10, 10 };
            fillRect(dot, color::FINISHED);
        }
        text(label, r.x + 34, r.y + (r.h - lineHeight()) / 2, color::TEXT);
    }

    void progress(const SDL_Rect& r, double fraction, SDL_Color fill, Uint32 ticks)
    {
        fillRect(r, color::INPUT);

        if (fraction < 0.0)                       // animation "en cours"
        {
            int w = r.w / 4;
            int pos = static_cast<int>((ticks / 6) % (r.w + w)) - w;
            int x0 = std::max(0, pos), x1 = std::min(r.w, pos + w);
            if (x1 > x0)
            {
                SDL_Rect f = { r.x + x0, r.y, x1 - x0, r.h };
                fillRect(f, fill);
            }
            return;
        }

        fraction = std::clamp(fraction, 0.0, 1.0);
        SDL_Rect f = { r.x, r.y, static_cast<int>(r.w * fraction), r.h };
        fillRect(f, fill);

        const std::string pct = std::to_string(static_cast<int>(fraction * 100)) + " %";
        text(pct, r.x + (r.w - textWidth(pct)) / 2, r.y + (r.h - lineHeight()) / 2, color::TEXT);
    }

    void logBox(const std::vector<std::string>& lines, int x, int y, int w, int rows)
    {
        SDL_Rect box = { x, y, w, rows * 26 + 20 };
        fillRect(box, color::PANEL);

        int start = std::max(0, static_cast<int>(lines.size()) - rows);
        for (size_t i = static_cast<size_t>(start), row = 0; i < lines.size(); ++i, ++row)
            text(fitHead(lines[i], w - 30), x + 15, y + 10 + static_cast<int>(row) * 26, color::MUTED);
    }

    void scrollbar(const SDL_Rect& area, int contentH, int offset)
    {
        if (contentH <= area.h) return;

        SDL_Rect bg = { area.x + area.w - 8, area.y, 8, area.h };
        fillRect(bg, color::INPUT);

        int barH = std::max(30, area.h * area.h / contentH);
        int maxOff = contentH - area.h;
        int barY = area.y + static_cast<int>((area.h - barH) * (double(offset) / maxOff));
        SDL_Rect bar = { bg.x, barY, bg.w, barH };
        fillRect(bar, color::BUTTON);
    }

    int clampScroll(int offset, int contentH, int viewH)
    {
        return std::clamp(offset, 0, std::max(0, contentH - viewH));
    }
}
