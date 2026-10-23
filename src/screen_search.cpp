// Recherche d'un manga et choix des chapitres
#include "screens.h"

#include "config.h"
#include "gfx.h"
#include "layout.h"
#include "utils.h"

namespace screens
{
    // ======================================================
    // RECHERCHE
    // ======================================================
    void renderSearch(App& app, int mx, int my, Uint32 ticks)
    {
        using namespace layout;

        header(".");

        // Deux sources exclusives
        gfx::radio(MODE_DIRECT,  "Anime-Sama (direct)",   app.useDirectSearch);
        gfx::radio(MODE_SITEMAP, "Sitemap (base locale)", !app.useDirectSearch);

        gfx::fillRect(SEARCH_BOX, color::INPUT);
        const bool focus = app.activeInput == &app.searchText;
        if (app.searchText.empty() && !focus)
            gfx::text("Entrez le nom d'un manga...", SEARCH_BOX.x + 15, SEARCH_BOX.y + 14, color::MUTED);
        else
            gfx::text(gfx::fitTail(app.searchText, SEARCH_BOX.w - 30) + ((focus && (ticks / 500) % 2) ? "|" : ""),
                      SEARCH_BOX.x + 15, SEARCH_BOX.y + 14, color::TEXT);

        gfx::button(SEARCH_BTN, "Rechercher", mx, my);

        if (app.searchBusy)
        {
            gfx::text("Recherche en cours sur Anime-Sama...", 40, 205, color::SEARCHING);
            gfx::progress(BAR_A, -1.0, color::SEARCHING, ticks);
        }
        else if (!app.searchError.empty())
        {
            gfx::text(gfx::fitHead(app.searchError, 920), 40, 205, color::SEARCHING);
        }
        else if (app.results.empty())
        {
            if (app.searched)
                gfx::text(app.useDirectSearch && app.showingScans
                              ? "Aucun scan trouvé pour cet anime. Essaie l'autre source."
                              : "Aucun résultat. Vérifie la source ou essaie l'autre méthode.",
                          40, 205, color::MUTED);
        }
        else
        {
            gfx::text(app.showingScans ? "Scans disponibles pour : " + app.selName
                                       : std::to_string(app.results.size()) + " résultat(s)",
                      40, 205, color::MUTED);

            const int contentH = int(app.results.size()) * SEARCH_ROW_H;
            app.searchScroll = gfx::clampScroll(app.searchScroll, contentH, SEARCH_LST.h);

            gfx::clip(&SEARCH_LST);
            for (size_t i = 0; i < app.results.size(); ++i)
            {
                SDL_Rect row = { SEARCH_LST.x, SEARCH_LST.y + int(i) * SEARCH_ROW_H - app.searchScroll,
                                 SEARCH_LST.w - 14, 55 };
                if (row.y + row.h < SEARCH_LST.y || row.y > SEARCH_LST.y + SEARCH_LST.h) continue;

                const bool hover = gfx::inRect(mx, my, row) && gfx::inRect(mx, my, SEARCH_LST);
                gfx::fillRect(row, hover ? color::HOVER : color::PANEL);
                gfx::text(gfx::fitHead(app.results[i].identifier, row.w - 30), row.x + 15, row.y + 6, color::TEXT);
                gfx::text(gfx::fitHead(app.results[i].url, row.w - 30), row.x + 15, row.y + 30, color::MUTED);
            }
            gfx::clip(nullptr);
            gfx::scrollbar(SEARCH_LST, contentH, app.searchScroll);
        }

        gfx::button(BACK, "Accueil", mx, my);
    }

    void clickSearch(App& app, int x, int y)
    {
        using namespace layout;

        if (gfx::inRect(x, y, SEARCH_BOX)) app.activeInput = &app.searchText;

        auto resetResults = [&]()
        {
            app.cancelSearch();
            app.results.clear();
            app.searched = false;
            app.showingScans = false;
            app.searchError.clear();
            app.searchScroll = 0;
        };

        if (gfx::inRect(x, y, MODE_DIRECT))
        {
            resetResults();
            app.useDirectSearch = true;
        }
        if (gfx::inRect(x, y, MODE_SITEMAP))
        {
            resetResults();
            app.useDirectSearch = false;
            app.refreshIndex();
            if (!app.indexOk) { app.startPrepare(true); return; }
        }

        if (gfx::inRect(x, y, SEARCH_BTN)) app.doSearch();
        if (gfx::inRect(x, y, BACK)) { app.goHome(); return; }

        if (gfx::inRect(x, y, SEARCH_LST))
        {
            for (size_t i = 0; i < app.results.size(); ++i)
            {
                SDL_Rect row = { SEARCH_LST.x, SEARCH_LST.y + int(i) * SEARCH_ROW_H - app.searchScroll,
                                 SEARCH_LST.w - 14, 55 };
                if (gfx::inRect(x, y, row)) { app.chooseResult(app.results[i]); break; }
            }
        }
    }

    // ======================================================
    // CHAPITRES
    // ======================================================
    void renderChapters(App& app, int mx, int my, Uint32)
    {
        using namespace layout;

        header("");
        gfx::text(gfx::fitHead(app.scrape.title, 600), 360, 36, color::TEXT);

        gfx::button(SEL_ALL,  "Tout sélectionner",   mx, my);
        gfx::button(SEL_NONE, "Tout désélectionner", mx, my);
        gfx::text(std::to_string(app.selectedCount()) + " / " + std::to_string(app.scrape.chapters.size()) +
                  " sélectionné(s)", 560, 108, color::MUTED);

        const int contentH = int(app.scrape.chapters.size()) * CHAP_ROW_H;
        app.chapterScroll = gfx::clampScroll(app.chapterScroll, contentH, CHAP_LST.h);

        gfx::clip(&CHAP_LST);
        for (size_t i = 0; i < app.scrape.chapters.size(); ++i)
        {
            SDL_Rect row = { CHAP_LST.x, CHAP_LST.y + int(i) * CHAP_ROW_H - app.chapterScroll,
                             CHAP_LST.w - 14, CHAP_ROW_H - 6 };
            if (row.y + row.h < CHAP_LST.y || row.y > CHAP_LST.y + CHAP_LST.h) continue;

            const Chapter& c = app.scrape.chapters[i];
            const bool hover = gfx::inRect(mx, my, row) && gfx::inRect(mx, my, CHAP_LST);
            gfx::fillRect(row, hover ? color::INPUT : color::PANEL);

            SDL_Rect box = { row.x + 12, row.y + 8, 24, 24 };
            gfx::checkbox(box, c.selected);

            gfx::text("Chapitre " + c.name, row.x + 52, row.y + 8, color::TEXT);
            const std::string imgs = std::to_string(c.images) + " image(s)";
            gfx::text(imgs, row.x + row.w - gfx::textWidth(imgs) - 15, row.y + 8, color::MUTED);
        }
        gfx::clip(nullptr);
        gfx::scrollbar(CHAP_LST, contentH, app.chapterScroll);

        gfx::button(BACK, "Retour", mx, my);
        gfx::button(PRIMARY, "Télécharger", mx, my, app.selectedCount() > 0);
    }

    void clickChapters(App& app, int x, int y)
    {
        using namespace layout;

        if (gfx::inRect(x, y, BACK))
        {
            app.screen = Screen::Search;
            app.activeInput = &app.searchText;
            return;
        }
        if (gfx::inRect(x, y, SEL_ALL))  for (auto& c : app.scrape.chapters) c.selected = true;
        if (gfx::inRect(x, y, SEL_NONE)) for (auto& c : app.scrape.chapters) c.selected = false;

        if (gfx::inRect(x, y, PRIMARY) && app.selectedCount() > 0) { app.startDownload(); return; }

        if (gfx::inRect(x, y, CHAP_LST))
        {
            for (size_t i = 0; i < app.scrape.chapters.size(); ++i)
            {
                SDL_Rect row = { CHAP_LST.x, CHAP_LST.y + int(i) * CHAP_ROW_H - app.chapterScroll,
                                 CHAP_LST.w - 14, CHAP_ROW_H - 6 };
                if (gfx::inRect(x, y, row)) { app.scrape.chapters[i].selected ^= true; break; }
            }
        }
    }
}
