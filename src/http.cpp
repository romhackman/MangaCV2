#include "http.h"

#include <fstream>

#include "platform.h"
#include "utils.h"

namespace http
{
    namespace
    {
        // cacert.pem facultatif, a cote de l'exe (ou dans le dossier courant)
        std::string caBundle()
        {
            static const std::string path = []() -> std::string
            {
                std::error_code ec;
                fs::path a = platform::exeDir() / "cacert.pem";
                if (fs::exists(a, ec)) return platform::pathToUtf8(a);
                fs::path b = fs::current_path(ec) / "cacert.pem";
                if (fs::exists(b, ec)) return platform::pathToUtf8(b);
                return "";
            }();
            return path;
        }

        int onProgress(void* p, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
        {
            auto* flag = static_cast<const std::atomic<bool>*>(p);
            return (flag && flag->load()) ? 1 : 0;      // 1 = abandonner
        }

        size_t writeString(char* data, size_t size, size_t count, void* user)
        {
            static_cast<std::string*>(user)->append(data, size * count);
            return size * count;
        }

        size_t writeFile(char* data, size_t size, size_t count, void* user)
        {
            auto* out = static_cast<std::ofstream*>(user);
            out->write(data, static_cast<std::streamsize>(size * count));
            return out->good() ? size * count : 0;
        }
    }

    void globalInit()    { curl_global_init(CURL_GLOBAL_DEFAULT); }
    void globalCleanup() { curl_global_cleanup(); }

    const char* browserUserAgent()
    {
        return "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
               "(KHTML, like Gecko) Chrome/139.0.0.0 Safari/537.36";
    }

    std::string urlEncode(const std::string& text)
    {
        return utils::percentEncode(text);
    }

    Session::Session()  { handle_ = curl_easy_init(); }
    Session::~Session() { if (handle_) curl_easy_cleanup(handle_); }

    // Prepare puis execute la requete ; le corps part dans (writer, userData)
    static Response run(CURL* h, const Request& req, curl_write_callback writer, void* userData,
                        const std::atomic<bool>* cancel)
    {
        Response res;
        if (!h) { res.error = "CURL non initialise."; return res; }

        curl_easy_reset(h);
        curl_easy_setopt(h, CURLOPT_URL, req.url.c_str());
        curl_easy_setopt(h, CURLOPT_USERAGENT,
                         req.userAgent.empty() ? browserUserAgent() : req.userAgent.c_str());
        curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(h, CURLOPT_MAXREDIRS, 10L);
        curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT, req.connectTimeout);
        curl_easy_setopt(h, CURLOPT_TIMEOUT, req.timeout);
        curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(h, CURLOPT_ACCEPT_ENCODING, "");
        curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);              // obligatoire avec des threads
        curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, writer);
        curl_easy_setopt(h, CURLOPT_WRITEDATA, userData);

        const std::string ca = caBundle();
        if (!ca.empty()) curl_easy_setopt(h, CURLOPT_CAINFO, ca.c_str());

        curl_easy_setopt(h, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(h, CURLOPT_XFERINFOFUNCTION, onProgress);
        curl_easy_setopt(h, CURLOPT_XFERINFODATA, const_cast<std::atomic<bool>*>(cancel));

        curl_slist* headers = nullptr;
        for (const auto& line : req.headers) headers = curl_slist_append(headers, line.c_str());
        if (headers) curl_easy_setopt(h, CURLOPT_HTTPHEADER, headers);

        if (req.post)
        {
            curl_easy_setopt(h, CURLOPT_POST, 1L);
            curl_easy_setopt(h, CURLOPT_POSTFIELDSIZE, static_cast<long>(req.postData.size()));
            curl_easy_setopt(h, CURLOPT_COPYPOSTFIELDS, req.postData.c_str());
        }

        CURLcode rc = curl_easy_perform(h);

        curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &res.code);
        char* eff = nullptr;
        if (curl_easy_getinfo(h, CURLINFO_EFFECTIVE_URL, &eff) == CURLE_OK && eff) res.finalUrl = eff;
        if (headers) curl_slist_free_all(headers);

        if (rc != CURLE_OK)
        {
            res.error = (rc == CURLE_ABORTED_BY_CALLBACK) ? "Annule" : curl_easy_strerror(rc);
            return res;
        }

        res.ok = !req.requireSuccess || (res.code >= 200 && res.code < 300);
        if (!res.ok) res.error = "HTTP " + std::to_string(res.code);
        return res;
    }

    Response Session::get(const Request& request, const std::atomic<bool>* cancel)
    {
        std::string body;
        Response res = run(handle_, request, writeString, &body, cancel);
        res.body = std::move(body);
        return res;
    }

    Response Session::download(const std::string& url, const fs::path& output,
                               const std::atomic<bool>* cancel)
    {
        std::ofstream out(output, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            Response r;
            r.error = "Impossible de creer " + platform::pathToUtf8(output);
            return r;
        }

        Request req;
        req.url = url;
        req.timeout = 30;
        Response res = run(handle_, req, writeFile, &out, cancel);
        out.close();

        if (!res.ok)
        {
            std::error_code ec;
            fs::remove(output, ec);
        }
        return res;
    }
}
