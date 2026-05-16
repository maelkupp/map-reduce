#include "RecursivelyEnumeratedSet.h"
#include "sudoku.h"
#include <iostream>
#include <unordered_set>


int main(int argc, char** argv){
    std::cout << "in main\n";

    //for now hard code sudokus with an array of size 81

    //nearly complete sudoku to test the single threaded map-reduce
    std::array<int, 81> grid = {5, 3, 0,  6, 7, 8,  9, 1, 2,
                                6, 7, 2,  1, 9, 5,  3, 4, 8,
                                1, 9, 8,  3, 4, 2,  5, 6, 7,

                                8, 5, 9,  7, 6, 1,  4, 2, 3,
                                4, 2, 6,  8, 5, 3,  7, 9, 1,
                                7, 1, 3,  9, 2, 4,  8, 5, 6,

                                9, 6, 1,  5, 3, 7,  2, 8, 4,
                                2, 8, 7,  4, 1, 9,  6, 3, 5,
                                3, 4, 5,  2, 8, 6,  1, 7, 0,
    };

    Sudoku_Node sudoku(grid);
    std::unordered_set<int> cand = sudoku.get_cand_numbers(8,8);
    RecursivelyEnumeratedSet<Sudoku_Node, bool> engine({sudoku}, sudoku_successors, false); // the neutral element is false
    bool solvable = engine.map_reduce(sudoku_map, sudoku_reduce);
    std::cout << "solvable " << solvable <<"\n";

    return 0;
}