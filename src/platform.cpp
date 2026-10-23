#include "platform.h"

#include <cctype>
#include <cstdio>
#include <iostream>

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>

#include "utils.h"

#ifndef ASSOCF_IS_PROTOCOL
#define ASSOCF_IS_PROTOCOL 0x00001000
#endif

namespace platform
{
    static bool g_console = false;

    // ---------------- UTF-8 ----------------

    std::wstring toWide(const std::string& s)
    {
        if (s.empty()) return L"";
        int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        std::wstring w(n > 0 ? n - 1 : 0, L'\0');
        if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
        return w;
    }

    std::string toUtf8(const std::wstring& w)
    {
        if (w.empty()) return "";
        int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string s(n > 0 ? n - 1 : 0, '\0');
        if (n > 1) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
        return s;
    }

    fs::path    pathFromUtf8(const std::string& s) { return fs::path(toWide(s)); }
    std::string pathToUtf8(const fs::path& p)      { return toUtf8(p.wstring()); }

    // ---------------- demarrage ----------------

    fs::path exeDir()
    {
        wchar_t buf[MAX_PATH * 4] = {};
        DWORD n = GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(sizeof(buf) / sizeof(buf[0])));
        if (n == 0) return fs::current_path();
        return fs::path(std::wstring(buf, n)).parent_path();
    }

    void enterExeDir()
    {
        SetCurrentDirectoryW(exeDir().c_str());
    }

    void attachParentConsole()
    {
        // Sortie deja redirigee (pipe, fichier, terminal MSYS2...) : on la garde.
        HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        if (out != nullptr && out != INVALID_HANDLE_VALUE)
        {
            g_console = true;
            return;
        }

        // Lance depuis cmd / PowerShell : on se rattache a leur console.
        // Lance par double-clic : AttachConsole echoue, aucune fenetre noire.
        if (AttachConsole(ATTACH_PARENT_PROCESS))
        {
            std::freopen("CONOUT$", "w", stdout);
            std::freopen("CONOUT$", "w", stderr);
            SetConsoleOutputCP(CP_UTF8);
            g_console = true;
        }
    }

    bool consoleActive() { return g_console; }

    void consoleWrite(const std::string& line)
    {
        if (!g_console) return;
        std::fputs(line.c_str(), stdout);
        std::fputc('\n', stdout);
        std::fflush(stdout);
    }

    // ---------------- dossiers / fichiers ----------------

    std::string defaultDestination()
    {
        wchar_t buf[MAX_PATH] = {};
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_MYDOCUMENTS, nullptr, 0, buf)))
            return toUtf8((fs::path(buf) / L"Manga").wstring());
        return "Manga";
    }

    std::string browseForFolder()
    {
        wchar_t display[MAX_PATH] = {};
        BROWSEINFOW bi{};
        bi.pszDisplayName = display;
        bi.lpszTitle = L"Choisir le dossier de destination";
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

        PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
        if (!pidl) return "";

        wchar_t path[MAX_PATH] = {};
        std::string result;
        if (SHGetPathFromIDListW(pidl, path)) result = toUtf8(path);
        CoTaskMemFree(pidl);
        return result;
    }

    void openFolder(const fs::path& p)
    {
        ShellExecuteW(nullptr, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    // file:///C:/dossier/fichier.pdf (UTF-8 encode en %XX)
    static std::wstring fileUrl(const fs::path& file)
    {
        std::string p = toUtf8(fs::absolute(file).generic_wstring());
        std::string enc;
        char buf[4];
        for (unsigned char c : p)
        {
            if (std::isalnum(c) || c == '/' || c == ':' || c == '-' || c == '_' || c == '.' || c == '~')
                enc += static_cast<char>(c);
            else
            {
                std::snprintf(buf, sizeof(buf), "%%%02X", c);
                enc += buf;
            }
        }
        // Chemin reseau //serveur/partage -> file://serveur/partage
        if (enc.rfind("//", 0) == 0) return L"file:" + toWide(enc);
        return L"file:///" + toWide(enc);
    }

    static bool defaultBrowserExe(std::wstring& exe)
    {
        wchar_t buf[MAX_PATH * 2] = {};
        DWORD size = static_cast<DWORD>(sizeof(buf) / sizeof(buf[0]));

        if (SUCCEEDED(AssocQueryStringW(ASSOCF_IS_PROTOCOL, ASSOCSTR_EXECUTABLE,
                                        L"http", L"open", buf, &size)) && buf[0])
        {
            exe = buf;
            return true;
        }

        size = static_cast<DWORD>(sizeof(buf) / sizeof(buf[0]));
        if (SUCCEEDED(AssocQueryStringW(0, ASSOCSTR_EXECUTABLE,
                                        L".html", L"open", buf, &size)) && buf[0])
        {
            exe = buf;
            return true;
        }
        return false;
    }

    void openInBrowser(const fs::path& file)
    {
        std::wstring browser;
        if (defaultBrowserExe(browser))
        {
            std::wstring args = L"\"" + fileUrl(file) + L"\"";
            HINSTANCE r = ShellExecuteW(nullptr, L"open", browser.c_str(), args.c_str(),
                                        nullptr, SW_SHOWNORMAL);
            if (reinterpret_cast<INT_PTR>(r) > 32) return;
        }
        // Secours : application associee aux PDF
        ShellExecuteW(nullptr, L"open", file.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    void showError(const std::string& title, const std::string& message)
    {
        MessageBoxW(nullptr, toWide(message).c_str(), toWide(title).c_str(), MB_ICONERROR | MB_OK);
    }
}
