// Accueil (bibliotheque + acces a la recherche) et Parametres
#include "screens.h"

#include "config.h"
#include "gfx.h"
#include "layout.h"
#include "platform.h"
#include "utils.h"

namespace screens
{
    namespace
    {
        // Ligne de liste sous la souris, ou -1
        int rowAt(const SDL_Rect& list, int scroll, int rowH, int count, int x, int y)
        {
            if (!gfx::inRect(x, y, list)) return -1;
            int idx = (y - list.y + scroll) / rowH;
            int within = (y - list.y + scroll) % rowH;
            if (idx < 0 || idx >= count || within >= rowH - 4) return -1;
            return idx;
        }

        SDL_Rect rowRect(const SDL_Rect& list, int scroll, int rowH, int idx)
        {
            return { list.x, list.y + idx * rowH - scroll, list.w - 14, rowH - 4 };
        }
    }

    // ======================================================
    // ACCUEIL
    // ======================================================
    void renderHome(App& app, int mx, int my, Uint32)
    {
        using namespace layout;

        header("");
        gfx::text("Dossier : " + gfx::fitTail(app.settings.destination, 640), 40, 68, color::MUTED);
        gfx::button(SETTINGS_BTN, "Paramètres", mx, my);

        // ---- colonne gauche : dossiers de la destination ----
        gfx::text("Dossiers  (" + std::to_string(app.folders.size()) + ")", FOLDER_LST.x, 100, color::TEXT);
        gfx::fillRect(FOLDER_LST, color::PANEL);

        const int folderH = int(app.folders.size()) * LIB_ROW_H;
        app.folderScroll = gfx::clampScroll(app.folderScroll, folderH, FOLDER_LST.h);

        if (app.folders.empty())
        {
            gfx::text("Aucun dossier pour l'instant.", FOLDER_LST.x + 15, FOLDER_LST.y + 15, color::MUTED);
        }
        else
        {
            gfx::clip(&FOLDER_LST);
            for (size_t i = 0; i < app.folders.size(); ++i)
            {
                SDL_Rect row = rowRect(FOLDER_LST, app.folderScroll, LIB_ROW_H, int(i));
                if (row.y + row.h < FOLDER_LST.y || row.y > FOLDER_LST.y + FOLDER_LST.h) continue;

                const FolderEntry& f = app.folders[i];
                const bool selected = f.name == app.selectedFolder;
                const bool hover = gfx::inRect(mx, my, row) && gfx::inRect(mx, my, FOLDER_LST);
                gfx::fillRect(row, selected ? color::BUTTON : (hover ? color::INPUT : color::PANEL));

                const std::string count = std::to_string(f.pdfCount) + " PDF";
                const int cw = gfx::textWidth(count);
                gfx::text(gfx::fitHead(f.name, row.w - cw - 40), row.x + 12, row.y + 8, color::TEXT);
                gfx::text(count, row.x + row.w - cw - 12, row.y + 8, selected ? color::TEXT : color::MUTED);
            }
            gfx::clip(nullptr);
            gfx::scrollbar(FOLDER_LST, folderH, app.folderScroll);
        }

        // ---- colonne centrale : PDF du dossier choisi ----
        gfx::text(app.selectedFolder.empty()
                      ? std::string("PDF")
                      : gfx::fitHead("PDF  -  " + app.selectedFolder, PDF_LST.w),
                  PDF_LST.x, 100, color::TEXT);
        gfx::fillRect(PDF_LST, color::PANEL);

        const int pdfH = int(app.pdfs.size()) * LIB_ROW_H;
        app.pdfScroll = gfx::clampScroll(app.pdfScroll, pdfH, PDF_LST.h);

        if (app.selectedFolder.empty())
        {
            gfx::text("Choisis un dossier à gauche.", PDF_LST.x + 15, PDF_LST.y + 15, color::MUTED);
        }
        else if (app.pdfs.empty())
        {
            gfx::text("Aucun PDF dans ce dossier.", PDF_LST.x + 15, PDF_LST.y + 15, color::MUTED);
        }
        else
        {
            gfx::clip(&PDF_LST);
            for (size_t i = 0; i < app.pdfs.size(); ++i)
            {
                SDL_Rect row = rowRect(PDF_LST, app.pdfScroll, LIB_ROW_H, int(i));
                if (row.y + row.h < PDF_LST.y || row.y > PDF_LST.y + PDF_LST.h) continue;

                const PdfEntry& p = app.pdfs[i];
                const bool hover = gfx::inRect(mx, my, row) && gfx::inRect(mx, my, PDF_LST);
                gfx::fillRect(row, hover ? color::HOVER : color::PANEL);

                const std::string size = utils::formatBytes(p.size);
                const int sw = gfx::textWidth(size);
                gfx::text(gfx::fitHead(p.name, row.w - sw - 40), row.x + 12, row.y + 8, color::TEXT);
                gfx::text(size, row.x + row.w - sw - 12, row.y + 8, color::MUTED);
            }
            gfx::clip(nullptr);
            gfx::scrollbar(PDF_LST, pdfH, app.pdfScroll);
        }

        // ---- colonne droite : acces a la recherche ----
        gfx::button(START_BTN, "Rechercher un manga", mx, my, !utils::trim(app.settings.destination).empty());

        if (!app.info.empty())
            gfx::text(gfx::fitHead(app.info, 780), 40, 648, color::FINISHED);
    }

    void clickHome(App& app, int x, int y)
    {
        using namespace layout;

        if (gfx::inRect(x, y, SETTINGS_BTN)) { app.goSettings(); return; }

        if (gfx::inRect(x, y, START_BTN) && !utils::trim(app.settings.destination).empty())
        {
            app.startSearchFlow();
            return;
        }

        int f = rowAt(FOLDER_LST, app.folderScroll, LIB_ROW_H, int(app.folders.size()), x, y);
        if (f >= 0) { app.selectFolder(app.folders[size_t(f)].name); return; }

        int p = rowAt(PDF_LST, app.pdfScroll, LIB_ROW_H, int(app.pdfs.size()), x, y);
        if (p >= 0) platform::openInBrowser(app.pdfs[size_t(p)].path);     // navigateur par defaut
    }

    // ======================================================
    // PARAMETRES
    // ======================================================
    void renderSettings(App& app, int mx, int my, Uint32 ticks)
    {
        using namespace layout;

        header("Paramètres");

        // ---- destination ----
        gfx::text("Dossier de destination", 40, 115, color::TEXT);
        gfx::fillRect(DEST_BOX, color::INPUT);
        const bool focus = app.activeInput == &app.settings.destination;
        gfx::text(gfx::fitTail(app.settings.destination, DEST_BOX.w - 30) + ((focus && (ticks / 500) % 2) ? "|" : ""),
                  DEST_BOX.x + 15, DEST_BOX.y + 14, color::TEXT);
        gfx::button(DEST_BTN, "Changer...", mx, my);

        gfx::checkbox(OPEN_CHECK, app.settings.openWhenDone);
        gfx::text("Ouvrir le dossier à la fin du téléchargement", 85, 228, color::TEXT);

        // ---- base de recherche ----
        gfx::text("Base de recherche", 40, 295, color::TEXT);
        if (app.indexOk)
            gfx::text("Prête  (" + std::to_string(app.indexCount) + " mangas)", 250, 295, color::FINISHED);
        else
            gfx::text("Non générée (créée à la première recherche locale)", 250, 295, color::SEARCHING);
        gfx::button(BASE_BTN, "Mettre à jour la base", mx, my);

        // ---- cache ----
        gfx::text("Cache", 40, 405, color::TEXT);
        gfx::text(utils::formatBytes(app.cacheBytes), 250, 405, color::MUTED);
        const bool confirming = SDL_GetTicks() < app.clearConfirmUntil;
        gfx::button(CACHE_BTN, confirming ? "Cliquer pour confirmer" : "Vider le cache", mx, my, true, true);

        if (!app.info.empty()) gfx::text(app.info, 40, 520, color::FINISHED);

        gfx::button(BACK, "Retour", mx, my);
    }

    void clickSettings(App& app, int x, int y)
    {
        using namespace layout;

        app.activeInput = gfx::inRect(x, y, DEST_BOX) ? &app.settings.destination : nullptr;

        if (gfx::inRect(x, y, DEST_BTN))
        {
            std::string p = platform::browseForFolder();
            if (!p.empty()) { app.settings.destination = p; saveSettings(app.settings); }
        }

        if (gfx::inRect(x, y, OPEN_CHECK))
        {
            app.settings.openWhenDone = !app.settings.openWhenDone;
            saveSettings(app.settings);
        }

        if (gfx::inRect(x, y, BASE_BTN)) { app.updateIndex(); return; }
        if (gfx::inRect(x, y, CACHE_BTN)) app.clearCacheOrConfirm();
        if (gfx::inRect(x, y, BACK)) app.leaveSettings();
    }
}
