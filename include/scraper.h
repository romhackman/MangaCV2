// ==========================================================
// scraper.h  -  Liste des chapitres d'un scan (ancien scraper.exe)
// ==========================================================
#pragma once

#include <string>

#include "model.h"
#include "task.h"

namespace scraper
{
    // "url" = page d'un scan : https://anime-sama.xx/catalogue/<id>/scan/vf/
    bool fetch(TaskContext& ctx, const std::string& url, ScrapeResult& out, std::string& error);
}
