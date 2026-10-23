#include "settings.h"

#include <fstream>

#include <nlohmann/json.hpp>

#include "config.h"
#include "platform.h"

using json = nlohmann::json;

Settings loadSettings()
{
    Settings s;
    s.destination = platform::defaultDestination();

    std::ifstream f(cfg::SETTINGS_FILE);
    if (!f) return s;

    try
    {
        json j;
        f >> j;
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
    j["destination"] = s.destination;
    j["openWhenDone"] = s.openWhenDone;

    std::ofstream f(cfg::SETTINGS_FILE, std::ios::trunc);
    if (f) f << j.dump(4);
}
