// ==========================================================
// search.h  -  Recherche d'un manga (ancien search.exe + recherche locale)
// ==========================================================
#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "model.h"
#include "task.h"

namespace search
{
    // Recherche directe sur Anime-Sama
    bool byTitle(TaskContext& ctx, const std::string& query,
                 std::vector<SearchResult>& out, std::string& error);

    // Liste des scans (VF, VOSTFR...) d'une fiche anime
    bool scanChoices(TaskContext& ctx, const std::string& animeUrl,
                     std::vector<SearchResult>& out, std::string& error);

    // Base locale (data/heritage.json)
    bool loadIndex(nlohmann::json& scans);
    std::vector<SearchResult> local(const nlohmann::json& scans, const std::string& query);
}
