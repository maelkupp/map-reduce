# CSE305 — MapReduce on Recursively Enumerated Sets
Team reference doc. Read together, work through in order.

---

## What are we building?

A parallel C++ engine that walks a tree of objects, runs a computation on every node, and combines the results using multiple threads. The user of the engine just defines the tree and what to compute. The engine handles all parallelism automatically.

The public interface looks like this:

```cpp
RecursivelyEnumeratedSet<Node, Result> engine(seeds, successors);
Result answer = engine.map_reduce(map_fn, reduce_fn);
```

That's it. Everything else is internal.

---

## The 4 ideas you need to understand

### 1. Recursively enumerated set

You define a set S implicitly with two things:
- `seeds` — the starting elements (often just one)
- `successors(x)` — given a node, return its children

S = everything reachable from seeds by applying successors repeatedly.
You never store S. You traverse it lazily. The structure is a tree.

Sudoku example:
- `seeds` = { the initial puzzle grid }
- `successors(grid)` = all grids with one more cell validly filled
- S = every possible partial or complete valid filling

### 2. Map-reduce

`map(node)` turns each node into a value.
`reduce(a, b)` combines two values into one.

You traverse the whole tree, map every node, reduce everything down to a single answer.

**Critical rule: reduce must be associative.**
`reduce(a, reduce(b, c)) == reduce(reduce(a, b), c)`
This is what makes parallel evaluation legal — partial results can be combined in any order.

| Goal | map(x) | reduce(a,b) |
|---|---|---|
| Count elements | `return 1` | `a + b` |
| Count satisfying P | `P(x) ? 1 : 0` | `a + b` |
| Find maximum | `return x` | `max(a, b)` |
| Check all satisfy Q | `return Q(x)` | `a && b` |
| Find any solution | `complete(x) ? x : nullopt` | first non-null |

The traversal never changes. Only map and reduce change per question. That's the whole abstraction.

### 3. Depth-first traversal

Always traverse depth-first. Why? Memory.
- BFS stores the entire frontier at each level → exponential memory
- DFS only stores the current path → linear memory

The basic logic each worker runs:

```
visit(node):
    result = map(node)
    for child in successors(node):
        result = reduce(result, visit(child))
    return result
```

Each thread runs this on its own portion of the tree and accumulates a local result.

### 4. Work-stealing

The tree is unbalanced. Some branches have millions of nodes, others terminate immediately. If you split work upfront, some threads finish early and sit idle.

Work-stealing fixes this dynamically:
- Each thread has its own **double-ended deque** of tasks
- The owner pushes/pops from one end (fast, no contention)
- Idle threads **steal** from the other end of a random busy thread's deque
- Stealing from the far end grabs large unexplored subtrees — maximizing the work gained per steal

The system is self-balancing with no central coordinator.

---

## How problems are encoded

Every problem follows the same recipe. You only need to fill in 4 things:

1. **Node type** — what fully describes "where you are" in the search?
2. **Seeds** — what is the starting state?
3. **Successors** — how do you make one step? Return empty to prune a branch.
4. **Map + reduce** — what do you want to know?

### Sudoku

```
Node        = SudokuGrid (9x9, some cells empty)
Seeds       = { initial_puzzle }
Successors  = pick next empty cell, return one child grid per valid digit
              return {} if no valid digit exists  ← prunes dead ends
Map         = grid.is_complete() ? 1 : 0
Reduce      = a + b                              ← counts solutions
```

Swap map/reduce to ask a different question without touching anything else:
- Find a solution: map returns the grid if complete, reduce returns first non-null
- Verify uniqueness: count solutions, check result == 1

### N-Queens

```
Node        = list of queen positions placed so far (one per row)
Seeds       = { [] }
Successors  = try placing queen in each column of next row, skip if attacked
Map         = len(queens) == n ? 1 : 0
Reduce      = a + b
```

### Hamiltonian paths

```
Node        = (current_vertex, set_of_visited_vertices)
Seeds       = { (start, {start}) }
Successors  = { (neighbor, visited + neighbor) | neighbor not in visited }
Map         = visited == all_vertices ? 1 : 0
Reduce      = a + b
```

The pattern is the same every time. The pruning inside successors is where the problem-specific intelligence lives. Smarter pruning = smaller tree = faster run, no engine changes needed.

---

## Architecture

### The 3 layers

| Layer | What it is | Knows about Node? |
|---|---|---|
| Engine (`RecursivelyEnumeratedSet`) | Template class + threads + scheduler | No — pure template |
| Glue (`seeds`, `successors`, `map`, `reduce`) | User-defined functions | Yes |
| Node (`SudokuGrid`, etc.) | The concrete data type | It is the node |

### Internal components you need to build

**WorkStealingDeque\<T\>**
Per-thread task queue. Owner pushes/pops one end. Thieves steal from the other.
This is the hardest part. Use the Chase-Lev deque design.

**Worker threads**
Each runs a DFS loop: pop node → call successors → push children → accumulate local result via reduce.
No shared mutable state during normal operation.

**Thread pool**
Manages N worker threads. Distributes seeds at startup. Joins on completion.

**Termination detector**
Determines when all work is exhausted across all threads. Harder than it sounds — see below.

**Result combiner**
Each thread keeps a local result. At the very end, reduce all local results together.
Never write to a shared global result during traversal — that's a bottleneck.

### What is and isn't shared between threads

| Data | Shared? | Why |
|---|---|---|
| Worker's deque (own end) | No | Single thread accesses it |
| Worker's deque (steal end) | Yes — needs atomics | Thieves can access it |
| Worker's local_result | No | Only that thread writes it |
| seeds / successors / map / reduce | Read-only | Const, set at construction |
| Final results array | Write-once at end | Barrier (join) is enough |

---

## The hard parts

### The work-stealing deque (Chase-Lev)

This is the most technically difficult piece. Study the Chase-Lev paper before implementing.

The tricky case: when the deque has exactly one element left, the owner and a thief can race. Both try to take it. Only one should win. This is resolved with a CAS (compare-and-swap) on the `top` index.

```cpp
struct WorkStealingDeque {
    atomic<int64_t> top;     // thieves steal from here
    atomic<int64_t> bottom;  // owner pushes/pops here
    atomic<CircularArray*> array;
};
```

Memory ordering matters here. Every `memory_order` annotation must be deliberate:
- `push` (bottom store) → `release` — publishes the new item to potential thieves
- `pop` (bottom load) → mostly `relaxed` — single owner, no concurrent reader
- `steal` (top CAS) → `acq_rel` — must coordinate with concurrent steals
- `steal` (array load) → `acquire` — must see items published by push

Examiners will read this code carefully. Know why each memory order is what it is.

### Termination detection

You need to know when the whole computation is done.
An idle thread does not mean work is done — it might be about to steal.

A simple protocol that works:
1. Keep an atomic counter of "active" threads (threads that have work or are executing)
2. When a thread runs out of work, it decrements the counter and enters a searching state
3. Termination is only declared when `active_count == 0` AND all deques are confirmed empty
4. The confirmation must happen with proper memory ordering to avoid missing in-flight work

Warning: checking "all deques are empty" naively is a race. Thread A's deque could be empty when you check, but Thread B might be about to push stolen work into it. You need a protocol, not just a loop checking sizes.

---

## Roadmap

Work through these phases in order. Don't jump ahead.

### Phase 0 — Read before coding
- [ ] Read the Chase-Lev deque paper (2005)
- [ ] Read the SageMath map_reduce docs linked in the project PDF
- [ ] Everyone understands the 4 core concepts above

### Phase 1 — Sequential engine
- [ ] Implement `RecursivelyEnumeratedSet` with single-threaded DFS map_reduce
- [ ] Test on Sudoku (small grids), N-Queens (n=8)
- [ ] Results are correct — this is your reference implementation

### Phase 2 — Work-stealing deque
- [ ] Implement Chase-Lev deque: push, pop, steal with correct atomics
- [ ] Unit test in isolation: stress test with many threads pushing and stealing simultaneously
- [ ] Run under ThreadSanitizer — zero data races
- [ ] Test edge cases: steal from empty deque, simultaneous steal + pop of last element

### Phase 3 — Parallel engine
- [ ] Thread pool with configurable thread count
- [ ] Distribute seeds into initial deques
- [ ] Worker DFS loop with stealing
- [ ] Termination detection protocol
- [ ] Per-thread result accumulation + final reduction

### Phase 4 — Validation
- [ ] Parallel results match sequential reference on all test problems
- [ ] Run under ThreadSanitizer and AddressSanitizer — clean
- [ ] Scaling benchmark: wall-clock time at 1, 2, 4, 8 threads
- [ ] Show work-stealing helps on an unbalanced tree vs static split

### Phase 5 — Applications and polish
- [ ] Sudoku: count solutions, find a solution, verify uniqueness
- [ ] Second application (N-Queens or graph paths)
- [ ] Prepare benchmark graphs for presentation

---

## What examiners will focus on

This is a concurrency project. They care about the parallel parts, not the Sudoku.

**The work-stealing deque** — they will read this code line by line.
Questions to be ready for:
- Is the owner end lock-free in the common case?
- Is the last-element race handled correctly with CAS?
- Why did you use this memory_order here and not relaxed/seq_cst?

**Termination detection** — they know this is hard. Getting it right cleanly stands out.
Questions to be ready for:
- Why is checking "all deques empty" insufficient on its own?
- What does your atomic counter represent exactly?
- How do you guarantee nothing is in-flight when you declare done?

**Race conditions** — they will ask "where could this have a data race?"
Know the difference between a data race (undefined behaviour) and a logical race (wrong answer but defined).

**Performance scaling** — have graphs ready. Be able to explain anomalies.
Why does speedup plateau at 8 threads? Why does the steal rate differ between Sudoku and N-Queens?

### Priority summary

| Area | Priority | Why |
|---|---|---|
| Work-stealing deque correctness | Critical | Examined closely |
| Termination detection | Critical | Classic pitfall |
| No data races / deadlocks | Critical | Fundamental |
| Scaling benchmarks | High | Evidence parallelism helps |
| Sudoku application | High | Main demo |
| Clean engine interface | High | Shows design understanding |
| Second application | Medium | Shows generality |

---

## Quick reference

**The three things that never change:**
- Seeds tell you where to start
- Successors tell you how to grow the tree (prune here!)
- DFS + work-stealing handles the rest

**The two things that change per question:**
- map
- reduce

**The one rule that must always hold:**
- reduce is associative
