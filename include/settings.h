// ==========================================================
// settings.h  -  Parametres de l'utilisateur (settings.json)
// ==========================================================
#pragma once

#include <string>

struct Settings
{
    std::string destination;          // UTF-8
    bool openWhenDone = true;
};

Settings loadSettings();
void saveSettings(const Settings& s);
