# Parallel Map-Reduce on Recursively Enumerated Sets

CSE305 project. The goal is a generic, parallel `map_reduce` over a recursively
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


- `benchmark.h` — the shared benchmark harness. `compare_strategies(...)` runs
  the same experiment for any application: sweep over thread counts, run all
  four stealing strategies, and report speedups against the single-thread
  baseline. Everything goes through this one function so the output format is
  identical for every problem.

The applications (each is one self-contained `.cpp` + its header)

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

## Tests and docs

- `chase_lev_deque_test.cpp` — unit and multithreaded stress tests for the deque
  (grow/shrink under theft, every item received exactly once, etc.).
- `Makefile` — builds everything.
- `CSE305_projects_MapReduce.pdf` — the assignment description.

---

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
