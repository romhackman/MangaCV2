// ==========================================================
// sitemap.h  -  Base de recherche locale (ancien preprocess.exe)
// ==========================================================
#pragma once

#include <string>

#include "task.h"

namespace sitemap
{
    // sitemap.xml -> liste des mangas et de leurs scans -> data/heritage.json
    bool buildIndex(TaskContext& ctx, std::string& error);
}
