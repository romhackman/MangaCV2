#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <set>
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <cstdlib>

#include <nlohmann/json.hpp>
#include <curl/curl.h>

using json = nlohmann::json;
namespace fs = std::filesystem;

// ============================================================
// Callback CURL pour écrire directement dans un fichier
// ============================================================

size_t writeFileCallback(
    void* ptr,
    size_t size,
    size_t nmemb,
    void* userdata)
{
    std::ofstream* file =
        static_cast<std::ofstream*>(userdata);

    file->write(
        static_cast<char*>(ptr),
        size * nmemb
    );

    return size * nmemb;
}

// ============================================================
// Télécharger une image
// ============================================================

bool downloadImage(
    const std::string& url,
    const fs::path& outputPath)
{
    CURL* curl = curl_easy_init();

    if (!curl)
    {
        std::cout
            << "[ERREUR] Impossible d'initialiser CURL.\n";

        return false;
    }

    std::ofstream outputFile(
        outputPath,
        std::ios::binary
    );

    if (!outputFile.is_open())
    {
        std::cout
            << "[ERREUR] Impossible de creer : "
            << outputPath.string()
            << "\n";

        curl_easy_cleanup(curl);

        return false;
    }

    curl_easy_setopt(
        curl,
        CURLOPT_URL,
        url.c_str()
    );

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEFUNCTION,
        writeFileCallback
    );

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEDATA,
        &outputFile
    );

    // Suivre les redirections
    curl_easy_setopt(
        curl,
        CURLOPT_FOLLOWLOCATION,
        1L
    );

    // Timeout de 30 secondes
    curl_easy_setopt(
        curl,
        CURLOPT_TIMEOUT,
        30L
    );

    // User-Agent
    curl_easy_setopt(
        curl,
        CURLOPT_USERAGENT,
        "Mozilla/5.0"
    );

    CURLcode result =
        curl_easy_perform(curl);

    long httpCode = 0;

    curl_easy_getinfo(
        curl,
        CURLINFO_RESPONSE_CODE,
        &httpCode
    );

    outputFile.close();

    curl_easy_cleanup(curl);

    if (result != CURLE_OK)
    {
        std::cout
            << "[ERREUR] "
            << curl_easy_strerror(result)
            << "\n";

        if (fs::exists(outputPath))
        {
            fs::remove(outputPath);
        }

        return false;
    }

    if (httpCode < 200 || httpCode >= 300)
    {
        std::cout
            << "[ERREUR] HTTP "
            << httpCode
            << "\n";

        if (fs::exists(outputPath))
        {
            fs::remove(outputPath);
        }

        return false;
    }

    return true;
}

// ============================================================
// Nettoyer un nom de dossier Windows
// ============================================================

std::string sanitizeFolderName(
    std::string name)
{
    const std::string invalidChars =
        "<>:\"/\\|?*";

    for (char& c : name)
    {
        if (invalidChars.find(c) !=
            std::string::npos)
        {
            c = '_';
        }
    }

    // Supprimer les espaces et points en fin
    while (!name.empty() &&
           (name.back() == ' ' ||
            name.back() == '.'))
    {
        name.pop_back();
    }

    if (name.empty())
    {
        name = "Anime";
    }

    return name;
}

// ============================================================
// Lancer pdf.exe
// ============================================================

bool launchPdfExe(
    const char* programPath)
{
    try
    {
        // Dossier contenant download.exe
        fs::path exePath =
            fs::absolute(
                fs::path(programPath)
            );

        fs::path exeDirectory =
            exePath.parent_path();

        // pdf.exe doit être dans le même dossier
        // que download.exe
        fs::path pdfPath =
            exeDirectory / "pdf.exe";

        std::cout << "\n";
        std::cout
            << "========================================\n";
        std::cout
            << "LANCEMENT DE PDF.EXE\n";
        std::cout
            << "========================================\n";

        std::cout
            << "Programme : "
            << pdfPath.string()
            << "\n";

        // Vérifier que pdf.exe existe
        if (!fs::exists(pdfPath))
        {
            std::cout
                << "[ERREUR] pdf.exe introuvable : "
                << pdfPath.string()
                << "\n";

            return false;
        }

        // Mettre le chemin entre guillemets
        // pour gérer les espaces dans le chemin
        std::string command =
            "\"" +
            pdfPath.string() +
            "\"";

        std::cout
            << "[INFO] Execution de pdf.exe...\n";

        int result =
            std::system(command.c_str());

        if (result != 0)
        {
            std::cout
                << "[ERREUR] pdf.exe a retourne le code : "
                << result
                << "\n";

            return false;
        }

        std::cout
            << "[INFO] pdf.exe termine avec succes.\n";

        return true;
    }
    catch (const std::exception& e)
    {
        std::cout
            << "[ERREUR] Impossible de lancer pdf.exe : "
            << e.what()
            << "\n";

        return false;
    }
}

// ============================================================
// Programme principal
// ============================================================

int main(
    int argc,
    char* argv[])
{
    // ========================================================
    // Arguments
    // ========================================================

    if (argc < 2)
    {
        std::cout << "Utilisation :\n";

        std::cout
            << "  download.exe -all\n";

        std::cout
            << "  download.exe -chapter \"[1,2,3]\"\n";

        return 1;
    }

    std::string option = argv[1];

    // ========================================================
    // Lire result.json
    // ========================================================

    std::ifstream file(
        "data/result.json"
    );

    if (!file.is_open())
    {
        std::cout
            << "[ERREUR] Impossible d'ouvrir "
               "data/result.json\n";

        return 1;
    }

    json data;

    try
    {
        file >> data;
    }
    catch (const std::exception& e)
    {
        std::cout
            << "[ERREUR] JSON invalide : "
            << e.what()
            << "\n";

        return 1;
    }

    file.close();

    // ========================================================
    // Vérifier results
    // ========================================================

    if (!data.contains("results") ||
        !data["results"].is_array())
    {
        std::cout
            << "[ERREUR] 'results' est absent "
               "ou invalide.\n";

        return 1;
    }

    // ========================================================
    // Chapitres sélectionnés
    // ========================================================

    std::set<int> selectedChapters;

    if (option == "-chapter")
    {
        if (argc < 3)
        {
            std::cout
                << "[ERREUR] Liste de chapitres manquante.\n";

            return 1;
        }

        std::string input;

        for (int i = 2; i < argc; i++)
        {
            if (!input.empty())
            {
                input += " ";
            }

            input += argv[i];
        }

        // Vérifier [1,2,3]
        if (input.size() < 2 ||
            input.front() != '[' ||
            input.back() != ']')
        {
            std::cout
                << "[ERREUR] Format invalide : "
                << input
                << "\n";

            std::cout
                << "Exemple : [1,2,3]\n";

            return 1;
        }

        // Retirer [ ]
        input =
            input.substr(
                1,
                input.size() - 2
            );

        // Retirer les espaces
        input.erase(
            std::remove(
                input.begin(),
                input.end(),
                ' '
            ),
            input.end()
        );

        std::stringstream ss(input);

        std::string value;

        while (std::getline(
            ss,
            value,
            ','))
        {
            if (value.empty())
            {
                continue;
            }

            try
            {
                int chapter =
                    std::stoi(value);

                if (chapter <= 0)
                {
                    std::cout
                        << "[ERREUR] Chapitre invalide : "
                        << value
                        << "\n";

                    return 1;
                }

                selectedChapters.insert(
                    chapter
                );
            }
            catch (...)
            {
                std::cout
                    << "[ERREUR] Chapitre invalide : "
                    << value
                    << "\n";

                return 1;
            }
        }

        if (selectedChapters.empty())
        {
            std::cout
                << "[ERREUR] Aucun chapitre selectionne.\n";

            return 1;
        }
    }
    else if (option != "-all")
    {
        std::cout
            << "[ERREUR] Option inconnue : "
            << option
            << "\n";

        return 1;
    }

    // ========================================================
    // Initialiser CURL
    // ========================================================

    CURLcode curlInit =
        curl_global_init(
            CURL_GLOBAL_DEFAULT
        );

    if (curlInit != CURLE_OK)
    {
        std::cout
            << "[ERREUR] Impossible d'initialiser CURL.\n";

        return 1;
    }

    // ========================================================
    // Statistiques
    // ========================================================

    int totalChapters = 0;
    int totalImages = 0;
    int downloadedImages = 0;
    int failedImages = 0;

    // ========================================================
    // Parcourir les résultats
    // ========================================================

    for (const auto& result :
         data["results"])
    {
        // ----------------------------------------------------
        // Titre anime
        // ----------------------------------------------------

        if (!result.contains("titre") ||
            !result["titre"].is_string())
        {
            std::cout
                << "[ERREUR] 'titre' manquant.\n";

            continue;
        }

        std::string animeTitle =
            result["titre"].get<std::string>();

        animeTitle =
            sanitizeFolderName(
                animeTitle
            );

        // ----------------------------------------------------
        // Créer temp/Anime
        // ----------------------------------------------------

        fs::path animeFolder =
            fs::path("temp") /
            animeTitle;

        try
        {
            fs::create_directories(
                animeFolder
            );
        }
        catch (const std::exception& e)
        {
            std::cout
                << "[ERREUR] Impossible de creer : "
                << animeFolder.string()
                << "\n";

            continue;
        }

        std::cout << "\n";

        std::cout
            << "========================================\n";

        std::cout
            << "ANIME : "
            << animeTitle
            << "\n";

        std::cout
            << "Dossier : "
            << animeFolder.string()
            << "\n";

        std::cout
            << "========================================\n";

        // ----------------------------------------------------
        // Vérifier chapters
        // ----------------------------------------------------

        if (!result.contains("chapters") ||
            !result["chapters"].is_object())
        {
            std::cout
                << "[ERREUR] Aucun chapitre.\n";

            continue;
        }

        const auto& chapters =
            result["chapters"];

        // ====================================================
        // Fonction locale pour télécharger un chapitre
        // ====================================================

        auto processChapter =
            [&](int chapter)
        {
            std::string chapterKey =
                std::to_string(chapter);

            // ------------------------------------------------
            // Chapitre inexistant
            // ------------------------------------------------

            if (!chapters.contains(chapterKey))
            {
                std::cout
                    << "\n[INFO] Chapitre "
                    << chapter
                    << " introuvable.\n";

                return;
            }

            const auto& chapterData =
                chapters[chapterKey];

            // ------------------------------------------------
            // Images absentes
            // ------------------------------------------------

            if (!chapterData.contains("images") ||
                !chapterData["images"].is_array())
            {
                std::cout
                    << "\n[ERREUR] Chapitre "
                    << chapter
                    << " : aucune image.\n";

                return;
            }

            const auto& images =
                chapterData["images"];

            // ------------------------------------------------
            // Créer dossier Chapter X
            // ------------------------------------------------

            fs::path chapterFolder =
                animeFolder /
                ("Chapter " +
                 std::to_string(chapter));

            try
            {
                fs::create_directories(
                    chapterFolder
                );
            }
            catch (const std::exception& e)
            {
                std::cout
                    << "[ERREUR] Impossible de creer "
                    << chapterFolder.string()
                    << "\n";

                return;
            }

            std::cout << "\n";

            std::cout
                << "----------------------------------------\n";

            std::cout
                << "Chapitre "
                << chapter
                << "\n";

            std::cout
                << "Images : "
                << images.size()
                << "\n";

            std::cout
                << "Dossier : "
                << chapterFolder.string()
                << "\n";

            std::cout
                << "----------------------------------------\n";

            // ------------------------------------------------
            // Télécharger les images
            // ------------------------------------------------

            int imageNumber = 0;

            for (const auto& image :
                 images)
            {
                if (!image.is_string())
                {
                    continue;
                }

                std::string url =
                    image.get<std::string>();

                imageNumber++;

                totalImages++;

                // --------------------------------------------
                // Nom : 001.jpg
                // --------------------------------------------

                std::ostringstream filename;

                filename
                    << std::setfill('0')
                    << std::setw(3)
                    << imageNumber
                    << ".jpg";

                fs::path outputPath =
                    chapterFolder /
                    filename.str();

                std::cout
                    << "["
                    << imageNumber
                    << "/"
                    << images.size()
                    << "] ";

                // --------------------------------------------
                // Si déjà téléchargé
                // --------------------------------------------

                if (fs::exists(outputPath) &&
                    fs::file_size(outputPath) > 0)
                {
                    std::cout
                        << "DEJA PRESENT\n";

                    downloadedImages++;

                    continue;
                }

                // --------------------------------------------
                // Télécharger
                // --------------------------------------------

                std::cout
                    << "Telechargement... ";

                bool success =
                    downloadImage(
                        url,
                        outputPath
                    );

                if (success)
                {
                    std::cout
                        << "OK\n";

                    downloadedImages++;
                }
                else
                {
                    std::cout
                        << "ECHEC\n";

                    failedImages++;
                }
            }

            totalChapters++;
        };

        // ====================================================
        // MODE -all
        // ====================================================

        if (option == "-all")
        {
            for (const auto& [
                chapterNumber,
                chapterData
            ] : chapters.items())
            {
                try
                {
                    int chapter =
                        std::stoi(
                            chapterNumber
                        );

                    processChapter(
                        chapter
                    );
                }
                catch (...)
                {
                    std::cout
                        << "[ERREUR] Numero de chapitre "
                        << "invalide : "
                        << chapterNumber
                        << "\n";
                }
            }
        }

        // ====================================================
        // MODE -chapter
        // ====================================================

        else
        {
            for (int chapter :
                 selectedChapters)
            {
                processChapter(
                    chapter
                );
            }
        }
    }

    // ========================================================
    // Nettoyage CURL
    // ========================================================

    curl_global_cleanup();

    // ========================================================
    // Résumé
    // ========================================================

    std::cout << "\n\n";

    std::cout
        << "========================================\n";

    std::cout
        << "TELECHARGEMENT TERMINE\n";

    std::cout
        << "========================================\n";

    std::cout
        << "Chapitres traites : "
        << totalChapters
        << "\n";

    std::cout
        << "Images trouvees   : "
        << totalImages
        << "\n";

    std::cout
        << "Images OK         : "
        << downloadedImages
        << "\n";

    std::cout
        << "Images en erreur  : "
        << failedImages
        << "\n";

    std::cout
        << "========================================\n";

    // ========================================================
    // Lancer pdf.exe
    // ========================================================

    launchPdfExe(argv[0]);

    // ========================================================
    // Fin
    // ========================================================

    std::cout << "\n";

    return 0;
}