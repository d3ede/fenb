#pragma once
#include <array>
#include <cstdint>
#include <cstddef>

// A bounded, non-owning per-thread cache. The registry changes its generation
// whenever an address is registered or retired, including address reuse.
template<class Value> class ResourceLookupCache {
    struct Entry { const void *key=nullptr;Value value{};uint64_t generation=0; };
    std::array<Entry,256> entries_{};
    static size_t index(const void *key) {
        auto bits=reinterpret_cast<uintptr_t>(key);
        return ((bits>>4)^(bits>>12)^(bits>>20))&255;
    }
  public:
    bool find(const void *key,uint64_t generation,Value &value) const {
        const auto &entry=entries_[index(key)];
        if(entry.key!=key || entry.generation!=generation)return false;
        value=entry.value;return true;
    }
    void remember(const void *key,uint64_t generation,Value value) {
        entries_[index(key)]={key,value,generation};
    }
};
