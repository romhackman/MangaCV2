// ==========================================================
// http.h  -  Requetes HTTP (libcurl)
//
// Une Session reutilise la meme connexion pour plusieurs requetes
// (beaucoup plus rapide pour telecharger des dizaines d'images).
// Une Session ne se partage pas entre threads.
// ==========================================================
#pragma once

#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

#include <curl/curl.h>

namespace http
{
    namespace fs = std::filesystem;

    struct Request
    {
        std::string url;
        bool post = false;
        std::string postData;
        std::vector<std::string> headers;          // "Nom: valeur"
        std::string userAgent;                     // vide = navigateur par defaut
        long connectTimeout = 15;
        long timeout = 30;
        bool requireSuccess = true;                // false : ne pas echouer sur un code HTTP != 2xx
    };

    struct Response
    {
        bool ok = false;
        long code = 0;
        std::string body;
        std::string finalUrl;                      // apres redirections
        std::string error;
    };

    void globalInit();
    void globalCleanup();

    const char* browserUserAgent();
    std::string urlEncode(const std::string& text);

    class Session
    {
    public:
        Session();
        ~Session();
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;

        // "cancel" : si passe a true, la requete en cours est interrompue
        Response get(const Request& request, const std::atomic<bool>* cancel = nullptr);
        Response download(const std::string& url, const fs::path& output,
                          const std::atomic<bool>* cancel = nullptr);

    private:
        CURL* handle_ = nullptr;
    };
}
