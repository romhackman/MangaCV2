// ==========================================================
// search.cpp  -  Recherche directe Anime-Sama (worker)
//
// Usage :
//   search.exe -query "titre"   -> cherche un anime (API de recherche)
//   search.exe -link  "url"     -> liste les scans d'une fiche anime
//
// Sortie : data/search.json, relu par app.cpp
//   {
//       "status": true,
//       "mode": "query" | "link",
//       "results": [ { "identifier": "...", "url": "...", "scanChoice": false } ]
//   }
//
// Codes de sortie :
//   0 : requete HTTP reussie (meme sans aucun resultat)
//   1 : argument invalide, echec reseau ou ecriture impossible
// ==========================================================

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

// --------------------------------------
// CONFIG
// --------------------------------------

static const std::string ANIME_SAMA_BASE = "https://anime-sama.to";
static const std::string ANIME_SAMA_API  = "https://anime-sama.to/template-php/defaut/fetch.php";
static const std::string SEARCH_FILE     = "data/search.json";

struct SearchResult
{
    std::string identifier;
    std::string url;
    bool scanChoice = false;
};

// --------------------------------------
// Utilitaires texte
// --------------------------------------

static std::string trim(std::string v)
{
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    v.erase(v.begin(), std::find_if(v.begin(), v.end(), notSpace));
    v.erase(std::find_if(v.rbegin(), v.rend(), notSpace).base(), v.end());
    return v;
}

static std::string decodeBasicHtml(std::string value)
{
    const std::pair<const char*, const char*> entities[] = {
        {"&amp;", "&"}, {"&quot;", "\""}, {"&#39;", "'"}, {"&apos;", "'"},
        {"&lt;", "<"}, {"&gt;", ">"}, {"&nbsp;", " "}
    };
    for (const auto& e : entities)
    {
        size_t pos = 0;
        while ((pos = value.find(e.first, pos)) != std::string::npos)
        {
            value.replace(pos, std::strlen(e.first), e.second);
            pos += std::strlen(e.second);
        }
    }
    return value;
}

static std::string stripHtmlTags(std::string value)
{
    value = std::regex_replace(value, std::regex(R"(<[^>]*>)"), "");
    return trim(decodeBasicHtml(value));
}

// "'Nom'" ou "\"chemin\"" -> nom ou chemin sans guillemets
static std::string cleanQuotes(std::string s)
{
    s = trim(s);
    if (!s.empty() && (s.front() == '"' || s.front() == '\'')) s.erase(0, 1);
    if (!s.empty() && (s.back() == '"' || s.back() == '\'')) s.pop_back();
    return trim(s);
}

static std::string extractBaseUrl(const std::string& url)
{
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0)
        return {};

    size_t start = url.find("://") + 3;
    size_t end = url.find('/', start);
    return end == std::string::npos ? url : url.substr(0, end);
}

// https://site/catalogue/<identifiant>/... -> <identifiant>
static std::string extractCatalogueIdentifier(const std::string& url)
{
    const std::string marker = "/catalogue/";
    size_t start = url.find(marker);
    if (start == std::string::npos) return {};

    start += marker.length();
    size_t end = url.find('/', start);
    if (end == std::string::npos) end = url.length();
    if (end <= start) return {};

    return url.substr(start, end - start);
}

// --------------------------------------
// Reseau (libcurl)
// --------------------------------------

static size_t writeCallback(char* data, size_t size, size_t count, void* userData)
{
    const size_t bytes = size * count;
    static_cast<std::string*>(userData)->append(data, bytes);
    return bytes;
}

static std::string urlEncode(const std::string& text)
{
    CURL* curl = curl_easy_init();
    if (!curl) return {};

    char* escaped = curl_easy_escape(curl, text.c_str(), static_cast<int>(text.size()));
    std::string out = escaped ? escaped : "";
    if (escaped) curl_free(escaped);
    curl_easy_cleanup(curl);
    return out;
}

// Retourne false si la requete echoue. body est vide dans ce cas.
static bool fetchHtml(const std::string& url, std::string& body, const std::string* postData = nullptr)
{
    body.clear();

    CURL* curl = curl_easy_init();
    if (!curl)
    {
        std::cout << "[ERREUR] Impossible d'initialiser CURL." << std::endl;
        return false;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0");
    headers = curl_slist_append(headers, "X-Requested-With: XMLHttpRequest");
    headers = curl_slist_append(headers, "Referer: https://anime-sama.to/");
    if (postData)
        headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded; charset=UTF-8");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 35L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    if (postData)
    {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData->c_str());
    }

    CURLcode code = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK)
    {
        std::cout << "[ERREUR] Requete CURL echouee : " << curl_easy_strerror(code) << std::endl;
        body.clear();
        return false;
    }
    return true;
}

// --------------------------------------
// Recherche par titre (API de recherche)
// --------------------------------------

static std::vector<SearchResult> searchByTitle(const std::string& query, bool& ok)
{
    std::vector<SearchResult> out;

    const std::string postData = "query=" + urlEncode(query);
    std::string html;
    ok = fetchHtml(ANIME_SAMA_API, html, &postData);
    if (!ok) return out;

    const std::regex anchorPattern(
        R"(<a\b[^>]*class\s*=\s*["'][^"']*\basn-search-result\b[^"']*["'][^>]*>[\s\S]*?</a>)",
        std::regex_constants::icase);
    const std::regex hrefPattern(
        R"(\bhref\s*=\s*["']([^"']+)["'])",
        std::regex_constants::icase);
    const std::regex titlePattern(
        R"(<h3\b[^>]*class\s*=\s*["'][^"']*\basn-search-result-title\b[^"']*["'][^>]*>([\s\S]*?)</h3>)",
        std::regex_constants::icase);

    for (auto it = std::sregex_iterator(html.begin(), html.end(), anchorPattern);
         it != std::sregex_iterator(); ++it)
    {
        const std::string block = it->str();
        std::smatch href, title;
        if (!std::regex_search(block, href, hrefPattern) ||
            !std::regex_search(block, title, titlePattern))
            continue;

        std::string url = decodeBasicHtml(href[1].str());
        if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0)
            url = ANIME_SAMA_BASE + (url.empty() || url.front() != '/' ? "/" : "") + url;

        out.push_back({ stripHtmlTags(title[1].str()), url, false });
    }

    std::cout << "[OK] " << out.size() << " anime(s) trouve(s)." << std::endl;
    return out;
}

// --------------------------------------
// Liste des scans d'une fiche anime
// --------------------------------------

static std::vector<SearchResult> fetchScanChoices(const std::string& animeUrl, bool& ok)
{
    std::vector<SearchResult> out;

    const std::string baseUrl = extractBaseUrl(animeUrl);
    const std::string identifier = extractCatalogueIdentifier(animeUrl);
    if (baseUrl.empty() || identifier.empty())
    {
        std::cout << "[ERREUR] URL d'anime invalide." << std::endl;
        ok = false;
        return out;
    }

    std::string html;
    ok = fetchHtml(animeUrl, html);
    if (!ok) return out;

    size_t pos = 0;
    std::unordered_set<std::string> seen;

    while (true)
    {
        pos = html.find("panneauScan(", pos);
        if (pos == std::string::npos) break;

        size_t start = html.find('(', pos);
        size_t end = (start == std::string::npos) ? std::string::npos : html.find(')', start);
        if (start == std::string::npos || end == std::string::npos) break;

        const std::string args = html.substr(start + 1, end - start - 1);
        const size_t comma = args.find(',');

        if (comma != std::string::npos)
        {
            std::string name = cleanQuotes(args.substr(0, comma));
            std::string path = cleanQuotes(args.substr(comma + 1));

            if (path != "url" && !path.empty())
            {
                if (name.empty() || name == "nom")
                    name = "Scans";

                // Evite les doublons de panneaux
                if (seen.insert(path).second)
                {
                    while (!path.empty() && path.front() == '/')
                        path.erase(path.begin());

                    std::string panelUrl = baseUrl + "/catalogue/" + identifier + "/" + path;
                    if (panelUrl.back() != '/')
                        panelUrl += '/';

                    out.push_back({ name, panelUrl, true });
                }
            }
        }

        pos = end + 1;
    }

    std::cout << "[OK] " << out.size() << " scan(s) trouve(s)." << std::endl;
    return out;
}

// --------------------------------------
// Ecriture du resultat pour app.cpp
// --------------------------------------

static bool writeResults(const std::string& mode, const std::vector<SearchResult>& results, bool ok)
{
    json j;
    j["status"] = ok;
    j["mode"] = mode;
    j["results"] = json::array();

    for (const auto& r : results)
    {
        j["results"].push_back({
            {"identifier", r.identifier},
            {"url", r.url},
            {"scanChoice", r.scanChoice}
        });
    }

    std::error_code ec;
    fs::create_directories("data", ec);

    std::ofstream f(SEARCH_FILE, std::ios::trunc);
    if (!f) return false;

    f << j.dump(4);
    return static_cast<bool>(f);
}

// --------------------------------------
// Main
// --------------------------------------

int main(int argc, char* argv[])
{
    setvbuf(stdout, nullptr, _IONBF, 0); // progression en direct vers l'app

    std::cout << "========================================" << std::endl;
    std::cout << "         ANIME-SAMA - RECHERCHE" << std::endl;
    std::cout << "========================================" << std::endl;

    if (argc != 3)
    {
        std::cout << "[ERREUR] Usage : search.exe -query \"titre\" | -link \"url\"" << std::endl;
        return 1;
    }

    const std::string option = argv[1];
    const std::string value = trim(argv[2]);

    if (value.empty())
    {
        std::cout << "[ERREUR] Valeur vide." << std::endl;
        return 1;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);

    std::string mode;
    bool ok = false;
    std::vector<SearchResult> results;

    if (option == "-query")
    {
        mode = "query";
        std::cout << "[INFO] Recherche : " << value << std::endl;
        results = searchByTitle(value, ok);
    }
    else if (option == "-link")
    {
        mode = "link";
        std::cout << "[INFO] Recuperation des scans : " << value << std::endl;
        results = fetchScanChoices(value, ok);
    }
    else
    {
        std::cout << "[ERREUR] Option inconnue : " << option << std::endl;
        curl_global_cleanup();
        return 1;
    }

    const bool written = writeResults(mode, results, ok);
    curl_global_cleanup();

    if (!written)
    {
        std::cout << "[ERREUR] Impossible d'ecrire " << SEARCH_FILE << std::endl;
        return 1;
    }

    if (!ok)
    {
        std::cout << "[ERREUR] Recherche Anime-Sama echouee." << std::endl;
        return 1;
    }

    std::cout << "[OK] Resultats transmis a l'application." << std::endl;
    return 0;
}
