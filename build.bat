@echo off
setlocal
cd /d "%~dp0"

REM ==========================================================
REM  Compilation de Manga Downloader (MinGW / g++)
REM  Structure attendue :
REM    include\  lib\  src\  workers\  dist\
REM ==========================================================

set "CXX=g++"
set "FLAGS=-std=c++17 -O2 -Iinclude -I."
set "LIBS=-Llib"

if not exist dist mkdir dist
if not exist dist\data\font mkdir dist\data\font

REM Verifier la presence du compilateur
where g++ >nul 2>nul
if errorlevel 1 (
    echo ERREUR : g++ est introuvable. Verifie ton installation MinGW.
    goto :error
)

REM Verifier les dossiers du projet
if not exist src\app.cpp (
    echo ERREUR : src\app.cpp introuvable.
    goto :error
)
if not exist workers\domaine.cpp (
    echo ERREUR : workers\domaine.cpp introuvable.
    goto :error
)

echo.
echo [1/6] Compilation de MangaDownloader.exe
%CXX% %FLAGS% src\app.cpp %LIBS% -o dist\MangaDownloader.exe -lmingw32 -lSDL2main -lSDL2_ttf -lSDL2 -lole32 -lshell32 -luuid -mwindows
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
echo Place la police dans :
echo dist\data\font\yoruka.ttf
echo.
echo Place les DLL necessaires a cote des executables :
echo SDL2.dll, SDL2_ttf.dll, libcurl et libhpdf
echo.
echo Lance : dist\MangaDownloader.exe
echo.
pause
exit /b 0

:error
echo.
echo ==========================================
echo ERREUR DE COMPILATION
echo Verifie les chemins des fichiers et des bibliotheques.
echo ==========================================
pause
exit /b 1