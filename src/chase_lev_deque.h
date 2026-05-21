#include <vector>
#include <memory>
#include <atomic>
#include <optional>
#include <mutex>
#include <unordered_map>

template <class T>
class CircularArray{
    std::vector<std::atomic<T>> elements;

    public:
        int64_t log_size; //the size elements vector is 1 << log_size
        CircularArray(int64_t log_size): log_size(log_size), elements(1<<log_size) {};
        int64_t size();
        T get(int64_t i);
        void put(int64_t i, T val);

};


template <class T>
int64_t CircularArray<T>::size(){
    return 1<<this->log_size;
};

template <class T>
T CircularArray<T>::get(int64_t i){
    return elements[i%size()].load();
};

template <class T>
void CircularArray<T>::put(int64_t i, T val){
    elements[i%size()].store(val);
};


template <class T>
class BufferPool {
    std::mutex m;
    std::unordered_map<int64_t, std::vector<CircularArray<T>*>> pool;

public:
    //function to acquire a pointer to a Circular Array of size 1<<log_size, a thread is reclaiming the array
    CircularArray<T>* acquire(int64_t log_size) {
        std::lock_guard<std::mutex> lock(m);
        auto& bucket = pool[log_size];
        if (!bucket.empty()) {
            CircularArray<T>* buf = bucket.back(); //get the last element in the vector, is O(1)
            bucket.pop_back();
            return buf;
        }
        //dont have a pointer to an array of that size, just create one
        return new CircularArray<T>(log_size);
    }

    //function that adds a pointer to an array of size 1<<log_size to the pool, a thread was finished with it so it is now free
    void release(CircularArray<T>* buf, int64_t log_size) {
        std::lock_guard<std::mutex> lock(m);
        pool[log_size].push_back(buf);
    }

    ~BufferPool() {
        for (auto& [size, bucket] : pool)
            for (auto* buf : bucket)
                delete buf;
    }
};



template <class T>
class WorkStealingDeque{
    std::atomic<int64_t> bottom {0};
    std::atomic<int64_t> top {0};

    std::atomic<CircularArray<T>*> active_array; //this is the pointer through which we access the array on the heap
    BufferPool<T>& pool; //the pool of buffers

    public:
        WorkStealingDeque(BufferPool<T>& pool) : pool(pool) {
            active_array.store(pool.acquire(3)); //start of with 8 elements
        };

        ~WorkStealingDeque() {
            CircularArray<T>* array = active_array.load();
            pool.release(array, array->log_size); // return current array, give it back to the pool
        };

        std::optional<T> steal(); //another worker can steal the top element
        void push_bottom(T val); //add an element to the bottom of the queue
        std::optional<T> pop_bottom(); //remove the last element of the queue we added
};


template <class T>
void WorkStealingDeque<T>::push_bottom(T val){
    //no need to be scared of race conditions or the concurrent environment for this method fucntion
    CircularArray<T>* array = this->active_array.load();
    int64_t b = bottom.load();
    int64_t t = top.load();

    int64_t occupancy = b - t;

    if(occupancy >= array->size() - 1){
        //there is only one slot left so we need to grow the array
        CircularArray<T>* old_array = array;
        array = pool.acquire(old_array->log_size + 1);

        //copy all the elements of the old array into the new array
        for(size_t i=t; i<b; ++i){
            array->put(i, old_array->get(i));
        }

        active_array.store(array); //store the newly built array with twice the size into active_array member
        pool.release(old_array, old_array->log_size); //give the old array to the pool
    }

    array->put(b, val); //points to the same array on the heap as active array so the element is also in *active_array
    bottom.store(b+1);
};

template <class T>
std::optional<T> WorkStealingDeque<T>::pop_bottom(){
    int64_t b = bottom.load() - 1;
    CircularArray<T>* array = active_array.load();
    bottom.store(b); //decrement atomically and get local variable
    int64_t t = top.load(); //load top

    int64_t size = b - t;
    if(size > 0){
        //more than one element was already in the array so we can take one without any issue
        return array->get(b);
    }else if(size == 0){
        //potential race condition with a steal as there was only 1 element in the array
        //store in bottom the next free slot which is t+1, since there are
        bottom.store(t+1); //in either case of what happens with this CAS top = t+1 (either we increment it or the thief did)
        if(this->top.compare_exchange_strong(t, t+1)){
            //we have won the race condition as the other thread did not alter the top variable
            return array->get(b);
        }
        //otherwise if we lost the race condition we return nullopt but we still handle incrementing the bottom pointer as the steal function does not

        return std::nullopt;
    }else{
        //size < 0 so we reset
        bottom.store(top.load());
        return std::nullopt;
    }
};

template <class T>
std::optional<T> WorkStealingDeque<T>::steal(){
    //read both values at the start of the function
    int64_t t = top.load();
    int64_t b = bottom.load();
    if(b - t <= 0){
        //the array is empty so we do not take from it
        return std::nullopt;
    }

    CircularArray<T>* array = this->active_array.load();
    T top_val = array->get(t); //get the top value as we know it exists
    if(this->top.compare_exchange_strong(t, t+1)){
        //if this->top == t still, it means no one took from the deque or nothing was added to it in the meantime
        return top_val;
    }
    //if this->top != t, it means we lost the race condition and another thread managed to take this->top before we could 
    return std::nullopt;
    
};