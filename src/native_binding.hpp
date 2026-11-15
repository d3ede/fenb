#pragma once
#include "client_layout.hpp"
#include "native_patch.hpp"
#include "generated/native_bindings.hpp"
#include <array>

// Reference RVAs are stable IDs, never addresses in a rebuilt client. Resolve
// every required code/data dependency before allowing any native hybrid patch.
namespace native_binding {
struct Point { DWORD rva=0,anchor=0; };
struct Binding {
    HMODULE module=nullptr;
    std::array<Point,std::size(catalog::specs)> points{};
    bool valid=false;
    DWORD rva(DWORD reference) const {
        if(!valid)return 0;
        auto it=std::lower_bound(std::begin(catalog::specs),std::end(catalog::specs),reference,
            [](const auto &spec,DWORD value) { return spec.reference<value; });
        return it!=std::end(catalog::specs) && it->reference==reference?points[size_t(it-std::begin(catalog::specs))].rva:0;
    }
    DWORD reference(DWORD mapped) const {
        if(!valid)return 0;
        for(size_t i=0;i<points.size();++i)if(mapped==points[i].rva)return catalog::specs[i].reference;
        return 0;
    }
    unsigned char *address(DWORD ref) const {
        const DWORD at=rva(ref);
        return at?reinterpret_cast<unsigned char *>(module)+at:nullptr;
    }
    bool validate(const client_detail::Image &scan) const {
        using catalog::Kind;
        if(!valid)return false;
        for(size_t i=0;i<points.size();++i) {
            const auto &s=catalog::specs[i];const auto &p=points[i];
            constexpr DWORD access=IMAGE_SCN_MEM_READ|IMAGE_SCN_MEM_WRITE|IMAGE_SCN_MEM_EXECUTE;
            if(!p.rva || !scan.section(p.rva,1,s.access,access&~s.access))return false;
            if(s.kind==Kind::Name) {
                std::vector<char> name(std::strlen(s.probe)+1);
                if(!scan.read(p.rva+s.offset,name.data(),name.size()) || std::strcmp(name.data(),s.probe))return false;
            } else if(s.kind==Kind::Table) {
                if(scan.typeTable(points[s.source].rva)!=p.rva)return false;
            } else {
                if(!scan.bytes(p.anchor,s.probe))return false;
                if(s.kind==Kind::Operand && scan.relative(scan.value(p.anchor+s.offset))!=p.rva)return false;
                if(s.kind==Kind::Method && scan.relative(scan.value(points[s.source].rva+s.offset))!=p.rva)return false;
            }
        }
        for(const auto &check:catalog::checks) {
            const DWORD at=points[check.source].anchor+check.offset;
            const DWORD target=check.relative?DWORD(at+4+int32_t(scan.value(at))):scan.relative(scan.value(at));
            if(target!=points[check.target].rva)return false;
        }
        return true;
    }
    bool discover(HMODULE image,const client_detail::Image &scan) {
        using catalog::Kind;
        valid=false;module=image;points={};
        for(size_t i=0;i<points.size();++i) {
            const auto &s=catalog::specs[i];auto &p=points[i];
            if(s.kind==Kind::Code || s.kind==Kind::Operand) {
                p.anchor=scan.find(s.probe);if(!p.anchor)return false;
                p.rva=s.kind==Kind::Code?p.anchor+s.offset:scan.relative(scan.value(p.anchor+s.offset));
            } else if(s.kind==Kind::Name) {
                p.anchor=scan.string(s.probe);if(p.anchor<s.offset)return false;
                p.rva=p.anchor-s.offset;
            }
        }
        for(size_t i=0;i<points.size();++i) {
            const auto &s=catalog::specs[i];if(s.kind==Kind::Table)points[i].rva=scan.typeTable(points[s.source].rva);
        }
        for(size_t i=0;i<points.size();++i) {
            const auto &s=catalog::specs[i];if(s.kind==Kind::Method) {
                points[i].rva=scan.relative(scan.value(points[s.source].rva+s.offset));points[i].anchor=points[i].rva;
            }
        }
        valid=true;valid=validate(scan);return valid;
    }
};
inline Binding cached;
inline bool prepare(HMODULE module=native_patch::image()) {
    client_detail::Image scan(module);
    if(cached.module==module && cached.validate(scan))return true;
    return cached.discover(module,scan);
}
class Image {
    client_detail::Image scan_;
    HMODULE module_;
  public:
    explicit Image(HMODULE module):scan_(module),module_(module) {
        if(cached.module!=module || !cached.valid)cached.discover(module,scan_);
    }
    unsigned char *address(DWORD ref) const { return cached.module==module_?cached.address(ref):nullptr; }
    DWORD value(DWORD ref,unsigned offset=0) const { const DWORD at=cached.rva(ref);return at && offset<=7?scan_.value(at+offset):0; }
    DWORD relative(DWORD absolute) const { return cached.reference(scan_.relative(absolute)); }
    bool calls(DWORD ref,DWORD target) const {
        const DWORD at=cached.rva(ref),callee=cached.rva(target);
        return at && callee && scan_.bytes(at,"E8") && DWORD(at+5+int32_t(scan_.value(at+1)))==callee;
    }
    bool bytes(DWORD ref,const char *pattern) const {
        const DWORD at=cached.rva(ref);if(!at)return false;
        return scan_.bytes(at,pattern);
    }
};
class Set:public native_patch::Set {
    static bool relocateCall(unsigned char *code,DWORD reference,unsigned char *mapped) {
        if(code[0]!=0xe8)return true;
        int32_t before=0;std::memcpy(&before,code+1,4);
        const auto target=cached.address(DWORD(reference+5+before));if(!target)return false;
        const uint32_t after=uint32_t(reinterpret_cast<uintptr_t>(target)-reinterpret_cast<uintptr_t>(mapped)-5);
        std::memcpy(code+1,&after,4);return true;
    }
  public:
    bool call(void *address,const void *expected5,void *target) {
        if(!address || !expected5 || !target || !cached.valid)return false;
        auto *mapped=static_cast<unsigned char *>(address);const DWORD reference=cached.reference(DWORD(reinterpret_cast<uintptr_t>(mapped)-reinterpret_cast<uintptr_t>(cached.module)));
        if(!reference)return false;
        unsigned char expected[5];std::memcpy(expected,expected5,5);
        return relocateCall(expected,reference,mapped) && native_patch::Set::call(address,expected,target);
    }
    bool add(void *address,const void *expected,const void *replacement,size_t size) {
        if(!address || !expected || !replacement || !size || !cached.valid)return false;
        auto *mapped=static_cast<unsigned char *>(address);const DWORD reference=cached.reference(DWORD(reinterpret_cast<uintptr_t>(mapped)-reinterpret_cast<uintptr_t>(cached.module)));
        if(!reference)return false;
        std::vector<unsigned char> bytes(size);std::memcpy(bytes.data(),expected,size);
        for(const auto &call:catalog::calls) if(call.site>=reference && uint64_t(call.site)+5<=uint64_t(reference)+size) {
            const auto offset=call.site-reference;
            int32_t relative=0;std::memcpy(&relative,bytes.data()+offset+1,4);
            if(bytes[offset]!=0xe8 || DWORD(call.site+5+relative)!=catalog::specs[call.target].reference ||
               !relocateCall(bytes.data()+offset,call.site,mapped+offset))return false;
        }
        return native_patch::Set::add(address,bytes.data(),replacement,size);
    }
};
}
