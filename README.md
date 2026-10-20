# Manga Downloader

![Platform](https://img.shields.io/badge/platform-Windows-0078D6)
![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus)
![SDL2](https://img.shields.io/badge/GUI-SDL2-2E2E2E)
![License](https://img.shields.io/badge/license-MIT-green)
![Version](https://img.shields.io/badge/version-v0.1.1-blue)

A Windows desktop application written in C++ with an SDL2 interface. Search for a manga, select its chapters, download the images, and generate one PDF per chapter.

> **Current version:** v0.1.1

---

## Table of Contents

1. [Features](#1-features)
2. [What's New](#2-whats-new)
3. [Downloading](#3-downloading)
4. [How It Works](#4-how-it-works)
5. [Usage](#5-usage)
6. [Building from Source (Developers)](#6-building-from-source-developers)
7. [Architecture](#7-architecture)
8. [Troubleshooting](#8-troubleshooting)
9. [Legal Disclaimer](#9-legal-disclaimer)
10. [License](#10-license)

---

## 1. Features

- **Two search sources**, selected with exclusive options:
  - **Anime-Sama (direct):** queries the site's search endpoint live, then lets you pick a scan panel for the selected title.
  - **Sitemap (local base):** searches a local database (`data/heritage.json`) generated from the site's sitemap. Once the base is built, searches run without contacting the site.
- **Chapter selection** with "select all" and "deselect all".
- **One PDF per chapter**, grouped in one folder per manga.
- **Live progress** for the preparation, scraping, download, PDF conversion, and copy steps.
- **Cancel at any time.** Cancelling terminates the whole process tree, including child processes.
- **Persistent settings** (destination folder and "open folder when done").
- **Cache management** with a two-click confirmation before clearing.
- **Run log** written to `last_run.log` after each download.

## 2. What's New

- **Direct Anime-Sama search** using libcurl, with scan panel selection.
- **Source selector** on the search screen, switchable at any time.
- **Automatic copy** of finished PDFs to your chosen destination, keeping one folder per manga.
- **Persistent settings** saved to `settings.json`.
- **Process-tree cancellation** using a Windows Job Object, so no worker process is left running.
- **Download retries:** each image is attempted up to 3 times and checked by its file signature, so HTML error pages returned with HTTP 200 are rejected.
- **Resume support:** images already on disk are skipped.
- **Missing-chapter report** on the final screen when some PDFs could not be created, with details in `last_run.log`.
- **PNG and WebP detection** in the download step, and PNG support in the PDF worker. The format is detected from the file contents, not the extension.
- **Automatic scaling** of pages to a maximum of 1000 × 1400 px, without enlarging small images.
- **PDF worker fixes:** corrected string literals that broke compilation, and a page counter replaces `HPDF_GetPageCount`, which is missing from some libharu builds. A PDF is only saved if at least one page was created.
- **Cache clearing** that keeps the search base (`heritage.json`) and the font.

## 3. Downloading

Go to the **[Releases](../../releases)** page of this repository and download the file you need:

| File | Who it's for | Content |
|---|---|---|
| `MangaCV2_win_vx.x.x.7z` | **End users** | The ready-to-run application. Extract it and launch `MangaDownloader.exe`. |
| `MangaCV2_vx.x.x.7z` | **Developers** | The source code, plus the `include/`, `lib/`, and `bin/` folders with the dependencies needed to compile. |

Replace `x.x.x` with the version number of the release.

To use the application, you only need the `win` archive. Extract it with [7-Zip](https://www.7-zip.org/) or any tool that supports `.7z`.

## 4. How It Works

### Overview

```
Home → Search base preparation → Search → Chapter list → Download & PDF → Copy → Done
```

The interface is a single window. The processing is done by five helper programs (workers) that run hidden in the background. The application reads their output in real time to update the progress bars. You never see their console windows.

| Worker | Role |
|---|---|
| `domaine.exe` | Finds the current domain used by the source site. |
| `preprocess.exe` | Downloads the sitemap and builds the local search base (`data/heritage.json`). |
| `scraper.exe` | Reads a selected manga's page and lists its chapters and image counts. |
| `download.exe` | Downloads the chapter images into `temp\`, then launches `pdf.exe`. |
| `pdf.exe` | Converts each chapter's images into a PDF in `scan\`, then deletes `temp\`. |

### Search modes

**Anime-Sama (direct):**
1. You type a title. The app sends it to the site's search endpoint.
2. The results are listed. Selecting one loads the scan panels available for that title.
3. Selecting a scan panel starts `scraper.exe` on its URL.

**Sitemap (local base):**
1. On first use, `domaine.exe` finds the active domain, then `preprocess.exe` downloads the sitemap.
2. The sitemap is filtered to keep only manga and scan pages, and duplicates are removed.
3. The result is saved as `data/heritage.json`, which maps each manga to its scan URLs.
4. Your search runs against this local file. Nothing is downloaded until you select a manga.

The local base is built once. Use **Mettre à jour la base** (Update the base) on the home screen to rebuild it when the site changes.

### Chapter retrieval

After you select a scan:
1. `scraper.exe` reads the title from the page. If it cannot find one, it uses the identifier from the URL.
2. It asks the site's API for the number of chapters and images per chapter.
3. It saves the results to `data/result.json` and sets `data/status.json` to `true` on success.
4. The app loads the chapters, sorts them numerically, and shows them in the list.

### Download and PDF creation

When you click **Télécharger** (Download):
1. The app clears the previous `temp\` and `scan\` folders.
2. If you selected every chapter, it runs `download.exe -all`. Otherwise it runs `download.exe -chapter [n, m, ...]`.
3. For each chapter, `download.exe` creates `temp\<Manga>\Chapter N\` and downloads each image as `001.jpg`, `002.jpg`, and so on.
4. Each image is retried up to 3 times. An image is accepted only if its file signature matches JPEG, PNG, or WebP.
5. Images already present are skipped, so an interrupted download can resume.
6. `download.exe` then runs `pdf.exe`. It creates one PDF per chapter at `scan\<Manga>\Chapter N.pdf`, scaling pages to fit within 1000 × 1400 px.
7. Unreadable images are skipped, and the chapter is still created from the remaining pages. If no page can be created, no PDF is saved.
8. When `pdf.exe` finishes, it deletes `temp\`.
9. The complete output is saved to `last_run.log`.

### Copy and cleanup

When the workers finish:
1. The app checks which chapters have a PDF. Any missing ones are listed on the final screen.
2. It copies everything in `scan\` to your destination folder, keeping the structure: `<Destination>\<Manga>\Chapter N.pdf`.
3. It deletes `scan\` and `temp\`.
4. If **Ouvrir le dossier à la fin du téléchargement** is enabled, it opens the destination folder.

### Cancelling

Pressing **Annuler** (Cancel) at any stage terminates every worker through the Job Object. Cancelling during a download also clears the cache. Cancelling a search or preparation only returns you to the previous screen.

## 5. Usage

1. Launch `MangaDownloader.exe`.
2. On the home screen, choose the destination folder. You can change it at any time.
3. Click **Rechercher un manga** (Search for a manga).
4. Choose a source:
   - **Anime-Sama (direct):** type a title, select a result, then select a scan panel.
   - **Sitemap (local base):** type a title. The base is built automatically on first use.
5. Select a manga from the results to load its chapters.
6. Tick the chapters you want, or use **Tout sélectionner** (Select all).
7. Click **Télécharger** (Download).
8. When the process finishes, the PDFs are copied to your destination folder.

Home screen options:

| Option | Effect |
|---|---|
| **Changer...** (Change...) | Chooses a new destination folder. |
| **Ouvrir le dossier à la fin du téléchargement** | Opens the destination folder when a download finishes. |
| **Mettre à jour la base** (Update the base) | Rebuilds the sitemap search base. |
| **Vider le cache** (Clear cache) | Deletes temporary files. Click twice within 3 seconds to confirm. The search base and font are kept. |

## 6. Building from Source (Developers)

Download the `MangaCV2_vx.x.x.7z` archive from the Releases page. It contains the source code, `include/`, `lib/`, and `bin/`. Extract it, open a terminal in the project folder, and run:

```bat
build.bat
```

The script compiles the six executables in order and stops at the first failure. Output goes to `dist\`.

Requirements for building:

- Windows
- MinGW-w64 with `g++` and C++17 support, available in your `PATH`
- SDL2 and SDL2_ttf
- libcurl
- libharu (`hpdf.h` and `libhpdf`)
- nlohmann/json
- The font `yoruka.ttf`

After building:

1. Place `yoruka.ttf` in `dist\data\font\`.
2. Copy the required DLLs (SDL2, SDL2_ttf, libcurl, libhpdf) into `dist\` if Windows cannot find them already. The `bin/` folder of the archive contains them.
3. Run `dist\MangaDownloader.exe`.

> The build script compiles the sources, but it cannot supply libraries or DLLs that are missing from your MinGW installation.

Workers use relative paths (`data\`, `temp\`, `scan\`). Keep the folder structure intact and run the executables from their expected location.

## 7. Architecture

| File | Role |
|---|---|
| `src/app.cpp` | SDL2 interface, task orchestration, copy to destination, settings, cache |
| `workers/domaine.cpp` | Determines the domain used by the source |
| `workers/preprocess.cpp` | Builds the search base from the sitemap |
| `workers/scraper.cpp` | Retrieves the chapter list and image counts for a selected scan |
| `workers/download.cpp` | Downloads chapter images with retries and validation, then launches `pdf.exe` |
| `workers/pdf.cpp` | Converts JPG and PNG images into one PDF per chapter |
| `build.bat` | Compiles all executables into `dist\` |

Runtime files:

| Path | Content |
|---|---|
| `settings.json` | Destination folder and "open when done" option |
| `data/domaine.json` | Domain detected by `domaine.exe` |
| `data/heritage.json` | Local search base (sitemap mode) |
| `data/result.json`, `data/status.json` | Output of the scraper |
| `temp\` | Downloaded images, deleted after PDF creation |
| `scan\` | Generated PDFs, copied to the destination and then deleted |
| `last_run.log` | Full log of the last download run |

## 8. Troubleshooting

| Message or symptom | What to check |
|---|---|
| Application does not start | Make sure you extracted the whole archive and kept its folder structure. |
| Font not found | Place `yoruka.ttf` in `data\font\` next to the application. |
| Anime-Sama search returns nothing | Check your internet connection and whether the site is reachable, then try the Sitemap source. |
| "Aucun scan trouvé" (no scan found) | This title has no scan panel on that source. Try the other source. |
| Manga missing from the sitemap base | Click **Mettre à jour la base** and confirm the sitemap is accessible. |
| "Aucun PDF n'a été créé" (no PDF created) | Read the last line of `last_run.log`. It usually indicates a download or network problem. |
| Some chapters missing at the end | They are listed on the final screen. Details are in `last_run.log`. |
| Some images skipped | The PDF worker skips unreadable images or unsupported formats. The count is reported in the log. |
| Build error: `g++` not found | Verify the MinGW-w64 installation and your `PATH`. |
| Build error: `hpdf.h` not found | Make sure the libharu headers are in `include\`, or adjust the `-I` options in `build.bat`. |
| Build error: `cannot find -lhpdf` | Make sure the libharu library is in `lib\` and matches your MinGW architecture. |
| Missing DLL at launch | Place the DLL next to the executables or add its folder to `PATH`. |

## 9. Legal Disclaimer

This project is provided for educational and technical purposes only, to learn and experiment with programming.

No copyrighted content is included in this repository. The author does not encourage or promote copyright infringement.

Users are solely responsible for how they use this software and for complying with applicable laws in their country. Respect the terms of service of any site you access, and only use the tool for content you are allowed to access.

The software is provided "as is", without warranty of any kind, under the MIT License. The author cannot be held liable for any misuse, damages, or legal consequences resulting from the use of this software.

## 10. License

This project is licensed under the **[MIT License](LICENSE)**.
