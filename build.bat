@echo off
setlocal EnableExtensions
cd /d "%~dp0"

REM ==========================================================
REM Compilation de Manga Downloader (MinGW / g++)
REM Structure attendue :
REM   include\  lib\  src\  workers\  dist\
REM Dependances attendues dans lib\ :
REM   SDL2, SDL2_ttf, libcurl, libharu (hpdf)
REM ==========================================================

set "CXX=g++"
set "FLAGS=-std=c++17 -O2 -Iinclude -I."
set "LIBS=-Llib"

if not exist dist mkdir dist
if not exist dist\data\font mkdir dist\data\font

where g++ >nul 2>nul
if errorlevel 1 (
    echo ERREUR : g++ est introuvable. Installe MinGW-w64 et ajoute g++ au PATH.
    goto :error
)

if not exist src\app.cpp (
    echo ERREUR : src\app.cpp introuvable.
    goto :error
)
if not exist workers\domaine.cpp (
    echo ERREUR : workers\domaine.cpp introuvable.
    goto :error
)
if not exist workers\preprocess.cpp (
    echo ERREUR : workers\preprocess.cpp introuvable.
    goto :error
)
if not exist workers\scraper.cpp (
    echo ERREUR : workers\scraper.cpp introuvable.
    goto :error
)
if not exist workers\download.cpp (
    echo ERREUR : workers\download.cpp introuvable.
    goto :error
)
if not exist workers\pdf.cpp (
    echo ERREUR : workers\pdf.cpp introuvable.
    goto :error
)

echo.
echo [1/6] Compilation de MangaDownloader.exe
%CXX% %FLAGS% src\app.cpp %LIBS% -lcurl -o dist\MangaDownloader.exe -lmingw32 -lSDL2main -lSDL2_ttf -lSDL2 -lole32 -lshell32 -luuid -lcomdlg32 -mwindows
if errorlevel 1 goto :error

echo.
echo [2/6] Compilation de domaine.exe
%CXX% %FLAGS% workers\domaine.cpp %LIBS% -lcurl -o dist\domaine.exe
if errorlevel 1 goto :error

echo.
echo [3/6] Compilation de preprocess.exe
%CXX% %FLAGS% workers\preprocess.cpp %LIBS% -lcurl -o dist\preprocess.exe
if errorlevel 1 goto :error

echo.
echo [4/6] Compilation de scraper.exe
%CXX% %FLAGS% workers\scraper.cpp %LIBS% -lcurl -o dist\scraper.exe
if errorlevel 1 goto :error

echo.
echo [5/6] Compilation de download.exe
%CXX% %FLAGS% workers\download.cpp %LIBS% -lcurl -o dist\download.exe
if errorlevel 1 goto :error

echo.
echo [6/6] Compilation de pdf.exe
%CXX% %FLAGS% workers\pdf.cpp %LIBS% -lhpdf -o dist\pdf.exe
if errorlevel 1 goto :error

echo.
echo ==========================================
echo COMPILATION TERMINEE AVEC SUCCES
echo ==========================================
echo.
echo Copie yoruka.ttf dans dist\data\font\
echo Copie les DLL SDL2, SDL2_ttf, libcurl et libhpdf necessaires dans dist\
echo Lance dist\MangaDownloader.exe
echo.
pause
exit /b 0

:error
echo.
echo ==========================================
echo ERREUR DE COMPILATION
echo Verifie les chemins, les headers et les bibliotheques.
echo ==========================================
pause
exit /b 1
