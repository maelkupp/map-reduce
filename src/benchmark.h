#ifndef BENCHMARK_H
#define BENCHMARK_H

//
// The idea: each application (sort, sudoku, graph, n-queens) builds its seeds
// and its map / reduce / successors functions, then hands them to
// compare_strategies(). The harness then runs the same experiment for all of
// them: sweep over a list of thread counts, run each of the four victim-
// selection strategies, and print speedups against the single-thread baseline.
//
// Because every app goes through the same function, the output format and the
// way speedup is computed are identical everywhere, so the four problems are
// directly comparable.

#include "parallel_engine.h"

#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <thread>
#include <vector>


using BenchClock = std::chrono::steady_clock;

// Three of our applications return an int (a count) but the sort example
// returns a whole std::vector<int>, which has no operator<<. 
// --------------------------------------------------------------------------
inline std::string brief_result(int v)       { return std::to_string(v); }
inline std::string brief_result(long v)      { return std::to_string(v); }
inline std::string brief_result(long long v) { return std::to_string(v); }

inline std::string brief_result(const std::vector<int>& v) {
    long long sum = 0;
    for (int x : v) sum += x;
    return "vec[n=" + std::to_string(v.size()) + ",sum=" + std::to_string(sum) + "]";
}

// fallback for any other result type that already knows how to print itself
template <class R>
std::string brief_result(const R& r) {
    std::ostringstream os; os << r; return os.str();
}

// Result of running the same configuration several times.
template <class Result>
struct TrialOutcome {
    double best;     // fastest of the trials 
    double mean;     // average over the trials
    Result result;   // value computed ( for correctness check)
};

// Run `trials` full map-reduce passes for one fixed (threads, strategy) pair.
template <class Node, class Result>
TrialOutcome<Result>
run_trials(const std::vector<Node>& seeds,
           std::function<std::vector<Node>(Node&)> succ,
           std::function<Result(Node&)> map,
           std::function<Result(Result, Result)> reduce,
           Result neutral, int threads, VictimStrategy strat, int trials) {
    double best = std::numeric_limits<double>::infinity();
    double sum  = 0.0;
    Result last = neutral;

    for (int t = 0; t < trials; ++t) {
        ParallelRES<Node, Result> eng(seeds, succ, neutral, threads);
        auto t0 = BenchClock::now();
        last = eng.map_reduce(map, reduce, nullptr, strat); // strategy is honoured here
        auto t1 = BenchClock::now();

        double s = std::chrono::duration<double>(t1 - t0).count();
        best = std::min(best, s);
        sum += s;
    }
    return { best, sum / trials, last };
}

// Runs the whole comparison for one application and
// prints:
//   1. a per-strategy table (best / mean / speedup / efficiency / correctness)
//   2. a final speedup matrix (thread counts x strategies) for a quick read
template <class Node, class Result>
void compare_strategies(const std::string& app_name,
                        const std::vector<Node>& seeds,
                        std::function<std::vector<Node>(Node&)> succ,
                        std::function<Result(Node&)> map,
                        std::function<Result(Result, Result)> reduce,
                        Result neutral,
                        const std::vector<int>& thread_counts,
                        int trials) {
    const std::vector<VictimStrategy> strategies = {
        VictimStrategy::Random, VictimStrategy::PowerOfTwo,
        VictimStrategy::Richest, VictimStrategy::Sticky };

    std::cout << "\n============================================================\n";
    std::cout << " " << app_name << "\n";
    std::cout << " cores available: " << std::thread::hardware_concurrency()
              << "   |   trials per point: " << trials << "\n";
    std::cout << "============================================================\n";

   
    run_trials<Node, Result>(seeds, succ, map, reduce, neutral, 1,
                             VictimStrategy::Random, 1);

    // Single-thread baseline. With one worker no stealing ever happens, so this
    // time does not depend on the strategy and is the natural reference point
    // for every speedup number below.
    TrialOutcome<Result> base =
        run_trials<Node, Result>(seeds, succ, map, reduce, neutral, 1,
                                 VictimStrategy::Random, trials);
    const double baseline   = base.best;
    const Result reference  = base.result;

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "baseline (1 thread): " << baseline << " s    reference result = "
              << brief_result(reference) << "\n";

    // matrix[strategy][thread_index] = speedup, filled in as we go and printed
    // as a compact table at the very end.
    std::vector<std::vector<double>> matrix(
        strategies.size(), std::vector<double>(thread_counts.size(), 0.0));

    for (size_t si = 0; si < strategies.size(); ++si) {
        VictimStrategy strat = strategies[si];

        std::cout << "\n-- strategy: " << strat_name(strat) << " --\n";
        std::cout << std::left
                  << std::setw(9)  << "threads"
                  << std::setw(11) << "best(s)"
                  << std::setw(11) << "mean(s)"
                  << std::setw(10) << "speedup"
                  << std::setw(8)  << "eff"
                  << std::setw(18) << "result"
                  << "check\n";
        std::cout << std::string(74, '-') << "\n";

        for (size_t i = 0; i < thread_counts.size(); ++i) {
            int p = thread_counts[i];
            TrialOutcome<Result> out =
                run_trials<Node, Result>(seeds, succ, map, reduce, neutral, p, strat, trials);

            double speedup = baseline / out.best;
            double eff     = speedup / p;          // efficiency = speedup per thread
            bool   ok      = (out.result == reference); // lost/duplicated work shows here
            matrix[si][i]  = speedup;

            std::cout << std::left
                      << std::setw(9)  << p
                      << std::fixed << std::setprecision(4)
                      << std::setw(11) << out.best
                      << std::setw(11) << out.mean
                      << std::setprecision(2)
                      << std::setw(10) << speedup
                      << std::setw(8)  << eff
                      << std::setw(18) << brief_result(out.result)
                      << (ok ? "ok" : "*** MISMATCH ***") << "\n";
        }
    }


    std::cout << "\n-- speedup summary (x faster than 1 thread) --\n";
    std::cout << std::left << std::setw(9) << "threads";
    for (VictimStrategy s : strategies) std::cout << std::setw(13) << strat_name(s);
    std::cout << "\n" << std::string(9 + 13 * strategies.size(), '-') << "\n";

    for (size_t i = 0; i < thread_counts.size(); ++i) {
        std::cout << std::left << std::setw(9) << thread_counts[i];
        for (size_t si = 0; si < strategies.size(); ++si)
            std::cout << std::setw(13) << std::fixed << std::setprecision(2) << matrix[si][i];
        std::cout << "\n";
    }
    std::cout << "\n";
}

#endif // BENCHMARK_H
