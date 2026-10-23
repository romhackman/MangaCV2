
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <filesystem>
#include <locale>
#include <iostream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

using json = nlohmann::json;
namespace fs = std::filesystem;

// --------------------------------------
// Chemin du dossier de l'executable
// --------------------------------------

fs::path getExeDir()
{
#ifdef _WIN32
    wchar_t exePath[MAX_PATH];

    DWORD length = GetModuleFileNameW(
        nullptr,
        exePath,
        MAX_PATH
    );

    if (length > 0 && length < MAX_PATH)
    {
        return fs::path(
            std::wstring(exePath, length)
        ).parent_path();
    }
#endif

    // Solution de secours
    return fs::current_path();
}

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
    if (size != 0 && nmemb > SIZE_MAX / size)
        return 0;

    size_t totalSize = size * nmemb;

    output->append(
        static_cast<char*>(contents),
        totalSize
    );

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
    std::cout << "[INFO] Connexion : "
              << url << std::endl;

    CURL* curl = curl_easy_init();

    if (!curl)
    {
        std::cout
            << "[ERREUR] Impossible d'initialiser CURL."
            << std::endl;

        return false;
    }

    // --------------------------------------
    // Recherche du certificat CA
    // --------------------------------------

    fs::path exeDir = getExeDir();
    fs::path caFile = exeDir / "cacert.pem";

    // En second choix, chercher dans le dossier courant
    if (!fs::exists(caFile))
    {
        fs::path currentCa =
            fs::current_path() / "cacert.pem";

        if (fs::exists(currentCa))
            caFile = currentCa;
    }

    if (fs::exists(caFile))
    {
        std::cout << "[INFO] Certificats CA : "
                  << caFile.string()
                  << std::endl;

        CURLcode optionResult = curl_easy_setopt(
            curl,
            CURLOPT_CAINFO,
            caFile.string().c_str()
        );

        if (optionResult != CURLE_OK)
        {
            std::cerr
                << "[ERREUR] Configuration du certificat impossible : "
                << curl_easy_strerror(optionResult)
                << std::endl;

            curl_easy_cleanup(curl);
            return false;
        }
    }
    else
    {
        std::cout
            << "[AVERTISSEMENT] cacert.pem introuvable."
            << std::endl;

        std::cout
            << "[INFO] Tentative avec la configuration CURL par defaut."
            << std::endl;
    }

    // --------------------------------------
    // Options CURL
    // --------------------------------------

    curl_easy_setopt(
        curl,
        CURLOPT_URL,
        url.c_str()
    );

    curl_easy_setopt(
        curl,
        CURLOPT_FOLLOWLOCATION,
        1L
    );

    curl_easy_setopt(
        curl,
        CURLOPT_MAXREDIRS,
        10L
    );

    curl_easy_setopt(
        curl,
        CURLOPT_CONNECTTIMEOUT,
        10L
    );

    curl_easy_setopt(
        curl,
        CURLOPT_TIMEOUT,
        20L
    );

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEFUNCTION,
        WriteCallback
    );

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEDATA,
        &response
    );

    curl_easy_setopt(
        curl,
        CURLOPT_USERAGENT,
        "Mozilla/5.0"
    );

    // Garder la verification SSL active
    curl_easy_setopt(
        curl,
        CURLOPT_SSL_VERIFYPEER,
        1L
    );

    curl_easy_setopt(
        curl,
        CURLOPT_SSL_VERIFYHOST,
        2L
    );

    // --------------------------------------
    // Execution de la requete
    // --------------------------------------

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK)
    {
        std::cerr
            << "[ERREUR] Requete CURL echouee : "
            << curl_easy_strerror(res)
            << std::endl;

        std::cerr
            << "[DEBUG] Code CURL : "
            << static_cast<int>(res)
            << std::endl;

        if (res == CURLE_SSL_CACERT_BADFILE)
        {
            std::cerr
                << "[CONSEIL] Verifie le chemin et le contenu de cacert.pem."
                << std::endl;
        }

        curl_easy_cleanup(curl);
        return false;
    }

    // --------------------------------------
    // Recuperation de l'URL finale
    // --------------------------------------

    char* effectiveUrl = nullptr;

    CURLcode infoResult = curl_easy_getinfo(
        curl,
        CURLINFO_EFFECTIVE_URL,
        &effectiveUrl
    );

    if (infoResult == CURLE_OK && effectiveUrl)
    {
        finalUrl = effectiveUrl;

        std::cout
            << "[INFO] URL finale : "
            << finalUrl
            << std::endl;
    }

    // --------------------------------------
    // Code HTTP
    // --------------------------------------

    long httpCode = 0;

    curl_easy_getinfo(
        curl,
        CURLINFO_RESPONSE_CODE,
        &httpCode
    );

    std::cout
        << "[INFO] Code HTTP : "
        << httpCode
        << std::endl;

    curl_easy_cleanup(curl);

    std::cout
        << "[OK] Requete terminee."
        << std::endl;

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
// Extraction extension du domaine
// --------------------------------------

std::string extractExtension(const std::string& url)
{
    std::string domain;

    size_t protocolPos = url.find("://");

    if (protocolPos != std::string::npos)
        domain = url.substr(protocolPos + 3);
    else
        domain = url;

    // Supprimer le chemin, la query et le fragment
    size_t endHost = domain.find_first_of("/?#");

    if (endHost != std::string::npos)
        domain = domain.substr(0, endHost);

    // Retirer un éventuel port
    size_t portPos = domain.find(':');

    if (portPos != std::string::npos)
        domain = domain.substr(0, portPos);

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
    setvbuf(stdout, nullptr, _IONBF, 0);

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    setlocale(LC_ALL, ".UTF8");

    std::cout
        << "========================================"
        << std::endl;

    std::cout
        << "       ANIME-SAMA - RECUPERATION"
        << std::endl;

    std::cout
        << "========================================"
        << std::endl << std::endl;

    std::cout
        << "[INFO] Initialisation de CURL..."
        << std::endl;

    CURLcode initResult =
        curl_global_init(CURL_GLOBAL_DEFAULT);

    if (initResult != CURLE_OK)
    {
        std::cerr
            << "[ERREUR] Initialisation globale de CURL impossible."
            << std::endl;

        return 1;
    }

    int exitCode = 1;

    try
    {
        fs::path exeDir = getExeDir();

        std::cout
            << "[INFO] Dossier executable : "
            << exeDir.string()
            << std::endl;

        const std::string baseUrl =
            "https://anime-sama.pw";

        // --------------------------------------
        // Première requête
        // --------------------------------------

        std::cout
            << "[INFO] Recuperation de la page principale..."
            << std::endl;

        std::string html;
        std::string finalUrl;

        if (!httpGet(baseUrl, html, finalUrl))
        {
            std::cerr
                << "[ERREUR] Impossible de recuperer la page."
                << std::endl;

            throw std::runtime_error(
                "Echec de la premiere requete HTTP"
            );
        }

        std::cout
            << "[OK] Page recuperee."
            << std::endl;

        // --------------------------------------
        // Extraction du lien
        // --------------------------------------

        std::cout
            << "[INFO] Recherche du bouton principal..."
            << std::endl;

        std::string href = extractHref(html);

        if (href.empty())
        {
            std::cerr
                << "[ERREUR] Aucun lien correspondant trouve."
                << std::endl;

            throw std::runtime_error(
                "Lien principal introuvable"
            );
        }

        std::cout
            << "[OK] Lien trouve : "
            << href
            << std::endl;

        // --------------------------------------
        // Conversion URL relative -> absolue
        // --------------------------------------

        if (href.rfind("//", 0) == 0)
        {
            href = "https:" + href;
        }
        else if (href.rfind("http://", 0) != 0 &&
                 href.rfind("https://", 0) != 0)
        {
            if (!href.empty() && href[0] == '/')
                href = baseUrl + href;
            else
                href = baseUrl + "/" + href;
        }

        std::cout
            << "[INFO] URL completee : "
            << href
            << std::endl;

        // --------------------------------------
        // Deuxième requête
        // --------------------------------------

        std::cout
            << "[INFO] Recuperation de l'URL de destination..."
            << std::endl;

        std::string response2;
        std::string redirectedUrl;

        if (!httpGet(href, response2, redirectedUrl))
        {
            std::cerr
                << "[ERREUR] Impossible de recuperer la destination."
                << std::endl;

            throw std::runtime_error(
                "Echec de la deuxieme requete HTTP"
            );
        }

        // --------------------------------------
        // Extraction du domaine
        // --------------------------------------

        std::cout
            << "[INFO] Extraction de l'extension du domaine..."
            << std::endl;

        std::string extension =
            extractExtension(redirectedUrl);

        if (extension.empty())
        {
            std::cerr
                << "[ERREUR] Extension du domaine introuvable."
                << std::endl;

            std::cerr
                << "[DEBUG] URL obtenue : "
                << redirectedUrl
                << std::endl;

            throw std::runtime_error(
                "Extension du domaine introuvable"
            );
        }

        std::cout
            << "[OK] Extension trouvee : "
            << extension
            << std::endl;

        // --------------------------------------
        // Creation du dossier data
        // --------------------------------------

        fs::path dataDir = exeDir / "data";
        fs::path jsonPath = dataDir / "domaine.json";

        std::cout
            << "[INFO] Dossier de sortie : "
            << dataDir.string()
            << std::endl;

        fs::create_directories(dataDir);

        std::cout
            << "[OK] Dossier data pret."
            << std::endl;

        // --------------------------------------
        // Creation du JSON
        // --------------------------------------

        json j;
        j["domaine"] = extension;

        std::cout
            << "[INFO] Ecriture du fichier JSON..."
            << std::endl;

        std::ofstream file(jsonPath);

        if (!file.is_open())
        {
            throw std::runtime_error(
                "Impossible d'ouvrir le fichier JSON"
            );
        }

        file << j.dump(4) << std::endl;
        file.close();

        if (!file)
        {
            throw std::runtime_error(
                "Erreur pendant l'ecriture du JSON"
            );
        }

        std::cout
            << "[OK] Fichier cree : "
            << jsonPath.string()
            << std::endl;

        exitCode = 0;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "[ERREUR] "
            << e.what()
            << std::endl;
    }
    catch (...)
    {
        std::cerr
            << "[ERREUR] Erreur inconnue."
            << std::endl;
    }

    // --------------------------------------
    // Fin
    // --------------------------------------

    curl_global_cleanup();

    std::cout << std::endl;
    std::cout
        << "========================================"
        << std::endl;

    if (exitCode == 0)
    {
        std::cout
            << "              TERMINE"
            << std::endl;
    }
    else
    {
        std::cout
            << "               ECHEC"
            << std::endl;
    }

    std::cout
        << "========================================"
        << std::endl;

    return exitCode;
}
