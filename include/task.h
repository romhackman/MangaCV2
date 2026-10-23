// ==========================================================
// task.h  -  Tache en arriere-plan (remplace les anciens .exe)
//
//  Une Task execute une fonction dans un thread. Pendant ce temps :
//   - elle ecrit son journal dans un Logger (lu par l'interface),
//   - elle publie sa progression dans un Progress (atomiques),
//   - elle verifie regulierement si l'utilisateur a demande l'annulation.
// ==========================================================
#pragma once

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// ---------------- Journal ----------------
class Logger
{
public:
    void line(const std::string& text);              // ajoute une ligne (+ terminal si lance depuis un)
    std::vector<std::string> tail(size_t n) const;   // n dernieres lignes
    std::string full() const;                        // journal complet
    void clear();

private:
    mutable std::mutex mutex_;
    std::deque<std::string> recent_;
    std::string full_;
};

// ---------------- Progression ----------------
enum class Phase { Idle = 0, Download, Pdf, Copy };

struct Progress
{
    std::atomic<int> phase{0};
    std::atomic<int> stage{0}, stageTotal{0};   // ex : chapitre 3 / 10
    std::atomic<int> item{0},  itemTotal{0};    // ex : image 12 / 40

    void reset();
    void beginPhase(Phase p, int newStageTotal = 0);
    Phase current() const { return static_cast<Phase>(phase.load()); }
};

// ---------------- Contexte donne au travail ----------------
struct TaskContext
{
    Logger& log;
    Progress& progress;
    const std::atomic<bool>& cancelFlag;

    bool cancelled() const { return cancelFlag.load(); }
};

// ---------------- Tache ----------------
class Task
{
public:
    // Retourne true si OK ; sinon remplit "error".
    using Work = std::function<bool(TaskContext&, std::string& error)>;

    Task() = default;
    ~Task();
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    void start(Work work);       // annule la tache precedente si besoin
    void cancel();               // demande l'arret et attend la fin du thread

    bool busy() const { return started_ && !done_.load(); }
    bool poll();                 // true UNE fois, quand le travail vient de se terminer

    bool succeeded() const { return ok_; }
    const std::string& error() const { return error_; }

    Logger&   log()      { return log_; }
    Progress& progress() { return progress_; }

private:
    std::thread thread_;
    std::atomic<bool> cancel_{false};
    std::atomic<bool> done_{false};
    bool started_ = false;
    bool ok_ = false;
    std::string error_;

    Logger log_;
    Progress progress_;
};
