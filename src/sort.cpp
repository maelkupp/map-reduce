#include "sort.h"
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

    // Print the vector

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