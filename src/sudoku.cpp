#include "sudoku.h"
#include "parallel_engine.h"
#include <unordered_set>
#include <stdexcept>
#include <algorithm>
#include <chrono>

void Sudoku_Node::set_grid_num(int row, int col, int num){
    if(num < 1 || num > 9){
        throw std::invalid_argument("Trying to set a cell in the grid with an invalid number");
    }

    if(0 <= row && row <= 8 && 0 <= col && col <= 8){
        this->grid[9*row + col] = num;
        return;
    }

    throw std::invalid_argument("Trying to set the grid number of an invalid row or column, both have to be between 0 and 8");


};

int Sudoku_Node::get_grid_num(int row, int col){
    if(0 <= row && row <= 8 && 0 <= col && col <= 8){
        return this->grid[9*row + col];
    }
    throw std::invalid_argument("Trying to get the grid number of an invalid row or column both have to be between 0 and 8");
};


//only call this function on empty cells, if the clients maintains this contract, the function is a lot faster
std::unordered_set<int> Sudoku_Node::get_cand_numbers(int row, int col){
    //return all the numbers between 1-9 we can legally place at position (row, col)
    if(this->grid[9*row + col] != 0){
        throw std::invalid_argument("Tried to obtain the legal numbers at an already filled position");
    }

    //at the start we assume that we can add all numbers and we will remove them one by one
    std::unordered_set<int> cand_nums = {1,2,3,4,5,6,7,8,9}; 

    //check row and column
    for(size_t i=0; i<9; ++i){
        cand_nums.erase(grid[9*row + i]); //remove the number we find at this position from the candidate numbers (safe behaviour)
        cand_nums.erase(grid[9*i + col]); 

    }

    //check cell's box, (8,8) -> (6,6) (everything gets mapped to the top left cell of the box)
    int box_row = (row/3)*3;
    int box_col = (col/3)*3; 
    for(size_t h1=0; h1<3; ++h1){
        for(size_t h2=0; h2<3; ++h2){
            cand_nums.erase(grid[9*(box_row + h1) + (box_col+h2)]);
        }
    }

    return cand_nums;
};



std::vector<Sudoku_Node> sudoku_successors(Sudoku_Node& node){
    std::vector<Sudoku_Node> children;
    std::unordered_set<int> cand_numbers;
    std::array<int, 81> new_grid = node.grid;
    for(size_t row=0; row<9; ++row){
        for(size_t col=0; col<9; ++col){
            if(node.grid[9*row + col] == 0){
                //this is an empty cell that we can fill
                cand_numbers = node.get_cand_numbers(row, col);
                for(int cand_num: cand_numbers){
                    //create a grid with cand_num at (row, col)
                    new_grid[9*row + col] = cand_num;
                    children.push_back(Sudoku_Node(new_grid));
                }
                new_grid[9*row + col] = 0; //reset the cell we change back to what it was previously
            }
        }
    }

    return children;

};
std::vector<Sudoku_Node> sudoku_successors_one(Sudoku_Node& node){
    std::vector<Sudoku_Node> children;
    std::unordered_set<int> cand_numbers;
    std::array<int, 81> new_grid = node.grid;
    for(size_t row=0; row<9; ++row){
        for(size_t col=0;col<9;++col){
            if(node.grid[9*row + col] == 0){
                //found our first 0
                cand_numbers = node.get_cand_numbers(row, col); //find all numbers we can put in the cell
                for(int cand_num: cand_numbers){
                    new_grid[9*row + col] = cand_num;
                    children.push_back(Sudoku_Node(new_grid));
                }
                return children;
            }
        }
    }

    //there are no children (leaf)
    return {};
}


//claude had the idea to use bitmasks
bool is_legal(Sudoku_Node& node){
    int row_mask, col_mask, box_mask;

    // check rows and columns
    for(size_t i=0; i<9; ++i){
        row_mask = 0;
        col_mask = 0;
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

    // check all 9 boxes
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


//so if we reach the leaves of the search tree (a complete sudoku) we know this sudoku is valid
bool sudoku_map(Sudoku_Node& node){
    //check if this is a complete sudoku
    int zero_count = std::count(node.grid.begin(), node.grid.end(), 0); //count the number of 0s (empty cells) there still are
    if(zero_count > 0){
        //not a complete grid
        return false;
    }
    return is_legal(node);
};

bool sudoku_reduce(bool b_1, bool b_2){
    return b_1 || b_2;
};

int sudoku_count_map(Sudoku_Node& node){
    for(int v: node.grid){
        if(v == 0){
            return 0;
        }
    }
    //the sudoku is complete so we check if it is legal
    return is_legal(node) ? 1 : 0;
};

int sudoku_count_reduce(int a, int b){
    return a+b;
};


int main(int argc, char** argv){


   std::array<int, 81> extreme_grid = {3, 0, 0,  0, 4, 9,  0, 0, 0,
                                       0, 0, 0,  6, 0, 0,  5, 0, 1,
                                       7, 5, 2,  0, 0, 1,  0, 0, 0,
                                       
                                       0, 0, 1,  0, 0, 0,  7, 0, 0,
                                       5, 0, 0,  3, 9, 6,  0, 0, 0,
                                       0, 0, 8,  1, 5, 0,  0, 9, 6,

                                       0, 0, 3,  0, 1, 0,  0, 6, 0,
                                       0, 0, 4,  0, 0, 0,  1, 0, 0,
                                       0, 0, 0,  0, 2, 8,  0, 0, 0,

    };

    std::array<int, 81> easy_grid = {8, 0, 1,  0, 0, 3,  9, 0, 6,
                                     0, 0, 9,  0, 0, 7,  8, 5, 0,
                                     2, 5, 0,  1, 0, 0,  4, 7, 0,

                                     5, 0, 0,  0, 6, 1,  7, 0, 4,
                                     7, 6, 0,  8, 3, 0,  0, 0, 0,
                                     0, 3, 2,  0, 0, 0,  0, 0, 0,

                                     0, 2, 0,  0, 1, 9,  5, 0, 0,
                                     0, 0, 5,  0, 0, 0,  3, 0, 2,
                                     0, 0, 0,  4, 5, 2,  1, 9, 7,
    };

    Sudoku_Node sudoku(extreme_grid);
    int threads = 4;
    ParallelRES<Sudoku_Node,int> eng({sudoku}, sudoku_successors_one, 0, threads);

    auto start = std::chrono::high_resolution_clock::now();
    int ret = eng.map_reduce(sudoku_count_map, sudoku_count_reduce);
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "got return value " << ret << " in time " << elapsed.count() << "\n";
    return 0;
};
