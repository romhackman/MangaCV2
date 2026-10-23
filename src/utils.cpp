#include "utils.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <utility>

namespace utils
{
    static unsigned char uc(char c) { return static_cast<unsigned char>(c); }

    std::string trim(std::string v)
    {
        auto notSpace = [](unsigned char c) { return !std::isspace(c); };
        v.erase(v.begin(), std::find_if(v.begin(), v.end(), notSpace));
        v.erase(std::find_if(v.rbegin(), v.rend(), notSpace).base(), v.end());
        return v;
    }

    std::string toLower(std::string v)
    {
        for (char& c : v) c = static_cast<char>(std::tolower(uc(c)));
        return v;
    }

    void eraseLastChar(std::string& s)
    {
        if (s.empty()) return;
        size_t i = s.size() - 1;
        while (i > 0 && (uc(s[i]) & 0xC0) == 0x80) --i;
        s.erase(i);
    }

    std::string formatBytes(std::uintmax_t b)
    {
        char buf[64];
        if (b < 1024)
            std::snprintf(buf, sizeof(buf), "%llu o", static_cast<unsigned long long>(b));
        else if (b < 1024ull * 1024)
            std::snprintf(buf, sizeof(buf), "%.1f Ko", b / 1024.0);
        else if (b < 1024ull * 1024 * 1024)
            std::snprintf(buf, sizeof(buf), "%.1f Mo", b / (1024.0 * 1024.0));
        else
            std::snprintf(buf, sizeof(buf), "%.2f Go", b / (1024.0 * 1024.0 * 1024.0));
        return buf;
    }

    std::string sanitizeName(std::string name, const std::string& fallback)
    {
        const std::string invalid = "<>:\"/\\|?*";
        for (char& c : name)
            if (invalid.find(c) != std::string::npos || uc(c) < 32) c = '_';

        while (!name.empty() && (name.back() == ' ' || name.back() == '.')) name.pop_back();
        name = trim(name);
        return name.empty() ? fallback : name;
    }

    // Premier nombre (avec decimales) d'une chaine
    static bool firstNumber(const std::string& s, double& out)
    {
        size_t i = 0;
        while (i < s.size() && !std::isdigit(uc(s[i]))) ++i;
        if (i == s.size()) return false;

        size_t j = i;
        while (j < s.size() && std::isdigit(uc(s[j]))) ++j;
        if (j + 1 < s.size() && s[j] == '.' && std::isdigit(uc(s[j + 1])))
        {
            ++j;
            while (j < s.size() && std::isdigit(uc(s[j]))) ++j;
        }
        try { out = std::stod(s.substr(i, j - i)); return true; }
        catch (...) { return false; }
    }

    bool naturalLess(const std::string& a, const std::string& b)
    {
        double x = 0, y = 0;
        if (firstNumber(a, x) && firstNumber(b, y) && x != y) return x < y;
        return toLower(a) < toLower(b);
    }

    std::string percentEncode(const std::string& s)
    {
        std::string out;
        char buf[4];
        for (char c : s)
        {
            if (std::isalnum(uc(c)) || c == '-' || c == '_' || c == '.' || c == '~')
                out += c;
            else
            {
                std::snprintf(buf, sizeof(buf), "%%%02X", uc(c));
                out += buf;
            }
        }
        return out;
    }

    // ---------------- HTML ----------------

    std::string decodeHtml(std::string value)
    {
        // &amp; en dernier pour ne pas decoder deux fois
        const std::pair<const char*, const char*> entities[] = {
            {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&#39;", "'"},
            {"&apos;", "'"}, {"&nbsp;", " "}, {"&amp;", "&"}
        };
        for (const auto& e : entities)
        {
            const std::string from = e.first, to = e.second;
            size_t pos = 0;
            while ((pos = value.find(from, pos)) != std::string::npos)
            {
                value.replace(pos, from.size(), to);
                pos += to.size();
            }
        }
        return value;
    }

    std::string stripTags(const std::string& value)
    {
        std::string out;
        bool inside = false;
        for (char c : value)
        {
            if (c == '<') { inside = true; continue; }
            if (c == '>') { inside = false; continue; }
            if (!inside) out += c;
        }
        return trim(decodeHtml(out));
    }

    std::string attrValue(const std::string& tag, const std::string& attr)
    {
        size_t pos = 0;
        while ((pos = tag.find(attr, pos)) != std::string::npos)
        {
            const bool startOk = pos > 0 && std::isspace(uc(tag[pos - 1]));
            size_t p = pos + attr.size();
            while (p < tag.size() && std::isspace(uc(tag[p]))) ++p;

            if (startOk && p < tag.size() && tag[p] == '=')
            {
                ++p;
                while (p < tag.size() && std::isspace(uc(tag[p]))) ++p;
                if (p < tag.size() && (tag[p] == '"' || tag[p] == '\''))
                {
                    const char quote = tag[p++];
                    size_t end = tag.find(quote, p);
                    if (end != std::string::npos) return tag.substr(p, end - p);
                }
                return "";
            }
            pos += attr.size();
        }
        return "";
    }

    bool hasClass(const std::string& classAttr, const std::string& token)
    {
        size_t i = 0;
        while (i < classAttr.size())
        {
            while (i < classAttr.size() && std::isspace(uc(classAttr[i]))) ++i;
            size_t j = i;
            while (j < classAttr.size() && !std::isspace(uc(classAttr[j]))) ++j;
            if (j > i && j - i == token.size() && classAttr.compare(i, j - i, token) == 0)
                return true;
            i = j;
        }
        return false;
    }

    std::string extractBetween(const std::string& text, const std::string& startToken,
                               const std::string& endToken, size_t from)
    {
        size_t start = text.find(startToken, from);
        if (start == std::string::npos) return "";
        start += startToken.size();
        size_t end = text.find(endToken, start);
        if (end == std::string::npos) return "";
        return text.substr(start, end - start);
    }
}
