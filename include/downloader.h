// ==========================================================
// downloader.h  -  Telechargement des images (ancien download.exe)
// ==========================================================
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "model.h"
#include "task.h"

namespace downloader
{
    struct Stats
    {
        int chapters = 0;
        int images = 0;
        int ok = 0;
        int failed = 0;
    };

    // Telecharge les chapitres "names" dans tempDir/<manga>/Chapter <n>/001.jpg ...
    // Retourne false uniquement si l'utilisateur annule.
    bool run(TaskContext& ctx, const ScrapeResult& manga, const std::vector<std::string>& names,
             const std::filesystem::path& tempDir, Stats& stats);
}
