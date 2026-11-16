#include "resource_lookup_cache.hpp"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <thread>

int main() {
    ResourceLookupCache<void*> cache;
    int key=0,first=0,replacement=0;void *value=nullptr;
    assert(!cache.find(&key,1,value));
    cache.remember(&key,1,&first);
    assert(cache.find(&key,1,value)&&value==&first);
    // A wrapper dies and a different resource is allocated at the same address.
    assert(!cache.find(&key,2,value));
    cache.remember(&key,2,&replacement);
    assert(cache.find(&key,2,value)&&value==&replacement);
    // Native pointers and unknown runtimes may be cached without adding refs.
    cache.remember(&first,2,nullptr);
    assert(cache.find(&first,2,value)&&value==nullptr);
    assert(!cache.find(&first,3,value));
    // More addresses than slots must evict, never return another key's value.
    int keys[2048]{};
    for(auto &item:keys)cache.remember(&item,4,&item);
    unsigned hits=0;
    for(auto &item:keys)if(cache.find(&item,4,value)){assert(value==&item);++hits;}
    assert(hits>0&&hits<=256);
    // Cross-thread retirement invalidates a cache warmed before the publication.
    std::atomic<uint64_t> generation{10};std::atomic<int> stage{0};
    std::thread reader([&] {
        ResourceLookupCache<void*> local;local.remember(&key,10,&first);
        stage.store(1,std::memory_order_release);
        while(stage.load(std::memory_order_acquire)!=2)std::this_thread::yield();
        void *out=nullptr;assert(!local.find(&key,generation.load(std::memory_order_acquire),out));
    });
    while(stage.load(std::memory_order_acquire)!=1)std::this_thread::yield();
    generation.fetch_add(1,std::memory_order_release);stage.store(2,std::memory_order_release);reader.join();
    puts("PASS: resource lookup cache invalidation, address reuse, native pointers, eviction and cross-thread retirement.");
}
