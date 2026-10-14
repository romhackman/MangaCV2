#include <cstdio>
#include <iostream>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <string>
#include <cctype>
#include <hpdf.h>

namespace fs = std::filesystem;

// ---------------------------------------------------------
// Vérifie si le fichier est un JPG / JPEG
// ---------------------------------------------------------
bool isJpeg(const fs::path& path)
{
    std::string ext = path.extension().string();

    std::transform(
        ext.begin(),
        ext.end(),
        ext.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        }
    );

    return ext == ".jpg" || ext == ".jpeg";
}

// ---------------------------------------------------------
// Trie les fichiers par numéro
// Exemple : 1.jpg, 2.jpg, 10.jpg
// ---------------------------------------------------------
bool comparePages(const fs::path& a, const fs::path& b)
{
    try
    {
        return std::stoi(a.stem().string())
             < std::stoi(b.stem().string());
    }
    catch (...)
    {
        return a.filename().string() < b.filename().string();
    }
}

// ---------------------------------------------------------
// Trie les dossiers de chapitres par numéro
// Exemple : Chapter 1, Chapter 2, Chapter 10
// ---------------------------------------------------------
bool compareChapters(const fs::path& a, const fs::path& b)
{
    try
    {
        std::string nameA = a.filename().string();
        std::string nameB = b.filename().string();

        // Cherche le premier nombre dans le nom
        size_t posA = 0;
        size_t posB = 0;

        while (posA < nameA.size() &&
               !std::isdigit(static_cast<unsigned char>(nameA[posA])))
        {
            ++posA;
        }

        while (posB < nameB.size() &&
               !std::isdigit(static_cast<unsigned char>(nameB[posB])))
        {
            ++posB;
        }

        if (posA < nameA.size() && posB < nameB.size())
        {
            int numA = std::stoi(nameA.substr(posA));
            int numB = std::stoi(nameB.substr(posB));

            return numA < numB;
        }
    }
    catch (...)
    {
    }

    return a.filename().string() < b.filename().string();
}

// ---------------------------------------------------------
// Crée un PDF à partir d'un dossier contenant les JPG
// ---------------------------------------------------------
bool createPdf(
    const fs::path& chapterFolder,
    const fs::path& outputPdf
)
{
    std::vector<fs::path> pages;

    // -----------------------------------------------------
    // Chercher les JPG
    // -----------------------------------------------------
    try
    {
        for (const auto& entry :
             fs::directory_iterator(chapterFolder))
        {
            if (!entry.is_regular_file())
                continue;

            if (isJpeg(entry.path()))
                pages.push_back(entry.path());
        }
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "Erreur lors de la lecture du dossier : "
            << chapterFolder
            << "\n"
            << e.what()
            << '\n';

        return false;
    }

    if (pages.empty())
    {
        std::cerr
            << "Aucune image JPG dans : "
            << chapterFolder
            << '\n';

        return false;
    }

    // -----------------------------------------------------
    // Trier les pages
    // -----------------------------------------------------
    std::sort(
        pages.begin(),
        pages.end(),
        comparePages
    );

    std::cout
        << "    Images trouvees : "
        << pages.size()
        << '\n';

    // -----------------------------------------------------
    // Créer le PDF
    // -----------------------------------------------------
    HPDF_Doc pdf = HPDF_New(nullptr, nullptr);

    if (!pdf)
    {
        std::cerr
            << "    Impossible de creer le PDF.\n";

        return false;
    }

    bool success = true;

    try
    {
        for (size_t i = 0; i < pages.size(); ++i)
        {
            const fs::path& imagePath = pages[i];

            std::cout
                << "    ["
                << (i + 1)
                << "/"
                << pages.size()
                << "] "
                << imagePath.filename().string()
                << '\n';

            // -------------------------------------------------
            // Charger l'image JPG
            // -------------------------------------------------
            HPDF_Image image =
                HPDF_LoadJpegImageFromFile(
                    pdf,
                    imagePath.string().c_str()
                );

            if (!image)
            {
                std::cerr
                    << "    Erreur avec l'image : "
                    << imagePath
                    << '\n';

                success = false;
                continue;
            }

            // -------------------------------------------------
            // Dimensions de l'image
            // -------------------------------------------------
            float width =
                static_cast<float>(
                    HPDF_Image_GetWidth(image)
                );

            float height =
                static_cast<float>(
                    HPDF_Image_GetHeight(image)
                );

            if (width <= 0 || height <= 0)
            {
                std::cerr
                    << "    Dimensions invalides : "
                    << imagePath
                    << '\n';

                success = false;
                continue;
            }

            // -------------------------------------------------
            // Taille maximale du PDF
            // -------------------------------------------------
            constexpr float MAX_WIDTH = 1000.0f;
            constexpr float MAX_HEIGHT = 1400.0f;

            float scale = std::min(
                MAX_WIDTH / width,
                MAX_HEIGHT / height
            );

            // Ne pas agrandir une petite image
            scale = std::min(scale, 1.0f);

            float pageWidth = width * scale;
            float pageHeight = height * scale;

            // -------------------------------------------------
            // Créer la page
            // -------------------------------------------------
            HPDF_Page page = HPDF_AddPage(pdf);

            if (!page)
            {
                std::cerr
                    << "    Impossible de creer une page.\n";

                success = false;
                continue;
            }

            HPDF_Page_SetWidth(
                page,
                pageWidth
            );

            HPDF_Page_SetHeight(
                page,
                pageHeight
            );

            // -------------------------------------------------
            // Dessiner l'image
            // -------------------------------------------------
            HPDF_Page_DrawImage(
                page,
                image,
                0,
                0,
                pageWidth,
                pageHeight
            );
        }

        // -----------------------------------------------------
        // Sauvegarder le PDF
        // -----------------------------------------------------
        if (success)
        {
            HPDF_SaveToFile(
                pdf,
                outputPdf.string().c_str()
            );
        }
    }
    catch (...)
    {
        std::cerr
            << "    Erreur pendant la creation du PDF.\n";

        success = false;
    }

    HPDF_Free(pdf);

    if (!success)
    {
        // Supprimer éventuellement un PDF incomplet
        try
        {
            if (fs::exists(outputPdf))
                fs::remove(outputPdf);
        }
        catch (...)
        {
        }

        return false;
    }

    std::cout
        << "    PDF cree : "
        << outputPdf
        << "\n";

    return true;
}

// ---------------------------------------------------------
// MAIN
// ---------------------------------------------------------
int main()
{
    setvbuf(stdout, nullptr, _IONBF, 0); // progression en direct vers l'app

    // ---------------------------------------------------------
    // Trouver le dossier où se trouve le programme
    // ---------------------------------------------------------
    fs::path programFolder;

    try
    {
        programFolder =
            fs::current_path();
    }
    catch (...)
    {
        std::cerr
            << "Impossible de trouver le dossier du programme.\n";

        return 1;
    }

    // ---------------------------------------------------------
    // Dossier TEMP
    //
    // Structure attendue :
    //
    // programme.exe
    // temp/
    //     Anime 1/
    //         Chapter 1/
    //             001.jpg
    //             002.jpg
    //         Chapter 2/
    //             001.jpg
    //
    //     Anime 2/
    //         Chapter 1/
    //             001.jpg
    //             002.jpg
    // ---------------------------------------------------------
    fs::path tempFolder =
        programFolder / "temp";

    // ---------------------------------------------------------
    // Dossier SCAN
    // ---------------------------------------------------------
    fs::path scanFolder =
        programFolder / "scan";

    // ---------------------------------------------------------
    // Vérifier TEMP
    // ---------------------------------------------------------
    if (!fs::exists(tempFolder))
    {
        std::cerr
            << "Le dossier 'temp' n'existe pas :\n"
            << tempFolder
            << '\n';

        return 1;
    }

    if (!fs::is_directory(tempFolder))
    {
        std::cerr
            << "'temp' n'est pas un dossier.\n";

        return 1;
    }

    std::cout
        << "========================================\n"
        << "        MANGA -> PDF\n"
        << "========================================\n\n";

    std::cout
        << "Dossier TEMP : "
        << tempFolder
        << "\n\n";

    // ---------------------------------------------------------
    // Créer SCAN
    // ---------------------------------------------------------
    try
    {
        fs::create_directories(scanFolder);
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "Impossible de creer le dossier scan.\n"
            << e.what()
            << '\n';

        return 1;
    }

    std::cout
        << "Dossier SCAN : "
        << scanFolder
        << "\n\n";

    // ---------------------------------------------------------
    // Chercher tous les ANIMES dans TEMP
    // ---------------------------------------------------------
    std::vector<fs::path> animeFolders;

    try
    {
        for (const auto& entry :
             fs::directory_iterator(tempFolder))
        {
            if (entry.is_directory())
                animeFolders.push_back(entry.path());
        }
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "Erreur lors de la lecture de temp.\n"
            << e.what()
            << '\n';

        return 1;
    }

    if (animeFolders.empty())
    {
        std::cerr
            << "Aucun dossier d'anime trouve dans temp.\n";

        return 1;
    }

    // Trier les anime par nom
    std::sort(
        animeFolders.begin(),
        animeFolders.end(),
        [](const fs::path& a, const fs::path& b)
        {
            return a.filename().string()
                 < b.filename().string();
        }
    );

    size_t totalAnime = 0;
    size_t totalChapters = 0;
    size_t totalPdf = 0;

    // ---------------------------------------------------------
    // Parcourir les ANIMES
    // ---------------------------------------------------------
    for (const fs::path& animeFolder : animeFolders)
    {
        ++totalAnime;

        std::string animeName =
            animeFolder.filename().string();

        std::cout
            << "\n========================================\n"
            << "ANIME : "
            << animeName
            << "\n"
            << "========================================\n";

        // -----------------------------------------------------
        // Créer scan/NomAnime
        // -----------------------------------------------------
        fs::path animeScanFolder =
            scanFolder / animeName;

        try
        {
            fs::create_directories(animeScanFolder);
        }
        catch (const std::exception& e)
        {
            std::cerr
                << "Impossible de creer : "
                << animeScanFolder
                << '\n'
                << e.what()
                << '\n';

            continue;
        }

        // -----------------------------------------------------
        // Chercher les chapitres
        // -----------------------------------------------------
        std::vector<fs::path> chapters;

        try
        {
            for (const auto& entry :
                 fs::directory_iterator(animeFolder))
            {
                if (entry.is_directory())
                    chapters.push_back(entry.path());
            }
        }
        catch (const std::exception& e)
        {
            std::cerr
                << "Erreur en lisant : "
                << animeFolder
                << '\n'
                << e.what()
                << '\n';

            continue;
        }

        if (chapters.empty())
        {
            std::cerr
                << "Aucun chapitre trouve pour : "
                << animeName
                << '\n';

            continue;
        }

        // -----------------------------------------------------
        // Trier les chapitres
        // -----------------------------------------------------
        std::sort(
            chapters.begin(),
            chapters.end(),
            compareChapters
        );

        // -----------------------------------------------------
        // Parcourir les CHAPITRES
        // -----------------------------------------------------
        for (const fs::path& chapterFolder : chapters)
        {
            ++totalChapters;

            std::string chapterName =
                chapterFolder.filename().string();

            std::cout
                << "\n  --------------------------------------\n"
                << "  CHAPITRE : "
                << chapterName
                << "\n"
                << "  --------------------------------------\n";

            // -------------------------------------------------
            // Nom du PDF
            // Exemple :
            //
            // scan/
            //   My Dress-up Darling/
            //       Chapter 1.pdf
            //       Chapter 2.pdf
            // -------------------------------------------------
            fs::path outputPdf =
                animeScanFolder /
                (chapterName + ".pdf");

            // -------------------------------------------------
            // Créer le PDF
            // -------------------------------------------------
            if (createPdf(
                    chapterFolder,
                    outputPdf))
            {
                ++totalPdf;
            }
            else
            {
                std::cerr
                    << "  ECHEC : "
                    << chapterName
                    << '\n';
            }
        }
    }

    // ---------------------------------------------------------
    // Résumé
    // ---------------------------------------------------------
    std::cout
        << "\n\n========================================\n"
        << "             TERMINE\n"
        << "========================================\n"
        << "Animes       : "
        << totalAnime
        << '\n'
        << "Chapitres    : "
        << totalChapters
        << '\n'
        << "PDF crees    : "
        << totalPdf
        << '\n'
        << "\nDossier scan : "
        << scanFolder
        << "\n";

    // ---------------------------------------------------------
    // Supprimer TEMP
    // ---------------------------------------------------------
    std::cout
        << "\nSuppression du dossier temp...\n";

    try
    {
        fs::remove_all(tempFolder);

        std::cout
            << "Dossier temp supprime.\n";
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "Impossible de supprimer temp :\n"
            << e.what()
            << '\n';

        return 1;
    }

    std::cout
        << "\n========================================\n"
        << "       TOUT EST TERMINE !\n"
        << "========================================\n";

    return 0;
}