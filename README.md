# Parallel Map-Reduce on Recursively Enumerated Sets
 
The goal is a generic, parallel `map_reduce` over a recursively
enumerated set: you give it some seed elements and a `successors` function, and
it explores everything reachable from the seeds, applies `map` to each element,
and combines the results with `reduce`. The exploration is parallelised with
work stealing so that idle threads pick up work from busy ones, which keeps
all cores busy even when the search tree is lopsided.
 
The engine is generic (`ParallelRES<Node, Result>`), and we plug four very
different problems into it: merge sort, Hamiltonian-path counting, Sudoku, and
N-Queens. The same engine and the same benchmark harness are used for all of
them, so they're directly comparable.
 
 
## What each file does
 
**The engine**
 
- `parallel_engine.h` — the heart of the project: `ParallelRES<Node, Result>`.
  It spawns `num_threads` workers, gives each its own work-stealing deque, and
  has every worker explore its deque depth-first (`pop_bottom`). When a worker
  runs dry it tries to steal from another worker's deque. It also supports
  an optional early-termination test (`should_stop`, e.g. "stop once a solution
  is found") and an optional sequential cutoff (`seq_cutoff`) that processes a
  subtree inline instead of pushing every node, to cut deque overhead.
- `chase_lev_deque.h` — the lock-free Chase-Lev work-stealing deque used by
  each worker, plus two helpers:
  - `CircularArray<T>` — a power-of-two ring buffer that can grow and shrink.
  - `BufferPool<T>` — recycles those arrays so growing/shrinking doesn't
    constantly hit the allocator.
  The owner pushes/pops at the bottom (LIFO, cache-friendly DFS); thieves take
  from the top (FIFO). The tricky concurrent cases (last element, grow/shrink
  races) are handled with the index-bump protocol from the Chase-Lev paper.
- `RecursivelyEnumeratedSet.h` — a simple single-threaded reference
  implementation (plain `std::deque`, DFS). It's the "obviously correct" version
  we started from and is handy for sanity-checking; the parallel benchmarks
  don't use it.
  
**The benchmark**
 
- `benchmark.h` — the shared benchmark harness. `compare_strategies(...)` runs
  the same experiment for any application: sweep over thread counts, run all
  four stealing strategies, and report speedups against the single-thread
  baseline. Everything goes through this one function so the output format is
  identical for every problem.
  
**The applications** (each is one self-contained `.cpp` + its header)
 
- `sort.h` / `sort.cpp` — parallel merge sort expressed as map-reduce. A node
  is a slice of the array; `successors` splits it in half; leaves (size 1) map to
  themselves; `reduce` merges two sorted vectors.
- `graph.h` / `graph.cpp` — Hamiltonian path counting. A node is a partial
  simple path (current vertex + visited bitmask); `successors` extends to unvisited
  neighbours; `map` returns 1 for a path covering every vertex; `reduce` adds.
  We test on the complete graph `K_n`, which has exactly `(n-1)!` such paths from
  a fixed start, so the answer is easy to verify.
- `sudoku.h` / `sudoku.cpp` — Sudoku. A node is a (partial) grid; `successors`
  fills the first empty cell with each legal digit; we count complete legal grids
  (so a proper puzzle gives 1). There's also a "find any solution" variant
  (`sudoku_map` + boolean `reduce`) that uses early termination.
- `N_queens.h` / `N_queens.cpp` — N-Queens counting. A node is a partial
  placement; `successors` places a queen on the next row in every safe column;
  `map` returns 1 for a full board; `reduce` adds.
- `skew.h` / `skew.cpp` — a synthetic unbalanced workload for stressing the
  work stealing itself rather than solving a real problem. A node carries a
  `budget`; `successors` splits it into a left part (`budget * skew_p`) and the
  rest, so `skew_p` controls how lopsided the tree is (0.5 = balanced, near 0 or
  1 = very skewed). `map` burns a fixed amount of CPU (`skew_work`) and returns 1,
  so the count is always `2*budget - 1`. This is the case where the choice of
  stealing strategy matters most.
  
**Other**
 
- `chase_lev_deque_test.cpp` — unit and multithreaded stress tests for the deque
  (grow/shrink under theft, every item received exactly once, etc.).
- `Makefile` — builds everything.


 
## Building
 
You need a C++17 compiler and a threads library. Then:
 
```bash
make            # builds deque_test, sort_bench, graph_bench, sudoku_bench, queens_bench
make test       # builds and runs the deque test suite
make clean      # removes the binaries
```
 
Or compile a single application by hand (each `.cpp` is self-contained):
 
```bash
g++ -std=c++17 -O2 -pthread N_queens.cpp -o queens_bench
```
 
---
 
## Running the benchmarks
 
Each application takes a problem size and a number of trials. The defaults are
chosen to run in a few seconds on a normal laptop; pass arguments to make them
bigger (more interesting speedups) or smaller (faster).
 
```bash
./queens_bench  [N]            [trials]      # default: N=13,      trials=3
./graph_bench   [n]            [trials]      # default: n=11,      trials=3   (uses K_n)
./sudoku_bench  [trials]                     # default: trials=3
./sort_bench    [array_length] [trials]      # default: N=50000,   trials=3
./skew_bench    [budget] [trials] [skew_p] [work]   # default: budget=500000, trials=3, p=0.5, work=200
```
 
Examples:
 
```bash
./queens_bench 14 5      # 14-queens, 5 trials per data point
./graph_bench  12        # Hamiltonian paths in K_12  (= 11!)
./sort_bench   200000    # sort 200k integers
./skew_bench 1000000 3 0.1     # heavily skewed tree ->  exercises load balancing
```
 
A run finishes by **checking correctness**: every parallel run must produce the
same result as the single-thread baseline. If work were ever lost or duplicated,
the `check` column would read `*** MISMATCH ***`.
 
---
 
## Reading the output
 
For each application you get one table per stealing strategy, then a summary
matrix. Here's an actual (short) `sudoku_bench` run so you can see the layout:
 
```
============================================================
 Sudoku solution count (hard grid, unique solution)
 cores available: 8   |   trials per point: 3
============================================================
baseline (1 thread): 0.0009 s    reference result = 1
 
-- strategy: random --
threads  best(s)    mean(s)    speedup   eff     result   check
--------------------------------------------------------------------------
1        0.0009     0.0009     1.00      1.00    1        ok
2        0.0009     0.0009     1.06      0.53    1        ok
4        ...
...
 
-- speedup summary (x faster than 1 thread) --
threads  random       power_of_two richest      sticky
-------------------------------------------------------------
1        1.00         0.99         1.01         1.00
2        1.06         0.86         1.06         1.02
4        ...
```
 
Columns:
 
- `best(s)` / `mean(s)` — fastest and average time over the trials (we report
  `best` because it's the least noisy estimate of the real cost).
- `speedup` — `baseline_time / best_time`, where the baseline is the
  single-thread time. Higher is better; ideal is "equal to the thread count".
- `eff` — efficiency, i.e. `speedup / threads`. 1.0 means perfect scaling.
- `result` / `check` — the value computed and whether it matched the baseline.
The speedup summary at the bottom puts the four strategies side by side at
each thread count, which is the quickest way to see which one wins.
 

 
## The four stealing strategies
 
When a worker goes idle it has to pick whose deque to steal from. We implemented
four "victim selection" strategies (in `parallel_engine.h`) and the benchmark
compares them:
 
- `random` — pick any other worker at random. Cheap, the classic default.
- `power_of_two` — pick two workers at random, steal from the bigger deque. A
  cheap way to avoid the unluckiest choices.
- `richest` — scan everyone and steal from the worker with the most work. Best
  load info, but the scan costs O(threads) per steal.
- `sticky` — keep stealing from the worker that last gave you work; fall back to
  `richest` when that dries up. The idea is to keep grabbing from one big subtree.
