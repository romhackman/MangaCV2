// ==========================================================
// platform.h  -  Tout ce qui est specifique a Windows
// ==========================================================
#pragma once

#include <filesystem>
#include <string>

namespace platform
{
    namespace fs = std::filesystem;

    // ---- UTF-8 <-> chemins ----
    std::wstring toWide(const std::string& s);
    std::string  toUtf8(const std::wstring& w);
    fs::path     pathFromUtf8(const std::string& s);
    std::string  pathToUtf8(const fs::path& p);

    // ---- demarrage ----
    fs::path exeDir();
    void     enterExeDir();            // SetCurrentDirectory(dossier de l'exe)

    // Pas de console propre : si l'app est lancee depuis un terminal,
    // on s'y rattache pour que les logs y apparaissent. Sinon, rien ne s'ouvre.
    void attachParentConsole();
    bool consoleActive();
    void consoleWrite(const std::string& line);

    // ---- dossiers / fichiers ----
    std::string defaultDestination();            // Documents\Manga
    std::string browseForFolder();               // boite de dialogue, "" si annule
    void        openFolder(const fs::path& p);   // Explorateur
    void        openInBrowser(const fs::path& file); // navigateur par defaut

    void showError(const std::string& title, const std::string& message);
}
