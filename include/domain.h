// ==========================================================
// domain.h  -  Domaine courant d'Anime-Sama (ancien domaine.exe)
// ==========================================================
#pragma once

#include <string>

#include "task.h"

namespace domain
{
    // Contacte la page d'entree, suit le bouton principal et memorise
    // l'extension trouvee (ex. "to") dans data/domaine.json.
    bool resolve(TaskContext& ctx, std::string& extension, std::string& error);

    // Lit data/domaine.json -> "https://anime-sama.<ext>"
    bool loadBaseUrl(std::string& baseUrl, std::string& error);

    // Idem, mais retombe sur le domaine par defaut si le fichier est absent
    std::string baseUrl();
}
