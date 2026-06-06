#include "skew.h"
#include "parallel_engine.h"
#include "benchmark.h"

#include <vector>
#include <string>

double skew_p    = 0.5;
int    skew_work = 200;

// Somewhere for the busy-work to land so the compiler can't optimise it away.
static volatile long long sink = 0;

// Split this node's budget into two children. The split is lopsided by skew_p,
// clamped so both sides keep at least 1 unit. A budget of 1 (or less) is a leaf.
std::vector<Skew_Node> skew_successors(Skew_Node& n){
    if(n.budget <= 1) return {};
    long long left = (long long)((double)n.budget * skew_p);
    if(left < 1)            left = 1;
    if(left > n.budget - 1) left = n.budget - 1;
    long long right = n.budget - left;
    return { Skew_Node{left, n.depth + 1}, Skew_Node{right, n.depth + 1} };
}

// Burn a fixed amount of CPU per node, then count this node as 1.
long long skew_map(Skew_Node& n){
    (void)n;
    long long acc = 0;
    for(int i = 0; i < skew_work; ++i) acc += (long long)i * i;
    sink += acc;
    return 1;
}

long long skew_reduce(long long a, long long b){ return a + b; }

int main(int argc, char** argv){
    // usage: ./skew_bench <budget> <trials> [skew_p] [work]
    long long budget = (argc > 1) ? std::stoll(argv[1]) : 500000;
    int       trials = (argc > 2) ? std::stoi(argv[2])  : 3;
    if(argc > 3) skew_p    = std::stod(argv[3]);   // optional: how lopsided
    if(argc > 4) skew_work = std::stoi(argv[4]);   // optional: work per node

    // A tree of budget B has  2B-1 nodes, and every node maps to 1, so
    // the expected count is 2*budget - 1 regardless of how skewed the tree is.
    std::vector<Skew_Node> seeds = { Skew_Node{budget, 0} };

    compare_strategies<Skew_Node, long long>(
        "Skewed tree (budget=" + std::to_string(budget) +
            ", p=" + std::to_string(skew_p) +
            ", work=" + std::to_string(skew_work) + ")",
        seeds, skew_successors, skew_map, skew_reduce,
        0LL, {1, 2, 4, 8}, trials);

    return 0;
}
