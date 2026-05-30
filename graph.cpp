#include "graph.h"
#include "parallel_engine.h"
#include <iostream>
#include <chrono>
#include <string>

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

int hamiltonian_map(Graph_Node& node){
    return (node.count == node.g->n) ? 1 : 0;
}

int hamiltonian_reduce(int a, int b){
    return a + b;
}

int main(int argc, char** argv){
    int n       = (argc > 1) ? std::stoi(argv[1]) : 8;
    int threads = (argc > 2) ? std::stoi(argv[2]) : 4;

    // complete graph K_n: from a fixed start there are exactly (n-1)! Hamiltonian paths
    Graph g(n);
    for(int u = 0; u < n; ++u)
        for(int v = u + 1; v < n; ++v)
            g.add_edge(u, v);

    int start = 0;
    Graph_Node seed(&g, start, (uint64_t)1 << start, 1);

    ParallelRES<Graph_Node, int> eng({seed}, graph_successors, 0, threads);

    auto t0 = std::chrono::high_resolution_clock::now();
    int ret = eng.map_reduce(hamiltonian_map, hamiltonian_reduce);
    auto t1 = std::chrono::high_resolution_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0);

    std::cout << "Hamiltonian paths from vertex " << start
              << " in K_" << n << " = " << ret
              << "  (threads " << threads << ", time " << us.count() << " us)\n";
    return 0;
}
