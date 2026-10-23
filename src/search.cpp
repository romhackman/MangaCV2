#include "search.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_set>

#include "config.h"
#include "domain.h"
#include "http.h"
#include "utils.h"

using json = nlohmann::json;

namespace
{
    // "'Nom'" ou "\"chemin\"" -> sans guillemets
    std::string cleanQuotes(std::string s)
    {
        s = utils::trim(s);
        if (!s.empty() && (s.front() == '"' || s.front() == '\'')) s.erase(0, 1);
        if (!s.empty() && (s.back() == '"' || s.back() == '\'')) s.pop_back();
        return utils::trim(s);
    }

    std::string baseOf(const std::string& url)
    {
        if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) return {};
        size_t start = url.find("://") + 3;
        size_t end = url.find('/', start);
        return end == std::string::npos ? url : url.substr(0, end);
    }

    // https://site/catalogue/<id>/... -> <id>
    std::string catalogueId(const std::string& url)
    {
        const std::string marker = "/catalogue/";
        size_t start = url.find(marker);
        if (start == std::string::npos) return {};
        start += marker.size();
        size_t end = url.find('/', start);
        if (end == std::string::npos) end = url.size();
        return end > start ? url.substr(start, end - start) : std::string();
    }

    // Titre du <h3 class="asn-search-result-title"> dans un bloc
    std::string findTitle(const std::string& block)
    {
        size_t pos = 0;
        while ((pos = block.find("<h3", pos)) != std::string::npos)
        {
            size_t gt = block.find('>', pos);
            if (gt == std::string::npos) break;

            std::string tag = block.substr(pos, gt - pos);
            if (utils::hasClass(utils::attrValue(tag, "class"), "asn-search-result-title"))
            {
                size_t close = block.find("</h3>", gt);
                if (close == std::string::npos) close = block.size();
                return utils::stripTags(block.substr(gt + 1, close - gt - 1));
            }
            pos = gt + 1;
        }
        return "";
    }
}

namespace search
{
    bool byTitle(TaskContext& ctx, const std::string& query,
                 std::vector<SearchResult>& out, std::string& error)
    {
        out.clear();
        const std::string base = domain::baseUrl();

        http::Request req;
        req.url = base + "/template-php/defaut/fetch.php";
        req.post = true;
        req.postData = "query=" + http::urlEncode(query);
        req.userAgent = "Mozilla/5.0";
        req.timeout = 35;
        req.headers = {
            "X-Requested-With: XMLHttpRequest",
            "Referer: " + base + "/",
            "Content-Type: application/x-www-form-urlencoded; charset=UTF-8"
        };

        ctx.log.line("Recherche : " + query);
        http::Session session;
        http::Response res = session.get(req, &ctx.cancelFlag);
        if (!res.ok) { error = res.error; return false; }

        const std::string& html = res.body;
        size_t pos = 0;

        while (true)
        {
            size_t a = html.find("<a", pos);
            if (a == std::string::npos || a + 2 >= html.size()) break;
            if (!std::isspace(static_cast<unsigned char>(html[a + 2]))) { pos = a + 2; continue; }

            size_t gt = html.find('>', a);
            if (gt == std::string::npos) break;

            std::string openTag = html.substr(a, gt - a);
            if (!utils::hasClass(utils::attrValue(openTag, "class"), "asn-search-result"))
            {
                pos = gt + 1;
                continue;
            }

            size_t close = html.find("</a>", gt);
            size_t end = (close == std::string::npos) ? html.size() : close + 4;
            std::string block = html.substr(a, end - a);
            pos = end;

            std::string url = utils::decodeHtml(utils::attrValue(openTag, "href"));
            std::string title = findTitle(block);
            if (url.empty() || title.empty()) continue;

            if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0)
                url = base + (url.front() != '/' ? "/" : "") + url;

            out.push_back({ title, url, false });
        }

        ctx.log.line(std::to_string(out.size()) + " anime(s) trouve(s).");
        return true;
    }

    bool scanChoices(TaskContext& ctx, const std::string& animeUrl,
                     std::vector<SearchResult>& out, std::string& error)
    {
        out.clear();
        const std::string base = baseOf(animeUrl);
        const std::string id = catalogueId(animeUrl);
        if (base.empty() || id.empty()) { error = "URL d'anime invalide."; return false; }

        http::Request req;
        req.url = animeUrl;
        req.userAgent = "Mozilla/5.0";
        req.timeout = 35;
        req.headers = { "Referer: " + base + "/" };

        ctx.log.line("Recuperation des scans : " + animeUrl);
        http::Session session;
        http::Response res = session.get(req, &ctx.cancelFlag);
        if (!res.ok) { error = res.error; return false; }

        const std::string& html = res.body;
        std::unordered_set<std::string> seen;
        size_t pos = 0;

        while ((pos = html.find("panneauScan(", pos)) != std::string::npos)
        {
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
                    if (name.empty() || name == "nom") name = "Scans";

                    if (seen.insert(path).second)      // evite les doublons de panneaux
                    {
                        while (!path.empty() && path.front() == '/') path.erase(path.begin());

                        std::string panel = base + "/catalogue/" + id + "/" + path;
                        if (panel.back() != '/') panel += '/';
                        out.push_back({ name, panel, true });
                    }
                }
            }
            pos = end + 1;
        }

        ctx.log.line(std::to_string(out.size()) + " scan(s) trouve(s).");
        return true;
    }

    bool loadIndex(json& scans)
    {
        scans = json::object();

        std::ifstream f(cfg::HERITAGE_FILE);
        if (!f) return false;

        try
        {
            json data;
            f >> data;
            if (data.is_object() && data.contains("scan") && data["scan"].is_object())
            {
                scans = data["scan"];
                return !scans.empty();
            }
        }
        catch (...) {}
        return false;
    }

    std::vector<SearchResult> local(const json& scans, const std::string& query)
    {
        std::vector<SearchResult> results;
        const std::string needle = utils::toLower(utils::trim(query));
        if (needle.empty()) return results;

        auto addAll = [&](const std::string& id, const json& arr)
        {
            if (!arr.is_array()) return;
            for (const auto& v : arr)
                if (v.is_string()) results.push_back({ id, v.get<std::string>(), false });
        };

        // 1) le titre contient la recherche
        for (auto it = scans.begin(); it != scans.end(); ++it)
            if (utils::toLower(it.key()).find(needle) != std::string::npos)
                addAll(it.key(), it.value());

        // 2) sinon : tous les mots, dans n'importe quel ordre
        if (results.empty())
        {
            std::istringstream ss(needle);
            std::vector<std::string> words;
            std::string w;
            while (ss >> w) words.push_back(w);

            for (auto it = scans.begin(); it != scans.end(); ++it)
            {
                const std::string low = utils::toLower(it.key());
                bool all = true;
                for (const auto& x : words)
                    if (low.find(x) == std::string::npos) { all = false; break; }
                if (all) addAll(it.key(), it.value());
            }
        }

        std::vector<SearchResult> unique;
        std::unordered_set<std::string> seen;
        for (auto& r : results)
            if (seen.insert(r.url).second) unique.push_back(r);
        return unique;
    }
}
