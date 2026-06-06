#include "N_queens.h"
#include "parallel_engine.h"
#include "benchmark.h"

#include <string>

// Is (row, col) attacked by an already-placed queen? We assume no queen shares
// this row yet, so we only have to check earlier columns and the two diagonals.
bool N_Queens_Node::under_attack(int row, int col, int num_queens){
    for(int r=0; r<num_queens; ++r)
        if(queens[r] == col) return true;          // same column

    int N = static_cast<int>(queens.size());
    // Walk both diagonals from the bottom upwards with a single counter.
    int bottom_right_steps = std::min(num_queens - 1 - row, N - 1 - col);
    int bs_row = row + bottom_right_steps;
    int bs_col = col + bottom_right_steps;

    int bottom_left_steps = std::min(num_queens - 1 - row, col);
    int fs_row = row + bottom_left_steps;
    int fs_col = col - bottom_left_steps;

    int h = 0;
    while (bs_row - h >= 0 || fs_row - h >= 0) {
        int br = bs_row - h, bc = bs_col - h;
        int fr = fs_row - h, fc = fs_col + h;
        if (br >= 0 && br < num_queens && queens[br] == bc) return true;
        if (fr >= 0 && fr < num_queens && queens[fr] == fc) return true;
        ++h;
    }
    return false;
}

// Place a queen in the next empty row in every safe column -> one child each.
std::vector<N_Queens_Node> queens_successors(N_Queens_Node& node){
    std::vector<N_Queens_Node> successors;
    int N = node.queens.size();
    if (N == node.depth) return successors;          // board already full

    int row = node.depth;
    for(int col=0; col<N; ++col){
        if(!node.under_attack(row, col, node.depth)){
            node.queens[row] = col;
            successors.push_back(N_Queens_Node(node.queens, node.depth+1));
            node.queens[row] = -1;                   // undo before next column
        }
    }
    return successors;
}

// A leaf with all N queens placed is one valid arrangement.
int queens_map(N_Queens_Node& node){
    return (node.depth == static_cast<int>(node.queens.size())) ? 1 : 0;
}
int queens_reduce(int a, int b){ return a + b; }

int main(int argc, char** argv){
    // usage: ./queens_bench <N> <trials>
    int N      = (argc > 1) ? std::stoi(argv[1]) : 13;
    int trials = (argc > 2) ? std::stoi(argv[2]) : 3;

    std::vector<int> start(N, -1);
    std::vector<N_Queens_Node> seeds = { N_Queens_Node(start, 0) };

    compare_strategies<N_Queens_Node, int>(
        "N-Queens solution count (N=" + std::to_string(N) + ")",
        seeds, queens_successors, queens_map, queens_reduce,
        0, {1, 2, 4, 8}, trials);

    return 0;
}
