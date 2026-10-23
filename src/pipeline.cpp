#include "pipeline.h"

#include "config.h"
#include "downloader.h"
#include "pdf_builder.h"
#include "storage.h"

namespace fs = std::filesystem;

namespace pipeline
{
    bool runDownload(TaskContext& ctx, const ScrapeResult& manga, const std::vector<std::string>& names,
                     const fs::path& destination, DownloadOutcome& outcome, std::string& error)
    {
        const fs::path tempDir = cfg::TEMP_DIR;
        const fs::path scanDir = cfg::SCAN_DIR;

        std::error_code ec;
        fs::remove_all(tempDir, ec);
        fs::remove_all(scanDir, ec);

        // 1) images
        ctx.progress.beginPhase(Phase::Download, static_cast<int>(names.size()));
        downloader::Stats stats;
        if (!downloader::run(ctx, manga, names, tempDir, stats)) { error = "Annule"; return false; }

        // 2) PDF
        ctx.progress.beginPhase(Phase::Pdf);
        if (!pdfbuilder::buildAll(ctx, tempDir, scanDir, outcome.pdfCount)) { error = "Annule"; return false; }

        outcome.missing.clear();
        for (const auto& name : names)
            if (!storage::pdfExists(scanDir, manga.title, name)) outcome.missing.push_back(name);

        if (outcome.pdfCount == 0)
        {
            std::vector<std::string> last = ctx.log.tail(1);
            error = "Aucun PDF n'a ete cree. Derniere ligne du journal : " +
                    (last.empty() ? std::string("(vide)") : last.back());
            return false;
        }

        // 3) copie vers la destination
        ctx.progress.beginPhase(Phase::Copy);
        if (!storage::copyTree(ctx, scanDir, destination, outcome.copied, error)) return false;

        fs::remove_all(scanDir, ec);
        fs::remove_all(tempDir, ec);
        ctx.log.line(std::to_string(outcome.copied) + " fichier(s) copie(s).");
        return true;
    }
}
