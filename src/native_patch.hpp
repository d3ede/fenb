#pragma once
#include "common.hpp"
#include <span>
#include <cstring>

// Verified fixed-size call/operand substitutions; never disassemble guessed prologues.
namespace native_patch {
// Tests can map the supplied PE without executing its entry point and exercise
// every signature/rollback against that disposable mapping.
inline HMODULE imageOverride=nullptr;
inline HMODULE image() { return imageOverride ? imageOverride : GetModuleHandleW(nullptr); }
class Set {
    struct Patch { unsigned char *address;std::vector<unsigned char> before,after;bool applied=false; };
    std::vector<Patch> patches_;
    static bool matches(const Patch &p,const std::vector<unsigned char> &expected) {
        std::vector<unsigned char> actual(expected.size());SIZE_T read=0;
        return ReadProcessMemory(GetCurrentProcess(),p.address,actual.data(),actual.size(),&read) &&
            read==actual.size() && actual==expected;
    }
    static bool write(Patch &p,const std::vector<unsigned char> &bytes) {
        DWORD old=0,unused=0;
        if(!VirtualProtect(p.address,bytes.size(),PAGE_EXECUTE_READWRITE,&old)) return false;
        std::memcpy(p.address,bytes.data(),bytes.size());
        FlushInstructionCache(GetCurrentProcess(),p.address,bytes.size());
        return VirtualProtect(p.address,bytes.size(),old,&unused)!=FALSE;
    }
  public:
    bool add(void *address,const void *expected,const void *replacement,size_t size) {
        if(!address || !expected || !replacement || !size) return false;
        Patch p{static_cast<unsigned char *>(address),{}, {}};
        auto *a=static_cast<const unsigned char *>(expected),*b=static_cast<const unsigned char *>(replacement);
        p.before.assign(a,a+size);p.after.assign(b,b+size);
        if(!matches(p,p.before)) return false;
        patches_.push_back(std::move(p));return true;
    }
    bool pointer(void **address,void *expected,void *replacement) { return add(address,&expected,&replacement,sizeof(void *)); }
    bool call(void *address,const void *expected5,void *target) {
        static_assert(sizeof(void *)==4);
        const auto displacement=uint32_t(reinterpret_cast<uintptr_t>(target)-reinterpret_cast<uintptr_t>(address)-5);
        unsigned char code[5]={0xe8};std::memcpy(code+1,&displacement,4);
        return add(address,expected5,code,5);
    }
    bool apply() {
        for(auto &p:patches_) {
            if(p.applied) continue;
            if(!matches(p,p.before)) { restore();return false; }
            // Even a protection-restore failure must include this write in rollback.
            p.applied=true;
            if(!write(p,p.after)) { restore();return false; }
        }
        return true;
    }
    void restore() {
        for(auto it=patches_.rbegin();it!=patches_.rend();++it) if(it->applied) {
            if(matches(*it,it->after)) write(*it,it->before);
            it->applied=false;
        }
    }
    void clear() { restore();patches_.clear(); }
    bool empty() const { return patches_.empty(); }
};
}
