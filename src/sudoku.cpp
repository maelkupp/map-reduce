#include "sudoku.h"
#include "parallel_engine.h"
#include "benchmark.h"

#include <unordered_set>
#include <stdexcept>
#include <algorithm>
#include <string>

void Sudoku_Node::set_grid_num(int row, int col, int num){
    if(num < 1 || num > 9)
        throw std::invalid_argument("Trying to set a cell with an invalid number");

    if(0 <= row && row <= 8 && 0 <= col && col <= 8){
        this->grid[9*row + col] = num;
        return;
    }
    throw std::invalid_argument("set: row/col must both be between 0 and 8");
}

int Sudoku_Node::get_grid_num(int row, int col){
    if(0 <= row && row <= 8 && 0 <= col && col <= 8)
        return this->grid[9*row + col];
    throw std::invalid_argument("get: row/col must both be between 0 and 8");
}

// Numbers 1-9 that can legally go in an empty cell. Caller must only ask about
// empty cells (the contract keeps this fast); we start from the full set and
// remove anything already present in the row, column, or 3x3 box.
std::unordered_set<int> Sudoku_Node::get_cand_numbers(int row, int col){
    if(this->grid[9*row + col] != 0)
        throw std::invalid_argument("Asked for candidates of an already-filled cell");

    std::unordered_set<int> cand_nums = {1,2,3,4,5,6,7,8,9};

    for(size_t i=0; i<9; ++i){
        cand_nums.erase(grid[9*row + i]);   // same row
        cand_nums.erase(grid[9*i + col]);   // same column
    }

    int box_row = (row/3)*3;
    int box_col = (col/3)*3;
    for(size_t h1=0; h1<3; ++h1)
        for(size_t h2=0; h2<3; ++h2)
            cand_nums.erase(grid[9*(box_row + h1) + (box_col+h2)]);

    return cand_nums;
}

// Branch on every empty cell (used by the "count all completions" variant).
std::vector<Sudoku_Node> sudoku_successors(Sudoku_Node& node){
    std::vector<Sudoku_Node> children;
    std::unordered_set<int> cand_numbers;
    std::array<int, 81> new_grid = node.grid;
    for(size_t row=0; row<9; ++row){
        for(size_t col=0; col<9; ++col){
            if(node.grid[9*row + col] == 0){
                cand_numbers = node.get_cand_numbers(row, col);
                for(int cand_num: cand_numbers){
                    new_grid[9*row + col] = cand_num;
                    children.push_back(Sudoku_Node(new_grid));
                }
                new_grid[9*row + col] = 0;
            }
        }
    }
    return children;
}

// Branch only on the *first* empty cell. This is the one we benchmark: the
// search tree is far smaller because each level fills exactly one fixed cell.
std::vector<Sudoku_Node> sudoku_successors_one(Sudoku_Node& node){
    std::vector<Sudoku_Node> children;
    std::unordered_set<int> cand_numbers;
    std::array<int, 81> new_grid = node.grid;
    for(size_t row=0; row<9; ++row){
        for(size_t col=0; col<9; ++col){
            if(node.grid[9*row + col] == 0){
                cand_numbers = node.get_cand_numbers(row, col);
                for(int cand_num: cand_numbers){
                    new_grid[9*row + col] = cand_num;
                    children.push_back(Sudoku_Node(new_grid));
                }
                return children;       // only the first empty cell
            }
        }
    }
    return {};                          // no empty cell -> leaf
}

// Bitmask validity check for a fully or partially filled grid.
bool is_legal(Sudoku_Node& node){
    int row_mask, col_mask, box_mask;

    for(size_t i=0; i<9; ++i){
        row_mask = 0; col_mask = 0;
        for(size_t j=0; j<9; ++j){
            int row_val = node.grid[9*i + j];
            int col_val = node.grid[9*j + i];
            if(row_val != 0){
                if(row_mask & (1 << row_val)) return false;
                row_mask |= (1 << row_val);
            }
            if(col_val != 0){
                if(col_mask & (1 << col_val)) return false;
                col_mask |= (1 << col_val);
            }
        }
    }

    for(size_t bi=0; bi<3; ++bi){
        for(size_t bj=0; bj<3; ++bj){
            box_mask = 0;
            for(size_t di=0; di<3; ++di){
                for(size_t dj=0; dj<3; ++dj){
                    int val = node.grid[9*(3*bi+di) + (3*bj+dj)];
                    if(val != 0){
                        if(box_mask & (1 << val)) return false;
                        box_mask |= (1 << val);
                    }
                }
            }
        }
    }
    return true;
}

// "find a solution" variant: true once we reach a complete, legal grid.
bool sudoku_map(Sudoku_Node& node){
    int zero_count = std::count(node.grid.begin(), node.grid.end(), 0);
    if(zero_count > 0) return false;
    return is_legal(node);
}
bool sudoku_reduce(bool b_1, bool b_2){ return b_1 || b_2; }

// "count solutions" variant: 1 for each complete legal grid we land on.
int sudoku_count_map(Sudoku_Node& node){
    for(int v: node.grid) if(v == 0) return 0;
    return is_legal(node) ? 1 : 0;
}
int sudoku_count_reduce(int a, int b){ return a + b; }

int main(int argc, char** argv){
    // usage: ./sudoku_bench <trials>
    int trials = (argc > 1) ? std::stoi(argv[1]) : 3;

    // A hard grid (unique solution) so the expected count is 1.
    std::array<int, 81> extreme_grid = {
        3, 0, 0,  0, 4, 9,  0, 0, 0,
        0, 0, 0,  6, 0, 0,  5, 0, 1,
        7, 5, 2,  0, 0, 1,  0, 0, 0,

        0, 0, 1,  0, 0, 0,  7, 0, 0,
        5, 0, 0,  3, 9, 6,  0, 0, 0,
        0, 0, 8,  1, 5, 0,  0, 9, 6,

        0, 0, 3,  0, 1, 0,  0, 6, 0,
        0, 0, 4,  0, 0, 0,  1, 0, 0,
        0, 0, 0,  0, 2, 8,  0, 0, 0,
    };

    Sudoku_Node start(extreme_grid);
    std::vector<Sudoku_Node> seeds = { start };

    compare_strategies<Sudoku_Node, int>(
        "Sudoku solution count (hard grid, unique solution)",
        seeds, sudoku_successors_one, sudoku_count_map, sudoku_count_reduce,
        0, {1, 2, 4, 8}, trials);

    return 0;
}

