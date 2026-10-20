# Manga Downloader

Application Windows en C++ avec interface SDL2 pour rechercher un manga, sélectionner ses chapitres, télécharger les images et créer un PDF par chapitre.

> **Version :** v0.1.1 — correctif de compilation du worker PDF

## Sommaire

1. [Fonctionnement](#1-fonctionnement)
2. [Installation et compilation](#2-installation-et-compilation)
3. [Utilisation](#3-utilisation)
4. [Fichiers et architecture](#4-fichiers-et-architecture)
5. [Dépannage](#5-dépannage)

---

## 1. Fonctionnement

Parcours habituel :

`Accueil → Préparation de la base → Recherche → Chapitres → Téléchargement et PDF → Copie → Terminé`

L'application possède une interface unique. Les programmes de travail (`domaine.exe`, `preprocess.exe`, `scraper.exe`, `download.exe` et `pdf.exe`) sont lancés en arrière-plan.

Dans l’écran Recherche, deux options exclusives permettent de choisir la source : **Anime-Sama (direct)** interroge le formulaire de recherche du site puis permet de choisir un scan ; **Sitemap (base locale)** recherche dans la base générée depuis le sitemap. La recherche directe utilise libcurl depuis l’application, tandis que le mode sitemap utilise `data/heritage.json`.

## 2. Installation et compilation

### Prérequis

- Windows
- MinGW-w64 avec `g++` et prise en charge de C++17
- SDL2 et SDL2_ttf
- libcurl (en-têtes disponibles pour `src/app.cpp` et les workers)
- libharu (en-tête `hpdf.h` et bibliothèque `libhpdf`)
- nlohmann/json
- La police `yoruka.ttf`

Les dossiers `include\` et `lib\` doivent contenir les en-têtes et bibliothèques correspondants. Les DLL requises par les bibliothèques doivent être accessibles à côté des exécutables ou via `PATH`.

### Compiler

Ouvre un terminal dans le dossier du projet puis exécute :

```bat
build.bat
```

Le script compile successivement les six exécutables et s'arrête dès qu'une étape échoue. Les exécutables sont placés dans `dist\`.

Structure attendue :

```text
MangaDownloader/
├── build.bat
├── include/
├── lib/
├── src/
│   └── app.cpp
├── workers/
│   ├── domaine.cpp
│   ├── preprocess.cpp
│   ├── scraper.cpp
│   ├── download.cpp
│   └── pdf.cpp
└── dist/
    ├── MangaDownloader.exe
    ├── domaine.exe
    ├── preprocess.exe
    ├── scraper.exe
    ├── download.exe
    ├── pdf.exe
    └── data/
        └── font/
            └── yoruka.ttf
```

Après la compilation :

1. Place `yoruka.ttf` dans `dist\data\font\`.
2. Copie les DLL nécessaires dans `dist\` si elles ne sont pas déjà trouvées par Windows.
3. Lance `dist\MangaDownloader.exe`.

> Le script compile les sources, mais ne peut pas fournir automatiquement les bibliothèques ou DLL absentes de ton installation MinGW.

## 3. Utilisation

1. Lance `MangaDownloader.exe`.
2. Choisis le dossier où enregistrer les PDF. Le chemin peut être modifié depuis l'accueil.
3. Clique sur **Rechercher un manga**.
4. Lors du premier lancement, l'application prépare sa base de recherche.
5. Saisis le titre et lance la recherche.
6. Sélectionne un résultat pour charger ses chapitres.
7. Coche les chapitres voulus, puis lance le téléchargement.
8. À la fin, les PDF sont copiés dans le dossier de destination, en conservant un dossier par manga.

Les options **Mettre à jour la base** et **Vider le cache**, lorsqu'elles sont disponibles dans l'interface, servent respectivement à reconstruire la base et à supprimer les fichiers temporaires. La base de recherche est conservée lors du vidage du cache.

## 4. Fichiers et architecture

| Fichier | Rôle |
|---|---|
| `src/app.cpp` | Interface SDL2 et orchestration des tâches |
| `workers/domaine.cpp` | Détermine le domaine utilisé par la source |
| `workers/preprocess.cpp` | Prépare la base de recherche depuis les données du sitemap |
| `workers/scraper.cpp` | Récupère la liste des chapitres d'une URL sélectionnée |
| `workers/download.cpp` | Télécharge les images des chapitres |
| `workers/pdf.cpp` | Convertit les images JPG/JPEG en PDF |
| `build.bat` | Compile les exécutables |

Les workers utilisent des chemins relatifs (`data\`, `temp\`, `scan\`). Il faut donc conserver la structure du dossier de l'application et lancer les exécutables depuis leur dossier prévu.

### Correctifs du worker PDF

Le fichier `workers/pdf.cpp` a été corrigé sur les points qui bloquaient la compilation :

- les chaînes et le caractère de fin de ligne coupés sur plusieurs lignes sont redevenus des littéraux C++ valides ;
- l'appel à `HPDF_GetPageCount`, qui n'est pas disponible dans certaines versions de libharu, a été remplacé par un compteur de pages créées par le programme ;
- le PDF n'est sauvegardé que si au moins une page a été créée.

## 5. Dépannage

| Message ou symptôme | Vérifications |
|---|---|
| `g++` introuvable | Vérifie l'installation MinGW-w64 et la variable `PATH`. |
| `hpdf.h` introuvable | Vérifie que les en-têtes libharu sont dans `include\` ou adapte les options `-I` de `build.bat`. |
| `cannot find -lhpdf` | Vérifie que la bibliothèque libharu est dans `lib\` et correspond à l'architecture de MinGW. |
| DLL manquante au lancement | Place la DLL correspondante à côté des exécutables ou ajoute son dossier au `PATH`. |
| Police introuvable | Place `yoruka.ttf` dans `dist\data\font\`. |
| La base ne contient pas un manga | Mets à jour la base et vérifie que le sitemap/source est accessible. |
| Aucun PDF créé | Vérifie que des images JPG/JPEG valides ont été téléchargées et consulte le journal de la dernière exécution. |
| Certaines images sont ignorées | Le worker PDF ignore les images illisibles ou dans un format non pris en charge. |

Respecte les conditions d'utilisation des sites sources et les droits d'auteur applicables. Utilise l'outil uniquement pour les contenus auxquels tu as le droit d'accéder.
