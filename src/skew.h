#ifndef SKEW_H
#define SKEW_H
 
#include <vector>
 // synthetic tree with fixed amount of total work 
//but based on value p work is distibuted to left or right child to create imbalances or balanced tree with same amount of work


struct Skew_Node {
    long long budget;   // how many leaves this subtree must still produce
    int       depth;
};
 
extern double skew_p;     // split ratio in [0,1]
extern int    skew_work;  // amount of busy-work each node does in skew_map
 
std::vector<Skew_Node> skew_successors(Skew_Node& n);
long long skew_map(Skew_Node& n);
long long skew_reduce(long long a, long long b);
 
#endif
 
