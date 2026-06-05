#ifndef SORT_H
#define SORT_H

#include <vector>

using Sort_Node = std::vector<int>;

std::vector<int> sort_reduce(std::vector<int> a, std::vector<int> b);
std::vector<Sort_Node> sort_successors(Sort_Node& s_node);
std::vector<int> sort_map(Sort_Node& s_node); //returns the vector at the leaves

#endif