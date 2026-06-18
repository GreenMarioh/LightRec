#include <iostream>
#include <cassert>
#include "../../src/util/SPSCQueue.h"

int main() {
    using LightRec::Util::SPSCQueue;
    
    // Test basic push/pop/empty/full
    SPSCQueue<int> q(4); // capacity 4, can hold 3 items
    
    int val = 0;
    assert(!q.try_pop(val)); // empty

    assert(q.try_push(1));
    assert(q.try_push(2));
    assert(q.try_push(3));
    assert(!q.try_push(4)); // full

    assert(q.try_pop(val) && val == 1);
    assert(q.try_pop(val) && val == 2);
    
    assert(q.try_push(5));
    assert(q.try_push(6));
    assert(!q.try_push(7)); // full

    assert(q.try_pop(val) && val == 3);
    assert(q.try_pop(val) && val == 5);
    assert(q.try_pop(val) && val == 6);
    
    assert(!q.try_pop(val)); // empty
    
    std::cout << "SPSCQueue tests passed!\n";
    return 0;
}
