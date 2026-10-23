// ==========================================================
// app.h  -  Etat de l'application et enchainement des ecrans
//
//   Accueil <-> Parametres
//   Accueil -> (Preparation) -> Recherche -> Chapitres
//           -> Telechargement + PDF + copie -> Termine
// ==========================================================
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <SDL2/SDL.h>
#include <nlohmann/json.hpp>

#include "model.h"
#include "pipeline.h"
#include "settings.h"
#include "storage.h"
#include "task.h"

enum class Screen { Home, Settings, Prepare, Search, Scraping, Chapters, Download, Done, Error };

struct App
{
    Screen screen = Screen::Home;
    Settings settings;
    Task task;                                   // tache d'arriere-plan en cours

    std::string errorMsg;
    std::string info;                            // petit message vert (accueil / parametres)
    std::string* activeInput = nullptr;          // champ texte qui recoit le clavier

    // ---- base de recherche locale ----
    nlohmann::json scans;
    bool indexOk = false;
    size_t indexCount = 0;

    // ---- cache ----
    std::uintmax_t cacheBytes = 0;
    Uint32 clearConfirmUntil = 0;

    // ---- bibliotheque (accueil) ----
    std::vector<FolderEntry> folders;
    std::string selectedFolder;
    std::vector<PdfEntry> pdfs;
    int folderScroll = 0, pdfScroll = 0;

    // ---- recherche ----
    std::string searchText;
    std::vector<SearchResult> results;
    bool searched = false;
    bool useDirectSearch = true;                 // false = base locale (sitemap)
    bool showingScans = false;
    bool searchBusy = false;
    bool prepThenSearch = true;
    std::string searchError;
    std::string selName;
    int searchScroll = 0;

    // ---- chapitres ----
    ScrapeResult scrape;
    int chapterScroll = 0;

    // ---- telechargement ----
    std::vector<std::string> selectedNames;
    DownloadOutcome outcome;

    // Resultats ecrits par le thread, lus seulement une fois la tache finie
    std::shared_ptr<std::vector<SearchResult>> pendingResults;
    std::shared_ptr<ScrapeResult> pendingScrape;
    std::shared_ptr<DownloadOutcome> pendingOutcome;

    // ---- navigation ----
    void goHome(const std::string& message = "");
    void goSettings();
    void leaveSettings();
    void fail(const std::string& message);
    void stopAll();

    // ---- donnees ----
    void refreshIndex();
    void refreshCache();
    void refreshLibrary();
    void selectFolder(const std::string& name);

    // ---- actions ----
    void startSearchFlow();
    void enterSearch();
    void startPrepare(bool thenSearch);
    void updateIndex();
    void doSearch();
    void cancelSearch();
    void chooseResult(const SearchResult& r);
    void startScrape(const SearchResult& r);
    void startDownload();
    void cancelDownload();
    void openDestination();
    void clearCacheOrConfirm();

    int selectedCount() const;

    // ---- boucle ----
    void update();                               // appele a chaque image
    void onWheel(int mx, int my, int dy);

private:
    void onPrepareDone();
    void onSearchDone();
    void onScrapeDone();
    void onDownloadDone();
};
