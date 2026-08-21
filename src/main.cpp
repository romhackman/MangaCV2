#include <windows.h>
#include <iostream>
#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

bool nettoyerData()
{
    try
    {
        fs::path dataDir = "data";
        fs::path fontDir = dataDir / "font";

        if (!fs::exists(dataDir))
        {
            std::cerr << "Le dossier data n'existe pas.\n";
            return false;
        }

        // Supprime tout ce qui se trouve dans data,
        // sauf le dossier font.
        for (const auto& entry : fs::directory_iterator(dataDir))
        {
            if (entry.path() == fontDir)
                continue;

            fs::remove_all(entry.path());
        }

        return true;
    }
    catch (const fs::filesystem_error& e)
    {
        std::cerr << "Erreur lors du nettoyage de data : "
                  << e.what() << '\n';
        return false;
    }
}

bool lancerEtAttendre(const char* exe)
{
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};

    si.cb = sizeof(si);

    // CreateProcess peut modifier le buffer,
    // donc on fait une copie modifiable.
    char cmd[MAX_PATH];
    strcpy_s(cmd, sizeof(cmd), exe);

    if (!CreateProcessA(
        nullptr,
        cmd,
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        nullptr,
        &si,
        &pi))
    {
        std::cerr << "Impossible de lancer " << exe
                  << " (erreur " << GetLastError() << ")\n";
        return false;
    }

    // Attend que le programme soit terminé.
    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return true;
}

int main()
{
    // ================================
    // Version
    // ================================
    std::cout << "=================================\n";
    std::cout << "          Version v0.0.1\n";
    std::cout << "=================================\n\n";

    // ================================
    // Nettoyage avant de lancer
    // les exécutables.
    // ================================
    std::cout << "Nettoyage du dossier data...\n";

    if (!nettoyerData())
        return 1;

    std::cout << "Nettoyage termine.\n\n";

    // ================================
    // 1. domaine.exe
    // ================================
    std::cout << "Lancement de domaine.exe...\n";

    if (!lancerEtAttendre("domaine.exe"))
        return 1;

    std::cout << "domaine.exe termine.\n\n";

    // ================================
    // 2. preprocess.exe
    // ================================
    std::cout << "Lancement de preprocess.exe...\n";

    if (!lancerEtAttendre("preprocess.exe"))
        return 1;

    std::cout << "preprocess.exe termine.\n\n";

    // ================================
    // 3. gui.exe
    // ================================
    std::cout << "Lancement de gui.exe...\n";

    if (!lancerEtAttendre("gui.exe"))
        return 1;

    std::cout << "gui.exe termine.\n\n";

    // ================================
    // 4. chapter.exe
    // ================================
    std::cout << "Lancement de chapter.exe...\n";

    if (!lancerEtAttendre("chapter.exe"))
        return 1;

    std::cout << "chapter.exe termine.\n\n";
    return 0;
}