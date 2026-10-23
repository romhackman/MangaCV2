#include "task.h"

#include "platform.h"
#include "utils.h"

// ---------------- Logger ----------------

void Logger::line(const std::string& text)
{
    std::string clean = utils::trim(text);
    if (clean.empty()) return;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        recent_.push_back(clean);
        if (recent_.size() > 200) recent_.pop_front();
        if (full_.size() < 4000000) { full_ += clean; full_ += '\n'; }
    }
    platform::consoleWrite(clean);
}

std::vector<std::string> Logger::tail(size_t n) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    size_t start = recent_.size() > n ? recent_.size() - n : 0;
    return std::vector<std::string>(recent_.begin() + static_cast<std::ptrdiff_t>(start), recent_.end());
}

std::string Logger::full() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return full_;
}

void Logger::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    recent_.clear();
    full_.clear();
}

// ---------------- Progress ----------------

void Progress::reset()
{
    phase = 0; stage = 0; stageTotal = 0; item = 0; itemTotal = 0;
}

void Progress::beginPhase(Phase p, int newStageTotal)
{
    stage = 0; item = 0; itemTotal = 0;
    stageTotal = newStageTotal;
    phase = static_cast<int>(p);
}

// ---------------- Task ----------------

Task::~Task() { cancel(); }

void Task::start(Work work)
{
    cancel();

    log_.clear();
    progress_.reset();
    cancel_ = false;
    done_ = false;
    ok_ = false;
    error_.clear();
    started_ = true;

    thread_ = std::thread([this, work = std::move(work)]()
    {
        TaskContext ctx{ log_, progress_, cancel_ };
        std::string err;
        bool ok = false;

        try { ok = work(ctx, err); }
        catch (const std::exception& e) { err = e.what(); }
        catch (...) { err = "Erreur inconnue."; }

        ok_ = ok;
        error_ = err;
        done_ = true;
    });
}

void Task::cancel()
{
    if (!started_) return;

    cancel_ = true;
    if (thread_.joinable()) thread_.join();
    started_ = false;
    done_ = false;
}

bool Task::poll()
{
    if (started_ && done_.load())
    {
        if (thread_.joinable()) thread_.join();
        started_ = false;
        return true;
    }
    return false;
}
