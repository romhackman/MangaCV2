// Ecrans d'attente : preparation, recuperation, telechargement, fin, erreur
#include "screens.h"

#include <algorithm>

#include "config.h"
#include "gfx.h"
#include "layout.h"
#include "utils.h"

namespace screens
{
    void renderPrepare(App& app, int mx, int my, Uint32 ticks)
    {
        using namespace layout;

        const int stage = std::max(1, app.task.progress().stage.load());
        header("Préparation  -  étape " + std::to_string(stage) + " / 2");

        gfx::text("Progression globale", 40, 195, color::MUTED);
        gfx::progress(BAR_A, (stage - 1) / 2.0, color::BUTTON, ticks);

        gfx::text(stage == 1 ? "Recherche du nom de domaine..." : "Création de la base de recherche...",
                  40, 285, color::SEARCHING);
        gfx::progress(BAR_B, -1.0, color::SEARCHING, ticks);

        gfx::logBox(app.task.log().tail(6), 40, 400, 920, 6);
        gfx::button(BACK, "Annuler", mx, my, true, true);
    }

    void renderScraping(App& app, int mx, int my, Uint32 ticks)
    {
        using namespace layout;

        header("Récupération des chapitres");
        gfx::text(gfx::fitHead(app.selName, 900), 40, 190, color::TEXT);
        gfx::text("Recherche en cours...", 40, 225, color::SEARCHING);
        gfx::progress(BAR_B, -1.0, color::SEARCHING, ticks);

        gfx::logBox(app.task.log().tail(6), 40, 400, 920, 6);
        gfx::button(BACK, "Annuler", mx, my, true, true);
    }

    void renderDownload(App& app, int mx, int my, Uint32 ticks)
    {
        using namespace layout;

        Progress& pr = app.task.progress();
        const Phase phase = pr.current();
        const int stage = pr.stage.load(), stageTotal = std::max(1, pr.stageTotal.load());
        const int item = pr.item.load(), itemTotal = pr.itemTotal.load();

        switch (phase)
        {
            case Phase::Pdf:  header("Création des PDF"); break;
            case Phase::Copy: header("Copie vers le dossier de destination"); break;
            default:          header("Téléchargement"); break;
        }
        gfx::text(gfx::fitHead(app.scrape.title, 900), 40, 150, color::TEXT);

        if (phase == Phase::Pdf)
        {
            gfx::text("Conversion des images en PDF...  " + std::to_string(std::min(stage + 1, stageTotal)) +
                      " / " + std::to_string(stageTotal), 40, 195, color::SEARCHING);
            gfx::progress(BAR_A, double(stage) / stageTotal, color::SEARCHING, ticks);
        }
        else if (phase == Phase::Copy)
        {
            gfx::text("Copie des fichiers...  " + std::to_string(item) + " / " + std::to_string(itemTotal),
                      40, 195, color::SEARCHING);
            gfx::progress(BAR_A, itemTotal > 0 ? double(item) / itemTotal : -1.0, color::SEARCHING, ticks);
            gfx::text(gfx::fitTail(app.settings.destination, 900), 40, 285, color::MUTED);
        }
        else
        {
            gfx::text("Chapitre " + std::to_string(std::max(1, stage)) + " / " + std::to_string(stageTotal),
                      40, 195, color::SEARCHING);
            double global = -1.0;
            if (itemTotal > 0)
                global = std::clamp((std::max(0, stage - 1) + double(item) / itemTotal) / stageTotal, 0.0, 1.0);
            gfx::progress(BAR_A, global, color::SEARCHING, ticks);

            gfx::text(itemTotal > 0 ? "Image " + std::to_string(item) + " / " + std::to_string(itemTotal)
                                    : std::string("Démarrage..."), 40, 285, color::MUTED);
            gfx::progress(BAR_B, itemTotal > 0 ? double(item) / itemTotal : -1.0, color::BUTTON, ticks);
        }

        gfx::logBox(app.task.log().tail(6), 40, 400, 920, 6);
        gfx::button(BACK, "Annuler", mx, my, true, true);
    }

    void renderDone(App& app, int mx, int my, Uint32 ticks)
    {
        using namespace layout;

        const bool warn = !app.outcome.missing.empty();
        header(warn ? "Terminé avec avertissements" : "Terminé");
        gfx::progress(BAR_A, 1.0, warn ? color::SEARCHING : color::FINISHED, ticks);

        gfx::text("PDF créés : " + std::to_string(app.outcome.copied) + " fichier(s)", 40, 290,
                  warn ? color::TEXT : color::FINISHED);
        gfx::text(gfx::fitTail(app.settings.destination, 900), 40, 330, color::MUTED);

        if (warn)
        {
            std::string list;
            for (size_t i = 0; i < app.outcome.missing.size(); ++i)
            {
                list += app.outcome.missing[i];
                if (i + 1 < app.outcome.missing.size()) list += ", ";
            }
            gfx::text(std::to_string(app.outcome.missing.size()) + " chapitre(s) non créé(s) :", 40, 390, color::SEARCHING);
            gfx::text(gfx::fitHead(list, 900), 40, 425, color::TEXT);
            gfx::text("Détails dans le fichier last_run.log (dossier de l'application).", 40, 470, color::MUTED);
        }

        gfx::button(PRIMARY, "Ouvrir le dossier", mx, my);
        gfx::button(BACK, "Accueil", mx, my);
    }

    void renderError(App& app, int mx, int my, Uint32)
    {
        header("Erreur");
        gfx::text("Une erreur est survenue", 40, 190, color::SEARCHING);

        // Message sur plusieurs lignes, coupe aux espaces
        std::string msg = app.errorMsg;
        int y = 235;
        while (!msg.empty() && y < 560)
        {
            std::string line = msg;
            while (gfx::textWidth(line) > 900 && line.size() > 1) utils::eraseLastChar(line);
            if (line.size() < msg.size())
            {
                size_t sp = line.find_last_of(' ');
                if (sp != std::string::npos && sp > 0) line = line.substr(0, sp);
            }
            gfx::text(line, 40, y, color::TEXT);
            msg.erase(0, line.size());
            msg = utils::trim(msg);
            y += 28;
        }

        gfx::button(layout::BACK, "Accueil", mx, my);
    }

    void clickProgress(App& app, int x, int y)
    {
        using namespace layout;

        switch (app.screen)
        {
            case Screen::Prepare:
                if (gfx::inRect(x, y, BACK)) app.goHome("Préparation annulée.");
                break;

            case Screen::Scraping:
                if (gfx::inRect(x, y, BACK))
                {
                    app.task.cancel();
                    app.screen = Screen::Search;
                    app.activeInput = &app.searchText;
                }
                break;

            case Screen::Download:
                if (gfx::inRect(x, y, BACK)) app.cancelDownload();
                break;

            case Screen::Done:
                if (gfx::inRect(x, y, PRIMARY)) app.openDestination();
                if (gfx::inRect(x, y, BACK))    app.goHome();
                break;

            case Screen::Error:
                if (gfx::inRect(x, y, BACK)) app.goHome();
                break;

            default: break;
        }
    }
}
