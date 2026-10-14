# Manga Downloader

Application Windows avec **une seule fenêtre** pour rechercher un manga, choisir ses chapitres, les télécharger et les convertir en PDF. Aucune console n'est visible : tout se fait dans l'interface.

> Version : v0.1.0

---

## Sommaire

1. [Utilisation](#1-utilisation)
2. [Installation](#2-installation)
3. [Dépannage](#3-dépannage)
4. [Partie développeur](#4-partie-développeur)

---

# 1. Utilisation

## Aperçu du parcours

```
Accueil → (Préparation) → Recherche → Chapitres → Téléchargement + PDF → Copie → Terminé
```

## Premier lancement

1. Lance `MangaDownloader.exe`.
2. Vérifie le **dossier de destination** (par défaut : `Documents\Manga`). Clique sur **Changer…** pour en choisir un autre.
3. Clique sur **Rechercher un manga**.
4. La base de recherche n'existe pas encore : l'application la crée automatiquement (écran *Préparation*, avec barre de progression). Cette étape n'est refaite que si tu le demandes.

## Rechercher et télécharger

1. **Recherche** : tape le nom du manga puis `Entrée` ou **Rechercher**. La liste défile à la molette.
2. Clique sur un résultat : l'application récupère la liste des chapitres.
3. **Chapitres** : coche ceux que tu veux (ou **Tout sélectionner** / **Tout désélectionner**), puis **Télécharger**.
4. **Téléchargement** : deux barres s'affichent, la progression des chapitres et celle des images du chapitre en cours. Ensuite vient l'étape « Création des PDF ».
5. **Copie** : les PDF sont copiés dans ton dossier de destination, sous la forme `Destination\Nom du manga\Chapter X.pdf`.
6. **Terminé** : clique sur **Ouvrir le dossier** ou retourne à l'accueil.

À chaque étape longue, **Annuler** arrête tout proprement.

## Paramètres (écran d'accueil)

| Paramètre | Rôle |
|---|---|
| **Dossier de destination** | Où sont copiés les PDF. Modifiable à la main (Ctrl+V pris en charge) ou via **Changer…**. Mémorisé automatiquement. |
| **Ouvrir le dossier à la fin** | Ouvre l'explorateur quand le téléchargement est terminé. Mémorisé aussi. |
| **Mettre à jour la base** | Supprime et recrée la base de recherche (utile si des mangas manquent ou si le site a changé d'adresse). |
| **Vider le cache** | Supprime les fichiers temporaires (voir ci-dessous). Un second clic dans les 3 secondes est demandé pour confirmer. |

### Que fait « Vider le cache » ?

Il supprime `temp/`, `scan/`, `data/temp/`, `data/result.json` et `data/status.json`. Il **conserve** la police et la base de recherche (`data/heritage.json`), donc tu n'as pas à refaire la préparation.

Annuler un téléchargement vide aussi le cache.

## Où sont mes fichiers ?

- Tes PDF : dans le **dossier de destination**.
- Tes réglages : `settings.json` (à côté de l'exe).
- Les fichiers de travail : `temp/`, `scan/` et `data/`, dans le dossier de l'application (nettoyés automatiquement).

---

# 2. Installation

## Contenu du dossier final

```
dist/
├── MangaDownloader.exe     ← l'application (à lancer)
├── domaine.exe             ┐
├── preprocess.exe          │ programmes lancés en arrière-plan
├── scraper.exe             │ (ne pas les lancer à la main)
├── download.exe            │
├── pdf.exe                 ┘
├── *.dll                   ← SDL2, SDL2_ttf, libcurl, libhpdf, ...
└── data/
    └── font/
        └── yoruka.ttf      ← police obligatoire
```

Tous les `.exe` doivent rester **dans le même dossier**.

## Compiler

Prérequis : MinGW (g++ avec C++17) et les bibliothèques **SDL2**, **SDL2_ttf**, **libcurl**, **libharu (hpdf)** et **nlohmann/json**.

```bat
build.bat
```

Le script crée `dist/` avec tous les exécutables. Si g++ ne trouve pas tes bibliothèques, ajoute les options `-I<dossier include>` et `-L<dossier lib>` dans `build.bat`. Ensuite :

1. Copie `yoruka.ttf` dans `dist\data\font\`.
2. Copie les DLL nécessaires à côté des `.exe`.
3. Lance `dist\MangaDownloader.exe`.

---

# 3. Dépannage

| Symptôme | Cause probable / solution |
|---|---|
| Message « Police introuvable » | `data/font/yoruka.ttf` est absent. |
| « Impossible de lancer domaine.exe » (ou autre) | Un `.exe` manque dans le dossier ou une DLL est absente. |
| Échec de la préparation | Pas d'accès à internet, ou le site est indisponible. Réessaie avec **Mettre à jour la base**. |
| La barre des images reste vide | `setvbuf(stdout, nullptr, _IONBF, 0);` manque dans `download.cpp`. |
| « Aucun PDF n'a été créé » | Regarde la dernière ligne du journal indiquée dans le message d'erreur. |
| Certains chapitres n'ont pas de PDF | L'écran final liste les chapitres manquants. Ouvre `last_run.log` (dossier de l'application) : il contient tout le journal de `download.exe` et `pdf.exe`. |
| Un manga n'apparaît pas | Fais **Mettre à jour la base**. |
| Disque qui se remplit | Clique sur **Vider le cache**. |

> Respecte les conditions d'utilisation des sites et les droits d'auteur. Cet outil est prévu pour un usage personnel.

---

# 4. Partie développeur

## 4.1 Architecture

```
MangaApp/
├── src/app.cpp          ← interface unique (SDL2) + orchestration
├── workers/
│   ├── domaine.cpp      → domaine.exe     (détecte l'extension du domaine)
│   ├── preprocess.cpp   → preprocess.exe  (construit la base de recherche)
│   ├── scraper.cpp      → scraper.exe     (liste les chapitres d'un manga)
│   ├── download.cpp     → download.exe    (télécharge les images)
│   └── pdf.cpp          → pdf.exe         (images → PDF)
└── build.bat
```

Principe : `app.cpp` ne fait **aucun scraping**. Il lance les programmes de `workers/` **cachés**, lit leur sortie standard en direct et met à jour l'interface. Chaque worker reste un programme console indépendant.

## 4.2 Enchaînement des workers

```
domaine.exe      → data/domaine.json
preprocess.exe   → data/heritage.json          (lit domaine.json)
scraper.exe -link "<url>"
                 → data/result.json + data/status.json
download.exe -all | -chapter [1, 2, 3]
                 → temp/<Manga>/Chapter N/001.jpg ...
   └─ lance pdf.exe
                 → scan/<Manga>/Chapter N.pdf
app (thread)     → copie scan/ vers le dossier de destination
```

Les workers utilisent des chemins **relatifs** (`data/...`, `temp/`, `scan/`). L'application fait donc `SetCurrentDirectory` vers le dossier de son exe au démarrage ; les workers héritent de ce dossier de travail.

## 4.3 Formats de fichiers

**`settings.json`** (hors de `data/`)
```json
{ "destination": "C:\\Users\\moi\\Documents\\Manga", "openWhenDone": true }
```

**`data/domaine.json`**
```json
{ "domaine": "<extension>" }
```

**`data/heritage.json`** (base de recherche)
```json
{ "scan": { "<identifiant>": ["<url>", "..."] } }
```

**`data/status.json`**
```json
{ "status": true }
```
`false` pendant le travail du scraper, `true` quand il a fini.

**`data/result.json`**
```json
{
  "results": [
    {
      "titre": "Nom du manga",
      "chapters": {
        "1": { "image_count": 20, "images": ["<url>", "..."] }
      }
    }
  ]
}
```

## 4.4 Protocole de progression

L'application ne reçoit que du texte. Elle interprète ces lignes :

| Worker | Ligne reconnue | Effet |
|---|---|---|
| `download.exe` | `[i/N] ...` | Met à jour la barre des images. Quand `i == 1`, un nouveau chapitre commence. |
| `download.exe` / `pdf.exe` | `MANGA -> PDF` ou `LANCEMENT DE PDF.EXE` | Passe en phase « Création des PDF ». |
| tous | n'importe quelle ligne | Affichée dans le journal (les codes ANSI sont retirés). |

Pourcentage de la barre des chapitres : `((chapitres_vus - 1) + image / total_images) / chapitres_sélectionnés`.

**Contrainte importante** : quand stdout est un pipe, la sortie est mise en tampon. Chaque worker commence donc son `main()` par :

```cpp
setvbuf(stdout, nullptr, _IONBF, 0);
```

Sans cette ligne, les barres n'avancent pas. Si tu modifies les messages de `download.cpp`, garde `[i/N]` en début de ligne.

## 4.5 Classe `Job` (processus cachés)

Dans `app.cpp`, `Job` encapsule un processus :

- `CreateProcessA` avec `CREATE_NO_WINDOW | CREATE_SUSPENDED` et stdout/stderr redirigés vers un pipe anonyme.
- Le processus est placé dans un **Job Object** Windows (`JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`), puis relancé (`ResumeThread`). Ainsi, `kill()` termine aussi les processus enfants (par exemple `pdf.exe` lancé par `download.exe`), et fermer la fenêtre ne laisse aucun processus orphelin.
- `pump()` est appelé à chaque frame : il lit le pipe sans bloquer (`PeekNamedPipe`), découpe en lignes (`log`, `fresh`) et détecte la fin du processus (`exitCode`).

Un seul `Job` est actif à la fois.

## 4.6 Machine à états de l'interface

```cpp
enum class Screen { Home, Prepare, Search, Scraping, Chapters,
                    Download, Copy, Done, Error };
```

La boucle principale fait toujours : événements → `app.update()` → `renderApp()`.

| Écran | `update()` | Transition |
|---|---|---|
| `Prepare` | surveille `domaine` puis `preprocess` | → `Search` (ou `Home` si mise à jour manuelle) |
| `Scraping` | surveille `scraper` ; réussite = code 0 **et** `status == true` | → `Chapters` ou `Error` |
| `Download` | parse les lignes ; à la fin, vérifie que `scan/` n'est pas vide | → `Copy` ou `Error` |
| `Copy` | attend la fin du thread de copie | → `Done` |

Les clics sont traités dans `onClick()` (un `switch` par écran), le dessin dans `renderApp()`.

## 4.6 bis Robustesse du téléchargement et des PDF

- `download.cpp` : 3 tentatives par image, et vérification des premiers octets du fichier (JPEG, PNG ou WEBP) pour rejeter les pages d'erreur HTML reçues avec un code 200.
- `pdf.cpp` : le vrai format est détecté par les premiers octets (JPEG ou PNG). Une image illisible est ignorée au lieu d'annuler tout le PDF du chapitre, et le PDF est enregistré s'il contient au moins une page.
- `app.cpp` : le journal complet de `download.exe` est écrit dans `last_run.log`, et les chapitres sélectionnés sans PDF dans `scan/` sont listés sur l'écran final.

## 4.7 Copie vers la destination

`startCopy()` lance un `std::thread` qui liste les fichiers de `scan/`, les copie un par un avec `fs::copy_file(..., overwrite_existing)` en conservant l'arborescence, puis supprime `scan/` et `temp/`. La progression passe par des `std::atomic<int>` (`copyDone`, `copyTotal`). Ce thread n'est jamais interrompu en cours de copie ; `stopAll()` attend sa fin avec `join()`.

## 4.8 Ajouter un écran ou une fonctionnalité

1. Ajoute une valeur à `enum class Screen`.
2. Déclare ses rectangles (constantes `R_...`) en haut du fichier.
3. Ajoute un `case` dans `onClick()` pour les clics.
4. Ajoute un `case` dans `renderApp()` pour le dessin.
5. Si l'écran surveille un processus, écris `updateXxx()` et branche-le dans `App::update()`.
6. Pour lancer un programme : `job.start(".\\mon_worker.exe args")`.

Composants réutilisables : `drawButton`, `drawProgress` (fraction < 0 = barre animée), `drawLog`, `drawScrollbar`, `fitHead` / `fitTail` (texte tronqué à la largeur), `clampScroll`.

## 4.9 Ajouter un worker

1. Écris un programme console qui lit/écrit ses fichiers en chemins relatifs.
2. Mets `setvbuf(stdout, nullptr, _IONBF, 0);` en début de `main()`.
3. Affiche la progression sous une forme parsable (ex. `[i/N]`).
4. Ajoute sa compilation dans `build.bat` et son parsing dans la méthode `update` correspondante.

## 4.10 Limites connues et pistes

- Les chapitres décimaux (ex. `2.5`) ne sont pas gérés par `download.cpp` : il convertit les numéros en entier (`std::stoi`), donc `2.5` est traité comme `2`. Ils apparaissent comme manquants à la fin.

- Seul le téléchargement affiche de vrais pourcentages ; les autres étapes ont une barre animée. Piste : faire afficher `[i/N]` aux autres workers.
- L'interface n'est pas adaptée au DPI élevé et la fenêtre a une taille fixe (1000×700).
- Le rendu recrée une texture par texte et par frame. C'est suffisant ici, mais un cache de textures serait plus économe.
- Fusionner tous les workers dans un seul `.exe` demanderait de transformer chaque `main()` en fonction avec annulation coopérative (drapeau `atomic<bool>`) et de remplacer `std::cout` par un canal de progression.
- Le code n'a pas encore été testé en conditions réelles sur toutes les étapes : signale les erreurs de compilation ou d'exécution pour les corriger.
