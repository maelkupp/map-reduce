#include "graph.h"
#include "parallel_engine.h"
#include "benchmark.h"

#include <string>

// Successors of a partial path: every neighbour of the current vertex that we
// haven't visited yet becomes a new, longer path. The visited set is a bitmask
// so copying a node is cheap.
std::vector<Graph_Node> graph_successors(Graph_Node& node){
    std::vector<Graph_Node> children;
    for(int nb : node.g->adj[node.current]){
        uint64_t bit = (uint64_t)1 << nb;
        if(!(node.visited & bit)){
            children.push_back(Graph_Node(node.g, nb, node.visited | bit, node.count + 1));
        }
    }
    return children;
}

// A path is Hamiltonian when it has visited every vertex.
int hamiltonian_map(Graph_Node& node){
    return (node.count == node.g->n) ? 1 : 0;
}

// Counting, so reduce is just addition.
int hamiltonian_reduce(int a, int b){
    return a + b;
}

int main(int argc, char** argv){
    // usage: ./graph_bench <n> <trials>
    // We use the complete graph K_n; from a fixed start it has exactly (n-1)!
    // Hamiltonian paths, which makes the expected answer easy to check.
    int n      = (argc > 1) ? std::stoi(argv[1]) : 11;
    int trials = (argc > 2) ? std::stoi(argv[2]) : 3;

    Graph g(n);
    for(int u = 0; u < n; ++u)
        for(int v = u + 1; v < n; ++v)
            g.add_edge(u, v);

    int start = 0;
    Graph_Node seed(&g, start, (uint64_t)1 << start, 1);
    std::vector<Graph_Node> seeds = { seed };

    compare_strategies<Graph_Node, int>(
        "Hamiltonian paths in K_" + std::to_string(n) + " (expect (n-1)!)",
        seeds, graph_successors, hamiltonian_map, hamiltonian_reduce,
        0, {1, 2, 4, 8}, trials);

    return 0;
}
