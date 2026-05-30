#ifndef PARALLEL_ENGINE_H
#define PARALLEL_ENGINE_H

#include "chase_lev_deque.h"
#include <functional>
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <optional>
#include <iostream>


enum class VictimStrategy { Random, PowerOfTwo, Richest, Sticky }; // different victim choosing strategies we have, wrote which one deos what in victim choosing right under in function under 

inline const char* strat_name(VictimStrategy s){
    switch(s){
        case VictimStrategy::Random:     return "random";
        case VictimStrategy::PowerOfTwo: return "power_of_two";//chooses two randomly, takes one with bigges deque size
        case VictimStrategy::Richest:    return "richest";//geos through all and takes ones with biggest deque
        case VictimStrategy::Sticky:     return "sticky";//tries to steal from one stole before unless it cant then takes richest
    }
}

struct StealState {
    std::mt19937 rng;
    int  last_victim = -1;
    bool last_ok     = false; // this is to see if last stealing worked or not
};

//decided to have the deques store Node* pointer not Node because CircularArray holds a std::vector<std::atomic<T>>, and std::atomic<T> is only 
//well formed when T is trivially copyable (does no have a user defined constructor), Sudoku_Node has one, but Node* is always copyable
template <class Node, class Result>
class ParallelRES {
public:

    // typnenames for the functions as they are long to write
    using MapFn = std::function<Result(Node&)>;
    using ReduceFn = std::function<Result(Result, Result)>;
    using SuccFn = std::function<std::vector<Node>(Node&)>;
    using StopFn = std::function<bool(const Result&)>; // this one is to see if we can stop now for early terminantion, was thinking of putting it in map or reduce but seemed easier this way


    SuccFn successors;

    std::vector<Node> seeds;

    Result neutral_element;

    int num_threads;





    ParallelRES(std::vector<Node> seeds, SuccFn successors,
                Result neutral_element, int num_threads)
        : successors(successors), seeds(seeds),
          neutral_element(neutral_element), num_threads(num_threads) {}




    //the shared state across all threads
    struct SharedState {
        std::atomic<int> active;   // # workers that hold or are seeking work

        std::atomic<bool> done;
    };

    //simple worker draining the entire queue, manages the std::optional returned by the WorkStealingDeque and cleans up the memory
    Result run_worker_local(WorkStealingDeque<Node*>& dq, const MapFn& map, const ReduceFn& reduce,SharedState& st,const StopFn& should_stop) {
        Result running_result = neutral_element;

        std::optional<Node*> node = dq.pop_bottom();
        while(node){
            if(st.done.load()){ delete node.value(); break; }//not good idea to use continue here, though good to drain ques and free them but race problem with stealing


            //while there is still an element in the dq
            Node* node_p = node.value();
            running_result = reduce(running_result, map(*node_p));

            if(should_stop && should_stop(running_result)){      //early termination here, we check should_stop first to make sure actually have a function if nullptr given to not get error
                st.done.store(true);
                delete node_p;
                break;
            }
            std::vector<Node> children = successors(*node_p);
            //all elements in the vector will remain on the stack as long as the function runs, so the pointer will not be dangling
            for(Node& child: children){

                dq.push_bottom(new Node(child));

            }
            delete node_p;

            node = dq.pop_bottom();
            
        }        
        return running_result;
    }







    int random_other(int t_id, StealState& s){
        int v = t_id;
        while(v == t_id){
            v = (int)(s.rng() % (unsigned)num_threads);
        }
        return v;
    }


    int richest_victim(int t_id, StealState& s,std::vector<WorkStealingDeque<Node*>*>& deques){//find richest, if none found do random
        int best = -1;
        int64_t best_sz = -1;
        for(int j = 0; j < num_threads; ++j){
            if(j == t_id){
                continue;
            }
            int64_t sz = deques[j]->size();
            if(sz > best_sz){
                best_sz = sz;
                best = j;
            }
        }
        if(best == -1){
            return random_other(t_id, s);
        }
        return best;
    }

    int find_victim(int t_id, VictimStrategy strat, StealState& s,std::vector<WorkStealingDeque<Node*>*>& deques){

        if(strat == VictimStrategy::Random){
            return random_other(t_id, s);
        }

        if(strat == VictimStrategy::PowerOfTwo){//find richest of two random victims
            int a = random_other(t_id, s);
            int b = random_other(t_id, s);
            if(deques[a]->size() >= deques[b]->size()){
                return a;
            }
            return b;
        }

        if(strat == VictimStrategy::Richest){
            return richest_victim(t_id, s, deques);
        }

        if(strat == VictimStrategy::Sticky){
            
            if(s.last_ok && s.last_victim != -1 && s.last_victim != t_id && deques[s.last_victim]->size() > 0){// keep taking from last victim if possible
                return s.last_victim;
            }
            
            return richest_victim(t_id, s, deques);//if not take richest
        }

        return random_other(t_id, s);   // fallback
    }

    void single_thread_work(int t_id, SharedState& st, std::vector<WorkStealingDeque<Node*>*>& deques,
        const MapFn& map, const ReduceFn& reduce, Result& thread_result, const StopFn& should_stop, VictimStrategy strat){
        Result local_result = thread_result; //neutral_element is passed as an argument as results[i] are initialised with neutral_element
        bool local_active = true;
        StealState state;
        state.rng.seed((unsigned)t_id * 2654435761u + 1u);
        while(st.active.load() > 0 && !st.done.load() ){// added early termination here
            //while there is at least one other active thread
            local_result = reduce(local_result, run_worker_local(*deques[t_id], map, reduce,st,should_stop));
            //thread has emptied its deque so looking to steal
            
            if(num_threads > 1){
                int victim_id = find_victim(t_id, strat, state, deques);
                std::optional<Node*> stolen_node = deques[victim_id]->steal();
                state.last_victim = victim_id;
                state.last_ok     = stolen_node.has_value(); //this is for richest strat
                if(stolen_node){

                    //the steal was successfull so we add the node to this threads deque
                    if(!local_active){

                        //if we were not active we now reactivate and increment the shared atomic counter
                        local_active = true;
                        st.active++;
                    }







                    deques[t_id]->push_bottom(stolen_node.value());
                }else{
                    if(local_active){
                        //if we were active, we no longer are as we failed to find a node to steal
                        local_active = false;
                        st.active--;
                    }
                }
            }else{
                //there is only one thread and we have finished our entire deque so we are done
                st.active--;
            }
        }






        thread_result = local_result;
    };
    
    Result map_reduce(const MapFn& map, const ReduceFn& reduce,const StopFn& should_stop = nullptr, VictimStrategy strat = VictimStrategy::Random) {
        BufferPool<Node*> bp;
        //think about having num_threads a constexpr that way I can have it as an argument in a std::array<num_threads, N> instead of these vectors

        std::vector<WorkStealingDeque<Node*>*> deques;  //thinking about storing the deques on the heap will see
        
        SharedState st = {num_threads, false};

        for(size_t i=0; i<num_threads; ++i){
            deques.push_back(new WorkStealingDeque(bp));
        }


        for(size_t i=0; i<seeds.size(); ++i){
            //round robin
            deques[i%num_threads]->push_bottom(new Node(seeds[i]));


        };



        std::vector<std::thread> workers(num_threads);

        std::vector<Result> results(num_threads);

        for(size_t i=0; i<num_threads; ++i){
            workers[i] = std::thread([this, i, &st, &deques, &map, &reduce, &results,&should_stop, strat] {
                        single_thread_work(i, st, deques, map, reduce, results[i],should_stop, strat);
                });
        };


        for(size_t i=0; i<num_threads; ++i){
            workers[i].join();
        }

        Result final_result = neutral_element;
        for(size_t i=0; i<num_threads; ++i){
            final_result = reduce(final_result, results[i]);
        }
        

        for(auto* dq : deques) delete dq;

        return final_result;
    }
};

#endif // PARALLEL_ENGINE_H