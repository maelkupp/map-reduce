//  chase_lev_deque_test.cpp
//  Build:  g++ -std=c++17 -O2 -pthread chase_lev_deque_test.cpp -o deque_test
//  Run:    ./deque_test
//
//  Tests are grouped into:
//    1. CircularArray unit tests
//    2. Single-threaded WorkStealingDeque correctness
//    3. Multi-threaded stress tests (grow, shrink, steal races)

#include "chase_lev_deque.h"

#include <cassert>
#include <thread>
#include <vector>
#include <atomic>
#include <algorithm>
#include <unordered_set>
#include <cstdio>
#include <numeric>
#include <random>

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

struct Node {
    int value;
    explicit Node(int v) : value(v) {}
};

// Thin RAII wrapper so tests don't leak Nodes.
struct NodePool {
    std::vector<Node*> nodes;
    Node* make(int v) {
        auto* n = new Node(v);
        nodes.push_back(n);
        return n;
    }
    ~NodePool() { for (auto* n : nodes) delete n; }
};

static int tests_run    = 0;
static int tests_passed = 0;

#define TEST(name) \
    static void name(); \
    struct _reg_##name { _reg_##name() { \
        ++tests_run; \
        name(); \
        ++tests_passed; \
        std::printf("  PASS  %s\n", #name); \
    }} _inst_##name; \
    static void name()

#define ASSERT(cond) \
    do { if (!(cond)) { \
        std::fprintf(stderr, "FAIL  %s:%d  assertion failed: %s\n", \
                     __FILE__, __LINE__, #cond); \
        std::abort(); \
    }} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_TRUE(x)  ASSERT(x)
#define ASSERT_FALSE(x) ASSERT(!(x))

// ---------------------------------------------------------------------------
//  1. CircularArray unit tests
// ---------------------------------------------------------------------------

TEST(circular_array_size_correct) {
    // 1<<3 = 8, 1<<5 = 32
    CircularArray<Node*> a3(3);
    CircularArray<Node*> a5(5);
    ASSERT_EQ(a3.size(), 8);
    ASSERT_EQ(a5.size(), 32);
}

TEST(circular_array_put_get_roundtrip) {
    NodePool np;
    CircularArray<Node*> arr(3); // 8 slots
    Node* n = np.make(42);
    arr.put(0, n);
    ASSERT_EQ(arr.get(0), n);
}

TEST(circular_array_wraps_modulo_size) {
    NodePool np;
    CircularArray<Node*> arr(3); // size = 8
    Node* n = np.make(99);
    // index 8 should wrap to slot 0
    arr.put(8, n);
    ASSERT_EQ(arr.get(0), n);
    ASSERT_EQ(arr.get(8), n);
}

TEST(circular_array_multiple_slots) {
    NodePool np;
    CircularArray<Node*> arr(4); // 16 slots
    std::vector<Node*> nodes;
    for (int i = 0; i < 16; ++i)
        nodes.push_back(np.make(i));
    for (int i = 0; i < 16; ++i)
        arr.put(i, nodes[i]);
    for (int i = 0; i < 16; ++i)
        ASSERT_EQ(arr.get(i), nodes[i]);
}

// ---------------------------------------------------------------------------
//  2. Single-threaded WorkStealingDeque correctness
// ---------------------------------------------------------------------------

TEST(empty_deque_pop_returns_nullopt) {
    BufferPool<Node*> pool;
    WorkStealingDeque<Node*> deque(pool);
    ASSERT_FALSE(deque.pop_bottom().has_value());
}

TEST(empty_deque_steal_returns_nullopt) {
    BufferPool<Node*> pool;
    WorkStealingDeque<Node*> deque(pool);
    ASSERT_FALSE(deque.steal().has_value());
}

TEST(push_then_pop_single_element) {
    NodePool np;
    BufferPool<Node*> pool;
    WorkStealingDeque<Node*> deque(pool);
    Node* n = np.make(1);
    deque.push_bottom(n);
    auto result = deque.pop_bottom();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), n);
}

TEST(push_then_steal_single_element) {
    NodePool np;
    BufferPool<Node*> pool;
    WorkStealingDeque<Node*> deque(pool);
    Node* n = np.make(7);
    deque.push_bottom(n);
    auto result = deque.steal();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), n);
}

TEST(pop_is_lifo) {
    // push A B C  →  pop should return C B A
    NodePool np;
    BufferPool<Node*> pool;
    WorkStealingDeque<Node*> deque(pool);
    Node* a = np.make(1);
    Node* b = np.make(2);
    Node* c = np.make(3);
    deque.push_bottom(a);
    deque.push_bottom(b);
    deque.push_bottom(c);
    ASSERT_EQ(deque.pop_bottom().value(), c);
    ASSERT_EQ(deque.pop_bottom().value(), b);
    ASSERT_EQ(deque.pop_bottom().value(), a);
    ASSERT_FALSE(deque.pop_bottom().has_value());
}

TEST(steal_is_fifo_from_top) {
    // push A B C  →  steal should return A then B then C
    NodePool np;
    BufferPool<Node*> pool;
    WorkStealingDeque<Node*> deque(pool);
    Node* a = np.make(10);
    Node* b = np.make(20);
    Node* c = np.make(30);
    deque.push_bottom(a);
    deque.push_bottom(b);
    deque.push_bottom(c);
    ASSERT_EQ(deque.steal().value(), a);
    ASSERT_EQ(deque.steal().value(), b);
    ASSERT_EQ(deque.steal().value(), c);
    ASSERT_FALSE(deque.steal().has_value());
}

TEST(pop_after_steal_empties_deque) {
    NodePool np;
    BufferPool<Node*> pool;
    WorkStealingDeque<Node*> deque(pool);
    Node* a = np.make(1);
    Node* b = np.make(2);
    deque.push_bottom(a);
    deque.push_bottom(b);
    ASSERT_EQ(deque.steal().value(), a);      // thief takes a
    ASSERT_EQ(deque.pop_bottom().value(), b); // owner takes b
    ASSERT_FALSE(deque.pop_bottom().has_value());
    ASSERT_FALSE(deque.steal().has_value());
}

TEST(deque_grows_past_initial_capacity) {
    // Initial capacity is 8 (log_size=3). Push 200 elements to force
    // multiple grow cycles.
    NodePool np;
    BufferPool<Node*> pool;
    WorkStealingDeque<Node*> deque(pool);
    const int N = 200;
    std::vector<Node*> nodes;
    for (int i = 0; i < N; ++i) {
        nodes.push_back(np.make(i));
        deque.push_bottom(nodes.back());
    }
    // Pop all in LIFO order and verify.
    for (int i = N - 1; i >= 0; --i) {
        auto r = deque.pop_bottom();
        ASSERT_TRUE(r.has_value());
        ASSERT_EQ(r.value()->value, i);
    }
    ASSERT_FALSE(deque.pop_bottom().has_value());
}

TEST(deque_shrinks_after_growth) {
    // Push enough to trigger growth, steal most elements (triggering
    // shrink in subsequent pops), then verify the remainder is correct.
    NodePool np;
    BufferPool<Node*> pool;
    WorkStealingDeque<Node*> deque(pool);
    const int N = 64;
    std::vector<Node*> nodes;
    for (int i = 0; i < N; ++i) {
        nodes.push_back(np.make(i));
        deque.push_bottom(nodes.back());
    }
    // Steal 60 elements from top (triggers shrink path when owner pops).
    int stolen = 0;
    for (int i = 0; i < 60; ++i) {
        auto r = deque.steal();
        ASSERT_TRUE(r.has_value());
        ASSERT_EQ(r.value()->value, i);
        ++stolen;
    }
    ASSERT_EQ(stolen, 60);
    // The owner should still be able to pop the remaining 4.
    for (int i = N - 1; i >= 60; --i) {
        auto r = deque.pop_bottom();
        ASSERT_TRUE(r.has_value());
        ASSERT_EQ(r.value()->value, i);
    }
    ASSERT_FALSE(deque.pop_bottom().has_value());
}

TEST(size_approximation_is_non_negative) {
    BufferPool<Node*> pool;
    WorkStealingDeque<Node*> deque(pool);
    ASSERT_EQ(deque.size(), 0);
    NodePool np;
    for (int i = 0; i < 10; ++i) {
        deque.push_bottom(np.make(i));
        ASSERT_TRUE(deque.size() > 0);
    }
}

TEST(push_pop_interleaved) {
    NodePool np;
    BufferPool<Node*> pool;
    WorkStealingDeque<Node*> deque(pool);
    // Simulate typical owner behaviour: push some, pop some, repeat.
    for (int round = 0; round < 10; ++round) {
        for (int i = 0; i < 20; ++i)
            deque.push_bottom(np.make(round * 100 + i));
        for (int i = 0; i < 15; ++i)
            ASSERT_TRUE(deque.pop_bottom().has_value());
    }
    // Drain remainder.
    int remaining = 0;
    while (deque.pop_bottom().has_value()) ++remaining;
    ASSERT_EQ(remaining, 10 * 5); // 10 rounds × 5 left each
}

// ---------------------------------------------------------------------------
//  3. Multi-threaded stress tests
// ---------------------------------------------------------------------------

// Test: one owner pushes N items; T thief threads steal; every item is
// received exactly once.
TEST(concurrent_steal_all_items_received_once) {
    const int N       = 10000;
    const int THIEVES = 4;

    NodePool np;
    BufferPool<Node*> pool;
    WorkStealingDeque<Node*> deque(pool);

    // Pre-allocate nodes so owner isn't also allocating during the test.
    std::vector<Node*> nodes;
    nodes.reserve(N);
    for (int i = 0; i < N; ++i)
        nodes.push_back(np.make(i));

    std::atomic<int> steal_count{0};
    // Track which values were stolen.
    std::vector<std::atomic<int>> seen(N);
    for (auto& a : seen) a.store(0);

    // Push all before thieves start so the deque is full.
    for (int i = 0; i < N; ++i)
        deque.push_bottom(nodes[i]);

    std::vector<std::thread> thieves;
    for (int tid = 0; tid < THIEVES; ++tid) {
        thieves.emplace_back([&]() {
            while (steal_count.load(std::memory_order_relaxed) < N) {
                auto r = deque.steal();
                if (r.has_value()) {
                    int v = r.value()->value;
                    seen[v].fetch_add(1, std::memory_order_relaxed);
                    steal_count.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    for (auto& th : thieves) th.join();

    ASSERT_EQ(steal_count.load(), N);
    for (int i = 0; i < N; ++i)
        ASSERT_EQ(seen[i].load(), 1); // each item stolen exactly once
}

// Test: owner pushes and pops while thieves steal concurrently.
// Every item that is successfully obtained (by owner pop or thief steal)
// must appear exactly once across all results.
TEST(concurrent_owner_push_pop_thieves_steal) {
    const int N       = 5000;
    const int THIEVES = 4;

    NodePool np;
    BufferPool<Node*> pool;
    WorkStealingDeque<Node*> deque(pool);

    std::vector<Node*> nodes;
    nodes.reserve(N);
    for (int i = 0; i < N; ++i)
        nodes.push_back(np.make(i));

    std::atomic<bool> done{false};
    std::vector<std::atomic<int>> seen(N);
    for (auto& a : seen) a.store(0);

    std::atomic<int> total_obtained{0};

    // Thief threads.
    std::vector<std::thread> thieves;
    for (int tid = 0; tid < THIEVES; ++tid) {
        thieves.emplace_back([&]() {
            while (!done.load(std::memory_order_acquire) || deque.size() > 0) {
                auto r = deque.steal();
                if (r.has_value()) {
                    seen[r.value()->value].fetch_add(1, std::memory_order_relaxed);
                    total_obtained.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    // Owner thread: push all N, occasionally pop some back.
    {
        std::mt19937 rng(42);
        for (int i = 0; i < N; ++i) {
            deque.push_bottom(nodes[i]);
            // Randomly pop ~20% of the time to exercise the race.
            if (std::uniform_int_distribution<int>(0, 4)(rng) == 0) {
                auto r = deque.pop_bottom();
                if (r.has_value()) {
                    seen[r.value()->value].fetch_add(1, std::memory_order_relaxed);
                    total_obtained.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }
        // Drain remaining items.
        while (true) {
            auto r = deque.pop_bottom();
            if (!r.has_value()) break;
            seen[r.value()->value].fetch_add(1, std::memory_order_relaxed);
            total_obtained.fetch_add(1, std::memory_order_relaxed);
        }
    }

    done.store(true, std::memory_order_release);
    for (auto& th : thieves) th.join();

    // Drain any items thieves missed after done was set.
    while (true) {
        auto r = deque.steal();
        if (!r.has_value()) break;
        seen[r.value()->value].fetch_add(1, std::memory_order_relaxed);
        total_obtained.fetch_add(1, std::memory_order_relaxed);
    }

    ASSERT_EQ(total_obtained.load(), N);
    for (int i = 0; i < N; ++i)
        ASSERT_EQ(seen[i].load(), 1);
}

// Test: hammer the grow/shrink cycle under theft pressure.
// Owner alternates between pushing 200 and stealing-via-pop; thieves
// steal continuously. No item should be seen more than once.
TEST(concurrent_grow_shrink_cycle_under_theft) {
    const int ROUNDS  = 20;
    const int BATCH   = 200; // large enough to trigger several grow/shrinks
    const int THIEVES = 4;

    NodePool np;
    BufferPool<Node*> pool;
    WorkStealingDeque<Node*> deque(pool);

    // All nodes pre-allocated and indexed.
    const int TOTAL = ROUNDS * BATCH;
    std::vector<Node*> nodes;
    nodes.reserve(TOTAL);
    for (int i = 0; i < TOTAL; ++i)
        nodes.push_back(np.make(i));

    std::vector<std::atomic<int>> seen(TOTAL);
    for (auto& a : seen) a.store(0);

    std::atomic<bool> done{false};
    std::atomic<int>  total_obtained{0};

    std::vector<std::thread> thieves;
    for (int tid = 0; tid < THIEVES; ++tid) {
        thieves.emplace_back([&]() {
            while (!done.load(std::memory_order_acquire) || deque.size() > 0) {
                auto r = deque.steal();
                if (r.has_value()) {
                    seen[r.value()->value].fetch_add(1, std::memory_order_relaxed);
                    total_obtained.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    int node_idx = 0;
    for (int round = 0; round < ROUNDS; ++round) {
        // Push a full batch (triggers grow).
        for (int i = 0; i < BATCH; ++i)
            deque.push_bottom(nodes[node_idx++]);
        // Pop most of them back (triggers shrink).
        for (int i = 0; i < BATCH * 3 / 4; ++i) {
            auto r = deque.pop_bottom();
            if (r.has_value()) {
                seen[r.value()->value].fetch_add(1, std::memory_order_relaxed);
                total_obtained.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }
    // Drain.
    while (true) {
        auto r = deque.pop_bottom();
        if (!r.has_value()) break;
        seen[r.value()->value].fetch_add(1, std::memory_order_relaxed);
        total_obtained.fetch_add(1, std::memory_order_relaxed);
    }

    done.store(true, std::memory_order_release);
    for (auto& th : thieves) th.join();

    while (true) {
        auto r = deque.steal();
        if (!r.has_value()) break;
        seen[r.value()->value].fetch_add(1, std::memory_order_relaxed);
        total_obtained.fetch_add(1, std::memory_order_relaxed);
    }

    ASSERT_EQ(total_obtained.load(), TOTAL);
    for (int i = 0; i < TOTAL; ++i)
        ASSERT_EQ(seen[i].load(), 1);
}

// Test: multiple deques sharing the same pool – ensures the pool correctly
// recycles buffers between different deques without data corruption.
TEST(multiple_deques_shared_pool) {
    const int N_DEQUES = 4;
    const int N_ITEMS  = 500;

    NodePool np;
    BufferPool<Node*> pool; // one shared pool for all deques

    std::vector<std::unique_ptr<WorkStealingDeque<Node*>>> deques;
    for (int d = 0; d < N_DEQUES; ++d)
        deques.push_back(std::make_unique<WorkStealingDeque<Node*>>(pool));

    // Each deque gets its own set of nodes: deque d gets values d*N_ITEMS .. (d+1)*N_ITEMS-1
    std::vector<Node*> all_nodes;
    all_nodes.reserve(N_DEQUES * N_ITEMS);
    for (int i = 0; i < N_DEQUES * N_ITEMS; ++i)
        all_nodes.push_back(np.make(i));

    // Push items, force growth (and pool reuse), then verify retrieval.
    for (int d = 0; d < N_DEQUES; ++d) {
        int base = d * N_ITEMS;
        for (int i = 0; i < N_ITEMS; ++i)
            deques[d]->push_bottom(all_nodes[base + i]);
    }

    // Drain each deque via steal and verify values are in [base, base+N_ITEMS).
    for (int d = 0; d < N_DEQUES; ++d) {
        int base = d * N_ITEMS;
        int count = 0;
        while (true) {
            auto r = deques[d]->steal();
            if (!r.has_value()) break;
            int v = r.value()->value;
            ASSERT_TRUE(v >= base && v < base + N_ITEMS);
            ++count;
        }
        ASSERT_EQ(count, N_ITEMS);
    }
}

// ---------------------------------------------------------------------------
//  main
// ---------------------------------------------------------------------------
int main() {
    std::printf("\n=== Chase-Lev Deque Test Suite ===\n\n");
    std::printf("Results: %d / %d passed\n\n", tests_passed, tests_run);
    if (tests_passed == tests_run)
        std::printf("ALL TESTS PASSED\n\n");
    else
        std::printf("SOME TESTS FAILED\n\n");
    return tests_passed == tests_run ? 0 : 1;
}
