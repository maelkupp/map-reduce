#ifndef RES_H
#define RES_H

#include <functional>
#include <deque>
#include <iostream>

template <class Node, class Result> 
class RecursivelyEnumeratedSet{
    public:
        std::function<std::vector<Node>(Node&)> successors; //the successors function that allows us to get the children of a node
        std::vector<Node> seeds; //the initial seeds we start our search with
        Result neutral_element;

        //the map and successor functions will get passed the Node object by reference
        RecursivelyEnumeratedSet(std::vector<Node> seeds, std::function<std::vector<Node>(Node&)> successors, Result neutral_element): 
                                seeds(seeds), successors(successors), neutral_element(neutral_element) {};
        
        
        Result map_reduce(std::function<Result(Node&)> map, std::function<Result(Result, Result)> reduce); //passes the two functions we will use in the map reduce
        Result dfs(Node seed, std::function<Result(Node&)> map, std::function<Result(Result, Result)> reduce); //performs the dfs search starting from a seed
};





template <typename Node, typename Result>
Result RecursivelyEnumeratedSet<Node, Result>::map_reduce(std::function<Result(Node&)> map, std::function<Result(Result, Result)> reduce){
    //a dfs map_reduce
    Result total = this->neutral_element;
    for(auto& seed: this->seeds){
        Result r = this->dfs(seed, map, reduce);
        total = reduce(total, r);
    }
    return total;
};


//a singlethreaded dfs search through the tree, with a single deque, just to test everything works before we go multithreaded
template <typename Node, typename Result>
Result RecursivelyEnumeratedSet<Node, Result>::dfs(Node seed, std::function<Result(Node&)> map, std::function<Result(Result, Result)> reduce){
    Result res = map(seed);
    std::deque<Node> tree_deque {seed};
    while(!tree_deque.empty()){
        Node curr_node = tree_deque.front(); //get the front of the deque
        tree_deque.pop_front(); //remove the front of the deque
        std::vector<Node> children = this->successors(curr_node);
        for(auto& child: children){
            res = reduce(res, map(child));
            tree_deque.push_front(child);
        }
    }

    return res;
}

#endif