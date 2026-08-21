#define SDL_MAIN_HANDLED

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

using json = nlohmann::json;

// ==========================================================
// CONFIGURATION
// ==========================================================

const std::string HERITAGE_FILE = "data/heritage.json";
const std::string STATUS_FILE   = "data/status.json";

const int WINDOW_WIDTH  = 1000;
const int WINDOW_HEIGHT = 700;

// ==========================================================
// COULEURS
// ==========================================================

const SDL_Color BG_COLOR =
{
    18, 18, 24, 255
};

const SDL_Color PANEL_COLOR =
{
    28, 28, 38, 255
};

const SDL_Color INPUT_COLOR =
{
    40, 40, 52, 255
};

const SDL_Color BUTTON_COLOR =
{
    70, 90, 180, 255
};

const SDL_Color HOVER_COLOR =
{
    90, 115, 220, 255
};

const SDL_Color TEXT_COLOR =
{
    235, 235, 240, 255
};

const SDL_Color MUTED_COLOR =
{
    150, 150, 160, 255
};

// ROUGE = recherche en cours
const SDL_Color SEARCHING_COLOR =
{
    190, 55, 55, 255
};

// VERT = recherche terminée
const SDL_Color FINISHED_COLOR =
{
    50, 180, 100, 255
};

// ==========================================================
// STRUCTURE
// ==========================================================

struct SearchResult
{
    std::string identifier;
    std::string url;
};

// ==========================================================
// TRIM
// ==========================================================

std::string trim(std::string value)
{
    auto notSpace =
        [](unsigned char c)
        {
            return !std::isspace(c);
        };

    value.erase(
        value.begin(),
        std::find_if(
            value.begin(),
            value.end(),
            notSpace
        )
    );

    value.erase(
        std::find_if(
            value.rbegin(),
            value.rend(),
            notSpace
        ).base(),
        value.end()
    );

    return value;
}

// ==========================================================
// LOWERCASE
// ==========================================================

std::string to_lower(std::string value)
{
    for (char& c : value)
    {
        c = static_cast<char>(
            std::tolower(
                static_cast<unsigned char>(c)
            )
        );
    }

    return value;
}

// ==========================================================
// CREATION STATUS.JSON
// ==========================================================

void reset_status()
{
    std::ofstream file(
        STATUS_FILE,
        std::ios::trunc
    );

    if (!file)
    {
        std::cerr
            << "[ERROR] Impossible de créer : "
            << STATUS_FILE
            << "\n";

        return;
    }

    json status;

    status["status"] = false;

    file << status.dump(4);

    file.close();
}

// ==========================================================
// LECTURE STATUS.JSON
// ==========================================================

bool read_status()
{
    std::ifstream file(
        STATUS_FILE
    );

    if (!file)
    {
        return false;
    }

    try
    {
        json status;

        file >> status;

        if (!status.is_object())
        {
            return false;
        }

        if (!status.contains("status"))
        {
            return false;
        }

        if (!status["status"].is_boolean())
        {
            return false;
        }

        return status["status"].get<bool>();
    }
    catch (...)
    {
        /*
         * chapter.exe peut être en train d'écrire
         * le fichier.
         */
        return false;
    }
}

// ==========================================================
// LANCER CHAPTER.EXE
// ==========================================================

bool launch_chapter(
    const std::string& url
)
{
#ifdef _WIN32

    if (url.empty())
    {
        std::cerr
            << "[ERROR] URL vide.\n";

        return false;
    }

    /*
     * Commande finale :
     *
     * chapter.exe -link "URL"
     */

    std::string command =
        ".\\scraper.exe -link \"" +
        url +
        "\"";

    STARTUPINFOA si{};

    PROCESS_INFORMATION pi{};

    si.cb = sizeof(si);

    /*
     * CreateProcess peut modifier
     * le buffer.
     */

    std::vector<char> commandBuffer(
        command.begin(),
        command.end()
    );

    commandBuffer.push_back('\0');

    BOOL success =
        CreateProcessA(
            nullptr,
            commandBuffer.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            nullptr,
            &si,
            &pi
        );

    if (!success)
    {
        std::cerr
            << "[ERROR] Impossible de lancer "
            << "chapter.exe.\n"
            << "Erreur Windows : "
            << GetLastError()
            << "\n";

        return false;
    }

    CloseHandle(
        pi.hThread
    );

    CloseHandle(
        pi.hProcess
    );

    return true;

#else

    std::cerr
        << "[ERROR] Ce programme est prévu "
        << "pour Windows.\n";

    return false;

#endif
}

// ==========================================================
// CHARGEMENT HERITAGE.JSON
// ==========================================================

json load_heritage(
    const std::string& filename
)
{
    std::ifstream file(
        filename
    );

    if (!file)
    {
        throw std::runtime_error(
            "Impossible d'ouvrir " +
            filename
        );
    }

    json data;

    try
    {
        file >> data;
    }
    catch (
        const json::parse_error& e
    )
    {
        throw std::runtime_error(
            std::string(
                "JSON invalide : "
            ) +
            e.what()
        );
    }

    if (!data.is_object())
    {
        throw std::runtime_error(
            "Le JSON doit être un objet."
        );
    }

    if (!data.contains("scan"))
    {
        throw std::runtime_error(
            "La clé 'scan' est absente."
        );
    }

    if (!data["scan"].is_object())
    {
        throw std::runtime_error(
            "La clé 'scan' doit contenir "
            "un objet."
        );
    }

    return data;
}

// ==========================================================
// RECHERCHE
// ==========================================================

std::vector<SearchResult> search_anime(
    const json& scans,
    const std::string& query
)
{
    std::vector<SearchResult> results;

    std::string search =
        to_lower(
            trim(query)
        );

    if (search.empty())
    {
        return results;
    }

    // ======================================================
    // RECHERCHE DIRECTE
    // ======================================================

    for (
        auto it = scans.begin();
        it != scans.end();
        ++it
    )
    {
        std::string identifier =
            it.key();

        std::string lowerIdentifier =
            to_lower(identifier);

        if (
            lowerIdentifier.find(
                search
            ) == std::string::npos
        )
        {
            continue;
        }

        if (!it.value().is_array())
        {
            continue;
        }

        for (
            const auto& value :
            it.value()
        )
        {
            if (!value.is_string())
            {
                continue;
            }

            results.push_back(
                {
                    identifier,
                    value.get<std::string>()
                }
            );
        }
    }

    // ======================================================
    // RECHERCHE MOT PAR MOT
    // ======================================================

    if (results.empty())
    {
        std::istringstream stream(
            search
        );

        std::vector<std::string> words;

        std::string word;

        while (stream >> word)
        {
            words.push_back(word);
        }

        for (
            auto it = scans.begin();
            it != scans.end();
            ++it
        )
        {
            std::string identifier =
                it.key();

            std::string lowerIdentifier =
                to_lower(identifier);

            bool matches = true;

            for (
                const auto& w :
                words
            )
            {
                if (
                    lowerIdentifier.find(
                        w
                    ) == std::string::npos
                )
                {
                    matches = false;
                    break;
                }
            }

            if (!matches)
            {
                continue;
            }

            if (!it.value().is_array())
            {
                continue;
            }

            for (
                const auto& value :
                it.value()
            )
            {
                if (!value.is_string())
                {
                    continue;
                }

                results.push_back(
                    {
                        identifier,
                        value.get<std::string>()
                    }
                );
            }
        }
    }

    // ======================================================
    // SUPPRESSION DOUBLONS
    // ======================================================

    std::vector<SearchResult>
        uniqueResults;

    std::unordered_set<std::string>
        seen;

    for (
        const auto& result :
        results
    )
    {
        if (
            seen.insert(
                result.url
            ).second
        )
        {
            uniqueResults.push_back(
                result
            );
        }
    }

    return uniqueResults;
}

// ==========================================================
// DESSIN TEXTE
// ==========================================================

void draw_text(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const std::string& text,
    int x,
    int y,
    SDL_Color color
)
{
    if (text.empty())
    {
        return;
    }

    SDL_Surface* surface =
        TTF_RenderUTF8_Blended(
            font,
            text.c_str(),
            color
        );

    if (!surface)
    {
        return;
    }

    SDL_Texture* texture =
        SDL_CreateTextureFromSurface(
            renderer,
            surface
        );

    if (!texture)
    {
        SDL_FreeSurface(
            surface
        );

        return;
    }

    SDL_Rect destination =
    {
        x,
        y,
        surface->w,
        surface->h
    };

    SDL_RenderCopy(
        renderer,
        texture,
        nullptr,
        &destination
    );

    SDL_DestroyTexture(
        texture
    );

    SDL_FreeSurface(
        surface
    );
}

// ==========================================================
// DESSIN PANEL
// ==========================================================

void draw_panel(
    SDL_Renderer* renderer,
    const SDL_Rect& rect,
    SDL_Color color
)
{
    SDL_SetRenderDrawColor(
        renderer,
        color.r,
        color.g,
        color.b,
        color.a
    );

    SDL_RenderFillRect(
        renderer,
        &rect
    );
}

// ==========================================================
// COLLISION SOURIS
// ==========================================================

bool point_in_rect(
    int x,
    int y,
    const SDL_Rect& rect
)
{
    return
        x >= rect.x &&
        x < rect.x + rect.w &&
        y >= rect.y &&
        y < rect.y + rect.h;
}

// ==========================================================
// MAIN
// ==========================================================

int main()
{
    // ======================================================
    // CHARGEMENT JSON
    // ======================================================

    json heritage;

    try
    {
        heritage =
            load_heritage(
                HERITAGE_FILE
            );
    }
    catch (
        const std::exception& e
    )
    {
        std::cerr
            << "[ERROR] "
            << e.what()
            << "\n";

        return 1;
    }

    const json& scans =
        heritage["scan"];

    // ======================================================
    // SDL
    // ======================================================

    if (
        SDL_Init(
            SDL_INIT_VIDEO
        ) != 0
    )
    {
        std::cerr
            << "Erreur SDL : "
            << SDL_GetError()
            << "\n";

        return 1;
    }

    if (
        TTF_Init() != 0
    )
    {
        std::cerr
            << "Erreur SDL_ttf : "
            << TTF_GetError()
            << "\n";

        SDL_Quit();

        return 1;
    }

    // ======================================================
    // POLICE
    // ======================================================

    const char* FONT_PATH =
        "data/font/yoruka.ttf";

    TTF_Font* font =
        TTF_OpenFont(
            FONT_PATH,
            20
        );

    if (!font)
    {
        std::cerr
            << "Impossible de charger "
            << "la police.\n"
            << "Chemin : "
            << FONT_PATH
            << "\n"
            << "Erreur SDL_ttf : "
            << TTF_GetError()
            << "\n";

        TTF_Quit();
        SDL_Quit();

        return 1;
    }

    TTF_Font* titleFont =
        TTF_OpenFont(
            FONT_PATH,
            28
        );

    if (!titleFont)
    {
        titleFont = font;
    }

    // ======================================================
    // FENETRE
    // ======================================================

    SDL_Window* window =
        SDL_CreateWindow(
            "Recherche Anime",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            WINDOW_WIDTH,
            WINDOW_HEIGHT,
            SDL_WINDOW_SHOWN
        );

    if (!window)
    {
        std::cerr
            << "Erreur fenêtre : "
            << SDL_GetError()
            << "\n";

        if (
            titleFont != font
        )
        {
            TTF_CloseFont(
                titleFont
            );
        }

        TTF_CloseFont(
            font
        );

        TTF_Quit();
        SDL_Quit();

        return 1;
    }

    // ======================================================
    // RENDERER
    // ======================================================

    SDL_Renderer* renderer =
        SDL_CreateRenderer(
            window,
            -1,
            SDL_RENDERER_ACCELERATED |
            SDL_RENDERER_PRESENTVSYNC
        );

    if (!renderer)
    {
        std::cerr
            << "Erreur renderer : "
            << SDL_GetError()
            << "\n";

        SDL_DestroyWindow(
            window
        );

        if (
            titleFont != font
        )
        {
            TTF_CloseFont(
                titleFont
            );
        }

        TTF_CloseFont(
            font
        );

        TTF_Quit();
        SDL_Quit();

        return 1;
    }

    // ======================================================
    // VARIABLES
    // ======================================================

    bool running = true;

    bool inputActive = false;

    std::string searchText;

    std::vector<SearchResult>
        results;

    int selectedIndex = -1;

    std::string selectedAnime;

    std::string selectedUrl;

    // ------------------------------------------------------
    // Etat recherche
    // ------------------------------------------------------

    bool searchRunning = false;

    bool searchFinished = false;

    // Permet de vérifier status.json
    Uint32 lastStatusCheck = 0;

    // ======================================================
    // RECTANGLES
    // ======================================================

    SDL_Rect searchBox =
    {
        40,
        85,
        700,
        50
    };

    SDL_Rect searchButton =
    {
        760,
        85,
        200,
        50
    };

    SDL_Rect quitButton =
    {
        850,
        630,
        110,
        40
    };

    // Nouveau bouton
    SDL_Rect nextButton =
    {
        40,
        630,
        250,
        40
    };

    // ======================================================
    // TEXT INPUT
    // ======================================================

    SDL_StartTextInput();

    // ======================================================
    // BOUCLE PRINCIPALE
    // ======================================================

    while (running)
    {
        SDL_Event event;

        while (
            SDL_PollEvent(
                &event
            )
        )
        {
            // ==================================================
            // QUITTER
            // ==================================================

            if (
                event.type ==
                SDL_QUIT
            )
            {
                running = false;
            }

            // ==================================================
            // SOURIS
            // ==================================================

            if (
                event.type ==
                SDL_MOUSEBUTTONDOWN &&
                event.button.button ==
                SDL_BUTTON_LEFT
            )
            {
                int mouseX =
                    event.button.x;

                int mouseY =
                    event.button.y;

                // ==================================================
                // CHAMP RECHERCHE
                // ==================================================

                if (
                    point_in_rect(
                        mouseX,
                        mouseY,
                        searchBox
                    )
                )
                {
                    inputActive = true;
                }
                else
                {
                    inputActive = false;
                }

                // ==================================================
                // BOUTON RECHERCHER
                // ==================================================

                if (
                    point_in_rect(
                        mouseX,
                        mouseY,
                        searchButton
                    )
                )
                {
                    if (
                        !trim(
                            searchText
                        ).empty()
                    )
                    {
                        /*
                         * Nouvelle recherche :
                         * status = false
                         */

                        reset_status();

                        searchRunning = false;

                        searchFinished = false;

                        selectedIndex = -1;

                        selectedAnime.clear();

                        selectedUrl.clear();

                        results =
                            search_anime(
                                scans,
                                searchText
                            );
                    }
                }

                // ==================================================
                // BOUTON PASSER À LA SUITE
                // ==================================================

                if (
                    searchFinished &&
                    point_in_rect(
                        mouseX,
                        mouseY,
                        nextButton
                    )
                )
                {
                    /*
                     * Ferme cette fenêtre.
                     */

                    running = false;
                }

                // ==================================================
                // BOUTON QUITTER
                // ==================================================

                if (
                    point_in_rect(
                        mouseX,
                        mouseY,
                        quitButton
                    )
                )
                {
                    running = false;
                }

                // ==================================================
                // RESULTATS
                // ==================================================

                const int resultStartY = 180;

                const int resultHeight = 65;

                for (
                    size_t i = 0;
                    i < results.size();
                    ++i
                )
                {
                    SDL_Rect resultRect =
                    {
                        40,
                        resultStartY +
                            static_cast<int>(i) *
                            resultHeight,
                        920,
                        55
                    };

                    if (
                        point_in_rect(
                            mouseX,
                            mouseY,
                            resultRect
                        )
                    )
                    {
                        selectedIndex =
                            static_cast<int>(
                                i
                            );

                        selectedAnime =
                            results[i]
                                .identifier;

                        selectedUrl =
                            results[i]
                                .url;

                        /*
                         * Nouvelle recherche
                         * pour chapter.exe.
                         */

                        reset_status();

                        searchRunning = true;

                        searchFinished = false;

                        lastStatusCheck = 0;

                        launch_chapter(
                            selectedUrl
                        );
                    }
                }
            }

            // ==================================================
            // CLAVIER
            // ==================================================

            if (
                event.type ==
                SDL_KEYDOWN
            )
            {
                // ==================================================
                // ENTER
                // ==================================================

                if (
                    event.key.keysym.sym ==
                    SDLK_RETURN
                )
                {
                    if (
                        inputActive &&
                        !trim(
                            searchText
                        ).empty()
                    )
                    {
                        reset_status();

                        searchRunning = false;

                        searchFinished = false;

                        selectedIndex = -1;

                        selectedAnime.clear();

                        selectedUrl.clear();

                        results =
                            search_anime(
                                scans,
                                searchText
                            );
                    }
                }

                // ==================================================
                // BACKSPACE
                // ==================================================

                if (
                    event.key.keysym.sym ==
                    SDLK_BACKSPACE
                )
                {
                    if (
                        inputActive &&
                        !searchText.empty()
                    )
                    {
                        searchText.pop_back();
                    }
                }

                // ==================================================
                // ESCAPE
                // ==================================================

                if (
                    event.key.keysym.sym ==
                    SDLK_ESCAPE
                )
                {
                    inputActive = false;
                }
            }

            // ==================================================
            // TEXTE
            // ==================================================

            if (
                event.type ==
                SDL_TEXTINPUT
            )
            {
                if (inputActive)
                {
                    searchText +=
                        event.text.text;
                }
            }
        }

        // ======================================================
        // SURVEILLANCE STATUS.JSON
        // ======================================================

        Uint32 currentTime =
            SDL_GetTicks();

        /*
         * On vérifie toutes les 250 ms.
         */

        if (
            searchRunning &&
            currentTime -
                lastStatusCheck >=
                250
        )
        {
            lastStatusCheck =
                currentTime;

            if (
                read_status()
            )
            {
                /*
                 * status.json == true
                 *
                 * La recherche est terminée.
                 */

                searchRunning = false;

                searchFinished = true;
            }
        }

        // ======================================================
        // RENDU
        // ======================================================

        SDL_SetRenderDrawColor(
            renderer,
            BG_COLOR.r,
            BG_COLOR.g,
            BG_COLOR.b,
            BG_COLOR.a
        );

        SDL_RenderClear(
            renderer
        );

        // ======================================================
        // TITRE
        // ======================================================

        draw_text(
            renderer,
            titleFont,
            "Recherche Anime",
            40,
            30,
            TEXT_COLOR
        );

        // ======================================================
        // CHAMP
        // ======================================================

        draw_panel(
            renderer,
            searchBox,
            INPUT_COLOR
        );

        if (
            searchText.empty() &&
            !inputActive
        )
        {
            draw_text(
                renderer,
                font,
                "Entrez le nom d'un anime...",
                searchBox.x + 15,
                searchBox.y + 14,
                MUTED_COLOR
            );
        }
        else
        {
            draw_text(
                renderer,
                font,
                searchText,
                searchBox.x + 15,
                searchBox.y + 14,
                TEXT_COLOR
            );
        }

        // ======================================================
        // SOURIS
        // ======================================================

        int mouseX;
        int mouseY;

        SDL_GetMouseState(
            &mouseX,
            &mouseY
        );

        // ======================================================
        // BOUTON RECHERCHER
        // ======================================================

        bool searchHover =
            point_in_rect(
                mouseX,
                mouseY,
                searchButton
            );

        draw_panel(
            renderer,
            searchButton,
            searchHover
                ? HOVER_COLOR
                : BUTTON_COLOR
        );

        draw_text(
            renderer,
            font,
            "Rechercher",
            searchButton.x + 48,
            searchButton.y + 14,
            TEXT_COLOR
        );

        // ======================================================
        // STATUS
        // ======================================================

        if (
            searchRunning
        )
        {
            draw_text(
                renderer,
                font,
                "Recherche en cours...",
                40,
                145,
                SEARCHING_COLOR
            );
        }
        else if (
            searchFinished
        )
        {
            draw_text(
                renderer,
                font,
                "Recherche terminée",
                40,
                145,
                FINISHED_COLOR
            );
        }

        // ======================================================
        // RESULTATS
        // ======================================================

        if (
            results.empty()
        )
        {
            if (
                !searchText.empty()
            )
            {
                draw_text(
                    renderer,
                    font,
                    "Aucun resultat.",
                    40,
                    180,
                    MUTED_COLOR
                );
            }
        }
        else
        {
            draw_text(
                renderer,
                font,
                std::to_string(
                    results.size()
                ) +
                " resultat(s)",
                40,
                165,
                MUTED_COLOR
            );

            const int resultStartY = 200;

            const int resultHeight = 65;

            for (
                size_t i = 0;
                i < results.size();
                ++i
            )
            {
                SDL_Rect resultRect =
                {
                    40,
                    resultStartY +
                        static_cast<int>(i) *
                        resultHeight,
                    920,
                    55
                };

                bool hover =
                    point_in_rect(
                        mouseX,
                        mouseY,
                        resultRect
                    );

                bool selected =
                    selectedIndex ==
                    static_cast<int>(i);

                SDL_Color color;

                if (selected)
                {
                    /*
                     * IMPORTANT :
                     *
                     * Rouge pendant la recherche.
                     * Vert quand status == true.
                     */

                    if (
                        searchFinished
                    )
                    {
                        color =
                            FINISHED_COLOR;
                    }
                    else
                    {
                        color =
                            SEARCHING_COLOR;
                    }
                }
                else if (hover)
                {
                    color =
                        HOVER_COLOR;
                }
                else
                {
                    color =
                        PANEL_COLOR;
                }

                draw_panel(
                    renderer,
                    resultRect,
                    color
                );

                // ------------------------------------------------
                // Nom
                // ------------------------------------------------

                draw_text(
                    renderer,
                    font,
                    results[i].identifier,
                    resultRect.x + 15,
                    resultRect.y + 7,
                    TEXT_COLOR
                );

                // ------------------------------------------------
                // URL
                // ------------------------------------------------

                std::string url =
                    results[i].url;

                if (
                    url.length() > 110
                )
                {
                    url =
                        url.substr(
                            0,
                            107
                        ) +
                        "...";
                }

                draw_text(
                    renderer,
                    font,
                    url,
                    resultRect.x + 15,
                    resultRect.y + 31,
                    MUTED_COLOR
                );
            }
        }

        // ======================================================
        // PANEL SELECTION
        // ======================================================

        if (
            !selectedUrl.empty()
        )
        {
            SDL_Rect selectedPanel =
            {
                40,
                540,
                790,
                80
            };

            SDL_Color selectionColor;

            /*
             * ROUGE :
             * chapter.exe travaille encore.
             *
             * VERT :
             * status.json == true.
             */

            if (
                searchFinished
            )
            {
                selectionColor =
                    FINISHED_COLOR;
            }
            else
            {
                selectionColor =
                    SEARCHING_COLOR;
            }

            draw_panel(
                renderer,
                selectedPanel,
                selectionColor
            );

            // --------------------------------------------------
            // Texte status
            // --------------------------------------------------

            if (
                searchFinished
            )
            {
                draw_text(
                    renderer,
                    font,
                    "Recherche terminée",
                    selectedPanel.x + 15,
                    selectedPanel.y + 8,
                    TEXT_COLOR
                );
            }
            else
            {
                draw_text(
                    renderer,
                    font,
                    "Recherche en cours...",
                    selectedPanel.x + 15,
                    selectedPanel.y + 8,
                    TEXT_COLOR
                );
            }

            // --------------------------------------------------
            // Anime
            // --------------------------------------------------

            draw_text(
                renderer,
                font,
                selectedAnime,
                selectedPanel.x + 15,
                selectedPanel.y + 35,
                TEXT_COLOR
            );
        }

        // ======================================================
        // BOUTON PASSER À LA SUITE
        // ======================================================

        if (
            searchFinished
        )
        {
            bool nextHover =
                point_in_rect(
                    mouseX,
                    mouseY,
                    nextButton
                );

            draw_panel(
                renderer,
                nextButton,
                nextHover
                    ? HOVER_COLOR
                    : BUTTON_COLOR
            );

            draw_text(
                renderer,
                font,
                "Passer a la suite",
                nextButton.x + 45,
                nextButton.y + 9,
                TEXT_COLOR
            );
        }

        // ======================================================
        // BOUTON QUITTER
        // ======================================================

        bool quitHover =
            point_in_rect(
                mouseX,
                mouseY,
                quitButton
            );

        draw_panel(
            renderer,
            quitButton,
            quitHover
                ? HOVER_COLOR
                : BUTTON_COLOR
        );

        draw_text(
            renderer,
            font,
            "Quitter",
            quitButton.x + 22,
            quitButton.y + 9,
            TEXT_COLOR
        );

        // ======================================================
        // PRESENTATION
        // ======================================================

        SDL_RenderPresent(
            renderer
        );
    }

    // ======================================================
    // CLEANUP
    // ======================================================

    SDL_StopTextInput();

    SDL_DestroyRenderer(
        renderer
    );

    SDL_DestroyWindow(
        window
    );

    if (
        titleFont != font
    )
    {
        TTF_CloseFont(
            titleFont
        );
    }

    TTF_CloseFont(
        font
    );

    TTF_Quit();

    SDL_Quit();

    return 0;
}