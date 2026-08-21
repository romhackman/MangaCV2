# Anime-Sama Downloader

Application C++ sous Windows permettant de récupérer les informations d'un catalogue, de rechercher les scans disponibles, de sélectionner des chapitres, de télécharger les images puis de générer des PDF.

Le projet est organisé autour de plusieurs exécutables qui communiquent principalement grâce à des fichiers JSON dans le dossier `data/`.

> **Plateforme actuelle : Windows**
>
> Plusieurs parties du projet utilisent directement l'API Windows (`CreateProcessA`, `STARTUPINFOA`, `PROCESS_INFORMATION`, `WaitForSingleObject`, etc.).

---

## 📌 Fonctionnement général

Le programme complet fonctionne comme une chaîne :

```text
main.exe
   │
   ├── Nettoyage de data/
   │
   ▼
domaine.exe
   │
   │  Trouve l'extension actuelle du domaine
   ▼
data/domaine.json
   │
   ▼
preprocess.exe
   │
   ├── Récupération du sitemap
   ├── Extraction des URLs
   ├── Filtrage des URLs catalogue/scan
   └── Construction de la hiérarchie
   │
   ▼
data/heritage.json
   │
   ▼
gui.exe
   │
   ├── Recherche d'un anime
   ├── Sélection d'un lien de scan
   └── Lance scraper.exe
   │
   ▼
scraper.exe
   │
   ├── Récupère les informations d'un scan
   └── Génère data/result.json
   │
   ▼
chapter.exe
   │
   ├── Lit result.json
   ├── Affiche les chapitres
   ├── Permet leur sélection
   └── Lance download.exe
   │
   ▼
download.exe
   │
   ├── Télécharge les images JPG
   └── Lance pdf.exe
   │
   ▼
pdf.exe
   │
   └── Génère les PDF à partir des images
```

Le lanceur principal exécute actuellement `domaine.exe`, puis `preprocess.exe`, puis `gui.exe`, puis `chapter.exe`, en attendant la fin de chaque programme.

---

# 📁 Structure du projet

Une structure attendue ressemble à ceci :

```text
.
├── main.cpp
├── domaine.cpp
├── sitemap.cpp
├── scraper.cpp
├── gui.cpp
├── chapter.cpp
├── download.cpp
├── pdf.cpp
│
├── domaine.exe
├── preprocess.exe
├── gui.exe
├── chapter.exe
├── scraper.exe
├── download.exe
└── pdf.exe
│
├── data/
│   ├── domaine.json
│   ├── heritage.json
│   ├── result.json
│   ├── status.json
│   └── temp/
│       ├── sitemap_1.xml
│       ├── all_link.json
│       └── scan_colecte.json
│
└── data/font/
    └── yoruka.ttf
```

Les fichiers temporaires utilisés par le module sitemap sont notamment `data/temp/sitemap_1.xml`, `data/temp/all_link.json` et `data/temp/scan_colecte.json`. Le résultat final de cette étape est `data/heritage.json`.

---

# ⚙️ Dépendances

## 1. C++ / compilateur

Le projet nécessite un compilateur C++ moderne avec support de :

* C++17 minimum recommandé
* `<filesystem>`
* `<fstream>`
* `<vector>`
* `<string>`
* `<algorithm>`
* etc.

Le projet utilise `std::filesystem`, notamment pour manipuler les dossiers et fichiers.

### Compilateur recommandé

Sous Windows :

* **MinGW-w64 / GCC**
* ou **Visual Studio / MSVC**

---

# 📦 Bibliothèques externes

## 2. libcurl

**libcurl** est utilisé pour effectuer les requêtes HTTP/HTTPS et télécharger les fichiers.

Elle est utilisée notamment dans :

* `domaine.cpp`
* `sitemap.cpp`
* `scraper.cpp`
* `download.cpp`

Exemple dans `download.cpp` :

```cpp
#include <curl/curl.h>
```

Le téléchargement d'image utilise `curl_easy_init()`, `curl_easy_setopt()` puis `curl_easy_perform()`.

### Rôle

```text
Internet
   │
   ▼
libcurl
   │
   ├── GET HTML
   ├── GET sitemap
   ├── GET pages de scan
   └── téléchargement des images
```

---

## 3. nlohmann/json

Le projet utilise **nlohmann/json** pour lire et écrire les fichiers JSON.

```cpp
#include <nlohmann/json.hpp>

using json = nlohmann::json;
```

Elle est utilisée dans plusieurs exécutables pour communiquer entre les différents composants.

Principaux fichiers concernés :

```text
data/domaine.json
data/heritage.json
data/result.json
data/status.json
data/temp/all_link.json
data/temp/scan_colecte.json
```

Par exemple, `scraper.cpp` charge `data/domaine.json`, vérifie la clé `domaine` et construit ensuite l'URL de base sous la forme `https://anime-sama.<extension>`.

---

## 4. SDL2

**SDL2** est utilisé pour créer l'interface graphique.

`chapter.cpp` et `gui.cpp` utilisent notamment :

```cpp
#include <SDL2/SDL.h>
```

SDL est initialisé pour créer la fenêtre et gérer les événements graphiques. `chapter.cpp` initialise également le système vidéo avec `SDL_INIT_VIDEO`.

---

## 5. SDL2_ttf

**SDL_ttf** est utilisé pour afficher du texte avec des polices TrueType.

```cpp
#include <SDL2/SDL_ttf.h>
```

Le programme charge notamment :

```text
data/font/yoruka.ttf
```

et utilise `TTF_OpenFont()` pour charger la police.

### Dépendance supplémentaire

SDL2_ttf dépend lui-même de SDL2.

```text
SDL2
  ▲
  │
SDL2_ttf
```

---

## 6. libharu

**libharu** est utilisée par `pdf.cpp` pour créer les PDF.

```cpp
#include <hpdf.h>
```

Le programme :

1. cherche les fichiers JPG ;
2. trie les pages ;
3. crée un document PDF ;
4. charge chaque JPG ;
5. crée une page ;
6. place l'image dans la page ;
7. sauvegarde le PDF.

Les images sont redimensionnées avec une taille maximale de :

```text
1000 × 1400
```

sans agrandir les images plus petites.

---

# 🧩 Résumé des dépendances

| Dépendance        | Utilisation                  | Fichiers concernés                                                                    |
| ----------------- | ---------------------------- | ------------------------------------------------------------------------------------- |
| **C++17**         | Langage / STL                | Tous                                                                                  |
| **libcurl**       | HTTP, HTTPS, téléchargements | `domaine.cpp`, `sitemap.cpp`, `scraper.cpp`, `download.cpp`                           |
| **nlohmann/json** | Lecture/écriture JSON        | `domaine.cpp`, `sitemap.cpp`, `scraper.cpp`, `gui.cpp`, `chapter.cpp`, `download.cpp` |
| **SDL2**          | Fenêtres / interface         | `gui.cpp`, `chapter.cpp`                                                              |
| **SDL2_ttf**      | Texte dans l'interface       | `gui.cpp`, `chapter.cpp`                                                              |
| **libharu**       | Création des PDF             | `pdf.cpp`                                                                             |
| **Windows API**   | Lancement des `.exe`         | `main.cpp`, `gui.cpp`, `chapter.cpp`                                                  |
| **yoruka.ttf**    | Police graphique             | `gui.cpp`, `chapter.cpp`                                                              |

---

# 🔄 1. `main.exe`

`main.cpp` est le lanceur général.

Avant de lancer les autres programmes, il nettoie le dossier `data/`, en conservant uniquement :

```text
data/font/
```

Il lance ensuite les programmes dans cet ordre :

```text
1. domaine.exe
2. preprocess.exe
3. gui.exe
4. chapter.exe
```

Chaque programme est lancé avec `CreateProcessA()` puis le programme principal attend sa fin avec `WaitForSingleObject()`.

### Important

Le code fourni appelle :

```text
preprocess.exe
```

mais aucun `preprocess.cpp` n'a été fourni avec les sources présentes dans ce dépôt.

Le fonctionnement exact de cet exécutable doit donc être considéré comme **non documenté ici**.

---

# 🌐 2. `domaine.exe`

`domaine.cpp` sert à déterminer l'extension actuellement utilisée par le domaine.

Il commence par contacter :

```text
https://anime-sama.pw
```

puis recherche le lien principal et récupère l'URL finale. Il extrait ensuite l'extension du domaine.
Le résultat est enregistré dans :

```text
data/domaine.json
```

Le reste du projet peut ensuite utiliser cette information sans avoir à modifier directement le code source.

Le format attendu est conceptuellement :

```json
{
    "domaine": "to"
}
```

Le scraper transforme ensuite cette valeur en :

```text
https://anime-sama.to
```

par exemple.

---

# 🗺️ 3. `sitemap.exe` / étape sitemap

`sitemap.cpp` récupère le sitemap du domaine et construit une liste d'URLs.

Les fichiers utilisés sont :

```text
data/temp/sitemap_1.xml
data/temp/all_link.json
data/temp/scan_colecte.json
data/heritage.json
```

Le programme extrait les URLs `<loc>` du sitemap.

Il conserve ensuite principalement les URLs appartenant à :

```text
/catalogue/
```

et reconnaît notamment les URLs de scans.

---

# 🌳 4. Construction de `heritage.json`

Le sitemap permet de construire une relation entre :

```text
Anime
 └── Scans
      ├── VF
      ├── VOSTFR
      └── ...
```

Le programme distingue les URLs parent :

```text
/catalogue/one-piece/
```

des URLs enfants :

```text
/catalogue/one-piece/scan/vf/
```

puis construit la structure :

```json
{
    "scan": {
        "one-piece": [
            "https://...",
            "https://..."
        ]
    }
}
```

Cette hiérarchie est sauvegardée dans :

```text
data/heritage.json
```

---

# 🖥️ 5. `gui.exe`

`gui.cpp` fournit l'interface graphique permettant de rechercher un anime.

Il utilise :

```text
SDL2
SDL2_ttf
nlohmann/json
```

et lit :

```text
data/heritage.json
data/status.json
```

L'interface peut rechercher dans les données de `heritage.json`, puis lancer le scraper pour récupérer les informations du scan sélectionné.

---

# 🔎 6. `scraper.exe`

`scraper.cpp` reçoit une URL avec :

```bash
scraper.exe -link "URL"
```

Le programme vérifie l'option :

```text
-link
```

et valide que l'URL fournie est correcte.

Il utilise ensuite :

```text
libcurl
nlohmann/json
```

pour récupérer la page et analyser les informations disponibles.

Le résultat final est écrit dans :

```text
data/result.json
```

---

# 📚 7. `chapter.exe`

`chapter.cpp` lit :

```text
data/result.json
```

puis extrait les chapitres présents dans la clé :

```json
{
    "results": [...]
}
```

Les chapitres sont ensuite triés numériquement :

```text
1
2
3
10
11
```

plutôt que :

```text
1
10
11
2
3
```

L'interface permet ensuite de sélectionner les chapitres à télécharger.

Le programme utilise :

```text
SDL2
SDL2_ttf
nlohmann/json
Windows API
```

---

# ⬇️ 8. `download.exe`

`download.exe` utilise :

```bash
download.exe -all
```

pour télécharger tous les chapitres.

Ou :

```bash
download.exe -chapter "[1,2,3]"
```

pour télécharger uniquement certains chapitres.

Le programme lit :

```text
data/result.json
```

puis parcourt les résultats et crée des dossiers temporaires pour les anime.

Les images sont téléchargées avec `libcurl`.

Le téléchargement suit également les redirections HTTP et possède un timeout de 30 secondes.

---

# 📄 9. `pdf.exe`

Une fois les images téléchargées, `download.exe` lance :

```text
pdf.exe
```

Le programme `pdf.cpp` recherche les JPG dans les dossiers de chapitres.

Les images sont triées numériquement :

```text
1.jpg
2.jpg
3.jpg
...
10.jpg
```

Chaque image devient une page du PDF.

Le PDF est ensuite sauvegardé dans le chemin de sortie prévu par le programme.

`download.exe` recherche `pdf.exe` dans le même dossier que lui.

---

# 📂 Fichiers JSON

## `data/domaine.json`

Contient l'extension du domaine.

Exemple :

```json
{
    "domaine": "to"
}
```

---

## `data/temp/all_link.json`

Contient les URLs récupérées depuis le sitemap.

Structure générale :

```json
{
    "link": [
        "https://...",
        "https://..."
    ]
}
```

---

## `data/temp/scan_colecte.json`

Contient les URLs filtrées correspondant aux pages de scans.

---

## `data/heritage.json`

Contient la hiérarchie anime → scans.

Exemple conceptuel :

```json
{
    "scan": {
        "anime-name": [
            "https://anime-sama.../catalogue/anime-name/scan/vf/"
        ]
    }
}
```

---

## `data/result.json`

Contient les informations récupérées par `scraper.exe`.

`chapter.exe` attend notamment une structure contenant :

```json
{
    "results": [
        {
            "titre": "...",
            "chapters": {}
        }
    ]
}
```

Le code de `chapter.cpp` vérifie la présence de `results` et extrait les chapitres à partir de cette structure.

---

## `data/status.json`

Utilisé pour communiquer un état entre les programmes.

`gui.cpp` initialise notamment :

```json
{
    "status": false
}
```

puis peut lire cette valeur pour déterminer si une opération est terminée.

---

# 🖋️ Police

Le projet attend :

```text
data/font/yoruka.ttf
```

Cette police est chargée par l'interface avec SDL_ttf.

Il faut donc conserver cette arborescence :

```text
data/
└── font/
    └── yoruka.ttf
```

---

# 🛠️ Compilation

Aucun `CMakeLists.txt`, `Makefile` ou autre système de build n'a été fourni dans les fichiers disponibles.

Il faut donc configurer manuellement le linker et les include paths selon l'environnement utilisé.

## Bibliothèques à rendre disponibles

```text
SDL2
SDL2_ttf
libcurl
nlohmann/json
libharu
```

Avec un compilateur Windows, il faudra également avoir accès aux bibliothèques système nécessaires à l'utilisation de l'API Windows.

---

# ⚠️ Points importants

### Windows uniquement

Certaines parties du projet contiennent explicitement :

```cpp
#include <windows.h>
```

et utilisent :

```cpp
CreateProcessA()
WaitForSingleObject()
CloseHandle()
```

Le projet n'est donc pas actuellement portable tel quel sous Linux ou macOS.

---

### `preprocess.cpp` manquant

Le lanceur principal exécute :

```text
preprocess.exe
```

mais le fichier source correspondant n'est pas présent parmi les sources fournies.

Il faudra donc ajouter/reconstituer ce programme pour obtenir une chaîne complète et compilable depuis les sources présentes.

---

### Les exécutables doivent être correctement placés

Plusieurs programmes lancent directement d'autres `.exe`.

Par exemple :

```text
chapter.exe
   └── download.exe
          └── pdf.exe
```

`download.exe` cherche explicitement `pdf.exe` dans son propre dossier.

Il est donc recommandé de conserver tous les exécutables au même niveau :

```text
project/
├── main.exe
├── domaine.exe
├── preprocess.exe
├── gui.exe
├── scraper.exe
├── chapter.exe
├── download.exe
└── pdf.exe
```

---

# 🚀 Utilisation

Une fois tous les exécutables compilés et les dépendances installées :

```bash
main.exe
```

Le programme effectue alors automatiquement :

```text
Nettoyage
   ↓
Détection du domaine
   ↓
Prétraitement / sitemap
   ↓
Interface graphique
   ↓
Recherche
   ↓
Sélection d'un scan
   ↓
Scraping
   ↓
Sélection des chapitres
   ↓
Téléchargement
   ↓
Génération des PDF
```

---

# 🧪 Utilisation manuelle

Les programmes peuvent également être utilisés individuellement.

### Scraper

```bash
scraper.exe -link "https://anime-sama.to/catalogue/..."
```

Le format `-link` est celui prévu par `scraper.cpp`.

### Télécharger tous les chapitres

```bash
download.exe -all
```

### Télécharger certains chapitres

```bash
download.exe -chapter "[1,2,3]"
```

Ces deux modes sont directement prévus dans `download.cpp`.

---

# 🧱 Architecture

Le projet peut être résumé en quatre grandes couches :

```text
┌──────────────────────────────┐
│          INTERFACE           │
│       gui.exe / chapter.exe  │
└──────────────┬───────────────┘
               │
               ▼
┌──────────────────────────────┐
│          DONNÉES             │
│         fichiers JSON        │
│                              │
│ domaine.json                 │
│ heritage.json                │
│ result.json                  │
│ status.json                  │
└──────────────┬───────────────┘
               │
               ▼
┌──────────────────────────────┐
│          SCRAPING            │
│ domaine / sitemap / scraper  │
│            libcurl            │
└──────────────┬───────────────┘
               │
               ▼
┌──────────────────────────────┐
│       TÉLÉCHARGEMENT         │
│          download.exe        │
│            ↓                 │
│           pdf.exe            │
│          libharu             │
└──────────────────────────────┘
```

---

# 📋 Sources fournies

| Source         | Fonction                                |
| -------------- | --------------------------------------- |
| `main.cpp`     | Lanceur principal et orchestration      |
| `domaine.cpp`  | Détection de l'extension du domaine     |
| `sitemap.cpp`  | Récupération et organisation du sitemap |
| `scraper.cpp`  | Scraping d'un scan                      |
| `gui.cpp`      | Interface graphique / recherche         |
| `chapter.cpp`  | Sélection des chapitres                 |
| `download.cpp` | Téléchargement des images               |
| `pdf.cpp`      | Génération des PDF                      |

---

# 📜 Licence

Aucune licence n'est définie dans les sources fournies.

Ajoutez un fichier `LICENSE` à la racine du dépôt si vous souhaitez définir les conditions de réutilisation du projet.

---

# ⚠️ Utilisation responsable

Ce logiciel effectue des requêtes réseau et télécharge des ressources depuis des sites externes.

Utilisez-le uniquement lorsque vous disposez des droits nécessaires pour accéder, télécharger et conserver les contenus concernés, et respectez les conditions d'utilisation des services auxquels vous vous connectez.
