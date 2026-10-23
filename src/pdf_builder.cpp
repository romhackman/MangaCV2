#include "pdf_builder.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <vector>

#include <hpdf.h>

#include "platform.h"
#include "utils.h"

namespace fs = std::filesystem;

namespace
{
    bool isJpegName(const fs::path& p)
    {
        std::string ext = utils::toLower(p.extension().string());
        return ext == ".jpg" || ext == ".jpeg";
    }

    std::vector<unsigned char> readFile(const fs::path& p)
    {
        std::ifstream f(p, std::ios::binary);
        return std::vector<unsigned char>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }

    // Vrai format d'apres les premiers octets
    enum class Kind { Jpeg, Png, Other };
    Kind detectKind(const std::vector<unsigned char>& d)
    {
        if (d.size() >= 3 && d[0] == 0xFF && d[1] == 0xD8 && d[2] == 0xFF) return Kind::Jpeg;
        if (d.size() >= 8 && d[0] == 0x89 && d[1] == 'P' && d[2] == 'N' && d[3] == 'G') return Kind::Png;
        return Kind::Other;                 // webp, page d'erreur, fichier corrompu...
    }
}

namespace pdfbuilder
{
    bool createPdf(TaskContext& ctx, const fs::path& chapterFolder, const fs::path& outputPdf)
    {
        std::vector<fs::path> pages;
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(chapterFolder, ec))
        {
            std::error_code e2;
            if (entry.is_regular_file(e2) && isJpegName(entry.path())) pages.push_back(entry.path());
        }
        if (pages.empty())
        {
            ctx.log.line("Aucune image dans " + platform::pathToUtf8(chapterFolder.filename()));
            return false;
        }

        std::sort(pages.begin(), pages.end(), [](const fs::path& a, const fs::path& b)
        {
            return utils::naturalLess(platform::pathToUtf8(a.stem()), platform::pathToUtf8(b.stem()));
        });

        HPDF_Doc pdf = HPDF_New(nullptr, nullptr);
        if (!pdf)
        {
            ctx.log.line("Impossible de creer le PDF.");
            return false;
        }

        size_t created = 0, skipped = 0;
        bool success = true;

        for (const fs::path& imagePath : pages)
        {
            if (ctx.cancelled()) { HPDF_Free(pdf); return false; }

            // Lecture en memoire : fonctionne meme avec des accents dans le chemin
            std::vector<unsigned char> data = readFile(imagePath);
            HPDF_Image image = nullptr;
            switch (detectKind(data))
            {
                case Kind::Jpeg:
                    image = HPDF_LoadJpegImageFromMem(pdf, data.data(), static_cast<HPDF_UINT>(data.size()));
                    break;
                case Kind::Png:
                    image = HPDF_LoadPngImageFromMem(pdf, data.data(), static_cast<HPDF_UINT>(data.size()));
                    break;
                default: break;
            }

            if (!image)
            {
                // Une image illisible ne fait pas perdre tout le chapitre
                ctx.log.line("Image ignoree : " + platform::pathToUtf8(imagePath.filename()));
                HPDF_ResetError(pdf);
                ++skipped;
                continue;
            }

            const float width  = static_cast<float>(HPDF_Image_GetWidth(image));
            const float height = static_cast<float>(HPDF_Image_GetHeight(image));
            if (width <= 0 || height <= 0) { ++skipped; continue; }

            // Page limitee a 1000 x 1400, sans agrandir les petites images
            float scale = std::min(1000.0f / width, 1400.0f / height);
            scale = std::min(scale, 1.0f);
            const float pw = width * scale, ph = height * scale;

            HPDF_Page page = HPDF_AddPage(pdf);
            if (!page) { HPDF_ResetError(pdf); ++skipped; continue; }

            HPDF_Page_SetWidth(page, pw);
            HPDF_Page_SetHeight(page, ph);
            HPDF_Page_DrawImage(page, image, 0, 0, pw, ph);
            ++created;
        }

        if (created == 0)
        {
            ctx.log.line("Aucune image utilisable.");
            success = false;
        }
        else
        {
            // PDF -> memoire -> fichier (chemin Unicode possible)
            if (HPDF_SaveToStream(pdf) != HPDF_OK)
            {
                ctx.log.line("Echec de la creation du PDF.");
                success = false;
            }
            else
            {
                HPDF_UINT32 size = HPDF_GetStreamSize(pdf);
                std::vector<HPDF_BYTE> buffer(size);
                HPDF_ResetStream(pdf);
                HPDF_ReadFromStream(pdf, buffer.data(), &size);

                std::ofstream out(outputPdf, std::ios::binary | std::ios::trunc);
                if (!out) success = false;
                else
                {
                    out.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(size));
                    out.close();
                    success = static_cast<bool>(out);
                }
                if (!success) ctx.log.line("Impossible d'ecrire le PDF.");
            }
        }

        HPDF_Free(pdf);

        if (!success)
        {
            std::error_code e3;
            fs::remove(outputPdf, e3);          // pas de PDF incomplet
            return false;
        }

        if (skipped > 0)
            ctx.log.line(std::to_string(skipped) + " image(s) ignoree(s) dans " +
                         platform::pathToUtf8(chapterFolder.filename()));
        return true;
    }

    bool buildAll(TaskContext& ctx, const fs::path& tempDir, const fs::path& scanDir, int& pdfCount)
    {
        pdfCount = 0;
        std::error_code ec;
        fs::create_directories(scanDir, ec);

        auto sortedSubdirs = [](const fs::path& dir, bool natural)
        {
            std::vector<fs::path> v;
            std::error_code e;
            for (const auto& entry : fs::directory_iterator(dir, e))
            {
                std::error_code e2;
                if (entry.is_directory(e2)) v.push_back(entry.path());
            }
            std::sort(v.begin(), v.end(), [natural](const fs::path& a, const fs::path& b)
            {
                std::string x = platform::pathToUtf8(a.filename()), y = platform::pathToUtf8(b.filename());
                return natural ? utils::naturalLess(x, y) : utils::toLower(x) < utils::toLower(y);
            });
            return v;
        };

        const std::vector<fs::path> mangas = sortedSubdirs(tempDir, false);

        // Total pour la barre de progression
        int total = 0;
        for (const auto& m : mangas) total += static_cast<int>(sortedSubdirs(m, true).size());
        ctx.progress.stageTotal = total;

        for (const fs::path& mangaFolder : mangas)
        {
            const fs::path outFolder = scanDir / mangaFolder.filename();
            fs::create_directories(outFolder, ec);

            for (const fs::path& chapterFolder : sortedSubdirs(mangaFolder, true))
            {
                if (ctx.cancelled()) return false;

                fs::path pdfName = chapterFolder.filename();
                pdfName += L".pdf";                         // "Chapter 10.5" + ".pdf"

                ctx.log.line("PDF : " + platform::pathToUtf8(chapterFolder.filename()));
                if (createPdf(ctx, chapterFolder, outFolder / pdfName)) ++pdfCount;
                else ctx.log.line("Echec : " + platform::pathToUtf8(chapterFolder.filename()));

                ctx.progress.stage++;
            }
        }

        ctx.log.line(std::to_string(pdfCount) + " PDF cree(s).");
        fs::remove_all(tempDir, ec);
        return true;
    }
}
