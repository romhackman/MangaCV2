# MangaCV2

![MangaCV2 Logo](https://github.com/romhackman/MangaCV2/blob/main/resources/logo_app.png?raw=true)

![Platform](https://img.shields.io/badge/platform-Windows-0078D6)
![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus)
![SDL2](https://img.shields.io/badge/GUI-SDL2-2E2E2E)
![License](https://img.shields.io/badge/license-MIT-green)
![Version](https://img.shields.io/badge/version-v0.1.1-blue)

Application Windows en C++ (interface SDL2) pour rechercher un manga, choisir ses chapitres, télécharger les images et créer un PDF par chapitre.

> **Version :** v0.2.0 — un seul exécutable, interface réorganisée, code découpé en modules.

---

## 1. Fonctionnement

Il n'y a plus qu'**un seul programme** : `MangaCV2.exe`. Les anciens `domaine.exe`, `preprocess.exe`, `scraper.exe`, `search.exe`, `download.exe` et `pdf.exe` sont devenus des modules internes qui tournent dans un thread, sans fenêtre de terminal.

Parcours habituel :

`Accueil → Recherche → Chapitres → Téléchargement / PDF / copie → Terminé`

(La préparation de la base n'a lieu que si tu choisis la recherche « Sitemap (base locale) » sans base existante.)

**Terminal :** double-clic = aucune console. Si tu lances `MangaCV2.exe` depuis cmd, PowerShell ou MSYS2, le journal s'affiche dans ce terminal.

### Écrans

- **Accueil** : le dossier de destination s'affiche en petit en haut ; le bouton **Paramètres** est en haut à droite. À gauche, les dossiers présents dans la destination ; un clic sur un dossier affiche ses PDF au centre ; un clic sur un PDF l'ouvre dans le navigateur par défaut. À droite, le bouton **Rechercher un manga**.
- **Paramètres** : dossier de destination, ouverture du dossier à la fin, mise à jour de la base de recherche, taille du cache et bouton pour le vider (deux clics pour confirmer).
- **Recherche** : deux sources exclusives, **Anime-Sama (direct)** ou **Sitemap (base locale)**.

## 2. Installation et compilation

### Prérequis (MSYS2 UCRT64)

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja \
          mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-SDL2_ttf \
          mingw-w64-ucrt-x86_64-curl mingw-w64-ucrt-x86_64-libharu mingw-w64-ucrt-x86_64-nlohmann-json
```

### Compiler

Dans le terminal **MSYS2 UCRT64**, à la racine du projet :

```bash
make check      # vérifie les outils
make            # compile -> dist/MangaCV2.exe
make static     # variante sans DLL (nécessite les bibliothèques statiques)
```

Sans `make` : `cmake -S . -B build -G Ninja && cmake --build build`.

Après la compilation :

1. Si tu as compilé en mode normal, les DLL de `/ucrt64/bin` (SDL2, SDL2_ttf, libcurl, libharu et leurs dépendances) doivent être à côté de l'exe ou dans le `PATH`.
2. La police `yoruka.ttf` va dans `dist/data/font/`. Si elle est absente, Segoe UI / Arial de Windows est utilisée.
3. `cacert.pem` à côté de l'exe est facultatif.
4. Une icône est ajoutée si `resources/app.ico` et `resources/app.rc` existent.

## 3. Utilisation

1. Lance `MangaCV2.exe`.
2. (Facultatif) **Paramètres** → change le dossier de destination.
3. Clique sur **Rechercher un manga**, saisis un titre, choisis un résultat (puis un scan : VF, VOSTFR…).
4. Coche les chapitres et clique sur **Télécharger**.
5. Les PDF sont copiés dans `<destination>/<manga>/Chapter N.pdf`, puis visibles sur l'accueil.

## 4. Architecture du code

```text
MangaCV2/
├── CMakeLists.txt  Makefile  README.md
├── include/                 en-têtes (.h)
├── src/                     implémentations (.cpp)
├── resources/               app.ico + app.rc (facultatif)
└── dist/                    sortie : MangaCV2.exe
```

| Module | Rôle |
|---|---|
| `main` | fenêtre SDL2, boucle d'événements |
| `app` | état global, enchaînement des écrans, lancement des tâches |
| `screens`, `screen_home`, `screen_search`, `screen_progress` | affichage et clics par écran |
| `gfx`, `layout`, `config` | dessin, positions, couleurs et constantes |
| `task` | thread d'arrière-plan + journal + progression + annulation |
| `http` | libcurl (connexion réutilisée, annulation) |
| `domain` | domaine courant du site (ex-`domaine.exe`) |
| `sitemap` | base de recherche locale (ex-`preprocess.exe`) |
| `search` | recherche directe et locale (ex-`search.exe`) |
| `scraper` | liste des chapitres (ex-`scraper.exe`) |
| `downloader` | téléchargement des images (ex-`download.exe`) |
| `pdf_builder` | images → PDF avec libharu (ex-`pdf.exe`) |
| `pipeline` | téléchargement → PDF → copie |
| `storage`, `settings`, `platform`, `utils`, `model` | fichiers/cache, réglages, API Windows, texte, structures |

Fichiers créés à côté de l'exe : `settings.json`, `data/domaine.json`, `data/heritage.json`, `last_run.log`, et temporairement `temp/` et `scan/`. L'ancien passage par `result.json`, `search.json` et `status.json` n'existe plus (les données restent en mémoire).

## 5. Dépannage

| Symptôme | Vérifications |
|---|---|
| `libharu introuvable` | `pacman -S mingw-w64-ucrt-x86_64-libharu` |
| DLL manquante au lancement | Copie-la à côté de l'exe, ajoute `/ucrt64/bin` au `PATH`, ou utilise `make static`. |
| Recherche directe sans résultat | Vérifie ta connexion ; le message d'erreur s'affiche sous la barre de recherche. |
| Aucun PDF créé | Consulte `last_run.log`. Les images WebP ou illisibles sont ignorées. |
| Pas de logs | Lance l'exe depuis un terminal (`./dist/MangaCV2.exe`). |
| La base ne contient pas un manga | Paramètres → **Mettre à jour la base**. |

Respecte les conditions d'utilisation des sites sources et les droits d'auteur. Utilise l'outil uniquement pour les contenus auxquels tu as le droit d'accéder.
