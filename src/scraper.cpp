#include "scraper.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "http.h"
#include "utils.h"

using json = nlohmann::json;

namespace
{
    std::string baseOf(const std::string& url)
    {
        if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) return {};
        size_t start = url.find("://") + 3;
        size_t end = url.find('/', start);
        return end == std::string::npos ? url : url.substr(0, end);
    }

    std::string catalogueId(const std::string& url)
    {
        const std::string marker = "/catalogue/";
        size_t start = url.find(marker);
        if (start == std::string::npos) return "";
        start += marker.size();
        size_t end = url.find('/', start);
        if (end == std::string::npos) end = url.size();
        return end > start ? url.substr(start, end - start) : std::string();
    }

    // Titre de l'oeuvre : id="titreOeuvre" dans le HTML, puis variable JavaScript
    std::string workName(const std::string& page, const std::string& identifier)
    {
        // 1) <div id="titreOeuvre">Titre</div> (et variantes de balises)
        const std::pair<const char*, const char*> patterns[] = {
            {"<div id=\"titreOeuvre\">",  "</div>"},
            {"<span id=\"titreOeuvre\">", "</span>"},
            {"<h1 id=\"titreOeuvre\">",   "</h1>"},
            {"<h2 id=\"titreOeuvre\">",   "</h2>"},
            {"<p id=\"titreOeuvre\">",    "</p>"},
        };
        for (const auto& p : patterns)
        {
            std::string v = utils::stripTags(utils::extractBetween(page, p.first, p.second));
            if (!v.empty()) return v;
        }

        // 2) id='titreOeuvre' ou id="titreOeuvre" avec d'autres attributs
        for (const char* marker : { "id=\"titreOeuvre\"", "id='titreOeuvre'" })
        {
            size_t pos = page.find(marker);
            if (pos == std::string::npos) continue;
            size_t gt = page.find('>', pos);
            if (gt == std::string::npos) continue;
            size_t end = page.find('<', gt + 1);
            if (end == std::string::npos) continue;

            std::string v = utils::stripTags(page.substr(gt + 1, end - gt - 1));
            if (!v.empty()) return v;
        }

        // 3) variable JavaScript
        const char* jsMarkers[] = {
            "titreOeuvre = \"", "titreOeuvre='", "titreOeuvre = '", "titreOeuvre=\"",
            "titreOeuvre: \"", "titreOeuvre: '", "\"titreOeuvre\":\"", "\"titreOeuvre\": \""
        };
        for (const char* m : jsMarkers)
        {
            const std::string marker = m;
            size_t pos = page.find(marker);
            if (pos == std::string::npos) continue;

            pos += marker.size();
            size_t end = page.find(marker.back(), pos);
            if (end == std::string::npos) continue;

            std::string v = utils::stripTags(page.substr(pos, end - pos));
            if (!v.empty()) return v;
        }

        return identifier;
    }
}

namespace scraper
{
    bool fetch(TaskContext& ctx, const std::string& url, ScrapeResult& out, std::string& error)
    {
        out = ScrapeResult{};

        const std::string base = baseOf(url);
        if (base.empty() || base.find("anime-sama.") == std::string::npos)
        {
            error = "L'URL doit appartenir a anime-sama.";
            return false;
        }

        const std::string identifier = catalogueId(url);
        if (identifier.empty())
        {
            error = "Impossible de determiner l'identifiant depuis l'URL.";
            return false;
        }

        http::Session session;
        http::Request req;
        req.url = url;
        req.headers = {
            "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8",
            "Accept-Language: fr-FR,fr;q=0.9,en-US;q=0.8,en;q=0.7",
            "Cache-Control: no-cache",
            "Pragma: no-cache"
        };

        ctx.log.line("Recuperation de la page : " + url);
        http::Response page = session.get(req, &ctx.cancelFlag);
        if (!page.ok) { error = page.error; return false; }
        if (page.body.empty()) { error = "Page vide."; return false; }

        std::string title = utils::trim(workName(page.body, identifier));
        if (title.empty()) title = identifier;
        ctx.log.line("Titre : " + title);

        const std::string encoded = http::urlEncode(title);
        req.url = base + "/s2/scans/get_nb_chap_et_img.php?oeuvre=" + encoded;

        ctx.log.line("Interrogation de l'API des chapitres...");
        http::Response api = session.get(req, &ctx.cancelFlag);
        if (!api.ok) { error = "API : " + api.error; return false; }
        if (api.body.empty()) { error = "Reponse de l'API vide."; return false; }

        json data = json::parse(api.body, nullptr, false);
        if (data.is_discarded()) { error = "JSON invalide renvoye par l'API."; return false; }
        if (!data.is_object())   { error = "L'API ne retourne pas un objet JSON."; return false; }

        if (data.contains("error"))
        {
            error = "Erreur retournee par l'API : " +
                    (data["error"].is_string() ? data["error"].get<std::string>() : data["error"].dump());
            return false;
        }

        long long totalImages = 0;
        for (auto it = data.begin(); it != data.end(); ++it)
        {
            long long count = 0;
            bool valid = false;
            try
            {
                if (it.value().is_number_integer()) { count = it.value().get<long long>(); valid = true; }
                else if (it.value().is_string())    { count = std::stoll(it.value().get<std::string>()); valid = true; }
            }
            catch (...) { valid = false; }

            if (!valid)
            {
                ctx.log.line("Nombre d'images invalide pour " + it.key() + " : ignore.");
                continue;
            }

            Chapter c;
            c.name = it.key();
            c.images = std::max(0LL, count);
            totalImages += c.images;
            out.chapters.push_back(std::move(c));
        }

        std::sort(out.chapters.begin(), out.chapters.end(),
                  [](const Chapter& a, const Chapter& b) { return utils::naturalLess(a.name, b.name); });

        if (out.chapters.empty()) { error = "Aucun chapitre disponible."; return false; }

        out.source = url;
        out.identifier = identifier;
        out.title = title;
        out.baseImageUrl = base + "/s2/scans/" + encoded + "/";

        ctx.log.line(std::to_string(out.chapters.size()) + " chapitre(s), " +
                     std::to_string(totalImages) + " image(s).");
        return true;
    }
}
