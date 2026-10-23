#include "domain.h"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "config.h"
#include "http.h"
#include "utils.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace
{
    // Premier <a ... btn-primary ... href="..."> de la page
    std::string extractHref(const std::string& html)
    {
        size_t aPos = html.find("<a");
        while (aPos != std::string::npos)
        {
            size_t endTag = html.find('>', aPos);
            if (endTag == std::string::npos) break;

            std::string tag = html.substr(aPos, endTag - aPos);
            if (tag.find("btn-primary") != std::string::npos)
            {
                std::string href = utils::attrValue(tag, "href");
                if (!href.empty()) return href;
            }
            aPos = html.find("<a", endTag);
        }
        return "";
    }

    // https://www.anime-sama.to/xyz -> "to"
    std::string extractExtension(const std::string& url)
    {
        std::string host;
        size_t proto = url.find("://");
        host = (proto != std::string::npos) ? url.substr(proto + 3) : url;

        size_t endHost = host.find_first_of("/?#");
        if (endHost != std::string::npos) host = host.substr(0, endHost);

        size_t port = host.find(':');
        if (port != std::string::npos) host = host.substr(0, port);

        if (host.rfind("www.", 0) == 0) host = host.substr(4);

        const std::string marker = "anime-sama.";
        size_t pos = host.find(marker);
        if (pos == std::string::npos) return "";
        return host.substr(pos + marker.size());
    }

    std::string makeAbsolute(std::string href, const std::string& base)
    {
        if (href.rfind("//", 0) == 0) return "https:" + href;
        if (href.rfind("http://", 0) == 0 || href.rfind("https://", 0) == 0) return href;
        if (!href.empty() && href[0] == '/') return base + href;
        return base + "/" + href;
    }
}

namespace domain
{
    bool resolve(TaskContext& ctx, std::string& extension, std::string& error)
    {
        http::Session session;
        const std::string portal = cfg::PORTAL_URL;

        http::Request req;
        req.url = portal;
        req.userAgent = "Mozilla/5.0";
        req.connectTimeout = 10;
        req.timeout = 20;
        req.requireSuccess = false;

        ctx.log.line("Connexion : " + portal);
        http::Response first = session.get(req, &ctx.cancelFlag);
        if (!first.ok)
        {
            error = "Impossible de joindre " + portal + " (" + first.error + ")";
            return false;
        }

        std::string href = extractHref(first.body);
        if (href.empty())
        {
            error = "Aucun lien de redirection trouve sur " + portal + ".";
            return false;
        }
        href = makeAbsolute(href, portal);
        ctx.log.line("Lien trouve : " + href);

        req.url = href;
        http::Response second = session.get(req, &ctx.cancelFlag);
        if (!second.ok)
        {
            error = "Impossible de joindre " + href + " (" + second.error + ")";
            return false;
        }

        extension = extractExtension(second.finalUrl);
        if (extension.empty())
        {
            error = "Extension du domaine introuvable (URL obtenue : " + second.finalUrl + ").";
            return false;
        }
        ctx.log.line("Extension du domaine : " + extension);

        std::error_code ec;
        fs::create_directories("data", ec);

        std::ofstream file(cfg::DOMAIN_FILE, std::ios::trunc);
        if (!file)
        {
            error = std::string("Impossible d'ecrire ") + cfg::DOMAIN_FILE;
            return false;
        }
        json j;
        j["domaine"] = extension;
        file << j.dump(4) << "\n";
        return static_cast<bool>(file);
    }

    bool loadBaseUrl(std::string& baseUrl, std::string& error)
    {
        std::ifstream file(cfg::DOMAIN_FILE);
        if (!file)
        {
            error = std::string("Impossible d'ouvrir ") + cfg::DOMAIN_FILE;
            return false;
        }

        json data;
        try { file >> data; }
        catch (const std::exception& e)
        {
            error = std::string("JSON invalide dans ") + cfg::DOMAIN_FILE + " : " + e.what();
            return false;
        }

        if (!data.is_object() || !data.contains("domaine") || !data["domaine"].is_string())
        {
            error = std::string(cfg::DOMAIN_FILE) + " doit contenir une chaine \"domaine\".";
            return false;
        }

        std::string ext = utils::trim(data["domaine"].get<std::string>());
        while (!ext.empty() && ext.front() == '.') ext.erase(ext.begin());
        while (!ext.empty() && ext.back() == '/') ext.pop_back();

        if (ext.empty() || ext.find('/') != std::string::npos)
        {
            error = "Le champ \"domaine\" doit contenir uniquement l'extension (ex. \"to\").";
            return false;
        }

        baseUrl = "https://anime-sama." + ext;
        return true;
    }

    std::string baseUrl()
    {
        std::string base, err;
        return loadBaseUrl(base, err) ? base : std::string(cfg::DEFAULT_BASE);
    }
}
