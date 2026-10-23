#include "storage.h"

#include <algorithm>
#include <fstream>

#include "config.h"
#include "platform.h"
#include "utils.h"

namespace fs = std::filesystem;

namespace
{
    bool isPdf(const fs::path& p)
    {
        return utils::toLower(p.extension().string()) == ".pdf";
    }
}

namespace storage
{
    std::uintmax_t folderSize(const fs::path& p)
    {
        std::error_code ec;
        std::uintmax_t total = 0;
        if (!fs::exists(p, ec)) return 0;
        if (fs::is_regular_file(p, ec)) return fs::file_size(p, ec);

        for (fs::recursive_directory_iterator it(p, fs::directory_options::skip_permission_denied, ec), end;
             it != end; it.increment(ec))
        {
            if (ec) break;
            std::error_code e2;
            if (it->is_regular_file(e2)) total += it->file_size(e2);
        }
        return total;
    }

    std::uintmax_t cacheSize()
    {
        return folderSize(cfg::TEMP_DIR) + folderSize(cfg::SCAN_DIR) + folderSize(cfg::DATA_TEMP_DIR);
    }

    void clearCache()
    {
        std::error_code ec;
        fs::remove_all(cfg::TEMP_DIR, ec);
        fs::remove_all(cfg::SCAN_DIR, ec);
        fs::remove_all(cfg::DATA_TEMP_DIR, ec);
    }

    void writeRunLog(const std::string& text)
    {
        std::ofstream f(cfg::LOG_FILE, std::ios::trunc | std::ios::binary);
        if (f) f << text;
    }

    std::vector<FolderEntry> listFolders(const fs::path& root)
    {
        std::vector<FolderEntry> out;
        std::error_code ec;
        if (!fs::is_directory(root, ec)) return out;

        for (const auto& entry : fs::directory_iterator(root, fs::directory_options::skip_permission_denied, ec))
        {
            std::error_code e2;
            if (!entry.is_directory(e2)) continue;

            FolderEntry f;
            f.path = entry.path();
            f.name = platform::pathToUtf8(entry.path().filename());

            for (const auto& sub : fs::directory_iterator(f.path, fs::directory_options::skip_permission_denied, e2))
            {
                std::error_code e3;
                if (sub.is_regular_file(e3) && isPdf(sub.path())) ++f.pdfCount;
            }
            out.push_back(std::move(f));
        }

        std::sort(out.begin(), out.end(), [](const FolderEntry& a, const FolderEntry& b)
        {
            return utils::toLower(a.name) < utils::toLower(b.name);
        });
        return out;
    }

    std::vector<PdfEntry> listPdfs(const fs::path& folder)
    {
        std::vector<PdfEntry> out;
        std::error_code ec;

        for (const auto& entry : fs::directory_iterator(folder, fs::directory_options::skip_permission_denied, ec))
        {
            std::error_code e2;
            if (!entry.is_regular_file(e2) || !isPdf(entry.path())) continue;

            PdfEntry p;
            p.path = entry.path();
            p.name = platform::pathToUtf8(entry.path().stem());
            p.size = entry.file_size(e2);
            out.push_back(std::move(p));
        }

        std::sort(out.begin(), out.end(), [](const PdfEntry& a, const PdfEntry& b)
        {
            return utils::naturalLess(a.name, b.name);
        });
        return out;
    }

    bool pdfExists(const fs::path& scanDir, const std::string& manga, const std::string& chapter)
    {
        std::error_code ec;
        const fs::path file = scanDir / platform::pathFromUtf8(utils::sanitizeName(manga, "Anime")) /
                              platform::pathFromUtf8("Chapter " + utils::sanitizeName(chapter, "0") + ".pdf");
        return fs::exists(file, ec);
    }

    bool copyTree(TaskContext& ctx, const fs::path& src, const fs::path& dst, int& copied, std::string& error)
    {
        copied = 0;
        std::error_code ec;

        std::vector<fs::path> files;
        if (fs::exists(src, ec))
            for (const auto& e : fs::recursive_directory_iterator(src, ec))
                if (e.is_regular_file()) files.push_back(e.path());

        ctx.progress.itemTotal = static_cast<int>(files.size());
        ctx.progress.item = 0;

        for (const auto& file : files)
        {
            if (ctx.cancelled()) { error = "Annule"; return false; }

            const fs::path target = dst / fs::relative(file, src, ec);
            fs::create_directories(target.parent_path(), ec);
            if (ec) { error = "Impossible de creer " + platform::pathToUtf8(target.parent_path()); return false; }

            fs::copy_file(file, target, fs::copy_options::overwrite_existing, ec);
            if (ec) { error = "Copie impossible : " + ec.message(); return false; }

            ++copied;
            ctx.progress.item = copied;
        }
        return true;
    }
}
