#include <fstream>
#include <string>
#include <filesystem>
#include <locale>
#include <iostream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

// --------------------------------------
// Callback CURL
// --------------------------------------

static size_t WriteCallback(
    void* contents,
    size_t size,
    size_t nmemb,
    std::string* output
)
{
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

// --------------------------------------
// HTTP GET
// --------------------------------------

bool httpGet(
    const std::string& url,
    std::string& response,
    std::string& finalUrl
)
{
    std::cout << "[INFO] Connexion : " << url << std::endl;

    CURL* curl = curl_easy_init();

    if (!curl)
    {
        std::cout << "[ERREUR] Impossible d'initialiser CURL." << std::endl;
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    curl_easy_setopt(
        curl,
        CURLOPT_USERAGENT,
        "Mozilla/5.0"
    );

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK)
    {
        std::cout << "[ERREUR] Requete CURL echouee : "
                  << curl_easy_strerror(res)
                  << std::endl;

        curl_easy_cleanup(curl);
        return false;
    }

    char* effectiveUrl = nullptr;

    curl_easy_getinfo(
        curl,
        CURLINFO_EFFECTIVE_URL,
        &effectiveUrl
    );

    if (effectiveUrl)
    {
        finalUrl = effectiveUrl;

        std::cout << "[INFO] URL finale : "
                  << finalUrl
                  << std::endl;
    }

    curl_easy_cleanup(curl);

    std::cout << "[OK] Requete terminee." << std::endl;

    return true;
}

// --------------------------------------
// Extraction href
// --------------------------------------

std::string extractHref(const std::string& html)
{
    size_t aPos = html.find("<a");

    while (aPos != std::string::npos)
    {
        size_t endTag = html.find(">", aPos);

        if (endTag == std::string::npos)
            break;

        std::string tag =
            html.substr(aPos, endTag - aPos);

        if (tag.find("btn-primary") != std::string::npos)
        {
            size_t hrefPos = tag.find("href=\"");

            if (hrefPos != std::string::npos)
            {
                hrefPos += 6;

                size_t endQuote =
                    tag.find("\"", hrefPos);

                if (endQuote != std::string::npos)
                {
                    return tag.substr(
                        hrefPos,
                        endQuote - hrefPos
                    );
                }
            }
        }

        aPos = html.find("<a", endTag);
    }

    return "";
}

// --------------------------------------
// Extraction extension
// --------------------------------------

std::string extractExtension(const std::string& url)
{
    std::string domain;

    size_t protocolPos = url.find("://");

    if (protocolPos != std::string::npos)
        domain = url.substr(protocolPos + 3);
    else
        domain = url;

    size_t slashPos = domain.find("/");

    if (slashPos != std::string::npos)
        domain = domain.substr(0, slashPos);

    if (domain.rfind("www.", 0) == 0)
        domain = domain.substr(4);

    size_t animePos = domain.find("anime-sama.");

    if (animePos == std::string::npos)
        return "";

    return domain.substr(animePos + 11);
}

// --------------------------------------
// Main
// --------------------------------------

int main()
{
    system("chcp 65001 > nul");

    setlocale(LC_ALL, ".UTF8");

    std::cout << "========================================" << std::endl;
    std::cout << "       ANIME-SAMA - RECUPERATION" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    std::cout << "[INFO] Initialisation de CURL..." << std::endl;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    const std::string baseUrl =
        "https://anime-sama.pw";

    // --------------------------------------
    // Première requête
    // --------------------------------------

    std::cout << "[INFO] Recuperation de la page principale..."
              << std::endl;

    std::string html;
    std::string finalUrl;

    if (!httpGet(baseUrl, html, finalUrl))
    {
        std::cout << "[ERREUR] Impossible de recuperer la page."
                  << std::endl;

        curl_global_cleanup();
        return 1;
    }

    std::cout << "[OK] Page recuperee." << std::endl;

    // --------------------------------------
    // Extraction du lien
    // --------------------------------------

    std::cout << "[INFO] Recherche du bouton principal..."
              << std::endl;

    std::string href = extractHref(html);

    if (href.empty())
    {
        std::cout << "[ERREUR] Aucun lien correspondant trouve."
                  << std::endl;

        curl_global_cleanup();
        return 1;
    }

    std::cout << "[OK] Lien trouve : "
              << href
              << std::endl;

    // --------------------------------------
    // Conversion URL relative -> absolue
    // --------------------------------------

    if (href.rfind("http", 0) != 0)
    {
        href =
            "https://anime-sama.pw/" + href;

        std::cout << "[INFO] URL completee : "
                  << href
                  << std::endl;
    }

    // --------------------------------------
    // Deuxième requête
    // --------------------------------------

    std::cout << "[INFO] Recuperation de l'URL de destination..."
              << std::endl;

    std::string response2;
    std::string redirectedUrl;

    if (!httpGet(
            href,
            response2,
            redirectedUrl))
    {
        std::cout << "[ERREUR] Impossible de recuperer la destination."
                  << std::endl;

        curl_global_cleanup();
        return 1;
    }

    // --------------------------------------
    // Extraction du domaine
    // --------------------------------------

    std::cout << "[INFO] Extraction de l'extension du domaine..."
              << std::endl;

    std::string extension =
        extractExtension(redirectedUrl);

    if (extension.empty())
    {
        std::cout << "[ERREUR] Extension du domaine introuvable."
                  << std::endl;

        curl_global_cleanup();
        return 1;
    }

    std::cout << "[OK] Extension trouvee : "
              << extension
              << std::endl;

    // --------------------------------------
    // Creation du dossier data
    // --------------------------------------

    fs::path exeDir =
        fs::current_path();

    fs::path dataDir =
        exeDir / "data";

    fs::path jsonPath =
        dataDir / "domaine.json";

    std::cout << "[INFO] Dossier de sortie : "
              << dataDir.string()
              << std::endl;

    try
    {
        if (!fs::exists(dataDir))
        {
            std::cout << "[INFO] Creation du dossier data..."
                      << std::endl;

            fs::create_directories(dataDir);

            std::cout << "[OK] Dossier data cree."
                      << std::endl;
        }
        else
        {
            std::cout << "[INFO] Le dossier data existe deja."
                      << std::endl;
        }

        // --------------------------------------
        // Creation du JSON
        // --------------------------------------

        json j;

        j["domaine"] = extension;

        std::cout << "[INFO] Ecriture du fichier JSON..."
                  << std::endl;

        std::ofstream file(jsonPath);

        if (!file.is_open())
        {
            std::cout << "[ERREUR] Impossible d'ouvrir : "
                      << jsonPath.string()
                      << std::endl;

            curl_global_cleanup();
            return 1;
        }

        file << j.dump(4);

        file.close();

        std::cout << "[OK] Fichier cree : "
                  << jsonPath.string()
                  << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cout << "[ERREUR] "
                  << e.what()
                  << std::endl;

        curl_global_cleanup();
        return 1;
    }
    catch (...)
    {
        std::cout << "[ERREUR] Erreur inconnue lors de l'ecriture."
                  << std::endl;

        curl_global_cleanup();
        return 1;
    }

    // --------------------------------------
    // Fin
    // --------------------------------------

    curl_global_cleanup();

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "              TERMINE" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}