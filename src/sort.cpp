#include "sort.h"
#include "parallel_engine.h"
#include "benchmark.h"

#include <vector>
#include <random>
#include <string>

// Merge two already-sorted vectors into one sorted vector.
// This is the reduce step: combining two sorted sub-results stays sorted, and
// since the merged result is always the fully sorted union it doesn't matter in
// what order the reductions happen (associative + commutative), which is what
// lets us run it in parallel.
std::vector<int> sort_reduce(std::vector<int> a, std::vector<int> b){
    if(a.empty()) return b;
    if(b.empty()) return a;

    size_t p_a{0}, p_b{0}, p_out{0};
    size_t N_a = a.size();
    size_t N_b = b.size();

    std::vector<int> merged;
    merged.resize(N_a + N_b);

    while(p_a < N_a && p_b < N_b){
        if(a[p_a] <= b[p_b]) merged[p_out++] = a[p_a++];
        else                 merged[p_out++] = b[p_b++];
    }
    while(p_a < N_a) merged[p_out++] = a[p_a++];
    while(p_b < N_b) merged[p_out++] = b[p_b++];

    return merged;
}

// Split a node in half. The two halves are the children explored further down
// the tree. A node of size 1 has no children (it's a leaf).
std::vector<Sort_Node> sort_successors(Sort_Node& s_node){
    auto mid = std::next(s_node.begin(), (s_node.end() - s_node.begin())/2);
    if (mid == s_node.begin()) return {};   // size <= 1, nothing to split

    std::vector<int> left (s_node.begin(), mid);
    std::vector<int> right(mid, s_node.end());
    return {left, right};
}

// Leaves (size 1) contribute their single element; internal nodes contribute
// nothing (an empty vector merges as identity).
std::vector<int> sort_map(Sort_Node& s_node){
    if(s_node.size() > 1) return {};
    return s_node;
}

int main(int argc, char** argv){
    // usage: ./sort_bench <array length> <trials>
    int N      = (argc > 1) ? std::stoi(argv[1]) : 50000;
    int trials = (argc > 2) ? std::stoi(argv[2]) : 3;

    // fixed seed -> the input is identical every run, so timings are comparable
    std::mt19937 gen(12345);
    std::uniform_int_distribution<int> dist(0, 1000000);
    std::vector<int> data(N);
    for(int i = 0; i < N; ++i) data[i] = dist(gen);

    std::vector<Sort_Node> seeds = { data };   // one seed: the whole array

    compare_strategies<Sort_Node, Sort_Node>(
        "Parallel merge sort (N=" + std::to_string(N) + ")",
        seeds, sort_successors, sort_map, sort_reduce,
        Sort_Node{}, {1, 2, 4, 8}, trials);

    return 0;
}#include "sort.h"
#include "parallel_engine.h"
#include "benchmark.h"


#include <vector>
#include <iostream>
#include <random>


std::vector<int> sort_reduce(std::vector<int> a, std::vector<int>b){
    //takes two sorted arrays and returns a merged sorted array
    if(a.empty()) return b;
    if(b.empty()) return a;


    size_t p_a {0};
    size_t p_b {0};
    size_t p_out {0};


    size_t N_a = a.size();
    size_t N_b = b.size();
    std::vector<int> merged {};
    merged.resize(N_a + N_b);
    while(p_a < N_a && p_b < N_b){
        if(a[p_a] <= b[p_b]){
            merged[p_out++] = a[p_a++];
        }else{
            merged[p_out++] = b[p_b++];
        }
    }

    while(p_a < N_a) merged[p_out++] = a[p_a++];
    

    while(p_b < N_b) merged[p_out++] = b[p_b++];
    

    return merged;
};


std::vector<Sort_Node> sort_successors(Sort_Node& s_node){
    auto mid = std::next(s_node.begin(), (s_node.end() - s_node.begin())/2);
    if (mid == s_node.begin()) return {};

    std::vector<int> left = std::vector<int>(s_node.begin(), mid);
    std::vector<int> right = std::vector<int>(mid, s_node.end());
    return {left, right};
};

std::vector<int> sort_map(Sort_Node& s_node){
    if(s_node.size() > 1) return {}; //internal node skip it
    return s_node;
};

int main(int argc, char** argv){
    if(argc < 3){
        std::cout << "Usage " << argv[0] << " <length of array>" << " <num_threads>\n";
        return 0;
    }

    int N = std::stoi(argv[1]);
    int n = std::stoi(argv[2]);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 100000); // range [0, 1000]

    // Generate vector of N random integers
    std::vector<int> seed(N);
    for (int i = 0; i < N; i++) {
        seed[i] = dist(gen);
    }


    std::cout << "num treads " <<  n << "\n";
    ParallelRES<Sort_Node,Sort_Node> eng({seed}, sort_successors, {}, n);

    auto start = std::chrono::high_resolution_clock::now();
    std::vector<int> ret = eng.map_reduce(sort_map, sort_reduce);
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout <<" in time " << elapsed.count() << "\n";

    auto s = std::chrono::high_resolution_clock::now();
    std::sort(seed.begin(), seed.end());
    auto e = std::chrono::high_resolution_clock::now();
    auto elapsed_std = e - s;
    std::cout << "std::sort in time " << elapsed_std.count() << "\n";

    return 0;
};
