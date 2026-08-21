#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using json = nlohmann::json;
namespace fs = std::filesystem;

// ==========================================================
// CONFIGURATION
// ==========================================================

const std::string DOMAIN_FILE = "data/domaine.json";
const std::string RESULT_FILE = "data/result.json";
const std::string STATUS_FILE = "data/status.json";

// ==========================================================
// COLORS
// ==========================================================

const std::string BLUE    = "\033[94m";
const std::string YELLOW  = "\033[93m";
const std::string GREEN   = "\033[92m";
const std::string RED     = "\033[91m";
const std::string CYAN    = "\033[96m";
const std::string MAGENTA = "\033[95m";
const std::string RESET   = "\033[0m";

// ==========================================================
// TRIM
// ==========================================================

std::string trim(std::string value)
{
    auto notSpace = [](unsigned char c)
    {
        return !std::isspace(c);
    };

    value.erase(
        value.begin(),
        std::find_if(
            value.begin(),
            value.end(),
            notSpace
        )
    );

    value.erase(
        std::find_if(
            value.rbegin(),
            value.rend(),
            notSpace
        ).base(),
        value.end()
    );

    return value;
}

// ==========================================================
// CHARGEMENT DU DOMAINE
// ==========================================================

std::string load_base_url()
{
    std::ifstream file(DOMAIN_FILE);

    if (!file)
    {
        throw std::runtime_error(
            "Impossible d'ouvrir " + DOMAIN_FILE
        );
    }

    json data;

    try
    {
        file >> data;
    }
    catch (const json::parse_error& e)
    {
        throw std::runtime_error(
            std::string("JSON invalide dans ") +
            DOMAIN_FILE +
            " : " +
            e.what()
        );
    }

    if (!data.is_object())
    {
        throw std::runtime_error(
            DOMAIN_FILE +
            " doit contenir un objet JSON."
        );
    }

    if (!data.contains("domaine"))
    {
        throw std::runtime_error(
            DOMAIN_FILE +
            " doit contenir la clé \"domaine\"."
        );
    }

    if (!data["domaine"].is_string())
    {
        throw std::runtime_error(
            "\"domaine\" doit être une chaîne de caractères."
        );
    }

    std::string domaine =
        data["domaine"].get<std::string>();

    domaine = trim(domaine);

    if (domaine.empty())
    {
        throw std::runtime_error(
            "Le domaine dans " +
            DOMAIN_FILE +
            " est vide."
        );
    }

    // Retire les points inutiles au début.
    while (
        !domaine.empty() &&
        domaine.front() == '.'
    )
    {
        domaine.erase(domaine.begin());
    }

    // Retire les slashs inutiles à la fin.
    while (
        !domaine.empty() &&
        domaine.back() == '/'
    )
    {
        domaine.pop_back();
    }

    // Le JSON doit contenir uniquement :
    // to
    // com
    // net
    // etc.
    if (
        domaine.find("://") != std::string::npos ||
        domaine.find('/') != std::string::npos
    )
    {
        throw std::runtime_error(
            "Le champ \"domaine\" doit contenir uniquement "
            "l'extension du domaine, par exemple \"to\"."
        );
    }

    return "https://anime-sama." + domaine;
}

// ==========================================================
// CURL CALLBACK
// ==========================================================

size_t WriteCallback(
    void* contents,
    size_t size,
    size_t nmemb,
    void* userp
)
{
    size_t totalSize = size * nmemb;

    std::string* output =
        static_cast<std::string*>(userp);

    output->append(
        static_cast<char*>(contents),
        totalSize
    );

    return totalSize;
}

// ==========================================================
// CURL REQUEST
// ==========================================================

std::string get_url(const std::string& url)
{
    CURL* curl = curl_easy_init();

    if (!curl)
    {
        throw std::runtime_error(
            "Impossible d'initialiser CURL."
        );
    }

    std::string response;

    // ------------------------------------------------------
    // Headers
    // ------------------------------------------------------

    struct curl_slist* headers = nullptr;

    headers = curl_slist_append(
        headers,
        "Accept: text/html,application/xhtml+xml,"
        "application/xml;q=0.9,image/avif,image/webp,"
        "*/*;q=0.8"
    );

    headers = curl_slist_append(
        headers,
        "Accept-Language: fr-FR,fr;q=0.9,en-US;q=0.8,en;q=0.7"
    );

    headers = curl_slist_append(
        headers,
        "Cache-Control: no-cache"
    );

    headers = curl_slist_append(
        headers,
        "Pragma: no-cache"
    );

    // ------------------------------------------------------
    // Options
    // ------------------------------------------------------

    curl_easy_setopt(
        curl,
        CURLOPT_URL,
        url.c_str()
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
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/139.0.0.0 Safari/537.36"
    );

    curl_easy_setopt(
        curl,
        CURLOPT_HTTPHEADER,
        headers
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
        15L
    );

    curl_easy_setopt(
        curl,
        CURLOPT_TIMEOUT,
        30L
    );

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

    // ------------------------------------------------------
    // Request
    // ------------------------------------------------------

    CURLcode result =
        curl_easy_perform(curl);

    if (result != CURLE_OK)
    {
        std::string error =
            curl_easy_strerror(result);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        throw std::runtime_error(
            "Erreur CURL : " + error
        );
    }

    // ------------------------------------------------------
    // HTTP code
    // ------------------------------------------------------

    long httpCode = 0;

    curl_easy_getinfo(
        curl,
        CURLINFO_RESPONSE_CODE,
        &httpCode
    );

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (httpCode < 200 || httpCode >= 300)
    {
        std::ostringstream error;

        error
            << "HTTP "
            << httpCode
            << " pour "
            << url;

        throw std::runtime_error(
            error.str()
        );
    }

    return response;
}

// ==========================================================
// URL ENCODING
// ==========================================================

std::string url_encode(const std::string& value)
{
    CURL* curl = curl_easy_init();

    if (!curl)
    {
        throw std::runtime_error(
            "Impossible d'initialiser CURL pour l'encodage."
        );
    }

    char* encoded =
        curl_easy_escape(
            curl,
            value.c_str(),
            static_cast<int>(value.length())
        );

    if (!encoded)
    {
        curl_easy_cleanup(curl);

        throw std::runtime_error(
            "Impossible d'encoder l'URL."
        );
    }

    std::string result = encoded;

    curl_free(encoded);
    curl_easy_cleanup(curl);

    return result;
}

// ==========================================================
// LOWERCASE
// ==========================================================

std::string to_lower(std::string value)
{
    for (char& c : value)
    {
        c = static_cast<char>(
            std::tolower(
                static_cast<unsigned char>(c)
            )
        );
    }

    return value;
}

// ==========================================================
// HTML TEXT CLEANUP
// ==========================================================

std::string clean_html_text(const std::string& value)
{
    // ------------------------------------------------------
    // Supprimer les balises HTML simples
    // ------------------------------------------------------

    std::string result;
    bool insideTag = false;

    for (char c : value)
    {
        if (c == '<')
        {
            insideTag = true;
            continue;
        }

        if (c == '>')
        {
            insideTag = false;
            continue;
        }

        if (!insideTag)
        {
            result += c;
        }
    }

    // ------------------------------------------------------
    // Entités HTML courantes
    // ------------------------------------------------------

    auto replace_all =
        [](std::string& str,
           const std::string& from,
           const std::string& to)
        {
            size_t pos = 0;

            while (
                (pos = str.find(from, pos))
                != std::string::npos
            )
            {
                str.replace(
                    pos,
                    from.length(),
                    to
                );

                pos += to.length();
            }
        };

    replace_all(result, "&amp;", "&");
    replace_all(result, "&quot;", "\"");
    replace_all(result, "&#39;", "'");
    replace_all(result, "&lt;", "<");
    replace_all(result, "&gt;", ">");

    // Espaces HTML
    replace_all(result, "&nbsp;", " ");

    return trim(result);
}

// ==========================================================
// EXTRACT BETWEEN
// ==========================================================

std::string extract_between(
    const std::string& text,
    const std::string& startToken,
    const std::string& endToken,
    size_t from = 0
)
{
    size_t start =
        text.find(
            startToken,
            from
        );

    if (start == std::string::npos)
    {
        return "";
    }

    start += startToken.length();

    size_t end =
        text.find(
            endToken,
            start
        );

    if (end == std::string::npos)
    {
        return "";
    }

    return text.substr(
        start,
        end - start
    );
}

// ==========================================================
// GET WORK NAME / TITLE
// ==========================================================

std::string get_work_name(
    const std::string& page,
    const std::string& identifier
)
{
    // ------------------------------------------------------
    // 1. #titreOeuvre
    // ------------------------------------------------------

    std::vector<
        std::pair<std::string, std::string>
    > htmlPatterns =
    {
        {
            "<div id=\"titreOeuvre\">",
            "</div>"
        },
        {
            "<span id=\"titreOeuvre\">",
            "</span>"
        },
        {
            "<h1 id=\"titreOeuvre\">",
            "</h1>"
        },
        {
            "<h2 id=\"titreOeuvre\">",
            "</h2>"
        },
        {
            "<p id=\"titreOeuvre\">",
            "</p>"
        }
    };

    for (const auto& pattern : htmlPatterns)
    {
        std::string value =
            extract_between(
                page,
                pattern.first,
                pattern.second
            );

        value =
            clean_html_text(value);

        if (!value.empty())
        {
            return value;
        }
    }

    // ------------------------------------------------------
    // 2. id='titreOeuvre' ou id="titreOeuvre"
    // ------------------------------------------------------

    {
        const std::vector<std::string> markers =
        {
            "id=\"titreOeuvre\"",
            "id='titreOeuvre'"
        };

        for (const auto& marker : markers)
        {
            size_t pos =
                page.find(marker);

            if (pos == std::string::npos)
            {
                continue;
            }

            size_t gt =
                page.find(
                    '>',
                    pos
                );

            if (gt == std::string::npos)
            {
                continue;
            }

            size_t end =
                page.find(
                    '<',
                    gt + 1
                );

            if (end == std::string::npos)
            {
                continue;
            }

            std::string value =
                page.substr(
                    gt + 1,
                    end - gt - 1
                );

            value =
                clean_html_text(value);

            if (!value.empty())
            {
                return value;
            }
        }
    }

    // ------------------------------------------------------
    // 3. JavaScript
    // ------------------------------------------------------

    const std::vector<std::string> jsMarkers =
    {
        "titreOeuvre = \"",
        "titreOeuvre='",
        "titreOeuvre = '",
        "titreOeuvre=\"",
        "titreOeuvre: \"",
        "titreOeuvre: '",
        "\"titreOeuvre\":\"",
        "\"titreOeuvre\": \""
    };

    for (const auto& marker : jsMarkers)
    {
        size_t pos =
            page.find(marker);

        if (pos == std::string::npos)
        {
            continue;
        }

        pos += marker.length();

        char quote = 0;

        if (
            marker.back() == '"' ||
            marker.back() == '\''
        )
        {
            quote = marker.back();
        }

        if (!quote)
        {
            continue;
        }

        size_t end =
            page.find(
                quote,
                pos
            );

        if (end == std::string::npos)
        {
            continue;
        }

        std::string value =
            page.substr(
                pos,
                end - pos
            );

        value =
            clean_html_text(value);

        if (!value.empty())
        {
            return value;
        }
    }

    // ------------------------------------------------------
    // 4. Fallback
    // ------------------------------------------------------

    return identifier;
}

// ==========================================================
// GET IDENTIFIER FROM URL
// ==========================================================

std::string get_identifier_from_url(
    const std::string& url
)
{
    const std::string marker =
        "/catalogue/";

    size_t start =
        url.find(marker);

    if (start == std::string::npos)
    {
        return "";
    }

    start += marker.length();

    size_t end =
        url.find(
            '/',
            start
        );

    if (end == std::string::npos)
    {
        end = url.length();
    }

    if (end <= start)
    {
        return "";
    }

    return url.substr(
        start,
        end - start
    );
}

// ==========================================================
// VALIDATION URL
// ==========================================================

bool is_valid_anime_sama_url(
    const std::string& url,
    const std::string& base_url
)
{
    std::string expected_https =
        base_url + "/";

    if (
        url.rfind(
            expected_https,
            0
        ) == 0
    )
    {
        return true;
    }

    // Autorise également HTTP.
    if (
        base_url.rfind(
            "https://",
            0
        ) == 0
    )
    {
        std::string http_base =
            "http://" +
            base_url.substr(
                std::string("https://").length()
            );

        std::string expected_http =
            http_base + "/";

        if (
            url.rfind(
                expected_http,
                0
            ) == 0
        )
        {
            return true;
        }
    }

    return false;
}

// ==========================================================
// GET SCAN INFORMATION
// ==========================================================

json get_scan_info(
    const std::string& url,
    const std::string& identifier,
    const std::string& base_url
)
{
    std::cout << "\n";
    std::cout
        << "==========================================\n";
    std::cout
        << "SCAN\n";
    std::cout
        << "==========================================\n";

    std::cout
        << "URL : "
        << url
        << "\n";

    // ======================================================
    // PAGE
    // ======================================================

    std::cout
        << "[*] Récupération de la page...\n";

    std::string page =
        get_url(url);

    if (page.empty())
    {
        throw std::runtime_error(
            "Page vide."
        );
    }

    std::cout
        << "[+] Page récupérée : "
        << page.size()
        << " octets\n";

    // ======================================================
    // TITLE / WORK
    // ======================================================

    std::string work_name =
        get_work_name(
            page,
            identifier
        );

    work_name =
        trim(work_name);

    if (work_name.empty())
    {
        work_name =
            identifier;
    }

    std::cout
        << "[+] Title : "
        << work_name
        << "\n";

    // ======================================================
    // ENCODAGE
    // ======================================================

    std::string encoded_work =
        url_encode(work_name);

    // ======================================================
    // API
    // ======================================================

    std::string api_url =
        base_url +
        "/s2/scans/get_nb_chap_et_img.php?oeuvre=" +
        encoded_work;

    std::cout
        << "[API] "
        << api_url
        << "\n";

    std::string api_response =
        get_url(api_url);

    if (api_response.empty())
    {
        throw std::runtime_error(
            "Réponse API vide."
        );
    }

    // ======================================================
    // PARSE JSON
    // ======================================================

    json data;

    try
    {
        data =
            json::parse(
                api_response
            );
    }
    catch (const json::parse_error& e)
    {
        std::cerr
            << "\n"
            << RED
            << "[ERROR] JSON API invalide."
            << RESET
            << "\n";

        std::cerr
            << "Réponse :\n"
            << api_response
            << "\n";

        throw std::runtime_error(
            std::string(
                "JSON API invalide : "
            ) +
            e.what()
        );
    }

    // ======================================================
    // DEBUG API
    // ======================================================

    std::cout
        << "\n"
        << CYAN
        << "[API RESPONSE]"
        << RESET
        << "\n";

    std::cout
        << data.dump(4)
        << "\n";

    // ======================================================
    // ERROR API
    // ======================================================

    if (
        data.is_object() &&
        data.contains("error")
    )
    {
        std::string error;

        if (
            data["error"].is_string()
        )
        {
            error =
                data["error"].get<std::string>();
        }
        else
        {
            error =
                data["error"].dump();
        }

        throw std::runtime_error(
            "Erreur retournée par l'API : " +
            error
        );
    }

    // ======================================================
    // VALIDATION
    // ======================================================

    if (!data.is_object())
    {
        throw std::runtime_error(
            "L'API ne retourne pas un objet JSON."
        );
    }

    // ======================================================
    // BASE IMAGE URL
    // ======================================================

    std::string base_image_url =
        base_url +
        "/s2/scans/" +
        encoded_work +
        "/";

    std::cout
        << "[+] Base image URL : "
        << base_image_url
        << "\n";

    // ======================================================
    // CHAPTERS
    // ======================================================

    json chapters =
        json::object();

    long long total_images =
        0;

    for (
        auto it = data.begin();
        it != data.end();
        ++it
    )
    {
        std::string chapter =
            it.key();

        // --------------------------------------------------
        // Nombre d'images
        // --------------------------------------------------

        long long image_count =
            0;

        bool valid =
            false;

        try
        {
            if (
                it.value().is_number_integer()
            )
            {
                image_count =
                    it.value().get<long long>();

                valid = true;
            }
            else if (
                it.value().is_string()
            )
            {
                std::string value =
                    it.value().get<std::string>();

                image_count =
                    std::stoll(value);

                valid = true;
            }
        }
        catch (...)
        {
            valid = false;
        }

        if (!valid)
        {
            std::cerr
                << RED
                << "[-] Nombre d'images invalide : "
                << chapter
                << " = "
                << it.value().dump()
                << RESET
                << "\n";

            continue;
        }

        if (image_count < 0)
        {
            image_count = 0;
        }

        // --------------------------------------------------
        // Images
        // --------------------------------------------------

        json images =
            json::array();

        for (
            long long page_number = 1;
            page_number <= image_count;
            ++page_number
        )
        {
            std::string image_url =
                base_image_url +
                chapter +
                "/" +
                std::to_string(page_number) +
                ".jpg";

            images.push_back(
                image_url
            );
        }

        chapters[chapter] =
        {
            {
                "image_count",
                image_count
            },
            {
                "images",
                images
            }
        };

        total_images +=
            image_count;
    }

    // ======================================================
    // RESULT
    // ======================================================

    json result =
    {
        {
            "source",
            url
        },
        {
            "identifier",
            identifier
        },

        // ==================================================
        // TITRE
        // ==================================================
        {
            "titre",
            work_name
        },

        // Conservé également pour compatibilité.
        {
            "work",
            work_name
        },

        {
            "base_image_url",
            base_image_url
        },
        {
            "chapter_count",
            chapters.size()
        },
        {
            "total_images",
            total_images
        },
        {
            "chapters",
            chapters
        }
    };

    std::cout
        << "[+] Titre : "
        << GREEN
        << work_name
        << RESET
        << "\n";

    std::cout
        << "[+] Chapitres : "
        << GREEN
        << chapters.size()
        << RESET
        << "\n";

    std::cout
        << "[+] Images : "
        << GREEN
        << total_images
        << RESET
        << "\n";

    return result;
}

// ==========================================================
// INITIALISATION RESULT.JSON
// ==========================================================

void clear_result_file()
{
    try
    {
        // --------------------------------------------------
        // Création du dossier data/
        // --------------------------------------------------

        fs::create_directories("data");

        // --------------------------------------------------
        // Création / vidage de result.json
        // --------------------------------------------------

        std::ofstream file(
            RESULT_FILE,
            std::ios::trunc
        );

        if (!file)
        {
            throw std::runtime_error(
                "Impossible de créer " +
                RESULT_FILE
            );
        }

        file
            << "{\n"
            << "    \"results\": []\n"
            << "}\n";

        file.close();
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error(
            std::string(
                "Erreur lors de l'initialisation de "
            ) +
            RESULT_FILE +
            " : " +
            e.what()
        );
    }
}

// ==========================================================
// SET STATUS
// ==========================================================

void set_status(bool status)
{
    try
    {
        // --------------------------------------------------
        // Création du dossier data/
        // --------------------------------------------------

        fs::create_directories("data");

        // --------------------------------------------------
        // Création / écriture de status.json
        // --------------------------------------------------

        json data =
        {
            {
                "status",
                status
            }
        };

        std::ofstream output(
            STATUS_FILE,
            std::ios::trunc
        );

        if (!output)
        {
            throw std::runtime_error(
                "Impossible d'écrire dans " +
                STATUS_FILE
            );
        }

        output
            << data.dump(4)
            << "\n";

        output.close();
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error(
            std::string(
                "Erreur lors de l'écriture de "
            ) +
            STATUS_FILE +
            " : " +
            e.what()
        );
    }
}

// ==========================================================
// SAVE RESULT
// ==========================================================

void save_result(const json& result)
{
    json data;

    // ------------------------------------------------------
    // Lecture du fichier existant
    // ------------------------------------------------------

    std::ifstream input(
        RESULT_FILE
    );

    if (input)
    {
        try
        {
            input >> data;
        }
        catch (...)
        {
            data =
            {
                {
                    "results",
                    json::array()
                }
            };
        }

        input.close();
    }

    // ------------------------------------------------------
    // Validation
    // ------------------------------------------------------

    if (!data.is_object())
    {
        data =
        {
            {
                "results",
                json::array()
            }
        };
    }

    if (
        !data.contains("results") ||
        !data["results"].is_array()
    )
    {
        data["results"] =
            json::array();
    }

    // ------------------------------------------------------
    // Ajout du résultat
    // ------------------------------------------------------

    data["results"].push_back(
        result
    );

    // ------------------------------------------------------
    // Réécriture
    // ------------------------------------------------------

    std::ofstream output(
        RESULT_FILE,
        std::ios::trunc
    );

    if (!output)
    {
        throw std::runtime_error(
            "Impossible d'écrire dans " +
            RESULT_FILE
        );
    }

    output
        << data.dump(4)
        << "\n";

    output.close();
}

// ==========================================================
// USAGE
// ==========================================================

void print_usage(const char* program)
{
    std::cout
        << "\n"
        << "Usage :\n"
        << "\n"
        << "  "
        << program
        << " -link "
        << "\"https://anime-sama.to/catalogue/.../scan/vf/\""
        << "\n"
        << "\n"
        << "Exemple :\n"
        << "\n"
        << "  "
        << program
        << " -link "
        << "\"https://anime-sama.to/catalogue/"
        << "all-my-alter-egos-are-tycoons/scan/vf/\""
        << "\n"
        << "\n";
}

// ==========================================================
// MAIN
// ==========================================================

int main(
    int argc,
    char* argv[]
)
{
    // ======================================================
    // ARGUMENTS
    // ======================================================

    if (argc != 3)
    {
        print_usage(argv[0]);
        return 1;
    }

    std::string option =
        argv[1];

    std::string url =
        argv[2];

    // ======================================================
    // VALIDATION OPTION
    // ======================================================

    if (option != "-link")
    {
        std::cerr
            << RED
            << "[ERROR] Option inconnue : "
            << option
            << RESET
            << "\n";

        print_usage(argv[0]);
        return 1;
    }

    // ======================================================
    // VALIDATION URL VIDE
    // ======================================================

    if (url.empty())
    {
        std::cerr
            << RED
            << "[ERROR] URL vide."
            << RESET
            << "\n";

        return 1;
    }

    // ======================================================
    // CURL INIT
    // ======================================================

    CURLcode curlResult =
        curl_global_init(
            CURL_GLOBAL_DEFAULT
        );

    if (curlResult != CURLE_OK)
    {
        std::cerr
            << RED
            << "[ERROR] Impossible d'initialiser CURL."
            << RESET
            << "\n";

        return 1;
    }

    // ======================================================
    // CHARGEMENT DU DOMAINE
    // ======================================================

    std::string base_url;

    try
    {
        base_url =
            load_base_url();

        std::cout
            << GREEN
            << "[OK] Domaine chargé : "
            << base_url
            << RESET
            << "\n";
    }
    catch (const std::exception& e)
    {
        std::cerr
            << RED
            << "[ERROR] "
            << e.what()
            << RESET
            << "\n";

        curl_global_cleanup();
        return 1;
    }

    // ======================================================
    // VALIDATION URL
    // ======================================================

    if (
        !is_valid_anime_sama_url(
            url,
            base_url
        )
    )
    {
        std::cerr
            << RED
            << "[ERROR] L'URL doit appartenir à "
            << base_url
            << "."
            << RESET
            << "\n";

        curl_global_cleanup();
        return 1;
    }

    // ======================================================
    // IDENTIFIER
    // ======================================================

    std::string identifier =
        get_identifier_from_url(
            url
        );

    if (identifier.empty())
    {
        std::cerr
            << RED
            << "[ERROR] Impossible de déterminer "
               "l'identifiant depuis l'URL."
            << RESET
            << "\n";

        curl_global_cleanup();
        return 1;
    }

    // ======================================================
    // INITIALISATION DES FICHIERS
    // ======================================================

    try
    {
        // --------------------------------------------------
        // result.json = vide
        // --------------------------------------------------

        clear_result_file();

        // --------------------------------------------------
        // status.json = false
        // --------------------------------------------------

        set_status(false);

        std::cout
            << GREEN
            << "[OK] "
            << RESULT_FILE
            << " vidé au lancement."
            << RESET
            << "\n";

        std::cout
            << GREEN
            << "[OK] "
            << STATUS_FILE
            << " initialisé à false."
            << RESET
            << "\n";
    }
    catch (const std::exception& e)
    {
        std::cerr
            << RED
            << "[ERROR] "
            << e.what()
            << RESET
            << "\n";

        curl_global_cleanup();
        return 1;
    }

    // ======================================================
    // INFORMATIONS
    // ======================================================

    std::cout
        << "\n"
        << "==========================================\n"
        << "ANIME-SAMA SCAN\n"
        << "==========================================\n";

    std::cout
        << "Domaine    : "
        << base_url
        << "\n";

    std::cout
        << "Identifier : "
        << identifier
        << "\n";

    std::cout
        << "URL        : "
        << url
        << "\n";

    // ======================================================
    // TRAITEMENT
    // ======================================================

    try
    {
        json result =
            get_scan_info(
                url,
                identifier,
                base_url
            );

        // --------------------------------------------------
        // SAUVEGARDE RESULT.JSON
        // --------------------------------------------------

        save_result(
            result
        );

        // --------------------------------------------------
        // TRAITEMENT TERMINÉ AVEC SUCCÈS
        // --------------------------------------------------

        set_status(true);

        std::cout
            << "\n"
            << GREEN
            << "[OK] Résultat sauvegardé dans "
            << RESULT_FILE
            << RESET
            << "\n";

        std::cout
            << GREEN
            << "[OK] Status terminé : true"
            << RESET
            << "\n";

        // --------------------------------------------------
        // AFFICHAGE COMPLET
        // --------------------------------------------------

        std::cout
            << "\n";

        std::cout
            << "==========================================\n"
            << "RESULTAT COMPLET\n"
            << "==========================================\n";

        std::cout
            << result.dump(4)
            << "\n";

        // --------------------------------------------------
        // RÉSUMÉ DES CHAPITRES
        // --------------------------------------------------

        if (
            result.contains("chapters") &&
            result["chapters"].is_object()
        )
        {
            std::cout
                << "\n";

            std::cout
                << "==========================================\n"
                << "CHAPITRES\n"
                << "==========================================\n";

            for (
                auto it =
                    result["chapters"].begin();
                it !=
                    result["chapters"].end();
                ++it
            )
            {
                std::cout
                    << "Chapitre "
                    << it.key()
                    << " : "
                    << it.value()["image_count"]
                    << " image(s)"
                    << "\n";
            }
        }
    }
    catch (const std::exception& e)
    {
        // --------------------------------------------------
        // En cas d'erreur :
        // status.json reste / repasse à false
        // --------------------------------------------------

        try
        {
            set_status(false);
        }
        catch (...)
        {
            // Ne pas masquer l'erreur originale.
        }

        std::cerr
            << "\n"
            << RED
            << "[ERROR] "
            << e.what()
            << RESET
            << "\n";

        curl_global_cleanup();
        return 1;
    }

    // ======================================================
    // CLEANUP
    // ======================================================

    curl_global_cleanup();

    std::cout
        << "\n"
        << GREEN
        << "Programme terminé."
        << RESET
        << "\n";

    return 0;
}