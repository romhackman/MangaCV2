#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

using json = nlohmann::json;
namespace fs = std::filesystem;

// ==================================================
// CONFIGURATION
// ==================================================

const std::string DOMAIN_FILE =
    "data/domaine.json";

std::string BASE_URL;

const std::string SITEMAP_FILE =
    "data/temp/sitemap_1.xml";

const std::string OUTPUT_FILE =
    "data/temp/all_link.json";

const std::string SCAN_OUTPUT_FILE =
    "data/temp/scan_colecte.json";

const std::string HERITAGE_OUTPUT_FILE =
    "data/heritage.json";

// ==================================================
// CONSOLE COLORS
// ==================================================

const std::string GREEN =
    "\033[92m";

const std::string YELLOW =
    "\033[93m";

const std::string RESET =
    "\033[0m";

// ==================================================
// CURL CALLBACK
// ==================================================

size_t WriteCallback(
    void* contents,
    size_t size,
    size_t nmemb,
    void* userp
)
{
    const size_t totalSize =
        size * nmemb;

    std::string* output =
        static_cast<std::string*>(userp);

    output->append(
        static_cast<char*>(contents),
        totalSize
    );

    return totalSize;
}

// ==================================================
// DOWNLOAD URL
// ==================================================

std::string get_url(
    const std::string& url
)
{
    CURL* curl =
        curl_easy_init();

    if (!curl)
    {
        std::cerr
            << "Impossible d'initialiser CURL.\n";

        return "";
    }

    std::string response;

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
        "Mozilla/5.0 (compatible; SimpleBot/1.0)"
    );

    curl_easy_setopt(
        curl,
        CURLOPT_TIMEOUT,
        10L
    );

    curl_easy_setopt(
        curl,
        CURLOPT_FOLLOWLOCATION,
        1L
    );

    CURLcode result =
        curl_easy_perform(curl);

    if (result != CURLE_OK)
    {
        std::cerr
            << "Error for "
            << url
            << ": "
            << curl_easy_strerror(result)
            << "\n";

        curl_easy_cleanup(curl);

        return "";
    }

    long httpCode = 0;

    curl_easy_getinfo(
        curl,
        CURLINFO_RESPONSE_CODE,
        &httpCode
    );

    if (httpCode < 200 || httpCode >= 300)
    {
        std::cerr
            << "HTTP error "
            << httpCode
            << " for "
            << url
            << "\n";

        curl_easy_cleanup(curl);

        return "";
    }

    curl_easy_cleanup(curl);

    return response;
}

// ==================================================
// SAVE FILE
// ==================================================

bool save_file(
    const std::string& filename,
    const std::string& content
)
{
    if (content.empty())
        return false;

    fs::path path(filename);

    if (path.has_parent_path())
    {
        fs::create_directories(
            path.parent_path()
        );
    }

    std::ofstream file(
        filename,
        std::ios::binary
    );

    if (!file)
    {
        std::cerr
            << "Impossible d'écrire : "
            << filename
            << "\n";

        return false;
    }

    file << content;

    file.close();

    std::cout
        << GREEN
        << "[OK] Saved: "
        << filename
        << RESET
        << "\n";

    return true;
}

// ==================================================
// PARSE SITEMAP
// ==================================================

std::vector<std::string> parse_sitemap(
    const std::string& content
)
{
    std::vector<std::string> urls;

    const std::string startTag =
        "<loc>";

    const std::string endTag =
        "</loc>";

    size_t position = 0;

    while (true)
    {
        size_t start =
            content.find(
                startTag,
                position
            );

        if (start == std::string::npos)
            break;

        start += startTag.length();

        size_t end =
            content.find(
                endTag,
                start
            );

        if (end == std::string::npos)
            break;

        std::string url =
            content.substr(
                start,
                end - start
            );

        // Trim début
        while (
            !url.empty() &&
            (
                url.front() == ' ' ||
                url.front() == '\n' ||
                url.front() == '\r' ||
                url.front() == '\t'
            )
        )
        {
            url.erase(url.begin());
        }

        // Trim fin
        while (
            !url.empty() &&
            (
                url.back() == ' ' ||
                url.back() == '\n' ||
                url.back() == '\r' ||
                url.back() == '\t'
            )
        )
        {
            url.pop_back();
        }

        if (!url.empty())
        {
            urls.push_back(url);
        }

        position =
            end + endTag.length();
    }

    return urls;
}

// ==================================================
// BUILD ALL_LINK.JSON
// ==================================================

json build_json_all(
    const std::vector<std::string>& urls
)
{
    json data;

    data["link"] =
        json::array();

    for (const auto& url : urls)
    {
        data["link"].push_back(url);
    }

    return data;
}

// ==================================================
// LOAD DOMAIN
// ==================================================

bool load_domain()
{
    std::ifstream file(DOMAIN_FILE);

    if (!file)
    {
        std::cerr
            << "ERROR: Cannot open domain file: "
            << DOMAIN_FILE
            << "\n";

        return false;
    }

    json data;

    try
    {
        file >> data;
    }
    catch (const json::parse_error& e)
    {
        std::cerr
            << "ERROR: Invalid domain JSON: "
            << e.what()
            << "\n";

        return false;
    }

    if (!data.contains("domaine") ||
        !data["domaine"].is_string())
    {
        std::cerr
            << "ERROR: 'domaine' must be a string.\n";

        return false;
    }

    std::string domaine =
        data["domaine"].get<std::string>();

    // Supprimer les espaces éventuels
    while (!domaine.empty() &&
           std::isspace(
               static_cast<unsigned char>(
                   domaine.front()
               )
           ))
    {
        domaine.erase(domaine.begin());
    }

    while (!domaine.empty() &&
           std::isspace(
               static_cast<unsigned char>(
                   domaine.back()
               )
           ))
    {
        domaine.pop_back();
    }

    if (domaine.empty())
    {
        std::cerr
            << "ERROR: Domain is empty.\n";

        return false;
    }

    // Permet aussi d'écrire "to" ou ".to"
    if (domaine.front() == '.')
    {
        domaine.erase(domaine.begin());
    }

    BASE_URL =
        "https://anime-sama." + domaine;

    std::cout
        << "Domain loaded: "
        << BASE_URL
        << "\n";

    return true;
}

// ==================================================
// DOWNLOAD SITEMAP
// ==================================================

bool download_sitemap()
{
    const std::string sitemap_url =
        BASE_URL + "/sitemap.xml";

    std::cout
        << "Downloading sitemap: "
        << sitemap_url
        << "\n";

    std::string sitemap_content =
        get_url(sitemap_url);

    if (sitemap_content.empty())
    {
        std::cerr
            << "Unable to download the sitemap.\n";

        return false;
    }

    return save_file(
        SITEMAP_FILE,
        sitemap_content
    );
}

// ==================================================
// GENERATE ALL_LINK.JSON
// ==================================================

bool main_json()
{
    if (!download_sitemap())
        return false;

    std::ifstream file(
        SITEMAP_FILE
    );

    if (!file)
    {
        std::cerr
            << "Impossible d'ouvrir "
            << SITEMAP_FILE
            << "\n";

        return false;
    }

    std::string sitemap_content(
        (
            std::istreambuf_iterator<char>(
                file
            )
        ),
        std::istreambuf_iterator<char>()
    );

    file.close();

    std::vector<std::string> urls =
        parse_sitemap(
            sitemap_content
        );

    std::cout
        << "URLs found in sitemap: "
        << urls.size()
        << "\n";

    json result =
        build_json_all(urls);

    fs::path outputPath(
        OUTPUT_FILE
    );

    if (outputPath.has_parent_path())
    {
        fs::create_directories(
            outputPath.parent_path()
        );
    }

    std::ofstream output(
        OUTPUT_FILE
    );

    if (!output)
    {
        std::cerr
            << "Impossible d'écrire "
            << OUTPUT_FILE
            << "\n";

        return false;
    }

    output
        << result.dump(4);

    output.close();

    std::cout
        << YELLOW
        << "JSON generated: "
        << OUTPUT_FILE
        << RESET
        << "\n";

    std::cout
        << "URLs saved: "
        << result["link"].size()
        << "\n";

    return true;
}

// ==================================================
// GET PATH FROM URL
// ==================================================

std::string get_path_from_url(
    const std::string& url
)
{
    size_t scheme =
        url.find("://");

    size_t pathStart;

    if (scheme != std::string::npos)
    {
        pathStart =
            url.find(
                '/',
                scheme + 3
            );
    }
    else
    {
        pathStart =
            url.find('/');
    }

    if (pathStart == std::string::npos)
        return "";

    std::string path =
        url.substr(pathStart);

    // Supprimer query string
    size_t query =
        path.find('?');

    if (query != std::string::npos)
    {
        path =
            path.substr(
                0,
                query
            );
    }

    // Supprimer fragment
    size_t fragment =
        path.find('#');

    if (fragment != std::string::npos)
    {
        path =
            path.substr(
                0,
                fragment
            );
    }

    // Supprimer les "/" au début
    while (
        !path.empty() &&
        path.front() == '/'
    )
    {
        path.erase(path.begin());
    }

    // Supprimer les "/" à la fin
    while (
        !path.empty() &&
        path.back() == '/'
    )
    {
        path.pop_back();
    }

    return path;
}

// ==================================================
// LOWERCASE
// ==================================================

std::string to_lower(
    std::string value
)
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

// ==================================================
// SPLIT PATH
// ==================================================

std::vector<std::string> split_path(
    const std::string& path
)
{
    std::vector<std::string> parts;

    size_t start = 0;

    while (start < path.size())
    {
        size_t end =
            path.find(
                '/',
                start
            );

        if (end == std::string::npos)
        {
            parts.push_back(
                path.substr(start)
            );

            break;
        }

        if (end > start)
        {
            parts.push_back(
                path.substr(
                    start,
                    end - start
                )
            );
        }

        start = end + 1;
    }

    return parts;
}

// ==================================================
// SHOULD KEEP
// ==================================================

bool should_keep(
    const std::string& url
)
{
    std::string path =
        get_path_from_url(url);

    if (path.empty())
        return false;

    const std::string prefix =
        "catalogue/";

    if (
        path.size() < prefix.size() ||
        path.compare(
            0,
            prefix.size(),
            prefix
        ) != 0
    )
    {
        return false;
    }

    std::vector<std::string> parts =
        split_path(path);

    // /catalogue/anime
    if (parts.size() == 2)
        return true;

    // /catalogue/anime/...
    for (
        size_t i = 2;
        i < parts.size();
        ++i
    )
    {
        std::string part =
            to_lower(parts[i]);

        if (
            part.find("scan") !=
            std::string::npos
        )
        {
            return true;
        }
    }

    return false;
}

// ==================================================
// SCAN COLECTOR
// ==================================================

bool scan_colector()
{
    fs::path base_dir =
        fs::current_path();

    fs::path input_file =
        base_dir /
        "data" /
        "temp" /
        "all_link.json";

    fs::path output_file =
        base_dir /
        "data" /
        "temp" /
        "scan_colecte.json";

    // ----------------------------------------------
    // Vérifier fichier
    // ----------------------------------------------

    if (!fs::exists(input_file))
    {
        std::cerr
            << "ERROR: File not found: "
            << input_file
            << "\n";

        return false;
    }

    // ----------------------------------------------
    // Lire JSON
    // ----------------------------------------------

    std::ifstream file(
        input_file
    );

    if (!file)
    {
        std::cerr
            << "ERROR: Cannot open: "
            << input_file
            << "\n";

        return false;
    }

    json data;

    try
    {
        file >> data;
    }
    catch (const json::parse_error& e)
    {
        std::cerr
            << "ERROR: Invalid JSON: "
            << e.what()
            << "\n";

        return false;
    }

    file.close();

    // ----------------------------------------------
    // Vérifier "link"
    // ----------------------------------------------

    if (
        !data.contains("link") ||
        !data["link"].is_array()
    )
    {
        std::cerr
            << "ERROR: 'link' must contain a list.\n";

        return false;
    }

    const json& links =
        data["link"];

    // ----------------------------------------------
    // Filtrage + suppression doublons
    // ----------------------------------------------

    std::vector<std::string> filtered_links;

    std::unordered_set<std::string> already_seen;

    for (const auto& link : links)
    {
        if (!link.is_string())
            continue;

        std::string url =
            link.get<std::string>();

        if (!should_keep(url))
            continue;

        if (
            already_seen.insert(url).second
        )
        {
            filtered_links.push_back(url);
        }
    }

    // ----------------------------------------------
    // Créer JSON
    // ----------------------------------------------

    json output_data;

    output_data["link"] =
        filtered_links;

    // ----------------------------------------------
    // Créer dossier
    // ----------------------------------------------

    fs::create_directories(
        output_file.parent_path()
    );

    // ----------------------------------------------
    // Écrire fichier
    // ----------------------------------------------

    std::ofstream output(
        output_file
    );

    if (!output)
    {
        std::cerr
            << "ERROR: Cannot write: "
            << output_file
            << "\n";

        return false;
    }

    output
        << output_data.dump(4);

    output.close();

    // ----------------------------------------------
    // Résultat
    // ----------------------------------------------

    std::cout
        << "-------------------------------------\n";

    std::cout
        << "Before : "
        << links.size()
        << " links\n";

    std::cout
        << "After  : "
        << filtered_links.size()
        << " links\n";

    std::cout
        << "-------------------------------------\n";

    return true;
}

// ==================================================
// GET CATALOGUE PARTS
// ==================================================

std::vector<std::string> get_catalogue_parts(
    const std::string& url
)
{
    std::string path =
        get_path_from_url(url);

    if (path.empty())
        return {};

    std::vector<std::string> parts =
        split_path(path);

    size_t catalogue_index =
        std::string::npos;

    for (size_t i = 0; i < parts.size(); ++i)
    {
        if (parts[i] == "catalogue")
        {
            catalogue_index = i;
            break;
        }
    }

    if (
        catalogue_index ==
        std::string::npos
    )
    {
        return {};
    }

    std::vector<std::string> result;

    for (
        size_t i = catalogue_index + 1;
        i < parts.size();
        ++i
    )
    {
        result.push_back(
            parts[i]
        );
    }

    return result;
}

// ==================================================
// BUILD HERITAGE
// ==================================================

json build_heritage(
    const std::vector<std::string>& urls
)
{
    // Les parents
    std::unordered_set<std::string> parents;

    // Les enfants
    std::vector<
        std::pair<std::string, std::string>
    > children;

    // ----------------------------------------------
    // Identifier parents / enfants
    // ----------------------------------------------

    for (const auto& url : urls)
    {
        std::vector<std::string> parts =
            get_catalogue_parts(url);

        if (parts.empty())
            continue;

        // Parent :
        //
        // /catalogue/one-piece/
        //
        if (parts.size() == 1)
        {
            parents.insert(
                parts[0]
            );
        }

        // Child :
        //
        // /catalogue/one-piece/scan/vf/
        //
        else if (parts.size() >= 2)
        {
            const std::string& anime =
                parts[0];

            children.emplace_back(
                anime,
                url
            );
        }
    }

    // ----------------------------------------------
    // Construire heritage
    // ----------------------------------------------

    json heritage =
        json::object();

    for (const auto& child : children)
    {
        const std::string& anime =
            child.first;

        const std::string& child_url =
            child.second;

        // Le parent doit exister
        if (
            parents.find(anime) ==
            parents.end()
        )
        {
            continue;
        }

        if (!heritage.contains(anime))
        {
            heritage[anime] =
                json::array();
        }

        heritage[anime].push_back(
            child_url
        );
    }

    json result;

    result["scan"] =
        heritage;

    return result;
}

// ==================================================
// GENERATE HERITAGE.JSON
// ==================================================

bool heritage()
{
    fs::path base_dir =
        fs::current_path();

    fs::path source =
        base_dir /
        "data" /
        "temp" /
        "scan_colecte.json";

    fs::path output =
        base_dir /
        "data" /
        "heritage.json";

    // ----------------------------------------------
    // Vérifier source
    // ----------------------------------------------

    if (!fs::exists(source))
    {
        std::cerr
            << "ERROR: source file not found: "
            << source
            << "\n";

        return false;
    }

    // ----------------------------------------------
    // Lire JSON
    // ----------------------------------------------

    std::ifstream file(source);

    if (!file)
    {
        std::cerr
            << "ERROR: Cannot open source file.\n";

        return false;
    }

    json data;

    try
    {
        file >> data;
    }
    catch (const json::parse_error& e)
    {
        std::cerr
            << "ERROR: Invalid JSON: "
            << e.what()
            << "\n";

        return false;
    }

    file.close();

    // ----------------------------------------------
    // Vérifier objet JSON
    // ----------------------------------------------

    if (!data.is_object())
    {
        std::cerr
            << "ERROR: The JSON must be an object.\n";

        return false;
    }

    // ----------------------------------------------
    // Vérifier "link"
    // ----------------------------------------------

    if (
        !data.contains("link") ||
        !data["link"].is_array()
    )
    {
        std::cerr
            << "ERROR: The 'link' property "
               "must be a list.\n";

        return false;
    }

    const json& json_urls =
        data["link"];

    std::vector<std::string> urls;

    for (const auto& item : json_urls)
    {
        if (item.is_string())
        {
            urls.push_back(
                item.get<std::string>()
            );
        }
    }

    std::cout
        << "Links found: "
        << urls.size()
        << "\n";

    // ----------------------------------------------
    // Construire hiérarchie
    // ----------------------------------------------

    json result =
        build_heritage(urls);

    // ----------------------------------------------
    // Créer dossier
    // ----------------------------------------------

    fs::create_directories(
        output.parent_path()
    );

    // ----------------------------------------------
    // Écrire heritage.json
    // ----------------------------------------------

    std::ofstream output_file(
        output
    );

    if (!output_file)
    {
        std::cerr
            << "ERROR: Cannot write output file: "
            << output
            << "\n";

        return false;
    }

    output_file
        << result.dump(4);

    output_file.close();

    // ----------------------------------------------
    // Compter les anime
    // ----------------------------------------------

    size_t anime_count = 0;

    if (
        result.contains("scan") &&
        result["scan"].is_object()
    )
    {
        anime_count =
            result["scan"].size();
    }

    std::cout << "\n";

    std::cout
        << "Anime with children: "
        << anime_count
        << "\n";

    std::cout << "\n";

    std::cout
        << YELLOW
        << "Done!"
        << RESET
        << "\n";

    return true;
}

// ==================================================
// MAIN
// ==================================================

int main()
{
    curl_global_init(
        CURL_GLOBAL_DEFAULT
    );

    // ==============================================
    // LOAD DOMAIN
    // ==============================================

    if (!load_domain())
    {
        curl_global_cleanup();
        return 1;
    }

    // ==============================================
    // ÉTAPE 1
    //
    // sitemap.xml
    //      ↓
    // all_link.json
    // ==============================================

    std::cout
        << "\n"
        << "=====================================\n"
        << " STEP 1 - SITEMAP\n"
        << "=====================================\n\n";

    bool success =
        main_json();

    if (!success)
    {
        curl_global_cleanup();

        return 1;
    }

    // ==============================================
    // ÉTAPE 2
    //
    // all_link.json
    //      ↓
    // scan_colecte.json
    // ==============================================

    std::cout
        << "\n"
        << "=====================================\n"
        << " STEP 2 - SCAN COLECTOR\n"
        << "=====================================\n\n";

    success =
        scan_colector();

    if (!success)
    {
        curl_global_cleanup();

        return 1;
    }

    // ==============================================
    // ÉTAPE 3
    //
    // scan_colecte.json
    //      ↓
    // heritage.json
    // ==============================================

    std::cout
        << "\n"
        << "=====================================\n"
        << " STEP 3 - HERITAGE\n"
        << "=====================================\n\n";

    success =
        heritage();

    // ==============================================
    // FIN
    // ==============================================

    curl_global_cleanup();

    if (success)
    {
        std::cout
            << "\n"
            << GREEN
            << "=====================================\n"
            << " ALL DONE\n"
            << "=====================================\n"
            << RESET;
    }

    return success ? 0 : 1;
}