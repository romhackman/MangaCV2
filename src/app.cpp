// ==========================================================
// app.cpp  -  Manga Downloader (interface unique)
//
// Une seule fenetre SDL2. Aucun cmd visible.
//
//   Accueil -> (Preparation) -> Recherche -> Chapitres
//           -> Telechargement + PDF -> Copie -> Termine
//
// Les anciens .exe (domaine, preprocess, scraper, download, pdf)
// tournent en arriere-plan, caches, et leur sortie alimente
// les barres de progression. "Annuler" tue tout l'arbre de
// processus (Job Object Windows).
// ==========================================================

#define SDL_MAIN_HANDLED

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <nlohmann/json.hpp>
#include <curl/curl.h>

#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

// ==========================================================
// CONFIG
// ==========================================================

const int WINDOW_WIDTH  = 1000;
const int WINDOW_HEIGHT = 700;

const char* APP_VERSION = "v0.1.1";

const std::string SETTINGS_FILE = "settings.json";
const std::string HERITAGE_FILE = "data/heritage.json";
const std::string RESULT_FILE   = "data/result.json";
const std::string STATUS_FILE   = "data/status.json";
const char* FONT_PATH           = "data/font/yoruka.ttf";

// ==========================================================
// COULEURS (identiques a l'ancien projet)
// ==========================================================

const SDL_Color BG_COLOR        = { 18,  18,  24, 255 };
const SDL_Color PANEL_COLOR     = { 28,  28,  38, 255 };
const SDL_Color INPUT_COLOR     = { 40,  40,  52, 255 };
const SDL_Color BUTTON_COLOR    = { 70,  90, 180, 255 };
const SDL_Color HOVER_COLOR     = { 90, 115, 220, 255 };
const SDL_Color TEXT_COLOR      = {235, 235, 240, 255 };
const SDL_Color MUTED_COLOR     = {150, 150, 160, 255 };
const SDL_Color SEARCHING_COLOR = {190,  55,  55, 255 };
const SDL_Color FINISHED_COLOR  = { 50, 180, 100, 255 };
const SDL_Color CHECKBOX_COLOR  = { 55,  55,  70, 255 };
const SDL_Color BORDER_COLOR    = { 90,  90, 110, 255 };
const SDL_Color DANGER_COLOR    = {160,  50,  50, 255 };
const SDL_Color DANGER_HOVER    = {200,  70,  70, 255 };

// ==========================================================
// UTF-8 / WIDE
// ==========================================================

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

fs::path pathFromUtf8(const std::string& s) { return fs::path(toWide(s)); }

void eraseLastChar(std::string& s)
{
    if (s.empty()) return;
    size_t i = s.size() - 1;
    while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) --i;
    s.erase(i);
}

std::string trim(std::string v)
{
    auto ns = [](unsigned char c) { return !std::isspace(c); };
    v.erase(v.begin(), std::find_if(v.begin(), v.end(), ns));
    v.erase(std::find_if(v.rbegin(), v.rend(), ns).base(), v.end());
    return v;
}

std::string toLower(std::string v)
{
    for (char& c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return v;
}

// Retire les codes couleur ANSI (scraper.exe en affiche)
std::string stripAnsi(const std::string& in)
{
    std::string out;
    for (size_t i = 0; i < in.size(); ++i)
    {
        if (in[i] == '\x1b')
        {
            if (i + 1 < in.size() && in[i + 1] == '[')
            {
                i += 2;
                while (i < in.size() && !std::isalpha(static_cast<unsigned char>(in[i]))) ++i;
            }
            continue;
        }
        out += in[i];
    }
    return out;
}

// ==========================================================
// PARAMETRES (settings.json, a cote de l'exe)
// ==========================================================

struct Settings
{
    std::string destination;
    bool openWhenDone = true;
};

std::string defaultDestination()
{
    wchar_t buf[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_MYDOCUMENTS, nullptr, 0, buf)))
        return toUtf8((fs::path(buf) / L"Manga").wstring());
    return "Manga";
}

Settings loadSettings()
{
    Settings s;
    s.destination = defaultDestination();

    std::ifstream f(SETTINGS_FILE);
    if (!f) return s;
    try
    {
        json j; f >> j;
        if (j.contains("destination") && j["destination"].is_string())
            s.destination = j["destination"].get<std::string>();
        if (j.contains("openWhenDone") && j["openWhenDone"].is_boolean())
            s.openWhenDone = j["openWhenDone"].get<bool>();
    }
    catch (...) {}
    return s;
}

void saveSettings(const Settings& s)
{
    json j;
    j["destination"]  = s.destination;
    j["openWhenDone"] = s.openWhenDone;
    std::ofstream f(SETTINGS_FILE, std::ios::trunc);
    if (f) f << j.dump(4);
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

// ==========================================================
// FICHIERS / CACHE
// ==========================================================

bool statusIsTrue()
{
    std::ifstream f(STATUS_FILE);
    if (!f) return false;
    try { json j; f >> j; return j.is_object() && j.value("status", false); }
    catch (...) { return false; }
}

void writeStatusFalse()
{
    std::error_code ec;
    fs::create_directories("data", ec);
    std::ofstream f(STATUS_FILE, std::ios::trunc);
    if (f) f << "{\n    \"status\": false\n}";
}

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
    return folderSize("temp") + folderSize("scan") + folderSize("data/temp") +
           folderSize(RESULT_FILE) + folderSize(STATUS_FILE);
}

// Vide temp/, scan/, data/temp et les fichiers de travail.
// Conserve : la police et la base de recherche (heritage.json).
void clearCache()
{
    std::error_code ec;
    fs::remove_all("temp", ec);
    fs::remove_all("scan", ec);
    fs::remove_all("data/temp", ec);
    fs::remove(RESULT_FILE, ec);
    fs::remove(STATUS_FILE, ec);
}

void writeRunLog(const std::string& text)
{
    std::ofstream f("last_run.log", std::ios::trunc | std::ios::binary);
    if (f) f << text;
}

// Un PDF "Chapter N.pdf" existe-t-il dans scan/<manga>/ ?
bool pdfExistsForChapter(const std::string& name)
{
    std::error_code ec;
    if (!fs::exists("scan", ec)) return false;

    for (const auto& entry : fs::directory_iterator("scan", ec))
    {
        std::error_code e2;
        if (!entry.is_directory(e2)) continue;
        if (fs::exists(entry.path() / ("Chapter " + name + ".pdf"), e2)) return true;
    }
    return false;
}

std::string formatBytes(std::uintmax_t b)
{
    char buf[64];
    if (b < 1024)                 std::snprintf(buf, sizeof(buf), "%llu o", (unsigned long long)b);
    else if (b < 1024 * 1024)     std::snprintf(buf, sizeof(buf), "%.1f Ko", b / 1024.0);
    else if (b < 1024ull * 1024 * 1024) std::snprintf(buf, sizeof(buf), "%.1f Mo", b / (1024.0 * 1024.0));
    else                          std::snprintf(buf, sizeof(buf), "%.2f Go", b / (1024.0 * 1024.0 * 1024.0));
    return buf;
}

// ==========================================================
// JOB : processus cache + sortie lue en direct
// (Job Object => "Annuler" tue aussi les processus enfants)
// ==========================================================

struct Job
{
    HANDLE process = nullptr;
    HANDLE pipe    = nullptr;
    HANDLE jobObj  = nullptr;
    bool   running = false;
    DWORD  exitCode = 0;

    std::string partial;
    std::deque<std::string> log;
    std::vector<std::string> fresh;
    std::string fullLog;               // journal complet (ecrit dans last_run.log)

    bool start(const std::string& cmd)
    {
        log.clear(); fresh.clear(); partial.clear(); fullLog.clear();
        exitCode = 0;

        SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
        HANDLE readEnd = nullptr, writeEnd = nullptr;
        if (!CreatePipe(&readEnd, &writeEnd, &sa, 0)) return false;
        SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        si.hStdOutput = writeEnd;
        si.hStdError  = writeEnd;
        si.hStdInput  = INVALID_HANDLE_VALUE;

        PROCESS_INFORMATION pi{};
        std::vector<char> buf(cmd.begin(), cmd.end());
        buf.push_back('\0');

        BOOL ok = CreateProcessA(nullptr, buf.data(), nullptr, nullptr, TRUE,
                                 CREATE_NO_WINDOW | CREATE_SUSPENDED,
                                 nullptr, nullptr, &si, &pi);
        CloseHandle(writeEnd);

        if (!ok)
        {
            CloseHandle(readEnd);
            return false;
        }

        jobObj = CreateJobObjectW(nullptr, nullptr);
        if (jobObj)
        {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
            info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(jobObj, JobObjectExtendedLimitInformation, &info, sizeof(info));
            AssignProcessToJobObject(jobObj, pi.hProcess);
        }

        ResumeThread(pi.hThread);
        CloseHandle(pi.hThread);

        process = pi.hProcess;
        pipe    = readEnd;
        running = true;
        return true;
    }

    void pushLine()
    {
        std::string line = trim(stripAnsi(partial));
        partial.clear();
        if (line.empty()) return;
        log.push_back(line);
        fresh.push_back(line);
        if (fullLog.size() < 4000000) { fullLog += line; fullLog += '\n'; }
        if (log.size() > 200) log.pop_front();
    }

    void drain()
    {
        if (!pipe) return;
        for (;;)
        {
            DWORD avail = 0;
            if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &avail, nullptr) || avail == 0) break;

            char buf[4096];
            DWORD n = 0;
            if (!ReadFile(pipe, buf, std::min<DWORD>(avail, sizeof(buf)), &n, nullptr) || n == 0) break;

            for (DWORD i = 0; i < n; ++i)
            {
                if (buf[i] == '\n' || buf[i] == '\r') pushLine();
                else partial += buf[i];
            }
        }
    }

    void closeHandles()
    {
        if (process) { CloseHandle(process); process = nullptr; }
        if (pipe)    { CloseHandle(pipe);    pipe = nullptr; }
        if (jobObj)  { CloseHandle(jobObj);  jobObj = nullptr; } // tue les restes
        running = false;
    }

    void pump()
    {
        if (!running) return;

        bool exited = WaitForSingleObject(process, 0) == WAIT_OBJECT_0;
        drain();

        if (exited)
        {
            drain();
            pushLine();
            GetExitCodeProcess(process, &exitCode);
            closeHandles();
        }
    }

    void kill()
    {
        if (!running) return;
        if (jobObj) TerminateJobObject(jobObj, 1);
        else        TerminateProcess(process, 1);
        WaitForSingleObject(process, 2000);
        closeHandles();
    }
};

// ==========================================================
// DESSIN
// ==========================================================

SDL_Renderer* gR = nullptr;
TTF_Font* gFont  = nullptr;
TTF_Font* gTitle = nullptr;

void setColor(SDL_Color c) { SDL_SetRenderDrawColor(gR, c.r, c.g, c.b, c.a); }

void drawPanel(const SDL_Rect& r, SDL_Color c)
{
    setColor(c);
    SDL_RenderFillRect(gR, &r);
}

void drawBorder(const SDL_Rect& r, SDL_Color c)
{
    setColor(c);
    SDL_RenderDrawRect(gR, &r);
}

void drawText(TTF_Font* font, const std::string& text, int x, int y, SDL_Color color)
{
    if (text.empty()) return;
    SDL_Surface* s = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!s) return;
    SDL_Texture* t = SDL_CreateTextureFromSurface(gR, s);
    if (t)
    {
        SDL_Rect d = { x, y, s->w, s->h };
        SDL_RenderCopy(gR, t, nullptr, &d);
        SDL_DestroyTexture(t);
    }
    SDL_FreeSurface(s);
}

int textWidth(const std::string& text)
{
    int w = 0, h = 0;
    TTF_SizeUTF8(gFont, text.c_str(), &w, &h);
    return w;
}

std::string fitTail(std::string text, int maxW)
{
    if (textWidth(text) <= maxW) return text;
    while (!text.empty() && textWidth("..." + text) > maxW)
    {
        size_t i = 1;
        while (i < text.size() && (static_cast<unsigned char>(text[i]) & 0xC0) == 0x80) ++i;
        text.erase(0, i);
    }
    return "..." + text;
}

std::string fitHead(std::string text, int maxW)
{
    if (textWidth(text) <= maxW) return text;
    while (!text.empty() && textWidth(text + "...") > maxW) eraseLastChar(text);
    return text + "...";
}

bool inRect(int x, int y, const SDL_Rect& r)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

void drawButton(const SDL_Rect& r, const std::string& label, int mx, int my,
                bool enabled = true, bool danger = false)
{
    SDL_Color base  = danger ? DANGER_COLOR : BUTTON_COLOR;
    SDL_Color hover = danger ? DANGER_HOVER : HOVER_COLOR;
    SDL_Color c = !enabled ? INPUT_COLOR : (inRect(mx, my, r) ? hover : base);
    drawPanel(r, c);
    int tw = textWidth(label);
    drawText(gFont, label, r.x + (r.w - tw) / 2, r.y + (r.h - 24) / 2,
             enabled ? TEXT_COLOR : MUTED_COLOR);
}

// fraction < 0 => barre indeterminee (animation)
void drawProgress(const SDL_Rect& r, double fraction, SDL_Color fill, Uint32 ticks)
{
    drawPanel(r, INPUT_COLOR);

    if (fraction < 0.0)
    {
        int w = r.w / 4;
        int pos = static_cast<int>((ticks / 6) % (r.w + w)) - w;
        int x0 = std::max(0, pos);
        int x1 = std::min(r.w, pos + w);
        if (x1 > x0)
        {
            SDL_Rect f = { r.x + x0, r.y, x1 - x0, r.h };
            drawPanel(f, fill);
        }
        return;
    }

    fraction = std::clamp(fraction, 0.0, 1.0);
    SDL_Rect f = { r.x, r.y, static_cast<int>(r.w * fraction), r.h };
    drawPanel(f, fill);

    std::string pct = std::to_string(static_cast<int>(fraction * 100)) + " %";
    drawText(gFont, pct, r.x + (r.w - textWidth(pct)) / 2, r.y + (r.h - 24) / 2, TEXT_COLOR);
}

void drawLog(const Job& job, int x, int y, int w, int lines)
{
    SDL_Rect box = { x, y, w, lines * 26 + 20 };
    drawPanel(box, PANEL_COLOR);

    int start = std::max<int>(0, static_cast<int>(job.log.size()) - lines);
    int row = 0;
    for (size_t i = start; i < job.log.size(); ++i, ++row)
        drawText(gFont, fitHead(job.log[i], w - 30), x + 15, y + 10 + row * 26, MUTED_COLOR);
}

void drawScrollbar(const SDL_Rect& area, int contentH, int offset)
{
    if (contentH <= area.h) return;

    SDL_Rect bg = { area.x + area.w - 8, area.y, 8, area.h };
    drawPanel(bg, INPUT_COLOR);

    int barH = std::max(30, area.h * area.h / contentH);
    int maxOff = contentH - area.h;
    int barY = area.y + static_cast<int>((area.h - barH) * (double(offset) / maxOff));
    SDL_Rect bar = { bg.x, barY, bg.w, barH };
    drawPanel(bar, BUTTON_COLOR);
}

int clampScroll(int offset, int contentH, int viewH)
{
    return std::clamp(offset, 0, std::max(0, contentH - viewH));
}

// ==========================================================
// MODELE
// ==========================================================

struct SearchResult { std::string identifier, url; bool scanChoice = false; };

struct Chapter
{
    std::string name;
    long long images = 0;
    bool selected = false;
};

bool chapterLess(const Chapter& a, const Chapter& b)
{
    try
    {
        double x = std::stod(a.name), y = std::stod(b.name);
        if (x != y) return x < y;
    }
    catch (...) {}
    return a.name < b.name;
}

std::vector<SearchResult> searchManga(const json& scans, const std::string& query)
{
    std::vector<SearchResult> results;
    std::string search = toLower(trim(query));
    if (search.empty()) return results;

    auto addAll = [&](const std::string& id, const json& arr)
    {
        if (!arr.is_array()) return;
        for (const auto& v : arr)
            if (v.is_string()) results.push_back({ id, v.get<std::string>() });
    };

    for (auto it = scans.begin(); it != scans.end(); ++it)
        if (toLower(it.key()).find(search) != std::string::npos)
            addAll(it.key(), it.value());

    if (results.empty())
    {
        std::istringstream ss(search);
        std::vector<std::string> words;
        std::string w;
        while (ss >> w) words.push_back(w);

        for (auto it = scans.begin(); it != scans.end(); ++it)
        {
            std::string low = toLower(it.key());
            bool all = true;
            for (const auto& x : words)
                if (low.find(x) == std::string::npos) { all = false; break; }
            if (all) addAll(it.key(), it.value());
        }
    }

    std::vector<SearchResult> unique;
    std::unordered_set<std::string> seen;
    for (auto& r : results)
        if (seen.insert(r.url).second) unique.push_back(r);
    return unique;
}

// Recherche directe Anime-Sama (API de recherche puis choix des scans).
const std::string ANIME_SAMA_BASE = "https://anime-sama.to";
const std::string ANIME_SAMA_API = "https://anime-sama.to/template-php/defaut/fetch.php";

size_t curlWriteCallback(char* data, size_t size, size_t count, void* userData)
{
    const size_t bytes = size * count;
    static_cast<std::string*>(userData)->append(data, bytes);
    return bytes;
}

std::string decodeBasicHtml(std::string value)
{
    const std::pair<const char*, const char*> entities[] = {
        {"&amp;", "&"}, {"&quot;", "\""}, {"&#39;", "'"}, {"&apos;", "'"},
        {"&lt;", "<"}, {"&gt;", ">"}, {"&nbsp;", " "}
    };
    for (const auto& e : entities) {
        size_t pos = 0;
        while ((pos = value.find(e.first, pos)) != std::string::npos) {
            value.replace(pos, std::strlen(e.first), e.second);
            pos += std::strlen(e.second);
        }
    }
    return value;
}

std::string stripHtmlTags(std::string value)
{
    value = std::regex_replace(value, std::regex(R"(<[^>]*>)"), "");
    return trim(decodeBasicHtml(value));
}

std::string fetchHtml(const std::string& url, const std::string* postData = nullptr)
{
    CURL* curl = curl_easy_init();
    if (!curl) return {};
    std::string response;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0");
    headers = curl_slist_append(headers, "X-Requested-With: XMLHttpRequest");
    headers = curl_slist_append(headers, "Referer: https://anime-sama.to/");
    if (postData) headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded; charset=UTF-8");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 35L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    if (postData) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData->c_str());
    }
    CURLcode code = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return code == CURLE_OK ? response : std::string();
}

std::vector<SearchResult> searchAnimeSamaDirect(const std::string& query)
{
    std::vector<SearchResult> out;
    CURL* curl = curl_easy_init();
    if (!curl) return out;
    char* escaped = curl_easy_escape(curl, query.c_str(), static_cast<int>(query.size()));
    if (!escaped) { curl_easy_cleanup(curl); return out; }
    std::string postData = "query=" + std::string(escaped);
    curl_free(escaped);
    curl_easy_cleanup(curl);
    const std::string html = fetchHtml(ANIME_SAMA_API, &postData);
    if (html.empty()) return out;

    const std::regex anchorPattern(R"(<a\b[^>]*class\s*=\s*["'][^"']*\basn-search-result\b[^"']*["'][^>]*>[\s\S]*?</a>)", std::regex_constants::icase);
    const std::regex hrefPattern(R"(\bhref\s*=\s*["']([^"']+)["'])", std::regex_constants::icase);
    const std::regex titlePattern(R"(<h3\b[^>]*class\s*=\s*["'][^"']*\basn-search-result-title\b[^"']*["'][^>]*>([\s\S]*?)</h3>)", std::regex_constants::icase);
    for (auto it = std::sregex_iterator(html.begin(), html.end(), anchorPattern); it != std::sregex_iterator(); ++it) {
        const std::string block = it->str();
        std::smatch href, title;
        if (!std::regex_search(block, href, hrefPattern) || !std::regex_search(block, title, titlePattern)) continue;
        std::string url = decodeBasicHtml(href[1].str());
        if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0)
            url = ANIME_SAMA_BASE + (url.empty() || url.front() != '/' ? "/" : "") + url;
        out.push_back({stripHtmlTags(title[1].str()), url, false});
    }
    return out;
}

std::vector<SearchResult> fetchAnimeScanChoices(const std::string& animeUrl)
{
    std::vector<SearchResult> out;
    const std::string html = fetchHtml(animeUrl);
    if (html.empty()) return out;
    const std::regex scanPattern(R"(panneauScan\s*\(\s*["']([^"']+)["']\s*,\s*["']([^"']+)["']\s*\))", std::regex_constants::icase);
    for (auto it = std::sregex_iterator(html.begin(), html.end(), scanPattern); it != std::sregex_iterator(); ++it) {
        std::string title = decodeBasicHtml((*it)[1].str());
        std::string url = decodeBasicHtml((*it)[2].str());
        if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) {
            if (url.empty() || url.front() != '/') url = "/" + url;
            url = ANIME_SAMA_BASE + url;
        }
        out.push_back({title, url, true});
    }
    std::vector<SearchResult> unique;
    std::unordered_set<std::string> seen;
    for (const auto& item : out) if (seen.insert(item.url).second) unique.push_back(item);
    return unique;
}

// ==========================================================
// APPLICATION
// ==========================================================

enum class Screen { Home, Prepare, Search, Scraping, Chapters, Download, Copy, Done, Error };

// ---- rectangles ----
const SDL_Rect R_QUIT       = { 850, 640,  110,  40 };
const SDL_Rect R_BACK       = {  40, 640,  180,  40 };
const SDL_Rect R_PRIMARY    = { 560, 640,  270,  40 };

// Accueil
const SDL_Rect R_DEST_BOX   = {  40, 150, 700,  50 };
const SDL_Rect R_DEST_BTN   = { 760, 150, 200,  50 };
const SDL_Rect R_OPEN_CHECK = {  40, 225,  30,  30 };
const SDL_Rect R_BASE_BTN   = {  40, 340, 300,  45 };
const SDL_Rect R_CACHE_BTN  = {  40, 450, 300,  45 };
const SDL_Rect R_START_BTN  = {  40, 560, 400,  55 };

// Recherche
const SDL_Rect R_MODE_ANIME = {  40,  78, 270,  30 };
const SDL_Rect R_MODE_SITEMAP = { 320,  78, 270,  30 };
const SDL_Rect R_SEARCH_BOX = {  40, 120, 700,  50 };
const SDL_Rect R_SEARCH_BTN = { 760, 120, 200,  50 };
const SDL_Rect R_SEARCH_LST = {  40, 230, 920, 390 };
const int      SEARCH_ROW_H = 65;

// Chapitres
const SDL_Rect R_SEL_ALL    = {  40, 100, 220,  40 };
const SDL_Rect R_SEL_NONE   = { 275, 100, 260,  40 };
const SDL_Rect R_CHAP_LST   = {  40, 155, 920, 465 };
const int      CHAP_ROW_H   = 48;

// Progression
const SDL_Rect R_BAR_A      = {  40, 230, 920,  36 };
const SDL_Rect R_BAR_B      = {  40, 320, 920,  36 };
const SDL_Rect R_BAR_C      = {  40, 410, 920,  36 };

struct App
{
    Screen screen = Screen::Home;
    Settings settings;
    std::string errorMsg;
    std::string homeInfo;
    Job job;

    std::string* activeInput = nullptr;

    // base de recherche
    json scans;
    bool heritageOk = false;
    size_t heritageCount = 0;

    // cache
    std::uintmax_t cacheBytes = 0;
    Uint32 clearConfirmUntil = 0;

    // preparation
    int prepIdx = 0;
    bool prepThenSearch = true;
    const char* prepCmds[2]   = { ".\\domaine.exe", ".\\preprocess.exe" };
    const char* prepLabels[2] = { "Recherche du nom de domaine...", "Création de la base de recherche..." };

    // recherche
    std::string searchText;
    std::vector<SearchResult> results;
    bool searched = false;
    bool useAnimeSamaSearch = true;
    bool showingScanChoices = false;
    std::string selectedAnimeUrl;
    int searchScroll = 0;
    std::string selName;

    // chapitres
    std::string mangaTitle;
    std::vector<Chapter> chapters;
    int chapterScroll = 0;

    // telechargement
    int chaptersTotal = 1, chaptersSeen = 0;
    int imgCur = 0, imgTot = 0;
    bool pdfPhase = false;
    std::vector<std::string> selectedNames;
    std::vector<std::string> missingChapters;

    // copie
    std::thread copyThread;
    std::atomic<int>  copyDone{0}, copyTotal{0};
    std::atomic<bool> copyFinished{false}, copyFailed{false};
    std::string copyError;

    // -------------------------------------------------
    void refreshHome()
    {
        cacheBytes = cacheSize();
        checkHeritage();
    }

    void checkHeritage()
    {
        heritageOk = false;
        heritageCount = 0;
        scans = json::object();

        std::ifstream f(HERITAGE_FILE);
        if (!f) return;
        try
        {
            json data; f >> data;
            if (data.is_object() && data.contains("scan") && data["scan"].is_object())
            {
                scans = data["scan"];
                heritageCount = scans.size();
                heritageOk = heritageCount > 0;
            }
        }
        catch (...) {}
    }

    void stopAll()
    {
        job.kill();
        if (copyThread.joinable()) copyThread.join();
    }

    void goHome(const std::string& info = "")
    {
        stopAll();
        homeInfo = info;
        activeInput = nullptr;
        refreshHome();
        screen = Screen::Home;
    }

    void fail(const std::string& msg)
    {
        stopAll();
        errorMsg = msg;
        activeInput = nullptr;
        screen = Screen::Error;
    }

    // -------------------------------------------------
    // ACCUEIL
    // -------------------------------------------------
    void startSearchFlow()
    {
        saveSettings(settings);

        try { fs::create_directories(pathFromUtf8(settings.destination)); }
        catch (const std::exception& e)
        {
            fail(std::string("Dossier de destination invalide : ") + e.what());
            return;
        }

        if (useAnimeSamaSearch || heritageOk) enterSearch();
        else                            startPrepare(true);
    }

    void enterSearch()
    {
        screen = Screen::Search;
        activeInput = &searchText;
    }

    void startPrepare(bool thenSearch)
    {
        saveSettings(settings);
        std::error_code ec;
        fs::create_directories("data", ec);

        prepIdx = 0;
        prepThenSearch = thenSearch;
        activeInput = nullptr;
        screen = Screen::Prepare;

        if (!job.start(prepCmds[0]))
            fail("Impossible de lancer domaine.exe (est-il dans le dossier de l'application ?)");
    }

    void updateBase()
    {
        std::error_code ec;
        fs::remove(HERITAGE_FILE, ec);
        fs::remove("data/domaine.json", ec);
        fs::remove_all("data/temp", ec);
        startPrepare(false);
    }

    // -------------------------------------------------
    // PREPARATION
    // -------------------------------------------------
    void updatePrepare()
    {
        job.pump();
        job.fresh.clear();
        if (job.running) return;

        if (job.exitCode != 0)
        {
            fail(std::string("Échec de ") + prepCmds[prepIdx] +
                 " (code " + std::to_string(job.exitCode) + "). Vérifie ta connexion internet.");
            return;
        }

        if (++prepIdx < 2)
        {
            if (!job.start(prepCmds[prepIdx]))
                fail(std::string("Impossible de lancer ") + prepCmds[prepIdx]);
            return;
        }

        checkHeritage();
        if (!heritageOk)
        {
            fail("La base de recherche est vide ou invalide (data/heritage.json).");
            return;
        }

        if (prepThenSearch) enterSearch();
        else goHome("Base de recherche mise à jour.");
    }

    // -------------------------------------------------
    // RECHERCHE
    // -------------------------------------------------
    void doSearch()
    {
        if (trim(searchText).empty()) return;
        results.clear();
        showingScanChoices = false;
        searched = true;
        searchScroll = 0;
        if (useAnimeSamaSearch) {
            results = searchAnimeSamaDirect(trim(searchText));
        } else {
            results = searchManga(scans, searchText);
        }
    }

    void chooseSearchResult(const SearchResult& r)
    {
        if (useAnimeSamaSearch && !r.scanChoice) {
            selectedAnimeUrl = r.url;
            selName = r.identifier;
            results = fetchAnimeScanChoices(r.url);
            showingScanChoices = true;
            searched = true;
            searchScroll = 0;
            if (results.empty()) {
                // Keep the selected anime visible as context and let the user retry/change method.
                searched = true;
            }
            return;
        }
        startScrape(r);
    }

    void startScrape(const SearchResult& r)
    {
        selName = r.identifier;
        writeStatusFalse();
        std::error_code ec;
        fs::remove(RESULT_FILE, ec);

        activeInput = nullptr;
        screen = Screen::Scraping;

        if (!job.start(".\\scraper.exe -link \"" + r.url + "\""))
            fail("Impossible de lancer scraper.exe");
    }

    void updateScraping()
    {
        job.pump();
        job.fresh.clear();
        if (job.running) return;

        if (job.exitCode != 0 || !statusIsTrue())
        {
            fail("La récupération des chapitres a échoué pour « " + selName + " ».");
            return;
        }

        if (!loadChapters())
        {
            fail("Aucun chapitre trouvé pour « " + selName + " ».");
            return;
        }

        chapterScroll = 0;
        screen = Screen::Chapters;
    }

    bool loadChapters()
    {
        chapters.clear();
        mangaTitle = selName;

        std::ifstream f(RESULT_FILE);
        if (!f) return false;

        json data;
        try { f >> data; } catch (...) { return false; }

        if (!data.contains("results") || !data["results"].is_array()) return false;

        for (const auto& result : data["results"])
        {
            if (mangaTitle == selName && result.contains("titre") && result["titre"].is_string())
                mangaTitle = result["titre"].get<std::string>();

            if (!result.contains("chapters") || !result["chapters"].is_object()) continue;

            const auto& obj = result["chapters"];
            for (auto it = obj.begin(); it != obj.end(); ++it)
            {
                Chapter c;
                c.name = it.key();
                if (it.value().is_object() && it.value().contains("image_count") &&
                    it.value()["image_count"].is_number())
                {
                    try { c.images = static_cast<long long>(it.value()["image_count"].get<double>()); }
                    catch (...) {}
                }
                chapters.push_back(c);
            }
        }

        std::sort(chapters.begin(), chapters.end(), chapterLess);
        return !chapters.empty();
    }

    int selectedCount() const
    {
        int n = 0;
        for (auto& c : chapters) if (c.selected) ++n;
        return n;
    }

    // -------------------------------------------------
    // TELECHARGEMENT
    // -------------------------------------------------
    void startDownload()
    {
        std::vector<std::string> sel;
        for (auto& c : chapters) if (c.selected) sel.push_back(c.name);
        if (sel.empty()) return;

        std::string args;
        if (sel.size() == chapters.size()) args = "-all";
        else
        {
            args = "-chapter [";
            for (size_t i = 0; i < sel.size(); ++i)
            {
                args += sel[i];
                if (i + 1 < sel.size()) args += ", ";
            }
            args += "]";
        }

        std::error_code ec;
        fs::remove_all("temp", ec);
        fs::remove_all("scan", ec);

        selectedNames = sel;
        missingChapters.clear();
        chaptersTotal = static_cast<int>(sel.size());
        chaptersSeen = 0; imgCur = imgTot = 0; pdfPhase = false;
        screen = Screen::Download;

        if (!job.start(".\\download.exe " + args))
            fail("Impossible de lancer download.exe");
    }

    void updateDownload()
    {
        job.pump();

        for (const std::string& line : job.fresh)
        {
            int i = 0, n = 0;
            if (!pdfPhase && std::sscanf(line.c_str(), "[%d/%d]", &i, &n) == 2 && n > 0)
            {
                imgCur = i; imgTot = n;
                if (i == 1) ++chaptersSeen;
            }
            if (line.find("MANGA -> PDF") != std::string::npos ||
                line.find("LANCEMENT DE PDF.EXE") != std::string::npos)
                pdfPhase = true;
        }
        job.fresh.clear();

        if (job.running) return;

        // download.exe (et pdf.exe) termine
        writeRunLog(job.fullLog);

        missingChapters.clear();
        for (const auto& n : selectedNames)
            if (!pdfExistsForChapter(n)) missingChapters.push_back(n);

        // on verifie qu'il y a des PDF
        if (folderSize("scan") == 0)
        {
            fail("Aucun PDF n'a été créé. Consulte la dernière ligne du journal : " +
                 (job.log.empty() ? std::string("(vide)") : job.log.back()));
            return;
        }
        startCopy();
    }

    double downloadFraction() const
    {
        if (imgTot <= 0) return -1.0;
        double f = (std::max(0, chaptersSeen - 1) + double(imgCur) / imgTot) / chaptersTotal;
        return std::clamp(f, 0.0, 1.0);
    }

    void cancelDownload()
    {
        stopAll();
        clearCache();
        goHome("Téléchargement annulé, cache nettoyé.");
    }

    // -------------------------------------------------
    // COPIE VERS LA DESTINATION
    // -------------------------------------------------
    void startCopy()
    {
        screen = Screen::Copy;
        copyDone = 0; copyTotal = 0;
        copyFinished = false; copyFailed = false;
        copyError.clear();

        std::string dest = settings.destination;

        copyThread = std::thread([this, dest]()
        {
            try
            {
                fs::path src = "scan";
                fs::path dst = pathFromUtf8(dest);

                std::vector<fs::path> files;
                if (fs::exists(src))
                    for (auto& e : fs::recursive_directory_iterator(src))
                        if (e.is_regular_file()) files.push_back(e.path());

                copyTotal = static_cast<int>(files.size());

                for (auto& f : files)
                {
                    fs::path target = dst / fs::relative(f, src);
                    fs::create_directories(target.parent_path());
                    fs::copy_file(f, target, fs::copy_options::overwrite_existing);
                    ++copyDone;
                }

                std::error_code ec;
                fs::remove_all(src, ec);
                fs::remove_all("temp", ec);
            }
            catch (const std::exception& e)
            {
                copyError = e.what();
                copyFailed = true;
            }
            copyFinished = true;
        });
    }

    void updateCopy()
    {
        if (!copyFinished) return;
        if (copyThread.joinable()) copyThread.join();

        if (copyFailed) { fail("Copie impossible : " + copyError); return; }

        screen = Screen::Done;
        if (settings.openWhenDone) openDestination();
    }

    void openDestination()
    {
        ShellExecuteW(nullptr, L"open", toWide(settings.destination).c_str(),
                      nullptr, nullptr, SW_SHOWNORMAL);
    }

    // -------------------------------------------------
    void update()
    {
        switch (screen)
        {
            case Screen::Prepare:  updatePrepare();  break;
            case Screen::Scraping: updateScraping(); break;
            case Screen::Download: updateDownload(); break;
            case Screen::Copy:     updateCopy();     break;
            default: break;
        }
    }
};

// ==========================================================
// EVENEMENTS
// ==========================================================

void onClick(App& app, int x, int y, bool& running)
{
    // "Quitter" est partout
    if (inRect(x, y, R_QUIT)) { running = false; return; }

    switch (app.screen)
    {
    // ---------------------------------------------------
    case Screen::Home:
    {
        app.activeInput = inRect(x, y, R_DEST_BOX) ? &app.settings.destination : nullptr;

        if (inRect(x, y, R_DEST_BTN))
        {
            std::string p = browseForFolder();
            if (!p.empty()) { app.settings.destination = p; saveSettings(app.settings); }
        }

        if (inRect(x, y, R_OPEN_CHECK))
        {
            app.settings.openWhenDone = !app.settings.openWhenDone;
            saveSettings(app.settings);
        }

        if (inRect(x, y, R_BASE_BTN)) app.updateBase();

        if (inRect(x, y, R_CACHE_BTN))
        {
            Uint32 now = SDL_GetTicks();
            if (now < app.clearConfirmUntil)
            {
                clearCache();
                app.clearConfirmUntil = 0;
                app.cacheBytes = cacheSize();
                app.homeInfo = "Cache vidé.";
            }
            else
            {
                app.clearConfirmUntil = now + 3000; // 2e clic dans les 3 s = confirmation
            }
        }

        if (inRect(x, y, R_START_BTN) && !trim(app.settings.destination).empty())
            app.startSearchFlow();
        break;
    }

    // ---------------------------------------------------
    case Screen::Prepare:
        if (inRect(x, y, R_BACK)) app.goHome("Préparation annulée.");
        break;

    // ---------------------------------------------------
    case Screen::Search:
    {
        app.activeInput = inRect(x, y, R_SEARCH_BOX) ? &app.searchText : app.activeInput;
        if (inRect(x, y, R_MODE_ANIME)) {
            app.useAnimeSamaSearch = true;
            app.results.clear(); app.searched = false; app.showingScanChoices = false; app.searchScroll = 0;
        }
        if (inRect(x, y, R_MODE_SITEMAP)) {
            app.useAnimeSamaSearch = false;
            app.results.clear(); app.searched = false; app.showingScanChoices = false; app.searchScroll = 0;
            if (!app.heritageOk) { app.startPrepare(true); break; }
        }

        if (!inRect(x, y, R_SEARCH_BOX) && !inRect(x, y, R_SEARCH_LST)) { /* garde le focus */ }

        if (inRect(x, y, R_SEARCH_BTN)) app.doSearch();
        if (inRect(x, y, R_BACK)) { app.goHome(); break; }

        if (inRect(x, y, R_SEARCH_LST))
        {
            for (size_t i = 0; i < app.results.size(); ++i)
            {
                SDL_Rect row = { R_SEARCH_LST.x,
                                 R_SEARCH_LST.y + int(i) * SEARCH_ROW_H - app.searchScroll,
                                 R_SEARCH_LST.w - 14, 55 };
                if (inRect(x, y, row)) { app.chooseSearchResult(app.results[i]); break; }
            }
        }
        break;
    }

    // ---------------------------------------------------
    case Screen::Scraping:
        if (inRect(x, y, R_BACK))
        {
            app.job.kill();
            app.screen = Screen::Search;
            app.activeInput = &app.searchText;
        }
        break;

    // ---------------------------------------------------
    case Screen::Chapters:
    {
        if (inRect(x, y, R_BACK))
        {
            app.screen = Screen::Search;
            app.activeInput = &app.searchText;
            break;
        }
        if (inRect(x, y, R_SEL_ALL))  for (auto& c : app.chapters) c.selected = true;
        if (inRect(x, y, R_SEL_NONE)) for (auto& c : app.chapters) c.selected = false;

        if (inRect(x, y, R_PRIMARY) && app.selectedCount() > 0) { app.startDownload(); break; }

        if (inRect(x, y, R_CHAP_LST))
        {
            for (size_t i = 0; i < app.chapters.size(); ++i)
            {
                SDL_Rect row = { R_CHAP_LST.x,
                                 R_CHAP_LST.y + int(i) * CHAP_ROW_H - app.chapterScroll,
                                 R_CHAP_LST.w - 14, CHAP_ROW_H - 6 };
                if (inRect(x, y, row)) { app.chapters[i].selected = !app.chapters[i].selected; break; }
            }
        }
        break;
    }

    // ---------------------------------------------------
    case Screen::Download:
        if (inRect(x, y, R_BACK)) app.cancelDownload();
        break;

    case Screen::Copy:
        break;

    // ---------------------------------------------------
    case Screen::Done:
        if (inRect(x, y, R_PRIMARY)) app.openDestination();
        if (inRect(x, y, R_BACK))    app.goHome();
        break;

    // ---------------------------------------------------
    case Screen::Error:
        if (inRect(x, y, R_BACK)) app.goHome();
        break;
    }
}

// ==========================================================
// RENDU
// ==========================================================

void renderHeader(const std::string& subtitle)
{
    drawText(gTitle, "Manga Downloader", 40, 25, TEXT_COLOR);
    drawText(gFont, APP_VERSION, 330, 36, MUTED_COLOR);
    if (!subtitle.empty()) drawText(gFont, subtitle, 40, 68, MUTED_COLOR);
}

void renderApp(App& app, int mx, int my, Uint32 ticks)
{
    setColor(BG_COLOR);
    SDL_RenderClear(gR);

    switch (app.screen)
    {
    // ================= ACCUEIL =================
    case Screen::Home:
    {
        renderHeader("Accueil");

        drawText(gFont, "Dossier de destination", 40, 115, TEXT_COLOR);
        drawPanel(R_DEST_BOX, INPUT_COLOR);
        drawText(gFont,
            fitTail(app.settings.destination, R_DEST_BOX.w - 30) +
                ((app.activeInput == &app.settings.destination && (ticks / 500) % 2) ? "|" : ""),
            R_DEST_BOX.x + 15, R_DEST_BOX.y + 14, TEXT_COLOR);
        drawButton(R_DEST_BTN, "Changer...", mx, my);

        drawPanel(R_OPEN_CHECK, CHECKBOX_COLOR);
        drawBorder(R_OPEN_CHECK, BORDER_COLOR);
        if (app.settings.openWhenDone)
        {
            SDL_Rect in = { R_OPEN_CHECK.x + 6, R_OPEN_CHECK.y + 6, 18, 18 };
            drawPanel(in, FINISHED_COLOR);
        }
        drawText(gFont, "Ouvrir le dossier à la fin du téléchargement", 85, 228, TEXT_COLOR);

        // ---- base de recherche ----
        drawText(gFont, "Base de recherche", 40, 295, TEXT_COLOR);
        if (app.heritageOk)
            drawText(gFont, "Prête  (" + std::to_string(app.heritageCount) + " mangas)", 250, 295, FINISHED_COLOR);
        else
            drawText(gFont, "Non générée (créée au premier lancement)", 250, 295, SEARCHING_COLOR);
        drawButton(R_BASE_BTN, "Mettre à jour la base", mx, my);

        // ---- cache ----
        drawText(gFont, "Cache", 40, 405, TEXT_COLOR);
        drawText(gFont, formatBytes(app.cacheBytes), 250, 405, MUTED_COLOR);
        bool confirming = SDL_GetTicks() < app.clearConfirmUntil;
        drawButton(R_CACHE_BTN, confirming ? "Cliquer pour confirmer" : "Vider le cache",
                   mx, my, true, true);

        if (!app.homeInfo.empty())
            drawText(gFont, app.homeInfo, 40, 520, FINISHED_COLOR);

        drawButton(R_START_BTN, "Rechercher un manga", mx, my, !trim(app.settings.destination).empty());
        break;
    }

    // ================= PREPARATION =================
    case Screen::Prepare:
    {
        renderHeader("Préparation  -  étape " + std::to_string(app.prepIdx + 1) + " / 2");

        drawText(gFont, "Progression globale", 40, 195, MUTED_COLOR);
        drawProgress(R_BAR_A, app.prepIdx / 2.0, BUTTON_COLOR, ticks);

        drawText(gFont, app.prepLabels[std::min(app.prepIdx, 1)], 40, 285, SEARCHING_COLOR);
        drawProgress(R_BAR_B, -1.0, SEARCHING_COLOR, ticks);

        drawLog(app.job, 40, 400, 920, 6);
        drawButton(R_BACK, "Annuler", mx, my, true, true);
        break;
    }

    // ================= RECHERCHE =================
    case Screen::Search:
    {
        renderHeader(".");

        // Deux options exclusives : recherche directe Anime-Sama ou base issue du sitemap.
        auto drawMode = [&](const SDL_Rect& rect, const std::string& label, bool selected) {
            drawPanel(rect, selected ? BUTTON_COLOR : PANEL_COLOR);
            drawBorder(rect, selected ? HOVER_COLOR : BORDER_COLOR);
            SDL_Rect check = { rect.x + 8, rect.y + 6, 18, 18 };
            drawPanel(check, INPUT_COLOR);
            drawBorder(check, BORDER_COLOR);
            if (selected) { SDL_Rect dot = {check.x + 4, check.y + 4, 10, 10}; drawPanel(dot, FINISHED_COLOR); }
            drawText(gFont, label, rect.x + 34, rect.y + 6, TEXT_COLOR);
        };
        drawMode(R_MODE_ANIME, "Anime-Sama (direct)", app.useAnimeSamaSearch);
        drawMode(R_MODE_SITEMAP, "Sitemap (base locale)", !app.useAnimeSamaSearch);

        drawPanel(R_SEARCH_BOX, INPUT_COLOR);
        bool focus = app.activeInput == &app.searchText;
        if (app.searchText.empty() && !focus)
            drawText(gFont, "Entrez le nom d'un manga...", R_SEARCH_BOX.x + 15, R_SEARCH_BOX.y + 14, MUTED_COLOR);
        else
            drawText(gFont,
                fitTail(app.searchText, R_SEARCH_BOX.w - 30) + ((focus && (ticks / 500) % 2) ? "|" : ""),
                R_SEARCH_BOX.x + 15, R_SEARCH_BOX.y + 14, TEXT_COLOR);

        drawButton(R_SEARCH_BTN, "Rechercher", mx, my);

        if (app.results.empty())
        {
            if (app.searched) {
                const std::string message = app.useAnimeSamaSearch && app.showingScanChoices
                    ? "Aucun scan trouve pour cet anime. Essaie l'autre source."
                    : "Aucun resultat. Verifie la source ou essaie l'autre methode.";
                drawText(gFont, message, 40, 205, MUTED_COLOR);
            }
        }
        else
        {
            std::string label = app.showingScanChoices
                ? "Scans disponibles pour : " + app.selName
                : std::to_string(app.results.size()) + " resultat(s)";
            drawText(gFont, label, 40, 205, MUTED_COLOR);

            int contentH = int(app.results.size()) * SEARCH_ROW_H;
            app.searchScroll = clampScroll(app.searchScroll, contentH, R_SEARCH_LST.h);

            SDL_RenderSetClipRect(gR, &R_SEARCH_LST);
            for (size_t i = 0; i < app.results.size(); ++i)
            {
                SDL_Rect row = { R_SEARCH_LST.x,
                                 R_SEARCH_LST.y + int(i) * SEARCH_ROW_H - app.searchScroll,
                                 R_SEARCH_LST.w - 14, 55 };
                if (row.y + row.h < R_SEARCH_LST.y || row.y > R_SEARCH_LST.y + R_SEARCH_LST.h) continue;

                bool hover = inRect(mx, my, row) && inRect(mx, my, R_SEARCH_LST);
                drawPanel(row, hover ? HOVER_COLOR : PANEL_COLOR);
                drawText(gFont, fitHead(app.results[i].identifier, row.w - 30), row.x + 15, row.y + 6, TEXT_COLOR);
                drawText(gFont, fitHead(app.results[i].url, row.w - 30), row.x + 15, row.y + 30, MUTED_COLOR);
            }
            SDL_RenderSetClipRect(gR, nullptr);
            drawScrollbar(R_SEARCH_LST, contentH, app.searchScroll);
        }

        drawButton(R_BACK, "Accueil", mx, my);
        break;
    }

    // ================= SCRAPING =================
    case Screen::Scraping:
    {
        renderHeader("Récupération des chapitres");

        drawText(gFont, fitHead(app.selName, 900), 40, 190, TEXT_COLOR);
        drawText(gFont, "Recherche en cours...", 40, 230 - 5, SEARCHING_COLOR);
        drawProgress(R_BAR_B, -1.0, SEARCHING_COLOR, ticks);

        drawLog(app.job, 40, 400, 920, 6);
        drawButton(R_BACK, "Annuler", mx, my, true, true);
        break;
    }

    // ================= CHAPITRES =================
    case Screen::Chapters:
    {
        renderHeader("");
        drawText(gFont, fitHead(app.mangaTitle, 600), 360, 36, TEXT_COLOR);

        drawButton(R_SEL_ALL,  "Tout sélectionner",   mx, my);
        drawButton(R_SEL_NONE, "Tout désélectionner", mx, my);
        drawText(gFont, std::to_string(app.selectedCount()) + " / " +
                        std::to_string(app.chapters.size()) + " sélectionné(s)",
                 560, 108, MUTED_COLOR);

        int contentH = int(app.chapters.size()) * CHAP_ROW_H;
        app.chapterScroll = clampScroll(app.chapterScroll, contentH, R_CHAP_LST.h);

        SDL_RenderSetClipRect(gR, &R_CHAP_LST);
        for (size_t i = 0; i < app.chapters.size(); ++i)
        {
            SDL_Rect row = { R_CHAP_LST.x,
                             R_CHAP_LST.y + int(i) * CHAP_ROW_H - app.chapterScroll,
                             R_CHAP_LST.w - 14, CHAP_ROW_H - 6 };
            if (row.y + row.h < R_CHAP_LST.y || row.y > R_CHAP_LST.y + R_CHAP_LST.h) continue;

            const Chapter& c = app.chapters[i];
            bool hover = inRect(mx, my, row) && inRect(mx, my, R_CHAP_LST);
            drawPanel(row, hover ? INPUT_COLOR : PANEL_COLOR);

            SDL_Rect box = { row.x + 12, row.y + 8, 24, 24 };
            drawPanel(box, CHECKBOX_COLOR);
            drawBorder(box, BORDER_COLOR);
            if (c.selected)
            {
                SDL_Rect in = { box.x + 5, box.y + 5, 14, 14 };
                drawPanel(in, FINISHED_COLOR);
            }

            drawText(gFont, "Chapitre " + c.name, row.x + 52, row.y + 8, TEXT_COLOR);
            std::string imgs = std::to_string(c.images) + " image(s)";
            drawText(gFont, imgs, row.x + row.w - textWidth(imgs) - 15, row.y + 8, MUTED_COLOR);
        }
        SDL_RenderSetClipRect(gR, nullptr);
        drawScrollbar(R_CHAP_LST, contentH, app.chapterScroll);

        drawButton(R_BACK, "Retour", mx, my);
        drawButton(R_PRIMARY, "Télécharger", mx, my, app.selectedCount() > 0);
        break;
    }

    // ================= TELECHARGEMENT =================
    case Screen::Download:
    {
        renderHeader(app.pdfPhase ? "Création des PDF" : "Téléchargement");
        drawText(gFont, fitHead(app.mangaTitle, 900), 40, 150, TEXT_COLOR);

        if (!app.pdfPhase)
        {
            drawText(gFont, "Chapitre " + std::to_string(std::max(1, app.chaptersSeen)) + " / " +
                            std::to_string(app.chaptersTotal), 40, 195, SEARCHING_COLOR);
            drawProgress(R_BAR_A, app.downloadFraction(), SEARCHING_COLOR, ticks);

            drawText(gFont, app.imgTot > 0
                ? "Image " + std::to_string(app.imgCur) + " / " + std::to_string(app.imgTot)
                : std::string("Démarrage..."), 40, 285, MUTED_COLOR);
            drawProgress(R_BAR_B,
                app.imgTot > 0 ? double(app.imgCur) / app.imgTot : -1.0, BUTTON_COLOR, ticks);
        }
        else
        {
            drawText(gFont, "Conversion des images en PDF...", 40, 195, SEARCHING_COLOR);
            drawProgress(R_BAR_A, -1.0, SEARCHING_COLOR, ticks);
        }

        drawLog(app.job, 40, 400, 920, 6);
        drawButton(R_BACK, "Annuler", mx, my, true, true);
        break;
    }

    // ================= COPIE =================
    case Screen::Copy:
    {
        renderHeader("Copie vers le dossier de destination");
        drawText(gFont, fitTail(app.settings.destination, 900), 40, 150, MUTED_COLOR);

        int total = app.copyTotal;
        drawText(gFont, "Copie des fichiers...  " + std::to_string(app.copyDone.load()) + " / " +
                        std::to_string(total), 40, 195, SEARCHING_COLOR);
        drawProgress(R_BAR_A, total > 0 ? double(app.copyDone) / total : -1.0, SEARCHING_COLOR, ticks);
        break;
    }

    // ================= TERMINE =================
    case Screen::Done:
    {
        bool warn = !app.missingChapters.empty();
        renderHeader(warn ? "Terminé avec avertissements" : "Terminé");
        drawProgress(R_BAR_A, 1.0, warn ? SEARCHING_COLOR : FINISHED_COLOR, ticks);
        drawText(gFont, "PDF créés : " + std::to_string(app.copyTotal.load()) + " fichier(s)",
                 40, 290, warn ? TEXT_COLOR : FINISHED_COLOR);
        drawText(gFont, fitTail(app.settings.destination, 900), 40, 330, MUTED_COLOR);

        if (warn)
        {
            std::string list;
            for (size_t i = 0; i < app.missingChapters.size(); ++i)
            {
                list += app.missingChapters[i];
                if (i + 1 < app.missingChapters.size()) list += ", ";
            }
            drawText(gFont, std::to_string(app.missingChapters.size()) + " chapitre(s) non créé(s) :",
                     40, 390, SEARCHING_COLOR);
            drawText(gFont, fitHead(list, 900), 40, 425, TEXT_COLOR);
            drawText(gFont, "Détails dans le fichier last_run.log (dossier de l'application).",
                     40, 470, MUTED_COLOR);
        }

        drawButton(R_PRIMARY, "Ouvrir le dossier", mx, my);
        drawButton(R_BACK, "Accueil", mx, my);
        break;
    }

    // ================= ERREUR =================
    case Screen::Error:
    {
        renderHeader("Erreur");
        drawText(gFont, "Une erreur est survenue", 40, 190, SEARCHING_COLOR);

        // message sur plusieurs lignes
        std::string msg = app.errorMsg;
        int y = 235;
        while (!msg.empty() && y < 560)
        {
            std::string line = msg;
            while (textWidth(line) > 900 && line.size() > 1) eraseLastChar(line);
            // coupe au dernier espace si possible
            if (line.size() < msg.size())
            {
                size_t sp = line.find_last_of(' ');
                if (sp != std::string::npos && sp > 0) line = line.substr(0, sp);
            }
            drawText(gFont, line, 40, y, TEXT_COLOR);
            msg.erase(0, line.size());
            msg = trim(msg);
            y += 28;
        }

        drawButton(R_BACK, "Accueil", mx, my);
        break;
    }
    }

    drawButton(R_QUIT, "Quitter", mx, my);
    SDL_RenderPresent(gR);
}

// ==========================================================
// MAIN
// ==========================================================

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    // Les workers cherchent "data/..." : on se place dans le dossier de l'exe
    {
        wchar_t exe[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        SetCurrentDirectoryW(fs::path(exe).parent_path().c_str());
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    if (TTF_Init() != 0) { SDL_Quit(); return 1; }

    gFont = TTF_OpenFont(FONT_PATH, 20);
    if (!gFont)
    {
        MessageBoxA(nullptr, "Police introuvable : data/font/yoruka.ttf", "Erreur", MB_ICONERROR);
        TTF_Quit(); SDL_Quit();
        return 1;
    }
    gTitle = TTF_OpenFont(FONT_PATH, 28);
    if (!gTitle) gTitle = gFont;

    SDL_Window* window = SDL_CreateWindow("Manga Downloader",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);

    gR = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!gR) gR = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);

    App app;
    app.settings = loadSettings();
    app.refreshHome();

    SDL_StartTextInput();

    bool running = true;

    while (running)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            switch (e.type)
            {
            case SDL_QUIT:
                running = false;
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (e.button.button == SDL_BUTTON_LEFT)
                    onClick(app, e.button.x, e.button.y, running);
                break;

            case SDL_MOUSEWHEEL:
                if (app.screen == Screen::Search)   app.searchScroll  -= e.wheel.y * 40;
                if (app.screen == Screen::Chapters) app.chapterScroll -= e.wheel.y * 48;
                break;

            case SDL_KEYDOWN:
            {
                SDL_Keycode k = e.key.keysym.sym;

                if (app.activeInput)
                {
                    if (k == SDLK_BACKSPACE) eraseLastChar(*app.activeInput);

                    if (k == SDLK_v && (SDL_GetModState() & KMOD_CTRL))
                    {
                        char* clip = SDL_GetClipboardText();
                        if (clip)
                        {
                            std::string c = clip;
                            c.erase(std::remove(c.begin(), c.end(), '\n'), c.end());
                            c.erase(std::remove(c.begin(), c.end(), '\r'), c.end());
                            *app.activeInput += c;
                            SDL_free(clip);
                        }
                    }
                }

                if (k == SDLK_RETURN || k == SDLK_KP_ENTER)
                {
                    if (app.screen == Screen::Search) app.doSearch();
                    else if (app.screen == Screen::Home && !trim(app.settings.destination).empty())
                    {
                        if (app.activeInput == &app.settings.destination) saveSettings(app.settings);
                    }
                }

                if (k == SDLK_ESCAPE) app.activeInput = nullptr;
                break;
            }

            case SDL_TEXTINPUT:
                if (app.activeInput) *app.activeInput += e.text.text;
                break;
            }
        }

        app.update();

        int mx, my;
        SDL_GetMouseState(&mx, &my);
        renderApp(app, mx, my, SDL_GetTicks());
    }

    // ---- nettoyage ----
    app.stopAll();
    saveSettings(app.settings);

    SDL_StopTextInput();
    SDL_DestroyRenderer(gR);
    SDL_DestroyWindow(window);
    if (gTitle != gFont) TTF_CloseFont(gTitle);
    TTF_CloseFont(gFont);
    TTF_Quit();
    SDL_Quit();
    CoUninitialize();
    return 0;
}
