#ifndef BENCH_H
#define BENCH_H

// to use you call run_sweep


#include "parallel_engine.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <vector>

using BenchClock = std::chrono::high_resolution_clock;

// One run on a fresh engine; returns seconds elapsed, writes the result to out.
template <class Node, class Result>
double bench_time_run(const std::vector<Node>& seeds,
                      std::function<std::vector<Node>(Node&)> succ,
                      Result neutral, int threads,
                      std::function<Result(Node&)> map,
                      std::function<Result(Result, Result)> reduce,
                      Result& out,
                      std::function<bool(const Result&)> should_stop = nullptr, VictimStrategy strat = VictimStrategy::Random) {
    ParallelRES<Node, Result> eng(seeds, succ, neutral, threads);
    auto t0 = BenchClock::now();
    out = eng.map_reduce(map, reduce,should_stop);
    auto t1 = BenchClock::now();
    return std::chrono::duration<double>(t1 - t0).count();
}

template <class Node, class Result>
void run_sweep(const std::string& name,
               const std::vector<Node>& seeds,
               std::function<std::vector<Node>(Node&)> succ,
               std::function<Result(Node&)> map,
               std::function<Result(Result, Result)> reduce,
               
               Result neutral,
               const std::vector<int>& thread_counts,
               int trials,
               std::function<bool(const Result&)> should_stop = nullptr, VictimStrategy strat = VictimStrategy::Random) {
    std::cout << "\n==================== " << name << " ====================\n";
    std::cout << "hardware_concurrency = " << std::thread::hardware_concurrency()
              << "   (speedup is only meaningful with that many physical cores)\n";

    { Result tmp; bench_time_run<Node, Result>(seeds, succ, neutral, 1, map, reduce, tmp, should_stop,strat); }

    Result reference{};
    bench_time_run<Node, Result>(seeds, succ, neutral, 1, map, reduce, reference,should_stop,strat);
    std::cout << "reference result (1 thread) = " << reference << "\n\n";

    std::cout << std::left
              << std::setw(8)  << "threads"
              << std::setw(12) << "best(s)"
              << std::setw(12) << "mean(s)"
              << std::setw(10) << "speedup"
              << std::setw(8)  << "eff"
              << std::setw(14) << "result"
              << "check\n"
              << std::string(72, '-') << "\n";

    double base_best = 0.0;  // best time at 1 thread, baseline for speedup

    for (int p : thread_counts) {
        double best = std::numeric_limits<double>::infinity();
        double sum  = 0.0;
        Result res{};
        bool   ok   = true;

        for (int t = 0; t < trials; ++t) {
            Result r;
            double s = bench_time_run<Node, Result>(seeds, succ, neutral, p, map, reduce, r,should_stop,strat);
            best = std::min(best, s);
            sum += s;
            res = r;
            if (r != reference) ok = false;   // lost / duplicated work surfaces here
        }

        if (p == 1) base_best = best;
        double mean    = sum / trials;
        double speedup = base_best / best;
        double eff     = speedup / p;

        std::cout << std::left << std::fixed << std::setprecision(4)
                  << std::setw(8)  << p
                  << std::setw(12) << best
                  << std::setw(12) << mean
                  << std::setw(10) << std::setprecision(2) << speedup
                  << std::setw(8)  << eff
                  << std::setw(14) << res
                  << (ok ? "OK" : "*** MISMATCH ***") << "\n";

        std::cout << "CSV," << name << "," << p << "," << best << "," << mean
                  << "," << speedup << "," << eff << "," << res
                  << "," << (ok ? "ok" : "MISMATCH") << "\n";
    }
}

#endif // BENCH_H