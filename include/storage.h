// ==========================================================
// storage.h  -  Dossiers de travail, cache, bibliotheque de PDF
// ==========================================================
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "task.h"

struct FolderEntry
{
    std::string name;
    std::filesystem::path path;
    int pdfCount = 0;
};

struct PdfEntry
{
    std::string name;                    // sans ".pdf"
    std::filesystem::path path;
    std::uintmax_t size = 0;
};

namespace storage
{
    std::uintmax_t folderSize(const std::filesystem::path& p);
    std::uintmax_t cacheSize();                     // temp/ + scan/ + data/temp/
    void clearCache();                              // garde la police et la base de recherche
    void writeRunLog(const std::string& text);

    // Bibliotheque (dossier de destination)
    std::vector<FolderEntry> listFolders(const std::filesystem::path& root);
    std::vector<PdfEntry>    listPdfs(const std::filesystem::path& folder);

    // Un "Chapter <n>.pdf" existe-t-il dans scan/<manga>/ ?
    bool pdfExists(const std::filesystem::path& scanDir, const std::string& manga, const std::string& chapter);

    // Copie recursive src -> dst avec progression ; false si annule ou erreur
    bool copyTree(TaskContext& ctx, const std::filesystem::path& src,
                  const std::filesystem::path& dst, int& copied, std::string& error);
}
