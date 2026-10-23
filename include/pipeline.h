// ==========================================================
// pipeline.h  -  Telechargement -> PDF -> copie, en une seule tache
// ==========================================================
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "model.h"
#include "task.h"

struct DownloadOutcome
{
    int pdfCount = 0;                         // PDF crees
    int copied = 0;                           // fichiers copies vers la destination
    std::vector<std::string> missing;         // chapitres sans PDF
};

namespace pipeline
{
    bool runDownload(TaskContext& ctx, const ScrapeResult& manga, const std::vector<std::string>& chapterNames,
                     const std::filesystem::path& destination, DownloadOutcome& outcome, std::string& error);
}
