// ==========================================================
// pdf_builder.h  -  Images -> PDF avec libharu (ancien pdf.exe)
// ==========================================================
#pragma once

#include <filesystem>

#include "task.h"

namespace pdfbuilder
{
    // Un PDF a partir des images d'un dossier de chapitre.
    bool createPdf(TaskContext& ctx, const std::filesystem::path& chapterFolder,
                   const std::filesystem::path& outputPdf);

    // tempDir/<manga>/Chapter N/*.jpg  ->  scanDir/<manga>/Chapter N.pdf
    // Supprime tempDir a la fin. Retourne false uniquement si annule.
    bool buildAll(TaskContext& ctx, const std::filesystem::path& tempDir,
                  const std::filesystem::path& scanDir, int& pdfCount);
}
