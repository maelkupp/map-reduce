#ifndef SUDOKU_H
#define SUDOKU_H

#include <array>
#include <unordered_set>
#include <vector>

//class to represent a sudoku, an empty cell is represented with a 0
class Sudoku_Node{
    public:
        Sudoku_Node(std::array<int, 81> grid): grid(grid) {};
        std::array<int, 81> grid;

        std::unordered_set<int> get_cand_numbers(int row, int col); // returns a set of integers which can be legally placed at grid[9*row + col]
        //setters and getters
        int get_grid_num(int row, int col);
        void set_grid_num(int row, int col, int num);

};

//returns all the valid sudokus we can create by adding one more number to a box, empty if we cannot add numbers (invalid sudoku)
std::vector<Sudoku_Node> sudoku_successors(Sudoku_Node& node);
//for now just keeping it simple and trying to find a single solution, nothing more
bool sudoku_map(Sudoku_Node& node); //maps from a node to true if complete false otherwise
bool sudoku_reduce(bool b_1, bool b_2); //returns true if b_1 is true or b_2 is true (we have found a solution to the sudoku)


std::vector<Sudoku_Node> sudoku_successors_one(Sudoku_Node& node);
int  sudoku_count_map(Sudoku_Node& node);   // 1 if grid complete & legal, else 0
int  sudoku_count_reduce(int a, int b);      // a + b

bool is_legal(Sudoku_Node& node);


#endif