#include "../../src/util/SPSCQueue.h"
#include <iostream>
#include <cassert>
#include <string>
#include <memory>
#include <thread>

using LightRec::Util::SPSCQueue;

void test_basic() {
    SPSCQueue<int> q(4);
    int val = 0;
    assert(!q.try_pop(val));
    assert(q.try_push(1));
    assert(q.try_push(2));
    assert(q.try_push(3));
    assert(!q.try_push(4));
    
    assert(q.try_pop(val) && val == 1);
    assert(q.try_pop(val) && val == 2);
    assert(q.try_pop(val) && val == 3);
    assert(!q.try_pop(val));
}

void test_string() {
    SPSCQueue<std::string> q(4);
    std::string val;
    assert(q.try_push("hello"));
    assert(q.try_pop(val) && val == "hello");
}

void test_move_only() {
    SPSCQueue<std::unique_ptr<int>> q(4);
    q.try_push(std::make_unique<int>(42));
    std::unique_ptr<int> val;
    assert(q.try_pop(val) && *val == 42);
}

void test_stress() {
    const int N = 100000;
    SPSCQueue<int> q(1024);
    
    std::thread producer([&]() {
        for (int i = 0; i < N; ++i) {
            while (!q.try_push(i)) {
                std::this_thread::yield();
            }
        }
    });
    
    std::thread consumer([&]() {
        for (int i = 0; i < N; ++i) {
            int val = 0;
            while (!q.try_pop(val)) {
                std::this_thread::yield();
            }
            assert(val == i);
        }
    });
    
    producer.join();
    consumer.join();
}

int main() {
    test_basic();
    test_string();
    test_move_only();
    test_stress();
    std::cout << "SPSCQueue tests passed\n";
    return 0;
}
