#ifndef GRAPH_H
#define GRAPH_H

#include <vector>
#include <cstdint>

struct Graph {
    int n;                                  // number of vertices (n <= 63)
    std::vector<std::vector<int>> adj;
    Graph(int n): n(n), adj(n) {}
    void add_edge(int u, int v){ adj[u].push_back(v); adj[v].push_back(u); }
};

// a partial simple path: where we are, and which vertices are already used
class Graph_Node {
    public:
        const Graph* g;        // shared graph, only the pointer is copied
        int current;
        uint64_t visited;      // bitmask of visited vertices
        int count;             // how many vertices visited so far
        Graph_Node(const Graph* g, int current, uint64_t visited, int count)
            : g(g), current(current), visited(visited), count(count) {}
};

std::vector<Graph_Node> graph_successors(Graph_Node& node); // extend path to unvisited neighbours
int hamiltonian_map(Graph_Node& node);                      // 1 if path covers every vertex
int hamiltonian_reduce(int a, int b);                       // a + b  -> counts Hamiltonian paths

#endif
