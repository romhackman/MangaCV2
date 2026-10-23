// ==========================================================
// utils.h  -  Texte, noms de fichiers, petit parseur HTML
// (aucune dependance a Windows ni a SDL)
// ==========================================================
#pragma once

#include <cstdint>
#include <string>

namespace utils
{
    std::string trim(std::string v);
    std::string toLower(std::string v);
    void        eraseLastChar(std::string& s);      // respecte l'UTF-8
    std::string formatBytes(std::uintmax_t bytes);

    // Remplace les caracteres interdits sous Windows ; "fallback" si vide
    std::string sanitizeName(std::string name, const std::string& fallback);

    // Tri "naturel" : Chapter 2 < Chapter 10 < Chapter 10.5
    bool naturalLess(const std::string& a, const std::string& b);

    std::string percentEncode(const std::string& s);

    // ---- HTML ----
    std::string decodeHtml(std::string value);
    std::string stripTags(const std::string& value);                 // sans balises, decode, trim
    std::string attrValue(const std::string& tag, const std::string& attr);
    bool        hasClass(const std::string& classAttr, const std::string& token);
    std::string extractBetween(const std::string& text, const std::string& startToken,
                               const std::string& endToken, size_t from = 0);
}
