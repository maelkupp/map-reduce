#ifndef N_QUEENS_H
#define N_QUEENS_H
#include <vector>
#include <memory>


/*
the N- queens problem asks in how many different way can we place N queens on a NxN chessboard so that all N queens are safe, no other queen is attacking it

*/
class N_Queens_Node{
    public:

        std::vector<int> queens; //queens[i] = j means on row i there is a queen at column j, 0<= i, j <= N-1, if j = -1 there is not queen on this row
        int depth; //how many queens we have placed
        N_Queens_Node(std::vector<int> queens, int depth): queens(queens), depth(depth) {};
        
        //returns if this position is under attack by another queen or not, assumes that the position is free and that there is no queen on the same row
        bool under_attack(int row, int col, int num_queens);

};


std::vector<N_Queens_Node> queens_successors(N_Queens_Node& node); //returns a vector of nodes
int queens_map(N_Queens_Node& node); //returns 1 if all N queens are on the board and they are all safe
int queens_reduce(int a, int b); // returns a + b, we want to count how many solutions there are


#endif
