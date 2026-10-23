#include "app.h"

#include <algorithm>
#include <filesystem>

#include "config.h"
#include "gfx.h"
#include "layout.h"
#include "platform.h"
#include "domain.h"
#include "scraper.h"
#include "search.h"
#include "sitemap.h"
#include "utils.h"

namespace fs = std::filesystem;

// ==========================================================
// Navigation
// ==========================================================

void App::stopAll()
{
    task.cancel();
    searchBusy = false;
}

void App::goHome(const std::string& message)
{
    stopAll();
    info = message;
    activeInput = nullptr;
    refreshLibrary();
    screen = Screen::Home;
}

void App::goSettings()
{
    stopAll();
    refreshCache();
    refreshIndex();
    activeInput = nullptr;
    screen = Screen::Settings;
}

void App::leaveSettings()
{
    saveSettings(settings);
    goHome();
}

void App::fail(const std::string& message)
{
    stopAll();
    errorMsg = message;
    activeInput = nullptr;
    screen = Screen::Error;
}

// ==========================================================
// Donnees
// ==========================================================

void App::refreshIndex()
{
    indexOk = search::loadIndex(scans);
    indexCount = scans.size();
}

void App::refreshCache()
{
    cacheBytes = storage::cacheSize();
}

void App::refreshLibrary()
{
    folders = storage::listFolders(platform::pathFromUtf8(settings.destination));

    // Garde la selection si le dossier existe encore
    bool found = false;
    for (const auto& f : folders)
        if (f.name == selectedFolder) { found = true; break; }

    if (found) selectFolder(selectedFolder);
    else { selectedFolder.clear(); pdfs.clear(); }

    folderScroll = gfx::clampScroll(folderScroll, int(folders.size()) * layout::LIB_ROW_H, layout::FOLDER_LST.h);
}

void App::selectFolder(const std::string& name)
{
    selectedFolder = name;
    pdfs.clear();
    pdfScroll = 0;
    for (const auto& f : folders)
        if (f.name == name) { pdfs = storage::listPdfs(f.path); break; }
}

void App::clearCacheOrConfirm()
{
    Uint32 now = SDL_GetTicks();
    if (now < clearConfirmUntil)          // 2e clic dans les 3 s = confirmation
    {
        storage::clearCache();
        clearConfirmUntil = 0;
        refreshCache();
        info = "Cache vidé.";
    }
    else
    {
        clearConfirmUntil = now + 3000;
    }
}

// ==========================================================
// Recherche
// ==========================================================

void App::startSearchFlow()
{
    saveSettings(settings);

    std::error_code ec;
    fs::create_directories(platform::pathFromUtf8(settings.destination), ec);
    if (ec)
    {
        fail("Dossier de destination invalide : " + ec.message());
        return;
    }

    refreshIndex();
    if (useDirectSearch || indexOk) enterSearch();
    else startPrepare(true);
}

void App::enterSearch()
{
    screen = Screen::Search;
    activeInput = &searchText;
}

// domaine -> sitemap, dans un seul thread
void App::startPrepare(bool thenSearch)
{
    saveSettings(settings);
    std::error_code ec;
    fs::create_directories("data", ec);

    prepThenSearch = thenSearch;
    activeInput = nullptr;
    screen = Screen::Prepare;

    task.start([](TaskContext& ctx, std::string& err)
    {
        ctx.progress.stageTotal = 2;

        ctx.progress.stage = 1;
        std::string ext;
        if (!domain::resolve(ctx, ext, err)) return false;

        ctx.progress.stage = 2;
        return sitemap::buildIndex(ctx, err);
    });
}

void App::updateIndex()
{
    std::error_code ec;
    fs::remove(cfg::HERITAGE_FILE, ec);
    fs::remove(cfg::DOMAIN_FILE, ec);
    fs::remove_all(cfg::DATA_TEMP_DIR, ec);
    startPrepare(false);
}

void App::onPrepareDone()
{
    if (!task.succeeded())
    {
        fail("La préparation de la base a échoué : " + task.error() +
             " Vérifie ta connexion internet.");
        return;
    }

    refreshIndex();
    if (!indexOk) { fail("La base de recherche est vide ou invalide (data/heritage.json)."); return; }

    if (prepThenSearch) enterSearch();
    else { goSettings(); info = "Base de recherche mise à jour."; }
}

void App::doSearch()
{
    if (searchBusy) return;
    const std::string query = utils::trim(searchText);
    if (query.empty()) return;

    searched = true;
    showingScans = false;
    searchScroll = 0;
    searchError.clear();
    results.clear();

    if (!useDirectSearch)
    {
        results = search::local(scans, query);
        return;
    }

    searchBusy = true;
    pendingResults = std::make_shared<std::vector<SearchResult>>();
    auto out = pendingResults;
    task.start([query, out](TaskContext& ctx, std::string& err)
    {
        return search::byTitle(ctx, query, *out, err);
    });
}

void App::cancelSearch()
{
    task.cancel();
    searchBusy = false;
}

void App::onSearchDone()
{
    searchBusy = false;
    if (task.succeeded()) results = *pendingResults;
    else
    {
        results.clear();
        searchError = "La recherche a échoué : " + task.error();
    }
}

void App::chooseResult(const SearchResult& r)
{
    if (searchBusy) return;

    if (useDirectSearch && !r.scanChoice)
    {
        // Fiche anime : on liste d'abord ses scans (VF, VOSTFR...)
        selName = r.identifier;
        searched = true;
        showingScans = true;
        searchScroll = 0;
        searchError.clear();
        results.clear();
        searchBusy = true;

        pendingResults = std::make_shared<std::vector<SearchResult>>();
        auto out = pendingResults;
        const std::string url = r.url;
        task.start([url, out](TaskContext& ctx, std::string& err)
        {
            return search::scanChoices(ctx, url, *out, err);
        });
        return;
    }
    startScrape(r);
}

void App::startScrape(const SearchResult& r)
{
    selName = r.identifier;
    activeInput = nullptr;
    screen = Screen::Scraping;

    pendingScrape = std::make_shared<ScrapeResult>();
    auto out = pendingScrape;
    const std::string url = r.url;
    task.start([url, out](TaskContext& ctx, std::string& err)
    {
        return scraper::fetch(ctx, url, *out, err);
    });
}

void App::onScrapeDone()
{
    if (!task.succeeded())
    {
        fail("La récupération des chapitres a échoué pour « " + selName + " » : " + task.error());
        return;
    }

    scrape = *pendingScrape;
    if (scrape.title.empty()) scrape.title = selName;
    chapterScroll = 0;
    screen = Screen::Chapters;
}

int App::selectedCount() const
{
    int n = 0;
    for (const auto& c : scrape.chapters) if (c.selected) ++n;
    return n;
}

// ==========================================================
// Telechargement
// ==========================================================

void App::startDownload()
{
    selectedNames.clear();
    for (const auto& c : scrape.chapters)
        if (c.selected) selectedNames.push_back(c.name);
    if (selectedNames.empty()) return;

    pendingOutcome = std::make_shared<DownloadOutcome>();
    auto out = pendingOutcome;
    auto manga = std::make_shared<ScrapeResult>(scrape);
    auto names = selectedNames;
    const fs::path dest = platform::pathFromUtf8(settings.destination);

    activeInput = nullptr;
    screen = Screen::Download;

    task.start([manga, names, dest, out](TaskContext& ctx, std::string& err)
    {
        return pipeline::runDownload(ctx, *manga, names, dest, *out, err);
    });
}

void App::onDownloadDone()
{
    storage::writeRunLog(task.log().full());

    if (!task.succeeded())
    {
        fail(task.error());
        return;
    }

    outcome = *pendingOutcome;
    screen = Screen::Done;
    refreshLibrary();
    if (settings.openWhenDone) openDestination();
}

void App::cancelDownload()
{
    stopAll();
    storage::clearCache();
    goHome("Téléchargement annulé, cache nettoyé.");
}

void App::openDestination()
{
    platform::openFolder(platform::pathFromUtf8(settings.destination));
}

// ==========================================================
// Boucle
// ==========================================================

void App::update()
{
    if (!task.poll()) return;               // rien de nouveau

    switch (screen)
    {
        case Screen::Prepare:  onPrepareDone();  break;
        case Screen::Search:   onSearchDone();   break;
        case Screen::Scraping: onScrapeDone();   break;
        case Screen::Download: onDownloadDone(); break;
        default: break;
    }
}

void App::onWheel(int mx, int my, int dy)
{
    switch (screen)
    {
        case Screen::Home:
            if (gfx::inRect(mx, my, layout::FOLDER_LST)) folderScroll -= dy * layout::LIB_ROW_H;
            if (gfx::inRect(mx, my, layout::PDF_LST))    pdfScroll    -= dy * layout::LIB_ROW_H;
            break;
        case Screen::Search:   searchScroll  -= dy * 40; break;
        case Screen::Chapters: chapterScroll -= dy * layout::CHAP_ROW_H; break;
        default: break;
    }
}
