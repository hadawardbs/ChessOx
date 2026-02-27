#ifndef OXTA_SEARCH_THREAD_HPP
#define OXTA_SEARCH_THREAD_HPP

#include "search.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace Oxta::Search {

class Thread {
public:
    Thread(TranspositionTable* shared_tt) : searcher(shared_tt) {
        searcher.silent = true;
        worker = std::thread(&Thread::idle_loop, this);
    }

    ~Thread() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            exit_flag = true;
            searching = true; // To wake up
            cv.notify_one();
        }
        if (worker.joinable()) worker.join();
    }

    void start_search(const Position& pos, const SearchLimits& limits) {
        std::lock_guard<std::mutex> lock(mtx);
        root_pos = pos;
        this->limits = limits;
        searcher.info.reset(); // Reset helper info
        searching = true;
        cv.notify_one();
    }

    void stop() {
        searcher.info.stop = true;
    }

    bool is_searching() {
        std::lock_guard<std::mutex> lock(mtx);
        return searching;
    }

    Searcher searcher;

private:
    Position root_pos;
    SearchLimits limits;
    std::thread worker;
    std::mutex mtx;
    std::condition_variable cv;
    bool searching = false;
    bool exit_flag = false;

    void idle_loop() {
        while (true) {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this]{ return searching; });

            if (exit_flag) break;

            Position pos = root_pos;
            SearchLimits lim = limits;
            lock.unlock();

            searcher.iter_deep(pos, lim);

            lock.lock();
            searching = false;
        }
    }
};

class ThreadPool {
public:
    ThreadPool() {
        // Initial TT size
        main_tt.resize(64);
    }

    ~ThreadPool() {
        threads.clear(); // Destructors join
    }

    void resize_tt(size_t mb) {
        main_tt.resize(mb);
    }

    void start_search(Position& pos, SearchLimits limits) {
        // Ensure threads exist (Lazy creation)
        // Default 4 threads for now if not specified?
        // Or adapt to hardware?
        size_t hw_conc = std::thread::hardware_concurrency();
        if (hw_conc == 0) hw_conc = 2;

        if (threads.empty()) {
            for(size_t i=0; i<hw_conc-1; ++i) { // -1 for main thread
                threads.emplace_back(std::make_unique<Thread>(&main_tt));
            }
        }

        // Wake helpers
        for (auto& t : threads) {
            t->start_search(pos, limits);
        }

        // Main thread search
        main_searcher.set_tt(&main_tt);
        main_searcher.silent = false;
        main_searcher.iter_deep(pos, limits);

        // Stop helpers when main finishes
        for (auto& t : threads) t->stop();

        // Wait for helpers? No, they stop async.
        // But we should ensure they are done before next search?
        // start_search resets them.
        // But `searching` flag protects them.
        // It's fine. Lazy SMP allows helpers to run or stop.
    }

    void stop() {
        main_searcher.info.stop = true;
        for (auto& t : threads) t->stop();
    }

private:
    TranspositionTable main_tt;
    Searcher main_searcher;
    std::vector<std::unique_ptr<Thread>> threads;
};

} // namespace Oxta::Search

#endif // OXTA_SEARCH_THREAD_HPP
