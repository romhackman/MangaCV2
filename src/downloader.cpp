#include "downloader.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <thread>

#include "http.h"
#include "platform.h"
#include "utils.h"

namespace fs = std::filesystem;

namespace
{
    // Evite de garder une page d'erreur HTML recue avec un code 200
    bool looksLikeImage(const fs::path& p)
    {
        std::ifstream f(p, std::ios::binary);
        unsigned char h[12] = {};
        f.read(reinterpret_cast<char*>(h), 12);
        const std::streamsize n = f.gcount();

        if (n >= 3 && h[0] == 0xFF && h[1] == 0xD8 && h[2] == 0xFF) return true;                  // JPEG
        if (n >= 8 && h[0] == 0x89 && h[1] == 'P' && h[2] == 'N' && h[3] == 'G') return true;     // PNG
        if (n >= 12 && std::memcmp(h, "RIFF", 4) == 0 && std::memcmp(h + 8, "WEBP", 4) == 0) return true;
        return false;
    }

    // 3 tentatives ; sort plus tot si annule
    bool downloadImage(TaskContext& ctx, http::Session& session, const std::string& url, const fs::path& out)
    {
        const int attempts = 3;
        for (int attempt = 1; attempt <= attempts; ++attempt)
        {
            http::Response r = session.download(url, out, &ctx.cancelFlag);
            if (r.ok && looksLikeImage(out)) return true;

            std::error_code ec;
            fs::remove(out, ec);
            if (ctx.cancelled()) return false;

            if (attempt < attempts)
                std::this_thread::sleep_for(std::chrono::milliseconds(700 * attempt));
        }
        return false;
    }
}

namespace downloader
{
    bool run(TaskContext& ctx, const ScrapeResult& manga, const std::vector<std::string>& names,
             const fs::path& tempDir, Stats& stats)
    {
        http::Session session;

        const fs::path mangaFolder = tempDir / platform::pathFromUtf8(utils::sanitizeName(manga.title, "Anime"));
        std::error_code ec;
        fs::create_directories(mangaFolder, ec);

        ctx.log.line("Manga : " + manga.title);
        ctx.progress.stageTotal = static_cast<int>(names.size());

        for (const std::string& name : names)
        {
            if (ctx.cancelled()) return false;

            ctx.progress.stage++;
            ctx.progress.item = 0;

            const Chapter* chapter = nullptr;
            for (const auto& c : manga.chapters)
                if (c.name == name) { chapter = &c; break; }

            if (!chapter)
            {
                ctx.log.line("Chapitre " + name + " introuvable.");
                continue;
            }

            const fs::path folder = mangaFolder / platform::pathFromUtf8("Chapter " + utils::sanitizeName(name, "0"));
            fs::create_directories(folder, ec);
            if (ec)
            {
                ctx.log.line("Impossible de creer le dossier du chapitre " + name + ".");
                continue;
            }

            const int total = static_cast<int>(chapter->images);
            ctx.progress.itemTotal = total;
            ctx.log.line("Chapitre " + name + " : " + std::to_string(total) + " image(s)");

            for (int i = 1; i <= total; ++i)
            {
                if (ctx.cancelled()) return false;
                ctx.progress.item = i;
                ++stats.images;

                char file[32];
                std::snprintf(file, sizeof(file), "%03d.jpg", i);
                const fs::path target = folder / file;

                std::error_code e2;
                if (fs::exists(target, e2) && fs::file_size(target, e2) > 0)
                {
                    ++stats.ok;
                    continue;
                }

                const std::string url = manga.baseImageUrl + name + "/" + std::to_string(i) + ".jpg";
                if (downloadImage(ctx, session, url, target))
                {
                    ++stats.ok;
                }
                else
                {
                    if (ctx.cancelled()) return false;
                    ++stats.failed;
                    ctx.log.line("[" + std::to_string(i) + "/" + std::to_string(total) + "] echec : " + url);
                }
            }
            ++stats.chapters;
        }

        ctx.log.line("Telechargement termine : " + std::to_string(stats.ok) + " image(s) OK, " +
                     std::to_string(stats.failed) + " en erreur.");
        return true;
    }
}
