#include "sitemap.h"

#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

#include "config.h"
#include "domain.h"
#include "http.h"
#include "utils.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace
{
    // Contenu de chaque <loc>...</loc>
    std::vector<std::string> parseSitemap(const std::string& xml)
    {
        std::vector<std::string> urls;
        const std::string open = "<loc>", close = "</loc>";
        size_t pos = 0;

        while (true)
        {
            size_t start = xml.find(open, pos);
            if (start == std::string::npos) break;
            start += open.size();

            size_t end = xml.find(close, start);
            if (end == std::string::npos) break;

            std::string url = utils::trim(xml.substr(start, end - start));
            if (!url.empty()) urls.push_back(url);
            pos = end + close.size();
        }
        return urls;
    }

    // https://site/a/b?x#y -> "a/b"
    std::string pathFromUrl(const std::string& url)
    {
        size_t scheme = url.find("://");
        size_t start = url.find('/', scheme != std::string::npos ? scheme + 3 : 0);
        if (start == std::string::npos) return "";

        std::string path = url.substr(start);
        path = path.substr(0, path.find('?'));
        path = path.substr(0, path.find('#'));

        while (!path.empty() && path.front() == '/') path.erase(path.begin());
        while (!path.empty() && path.back() == '/') path.pop_back();
        return path;
    }

    std::vector<std::string> splitPath(const std::string& path)
    {
        std::vector<std::string> parts;
        size_t start = 0;
        while (start < path.size())
        {
            size_t end = path.find('/', start);
            if (end == std::string::npos) { parts.push_back(path.substr(start)); break; }
            if (end > start) parts.push_back(path.substr(start, end - start));
            start = end + 1;
        }
        return parts;
    }

    // Garde /catalogue/<manga> et les pages dont le chemin contient "scan"
    bool shouldKeep(const std::string& url)
    {
        std::string path = pathFromUrl(url);
        if (path.rfind("catalogue/", 0) != 0) return false;

        std::vector<std::string> parts = splitPath(path);
        if (parts.size() == 2) return true;

        for (size_t i = 2; i < parts.size(); ++i)
            if (utils::toLower(parts[i]).find("scan") != std::string::npos) return true;
        return false;
    }

    // Parties du chemin situees apres "catalogue"
    std::vector<std::string> cataloguePartsOf(const std::string& url)
    {
        std::vector<std::string> parts = splitPath(pathFromUrl(url));
        for (size_t i = 0; i < parts.size(); ++i)
        {
            if (parts[i] == "catalogue")
                return std::vector<std::string>(parts.begin() + static_cast<std::ptrdiff_t>(i) + 1, parts.end());
        }
        return {};
    }

    // { "manga": [ urls des scans ] } -- uniquement si la fiche "manga" existe
    json buildHeritage(const std::vector<std::string>& urls)
    {
        std::unordered_set<std::string> parents;
        std::vector<std::pair<std::string, std::string>> children;

        for (const auto& url : urls)
        {
            std::vector<std::string> parts = cataloguePartsOf(url);
            if (parts.empty()) continue;
            if (parts.size() == 1) parents.insert(parts[0]);
            else children.emplace_back(parts[0], url);
        }

        json heritage = json::object();
        for (const auto& [anime, url] : children)
        {
            if (!parents.count(anime)) continue;
            if (!heritage.contains(anime)) heritage[anime] = json::array();
            heritage[anime].push_back(url);
        }

        json result;
        result["scan"] = heritage;
        return result;
    }
}

namespace sitemap
{
    bool buildIndex(TaskContext& ctx, std::string& error)
    {
        std::string base;
        if (!domain::loadBaseUrl(base, error)) return false;

        http::Session session;
        http::Request req;
        req.url = base + "/sitemap.xml";
        req.userAgent = "Mozilla/5.0 (compatible; SimpleBot/1.0)";
        req.timeout = 60;

        ctx.log.line("Telechargement du sitemap : " + req.url);
        http::Response res = session.get(req, &ctx.cancelFlag);
        if (!res.ok || res.body.empty())
        {
            error = "Impossible de telecharger le sitemap (" +
                    (res.error.empty() ? std::string("reponse vide") : res.error) + ").";
            return false;
        }

        std::vector<std::string> all = parseSitemap(res.body);
        ctx.log.line("URLs dans le sitemap : " + std::to_string(all.size()));
        if (ctx.cancelled()) { error = "Annule"; return false; }

        std::vector<std::string> kept;
        std::unordered_set<std::string> seen;
        for (const auto& url : all)
            if (shouldKeep(url) && seen.insert(url).second) kept.push_back(url);
        ctx.log.line("Liens conserves : " + std::to_string(kept.size()));

        json result = buildHeritage(kept);

        std::error_code ec;
        fs::create_directories("data", ec);
        std::ofstream out(cfg::HERITAGE_FILE, std::ios::trunc);
        if (!out)
        {
            error = std::string("Impossible d'ecrire ") + cfg::HERITAGE_FILE;
            return false;
        }
        out << result.dump();
        out.close();
        if (!out) { error = "Erreur pendant l'ecriture de la base."; return false; }

        ctx.log.line("Mangas indexes : " + std::to_string(result["scan"].size()));
        return true;
    }
}
