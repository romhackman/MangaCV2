// ==========================================================
// model.h  -  Structures de donnees partagees
// ==========================================================
#pragma once

#include <string>
#include <vector>

struct SearchResult
{
    std::string identifier;
    std::string url;
    bool scanChoice = false;     // true = un scan (VF, VOSTFR...) d'une fiche
};

struct Chapter
{
    std::string name;            // "1", "10.5"...
    long long images = 0;
    bool selected = false;
};

// Resultat de la recuperation des chapitres d'un scan
struct ScrapeResult
{
    std::string source;          // URL du scan
    std::string identifier;      // identifiant dans /catalogue/<id>/
    std::string title;
    std::string baseImageUrl;    // .../s2/scans/<titre>/
    std::vector<Chapter> chapters;
};
