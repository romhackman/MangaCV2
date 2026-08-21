#define SDL_MAIN_HANDLED

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

using json = nlohmann::json;

// ==========================================================
// CONFIGURATION
// ==========================================================

const std::string RESULT_FILE = "data/result.json";
const char* FONT_PATH = "data/font/yoruka.ttf";

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

const SDL_Color CHECKBOX_COLOR =
{
    55, 55, 70, 255
};

const SDL_Color CHECKED_COLOR =
{
    50, 180, 100, 255
};

const SDL_Color BORDER_COLOR =
{
    90, 90, 110, 255
};

const SDL_Color ERROR_COLOR =
{
    210, 70, 70, 255
};

// ==========================================================
// STRUCTURE CHAPITRE
// ==========================================================

struct Chapter
{
    std::string name;
    long long image_count;
    bool selected;
};

// ==========================================================
// LIRE RESULT.JSON
// ==========================================================

json load_result()
{
    std::ifstream file(
        RESULT_FILE
    );

    if (!file)
    {
        throw std::runtime_error(
            "Impossible d'ouvrir " +
            RESULT_FILE
        );
    }

    json data;

    try
    {
        file >> data;
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error(
            std::string(
                "JSON invalide : "
            ) +
            e.what()
        );
    }

    return data;
}

// ==========================================================
// EXTRAIRE LES CHAPITRES
// ==========================================================

std::vector<Chapter> get_chapters(
    const json& data
)
{
    std::vector<Chapter> chapters;

    // ======================================================
    // VERIFICATION RESULTS
    // ======================================================

    if (
        !data.contains("results") ||
        !data["results"].is_array()
    )
    {
        return chapters;
    }

    // ======================================================
    // PARCOURIR RESULTS
    // ======================================================

    for (
        const auto& result :
        data["results"]
    )
    {
        if (
            !result.contains("chapters")
        )
        {
            continue;
        }

        if (
            !result["chapters"].is_object()
        )
        {
            continue;
        }

        const auto& chaptersObject =
            result["chapters"];

        // ==================================================
        // PARCOURIR CHAPITRES
        // ==================================================

        for (
            auto it = chaptersObject.begin();
            it != chaptersObject.end();
            ++it
        )
        {
            Chapter chapter;

            chapter.name =
                it.key();

            chapter.image_count =
                0;

            chapter.selected =
                false;

            // ==================================================
            // IMAGE COUNT
            // ==================================================

            if (
                it.value().is_object() &&
                it.value().contains(
                    "image_count"
                )
            )
            {
                try
                {
                    if (
                        it.value()["image_count"]
                            .is_number_integer()
                    )
                    {
                        chapter.image_count =
                            it.value()
                                ["image_count"]
                                .get<long long>();
                    }
                    else if (
                        it.value()["image_count"]
                            .is_number()
                    )
                    {
                        chapter.image_count =
                            static_cast<long long>(
                                it.value()
                                    ["image_count"]
                                    .get<double>()
                            );
                    }
                }
                catch (...)
                {
                    chapter.image_count = 0;
                }
            }

            chapters.push_back(
                chapter
            );
        }
    }

    return chapters;
}

// ==========================================================
// TRI DES CHAPITRES
// ==========================================================
//
// 1
// 2
// 3
// 10
// 11
//
// et non :
//
// 1
// 10
// 11
// 2
// 3
//
// ==========================================================

bool chapter_less(
    const Chapter& a,
    const Chapter& b
)
{
    try
    {
        double numberA =
            std::stod(a.name);

        double numberB =
            std::stod(b.name);

        if (
            numberA != numberB
        )
        {
            return numberA < numberB;
        }
    }
    catch (...)
    {
        // Pas numérique.
    }

    return a.name < b.name;
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
// DESSIN CONTOUR
// ==========================================================

void draw_border(
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

    SDL_RenderDrawRect(
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
// COMPTER SELECTION
// ==========================================================

int count_selected(
    const std::vector<Chapter>& chapters
)
{
    int count = 0;

    for (
        const auto& chapter :
        chapters
    )
    {
        if (chapter.selected)
        {
            ++count;
        }
    }

    return count;
}

// ==========================================================
// TOUT SELECTIONNER
// ==========================================================

void select_all(
    std::vector<Chapter>& chapters
)
{
    for (
        auto& chapter :
        chapters
    )
    {
        chapter.selected = true;
    }
}

// ==========================================================
// TOUT DESELECTIONNER
// ==========================================================

void deselect_all(
    std::vector<Chapter>& chapters
)
{
    for (
        auto& chapter :
        chapters
    )
    {
        chapter.selected = false;
    }
}

// ==========================================================
// AFFICHER SELECTION DANS CONSOLE
// ==========================================================

void print_selected_chapters(
    const std::vector<Chapter>& chapters
)
{
    std::cout
        << "\n"
        << "==========================================\n"
        << "        CHAPITRES SELECTIONNES\n"
        << "==========================================\n";

    int count = 0;

    for (
        const auto& chapter :
        chapters
    )
    {
        if (!chapter.selected)
        {
            continue;
        }

        std::cout
            << "Chapitre "
            << chapter.name
            << " : "
            << chapter.image_count
            << " image(s)\n";

        ++count;
    }

    std::cout
        << "==========================================\n"
        << "Total : "
        << count
        << "\n"
        << "==========================================\n";
}

// ==========================================================
// LANCER DOWNLOAD.EXE
// ==========================================================
//
// Si tous les chapitres sont sélectionnés:
//
//     download.exe -all
//
// Sinon:
//
//     download.exe -chapter [1, 2, 3, 18]
//
// ==========================================================

bool launch_download(
    const std::vector<Chapter>& chapters
)
{
#ifdef _WIN32

    // ======================================================
    // RECUPERER LES CHAPITRES SELECTIONNES
    // ======================================================

    std::vector<std::string> selected;

    for (
        const auto& chapter :
        chapters
    )
    {
        if (chapter.selected)
        {
            selected.push_back(
                chapter.name
            );
        }
    }

    // ======================================================
    // AUCUN CHAPITRE
    // ======================================================

    if (selected.empty())
    {
        std::cerr
            << "[ERROR] Aucun chapitre "
            << "selectionne.\n";

        return false;
    }

    // ======================================================
    // CONSTRUIRE LA COMMANDE
    // ======================================================

    std::string command;

    // ------------------------------------------------------
    // TOUS LES CHAPITRES
    // ------------------------------------------------------

    if (
        selected.size() ==
        chapters.size()
    )
    {
        command =
            ".\\download.exe -all";
    }

    // ------------------------------------------------------
    // SELECTION PARTIELLE
    // ------------------------------------------------------

    else
    {
        command =
            ".\\download.exe -chapter [";

        for (
            size_t i = 0;
            i < selected.size();
            ++i
        )
        {
            command +=
                selected[i];

            if (
                i + 1 <
                selected.size()
            )
            {
                command +=
                    ", ";
            }
        }

        command +=
            "]";
    }

    // ======================================================
    // AFFICHAGE CONSOLE
    // ======================================================

    std::cout
        << "\n"
        << "[DOWNLOAD] "
        << command
        << "\n";

    // ======================================================
    // STARTUPINFO
    // ======================================================

    STARTUPINFOA si{};

    PROCESS_INFORMATION pi{};

    si.cb = sizeof(si);

    // ======================================================
    // BUFFER MODIFIABLE
    // ======================================================

    std::vector<char> commandBuffer(
        command.begin(),
        command.end()
    );

    commandBuffer.push_back(
        '\0'
    );

    // ======================================================
    // LANCER PROCESSUS
    // ======================================================

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
            << "download.exe.\n"
            << "Erreur Windows : "
            << GetLastError()
            << "\n";

        return false;
    }

    // ======================================================
    // FERMER HANDLES
    // ======================================================

    CloseHandle(
        pi.hThread
    );

    CloseHandle(
        pi.hProcess
    );

    std::cout
        << "[OK] download.exe lance.\n";

    return true;

#else

    std::cerr
        << "[ERROR] Ce programme est "
        << "prevu pour Windows.\n";

    return false;

#endif
}

// ==========================================================
// MAIN
// ==========================================================

int main()
{
    // ======================================================
    // CHARGEMENT JSON
    // ======================================================

    json data;

    try
    {
        data =
            load_result();
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

    // ======================================================
    // CHARGEMENT CHAPITRES
    // ======================================================

    std::vector<Chapter> chapters =
        get_chapters(
            data
        );

    // ======================================================
    // TRI
    // ======================================================

    std::sort(
        chapters.begin(),
        chapters.end(),
        chapter_less
    );

    // ======================================================
    // AUCUN CHAPITRE
    // ======================================================

    if (chapters.empty())
    {
        std::cerr
            << "[ERROR] Aucun chapitre trouve.\n";

        return 1;
    }

    // ======================================================
    // SDL INIT
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

    // ======================================================
    // SDL TTF INIT
    // ======================================================

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

    // ======================================================
    // POLICE TITRE
    // ======================================================

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
            "Selection des chapitres",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            WINDOW_WIDTH,
            WINDOW_HEIGHT,
            SDL_WINDOW_SHOWN
        );

    if (!window)
    {
        std::cerr
            << "Erreur fenetre : "
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
    // RECTANGLES BOUTONS
    // ======================================================

    SDL_Rect selectAllButton =
    {
        40,
        85,
        230,
        45
    };

    // Plus large pour que le texte rentre
    SDL_Rect deselectAllButton =
    {
        285,
        85,
        250,
        45
    };

    SDL_Rect validateButton =
    {
        630,
        630,
        150,
        40
    };

    SDL_Rect quitButton =
    {
        810,
        630,
        150,
        40
    };

    // ======================================================
    // PANEL CHAPITRES
    // ======================================================

    SDL_Rect chaptersPanel =
    {
        40,
        150,
        920,
        450
    };

    // ======================================================
    // PARAMETRES LISTE
    // ======================================================

    const int chapterHeight =
        55;

    const int chapterSpacing =
        8;

    const int contentHeight =
        static_cast<int>(
            chapters.size()
        ) *
        (
            chapterHeight +
            chapterSpacing
        );

    const int visibleHeight =
        chaptersPanel.h;

    const int maxScroll =
        std::max(
            0,
            contentHeight -
            visibleHeight
        );

    int scrollOffset = 0;

    // ======================================================
    // VARIABLES
    // ======================================================

    bool running = true;

    bool selectionValidated =
        false;

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
            // QUITTER FENETRE
            // ==================================================

            if (
                event.type ==
                SDL_QUIT
            )
            {
                running = false;
            }

            // ==================================================
            // MOLETTE
            // ==================================================

            if (
                event.type ==
                SDL_MOUSEWHEEL
            )
            {
                int mouseX;
                int mouseY;

                SDL_GetMouseState(
                    &mouseX,
                    &mouseY
                );

                if (
                    point_in_rect(
                        mouseX,
                        mouseY,
                        chaptersPanel
                    )
                )
                {
                    scrollOffset -=
                        event.wheel.y *
                        35;

                    if (
                        scrollOffset < 0
                    )
                    {
                        scrollOffset = 0;
                    }

                    if (
                        scrollOffset >
                        maxScroll
                    )
                    {
                        scrollOffset =
                            maxScroll;
                    }
                }
            }

            // ==================================================
            // CLIC SOURIS
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
                // TOUT SELECTIONNER
                // ==================================================

                if (
                    point_in_rect(
                        mouseX,
                        mouseY,
                        selectAllButton
                    )
                )
                {
                    select_all(
                        chapters
                    );
                }

                // ==================================================
                // TOUT DESELECTIONNER
                // ==================================================

                if (
                    point_in_rect(
                        mouseX,
                        mouseY,
                        deselectAllButton
                    )
                )
                {
                    deselect_all(
                        chapters
                    );
                }

                // ==================================================
                // CHAPITRE
                // ==================================================

                if (
                    point_in_rect(
                        mouseX,
                        mouseY,
                        chaptersPanel
                    )
                )
                {
                    int localY =
                        mouseY -
                        chaptersPanel.y +
                        scrollOffset;

                    int rowSize =
                        chapterHeight +
                        chapterSpacing;

                    int index =
                        localY /
                        rowSize;

                    if (
                        index >= 0 &&
                        index <
                            static_cast<int>(
                                chapters.size()
                            )
                    )
                    {
                        int rowY =
                            index *
                            rowSize;

                        int insideRow =
                            localY -
                            rowY;

                        if (
                            insideRow >= 0 &&
                            insideRow <
                                chapterHeight
                        )
                        {
                            chapters[
                                index
                            ].selected =
                                !chapters[
                                    index
                                ].selected;
                        }
                    }
                }

                // ==================================================
                // VALIDER
                // ==================================================

                if (
                    point_in_rect(
                        mouseX,
                        mouseY,
                        validateButton
                    )
                )
                {
                    int selectedCount =
                        count_selected(
                            chapters
                        );

                    // ----------------------------------------------
                    // Aucun chapitre
                    // ----------------------------------------------

                    if (
                        selectedCount == 0
                    )
                    {
                        std::cerr
                            << "[ERROR] Aucun chapitre "
                            << "selectionne.\n";
                    }

                    // ----------------------------------------------
                    // Au moins un chapitre
                    // ----------------------------------------------

                    else
                    {
                        selectionValidated =
                            true;

                        // Affichage console
                        print_selected_chapters(
                            chapters
                        );

                        // Lancement download.exe
                        launch_download(
                            chapters
                        );

                        // Fermer interface
                        running = false;
                    }
                }

                // ==================================================
                // QUITTER
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
                // ESCAPE
                // ==================================================

                if (
                    event.key.keysym.sym ==
                    SDLK_ESCAPE
                )
                {
                    running = false;
                }

                // ==================================================
                // CTRL + A
                // ==================================================

                if (
                    event.key.keysym.sym ==
                        SDLK_a &&
                    (
                        event.key.keysym.mod &
                        KMOD_CTRL
                    )
                )
                {
                    select_all(
                        chapters
                    );
                }
            }
        }

        // ======================================================
        // POSITION SOURIS
        // ======================================================

        int mouseX;
        int mouseY;

        SDL_GetMouseState(
            &mouseX,
            &mouseY
        );

        // ======================================================
        // FOND
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
            "Selection des chapitres",
            40,
            30,
            TEXT_COLOR
        );

        // ======================================================
        // BOUTON TOUT SELECTIONNER
        // ======================================================

        bool selectAllHover =
            point_in_rect(
                mouseX,
                mouseY,
                selectAllButton
            );

        draw_panel(
            renderer,
            selectAllButton,
            selectAllHover
                ? HOVER_COLOR
                : BUTTON_COLOR
        );

        draw_text(
            renderer,
            font,
            "Tout selectionner",
            selectAllButton.x + 32,
            selectAllButton.y + 12,
            TEXT_COLOR
        );

        // ======================================================
        // BOUTON TOUT DESELECTIONNER
        // ======================================================

        bool deselectAllHover =
            point_in_rect(
                mouseX,
                mouseY,
                deselectAllButton
            );

        draw_panel(
            renderer,
            deselectAllButton,
            deselectAllHover
                ? HOVER_COLOR
                : BUTTON_COLOR
        );

        draw_text(
            renderer,
            font,
            "Tout deselectionner",
            deselectAllButton.x + 32,
            deselectAllButton.y + 12,
            TEXT_COLOR
        );

        // ======================================================
        // COMPTEUR
        // ======================================================

        int selectedCount =
            count_selected(
                chapters
            );

        std::string counterText =
            std::to_string(
                selectedCount
            ) +
            " / " +
            std::to_string(
                chapters.size()
            ) +
            " chapitre(s) selectionne(s)";

        draw_text(
            renderer,
            font,
            counterText,
            555,
            97,
            selectedCount > 0
                ? CHECKED_COLOR
                : MUTED_COLOR
        );

        // ======================================================
        // PANEL CHAPITRES
        // ======================================================

        draw_panel(
            renderer,
            chaptersPanel,
            PANEL_COLOR
        );

        draw_border(
            renderer,
            chaptersPanel,
            BORDER_COLOR
        );

        // ======================================================
        // CLIP
        // ======================================================

        SDL_RenderSetClipRect(
            renderer,
            &chaptersPanel
        );

        // ======================================================
        // DESSIN CHAPITRES
        // ======================================================

        for (
            size_t i = 0;
            i < chapters.size();
            ++i
        )
        {
            const Chapter& chapter =
                chapters[i];

            int rowY =
                chaptersPanel.y +
                static_cast<int>(
                    i
                ) *
                (
                    chapterHeight +
                    chapterSpacing
                ) -
                scrollOffset;

            SDL_Rect chapterRect =
            {
                chaptersPanel.x + 10,
                rowY,
                chaptersPanel.w - 20,
                chapterHeight
            };

            // ==================================================
            // HORS ECRAN
            // ==================================================

            if (
                chapterRect.y +
                    chapterRect.h <
                chaptersPanel.y
            )
            {
                continue;
            }

            if (
                chapterRect.y >
                chaptersPanel.y +
                    chaptersPanel.h
            )
            {
                continue;
            }

            // ==================================================
            // HOVER
            // ==================================================

            bool hover =
                point_in_rect(
                    mouseX,
                    mouseY,
                    chapterRect
                );

            // ==================================================
            // COULEUR LIGNE
            // ==================================================

            SDL_Color rowColor;

            if (
                chapter.selected
            )
            {
                rowColor =
                    CHECKED_COLOR;
            }
            else if (
                hover
            )
            {
                rowColor =
                    HOVER_COLOR;
            }
            else
            {
                rowColor =
                    INPUT_COLOR;
            }

            draw_panel(
                renderer,
                chapterRect,
                rowColor
            );

            // ==================================================
            // CHECKBOX
            // ==================================================

            SDL_Rect checkbox =
            {
                chapterRect.x + 12,
                chapterRect.y + 12,
                30,
                30
            };

            if (
                chapter.selected
            )
            {
                draw_panel(
                    renderer,
                    checkbox,
                    CHECKED_COLOR
                );

                draw_border(
                    renderer,
                    checkbox,
                    TEXT_COLOR
                );

                // ==================================================
                // SYMBOLE CHECK
                // ==================================================

                SDL_SetRenderDrawColor(
                    renderer,
                    255,
                    255,
                    255,
                    255
                );

                SDL_RenderDrawLine(
                    renderer,
                    checkbox.x + 7,
                    checkbox.y + 15,
                    checkbox.x + 13,
                    checkbox.y + 22
                );

                SDL_RenderDrawLine(
                    renderer,
                    checkbox.x + 13,
                    checkbox.y + 22,
                    checkbox.x + 24,
                    checkbox.y + 8
                );
            }
            else
            {
                draw_panel(
                    renderer,
                    checkbox,
                    CHECKBOX_COLOR
                );

                draw_border(
                    renderer,
                    checkbox,
                    BORDER_COLOR
                );
            }

            // ==================================================
            // NOM CHAPITRE
            // ==================================================

            draw_text(
                renderer,
                font,
                "Chapitre " +
                    chapter.name,
                chapterRect.x + 60,
                chapterRect.y + 7,
                TEXT_COLOR
            );

            // ==================================================
            // NOMBRE IMAGES
            // ==================================================

            draw_text(
                renderer,
                font,
                std::to_string(
                    chapter.image_count
                ) +
                " image(s)",
                chapterRect.x + 60,
                chapterRect.y + 30,
                MUTED_COLOR
            );
        }

        // ======================================================
        // FIN CLIP
        // ======================================================

        SDL_RenderSetClipRect(
            renderer,
            nullptr
        );

        // ======================================================
        // BARRE SCROLL
        // ======================================================

        if (
            maxScroll > 0
        )
        {
            SDL_Rect scrollbarBackground =
            {
                chaptersPanel.x +
                    chaptersPanel.w - 8,
                chaptersPanel.y + 5,
                5,
                chaptersPanel.h - 10
            };

            draw_panel(
                renderer,
                scrollbarBackground,
                CHECKBOX_COLOR
            );

            float ratio =
                static_cast<float>(
                    visibleHeight
                ) /
                static_cast<float>(
                    contentHeight
                );

            int scrollbarHeight =
                static_cast<int>(
                    (
                        chaptersPanel.h -
                        10
                    ) *
                    ratio
                );

            if (
                scrollbarHeight < 30
            )
            {
                scrollbarHeight = 30;
            }

            float scrollRatio =
                static_cast<float>(
                    scrollOffset
                ) /
                static_cast<float>(
                    maxScroll
                );

            int scrollbarY =
                chaptersPanel.y +
                5 +
                static_cast<int>(
                    (
                        chaptersPanel.h -
                        10 -
                        scrollbarHeight
                    ) *
                    scrollRatio
                );

            SDL_Rect scrollbar =
            {
                chaptersPanel.x +
                    chaptersPanel.w - 8,
                scrollbarY,
                5,
                scrollbarHeight
            };

            draw_panel(
                renderer,
                scrollbar,
                BUTTON_COLOR
            );
        }

        // ======================================================
        // TEXTE BAS
        // ======================================================

        draw_text(
            renderer,
            font,
            "Cliquez sur un chapitre pour "
            "le selectionner.",
            40,
            610,
            MUTED_COLOR
        );

        // ======================================================
        // BOUTON VALIDER
        // ======================================================

        bool validateHover =
            point_in_rect(
                mouseX,
                mouseY,
                validateButton
            );

        SDL_Color validateColor;

        if (
            selectedCount > 0
        )
        {
            validateColor =
                validateHover
                    ? HOVER_COLOR
                    : BUTTON_COLOR;
        }
        else
        {
            validateColor =
                CHECKBOX_COLOR;
        }

        draw_panel(
            renderer,
            validateButton,
            validateColor
        );

        draw_text(
            renderer,
            font,
            "Valider",
            validateButton.x + 45,
            validateButton.y + 9,
            TEXT_COLOR
        );

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
            quitButton.x + 45,
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
