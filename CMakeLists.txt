cmake_minimum_required(VERSION 3.20)
project(MangaCV2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT MINGW OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "Utilise GCC de MSYS2 UCRT64.")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(STATIC_LINK "Lier statiquement les bibliotheques (aucune DLL a cote de l'exe)" OFF)

# ---- sortie : dist/MangaCV2.exe ----
set(DIST_DIR "${CMAKE_SOURCE_DIR}/dist")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${DIST_DIR}")
foreach(CONFIG DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG} "${DIST_DIR}")
endforeach()

# ---- bibliotheques ----
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2     REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SDL2_TTF REQUIRED IMPORTED_TARGET SDL2_ttf)
pkg_check_modules(CURL     REQUIRED IMPORTED_TARGET libcurl)

# libharu : pkg-config d'abord, sinon recherche manuelle
pkg_check_modules(HPDF IMPORTED_TARGET libharu)
if(NOT HPDF_FOUND)
    find_path(HPDF_INCLUDE_DIR hpdf.h)
    find_library(HPDF_LIBRARY NAMES hpdf libhpdf)
    if(NOT HPDF_INCLUDE_DIR OR NOT HPDF_LIBRARY)
        message(FATAL_ERROR "libharu introuvable : pacman -S mingw-w64-ucrt-x86_64-libharu")
    endif()
endif()

# ---- sources ----
set(SOURCES
    src/main.cpp
    src/app.cpp
    src/gfx.cpp
    src/platform.cpp
    src/task.cpp
    src/http.cpp
    src/utils.cpp
    src/settings.cpp
    src/storage.cpp
    src/domain.cpp
    src/sitemap.cpp
    src/search.cpp
    src/scraper.cpp
    src/downloader.cpp
    src/pdf_builder.cpp
    src/pipeline.cpp
    src/screens.cpp
    src/screen_home.cpp
    src/screen_search.cpp
    src/screen_progress.cpp
)

# Icone Windows (facultative : resources/app.ico + resources/app.rc)
set(APP_RC "${CMAKE_SOURCE_DIR}/resources/app.rc")
if(EXISTS "${APP_RC}" AND EXISTS "${CMAKE_SOURCE_DIR}/resources/app.ico")
    set_source_files_properties("${APP_RC}" PROPERTIES OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/resources/app.ico")
    list(APPEND SOURCES "${APP_RC}")
else()
    message(STATUS "Pas d'icone (resources/app.rc ou app.ico absent) : exe sans icone.")
endif()

add_executable(MangaCV2 ${SOURCES})

target_include_directories(MangaCV2 PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_compile_definitions(MangaCV2 PRIVATE SDL_MAIN_HANDLED UNICODE _UNICODE)
target_compile_options(MangaCV2 PRIVATE -Wall -Wextra $<$<CONFIG:Release>:-O2>)

if(HPDF_FOUND)
    set(HPDF_TARGET PkgConfig::HPDF)
else()
    target_include_directories(MangaCV2 PRIVATE "${HPDF_INCLUDE_DIR}")
    set(HPDF_TARGET "${HPDF_LIBRARY}")
endif()

if(STATIC_LINK)
    target_compile_options(MangaCV2 PRIVATE ${SDL2_STATIC_CFLAGS_OTHER} ${CURL_STATIC_CFLAGS_OTHER})
    target_compile_definitions(MangaCV2 PRIVATE CURL_STATICLIB)
    target_link_libraries(MangaCV2 PRIVATE
        ${SDL2_TTF_STATIC_LDFLAGS} ${SDL2_STATIC_LDFLAGS} ${CURL_STATIC_LDFLAGS}
        ${HPDF_STATIC_LDFLAGS} ${HPDF_LIBRARY})
    target_link_options(MangaCV2 PRIVATE -static)
else()
    target_link_libraries(MangaCV2 PRIVATE
        PkgConfig::SDL2_TTF PkgConfig::SDL2 PkgConfig::CURL ${HPDF_TARGET})
endif()

target_link_libraries(MangaCV2 PRIVATE ole32 shell32 shlwapi uuid comdlg32)

# Application graphique : aucune fenetre de terminal
target_link_options(MangaCV2 PRIVATE -mwindows)

file(MAKE_DIRECTORY "${DIST_DIR}/data/font")
message(STATUS "Sortie : ${DIST_DIR}/MangaCV2.exe (STATIC_LINK=${STATIC_LINK})")
