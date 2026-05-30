#include "N_queens.h"
#include "parallel_engine.h"

#include <chrono>
#include <iostream>

bool N_Queens_Node::under_attack(int row, int col, int num_queens){
            //we assume that this function gets called when we know there is no queen on the same row
            for(int r=0; r<num_queens; ++r){
                if(queens[r] == col)
                    return true; //position is under attack
            }

            int N = static_cast<int>(queens.size());
            //check diagonals, we want to have one counter so we start both search in the bottom of the diagonal and move upwards
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

                if (br >= 0 && br < num_queens && queens[br] == bc)
                    return true;

                if (fr >= 0 && fr < num_queens && queens[fr] == fc)
                    return true;

                ++h;
            }

            return false;
};

std::vector<N_Queens_Node> queens_successors(N_Queens_Node& node){
    std::vector<N_Queens_Node> successors;
    int N = node.queens.size();
    if (N == node.depth) return successors; //terminal node

    int row = node.depth; //the next row to fill is always depth
    for(int col=0; col<N; ++col){
        if(!node.under_attack(row, col, node.depth)){
            node.queens[row] = col;
            successors.push_back(N_Queens_Node(node.queens, node.depth+1));
            node.queens[row] = -1; //reset it
        }
    }
    
    
    return successors;

};

int queens_map(N_Queens_Node& node){
    return (node.depth == static_cast<int>(node.queens.size())) ? 1 : 0;
};


int queens_reduce(int a, int b){
    return a+b;
};

int main(int argc, char** argv){

    int N = 8;
    std::vector<int> queens;
    queens.resize(N, -1);
    N_Queens_Node queen_start(queens, 0);

    int threads = 4;

    ParallelRES<N_Queens_Node,int> eng({queen_start}, queens_successors, 0, threads);

    auto start = std::chrono::high_resolution_clock::now();
    int ret = eng.map_reduce(queens_map, queens_reduce);
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "with threads " << threads << " in time " << elapsed.count() << " got result " << ret << "\n";
    return 0;
    

};
