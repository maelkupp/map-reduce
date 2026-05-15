#include <functional>


template <class Node, class Result> 
class RecursivelyEnumeratedSet{
    public:
        std::function<std::vector<Node>(Node)> successors; //the successors function that allows us to get the children of a node
        std::vector<Node> seeds; //the initial seeds we start our search with


        RecursivelyEnumeratedSet(std::vector<Node> seeds, std::function<std::vector<Node>(Node)> successors): seeds(seeds), successors(successors);
        Result map_reduce(std::function<Result(Node)> map, std::function<Result(Result, Result)> reduce); //passes the two functions we will use in the map reduce
};