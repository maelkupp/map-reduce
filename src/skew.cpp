#include "skew.h"
#include "parallel_engine.h"
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <chrono>
#include <algorithm>

double skew_p    = 0.5;
int    skew_work = 200;

static volatile long long sink = 0;

std::vector<Skew_Node> skew_successors(Skew_Node& n){
    if(n.budget <= 1) return {};                       
    long long left = (long long)((double)n.budget * skew_p);
    if(left < 1)            left = 1;
    if(left > n.budget - 1) left = n.budget - 1;        
    long long right = n.budget - left;
    return { Skew_Node{left, n.depth + 1}, Skew_Node{right, n.depth + 1} };
}

long long skew_map(Skew_Node& n){
    (void)n;
    long long acc = 0;
    for(int i = 0; i < skew_work; ++i) acc += (long long)i * i;
    sink += acc;
    return 1;                                          
}

long long skew_reduce(long long a, long long b){ return a + b; }

