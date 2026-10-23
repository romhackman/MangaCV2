#include "screens.h"

#include "config.h"
#include "gfx.h"
#include "layout.h"

namespace screens
{
    void header(const std::string& subtitle)
    {
        gfx::text("Manga Downloader", 40, 25, color::TEXT, true);
        gfx::text(cfg::APP_VERSION, 330, 36, color::MUTED);
        if (!subtitle.empty()) gfx::text(subtitle, 40, 68, color::MUTED);
    }

    void render(App& app, int mx, int my, Uint32 ticks)
    {
        gfx::beginFrame();

        switch (app.screen)
        {
            case Screen::Home:     renderHome(app, mx, my, ticks);     break;
            case Screen::Settings: renderSettings(app, mx, my, ticks); break;
            case Screen::Prepare:  renderPrepare(app, mx, my, ticks);  break;
            case Screen::Search:   renderSearch(app, mx, my, ticks);   break;
            case Screen::Scraping: renderScraping(app, mx, my, ticks); break;
            case Screen::Chapters: renderChapters(app, mx, my, ticks); break;
            case Screen::Download: renderDownload(app, mx, my, ticks); break;
            case Screen::Done:     renderDone(app, mx, my, ticks);     break;
            case Screen::Error:    renderError(app, mx, my, ticks);    break;
        }

        gfx::button(layout::QUIT, "Quitter", mx, my);
        gfx::endFrame();
    }

    void click(App& app, int x, int y, bool& running)
    {
        if (gfx::inRect(x, y, layout::QUIT)) { running = false; return; }   // "Quitter" est partout

        switch (app.screen)
        {
            case Screen::Home:     clickHome(app, x, y);     break;
            case Screen::Settings: clickSettings(app, x, y); break;
            case Screen::Search:   clickSearch(app, x, y);   break;
            case Screen::Chapters: clickChapters(app, x, y); break;
            default:               clickProgress(app, x, y); break;
        }
    }
}
