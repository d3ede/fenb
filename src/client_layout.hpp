#pragma once
#include "common.hpp"
#include <cstring>
#include <sstream>

// Read-only discovery of the compatible x86 renderer ABI. Relocated operands are
// followed only inside this image; timestamps, resources and file hashes are irrelevant.
namespace client_detail {
struct Layout {
    DWORD gate = 0, flag = 0, tick = 0, elapsed = 0, timeImport = 0, sleepImport = 0;
    DWORD application = 0, vtable = 0, move = 0, render = 0;
    DWORD clock = 0, sky = 0, player = 0, weather = 0, initFall = 0, compute = 0;
    bool frames() const {
        return gate && application && vtable && move && render;
    }
    bool environment() const {
        return frames() && clock && sky && player && weather && initFall && compute;
    }
};
class Image {
    struct Section {
        DWORD rva, size, flags;
        std::vector<unsigned char> bytes;
    };
    HMODULE module_;
    DWORD size_ = 0;
    std::vector<Section> sections_;
    mutable std::vector<std::pair<DWORD,DWORD>> pointers_;
    mutable bool indexed_ = false;

  public:
    explicit Image(HMODULE module) : module_(module) {
        IMAGE_DOS_HEADER dos{};
        IMAGE_NT_HEADERS32 nt{};
        if (!read(0, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
            dos.e_lfanew > 4096 || !read(dos.e_lfanew, &nt, sizeof(nt)) ||
            nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
            nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC || nt.FileHeader.NumberOfSections < 1 ||
            nt.FileHeader.NumberOfSections > 32 || nt.OptionalHeader.SizeOfImage < 4096 ||
            nt.OptionalHeader.SizeOfImage > 128 * 1024 * 1024)
            return;
        size_ = nt.OptionalHeader.SizeOfImage;
        DWORD start = dos.e_lfanew + 24 + nt.FileHeader.SizeOfOptionalHeader;
        for (unsigned i = 0; i < nt.FileHeader.NumberOfSections; ++i) {
            IMAGE_SECTION_HEADER sh{};
            if (!read(start + i * sizeof(sh), &sh, sizeof(sh))) {
                sections_.clear();
                return;
            }
            DWORD n = sh.Misc.VirtualSize;
            if (!n || sh.VirtualAddress >= size_ || n > size_ - sh.VirtualAddress)
                continue;
            if (sh.Characteristics & IMAGE_SCN_MEM_DISCARDABLE)
                continue;
            Section s{sh.VirtualAddress, n, sh.Characteristics, {}};
            s.bytes.resize(n);
            if (!read(s.rva, s.bytes.data(), n))
                continue;
            sections_.push_back(std::move(s));
        }
    }
    bool read(DWORD rva, void *out, size_t n) const {
        if (!module_ || (size_ && (rva >= size_ || n > size_ - rva)))
            return false;
        SIZE_T got = 0;
        return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const BYTE *>(module_) + rva, out, n,
                                 &got) &&
               got == n;
    }
    DWORD value(DWORD rva) const {
        for (auto &s : sections_)
            if (rva >= s.rva && rva - s.rva < s.size && 4 <= s.size - (rva - s.rva)) {
                DWORD v = 0;
                memcpy(&v, s.bytes.data() + rva - s.rva, 4);
                return v;
            }
        return 0;
    }
    DWORD relative(DWORD absolute) const {
        auto base = reinterpret_cast<uintptr_t>(module_);
        return absolute >= base && absolute - base < size_ ? DWORD(absolute - base) : 0;
    }
    bool section(DWORD rva, size_t n, DWORD required, DWORD forbidden = 0) const {
        for (auto &s : sections_)
            if (rva >= s.rva && rva - s.rva < s.size && n <= s.size - (rva - s.rva))
                return (s.flags & required) == required && !(s.flags & forbidden);
        return false;
    }
    DWORD find(const char *pattern) const {
        std::istringstream input(pattern);
        std::string token;
        std::vector<int> p;
        while (input >> token)
            p.push_back(token == "??" ? -1 : std::stoi(token, nullptr, 16));
        size_t seed=0,length=0,run=0;
        for(size_t i=0;i<=p.size();++i) {
            if(i<p.size() && p[i]>=0)++run;
            else { if(run>length) { seed=i-run;length=run; }run=0; }
        }
        if(!length)return 0;
        std::vector<unsigned char> literal;
        for(size_t i=0;i<length;++i)literal.push_back(static_cast<unsigned char>(p[seed+i]));
        DWORD result = 0;
        for (auto &s : sections_)
            if (s.flags & IMAGE_SCN_MEM_EXECUTE) {
                if (s.bytes.size() < p.size())
                    continue;
                const size_t last=s.bytes.size()-p.size();
                for (size_t cursor=0;cursor<=last;) {
                    const auto *found=static_cast<const unsigned char *>(std::memchr(s.bytes.data()+cursor+seed,literal[0],last-cursor+1));
                    if(!found)break;
                    const size_t at=size_t(found-s.bytes.data())-seed;cursor=at+1;
                    if(std::memcmp(found,literal.data(),length))continue;
                    bool match = true;
                    for (size_t i = 0; i < p.size(); ++i)
                        if (p[i] >= 0 && s.bytes[at + i] != p[i]) {
                            match = false;
                            break;
                        }
                    if (match) {
                        if (result)
                            return 0;
                        result = s.rva + DWORD(at);
                    }
                }
            }
        return result;
    }
    bool bytes(DWORD rva, const char *pattern) const {
        std::istringstream input(pattern);
        std::string token;
        while (input >> token) {
            unsigned char v = 0;
            if (!read(rva++, &v, 1) || (token != "??" && v != std::stoi(token, nullptr, 16)))
                return false;
        }
        return true;
    }
    DWORD table(unsigned slot,DWORD function,const char *firstMethod=nullptr) const {
        if(!function || !section(function,1,IMAGE_SCN_MEM_EXECUTE))return 0;
        DWORD result=0;
        for(const auto &s:sections_) {
            if(!(s.flags&IMAGE_SCN_MEM_READ) || (s.flags&(IMAGE_SCN_MEM_EXECUTE|IMAGE_SCN_MEM_WRITE)))continue;
            for(DWORD at=slot*4;at+4<=s.size;at+=4) {
                DWORD pointer=0;memcpy(&pointer,s.bytes.data()+at,4);
                if(relative(pointer)!=function)continue;
                if(firstMethod) {
                    const DWORD first=relative(value(s.rva+at-slot*4));
                    if(!first || !bytes(first,firstMethod))continue;
                }
                if(result)return 0; // A shared/ambiguous method cannot identify its owner.
                result=s.rva+at-slot*4;
            }
        }
        return result;
    }
    DWORD string(const char *text) const {
        const size_t n=std::strlen(text)+1;
        DWORD result=0;
        for(const auto &s:sections_) {
            if(!(s.flags&IMAGE_SCN_MEM_READ) || (s.flags&IMAGE_SCN_MEM_EXECUTE))continue;
            auto cursor=s.bytes.begin();
            while(cursor!=s.bytes.end()) {
                auto found=std::search(cursor,s.bytes.end(),text,text+n);
                if(found==s.bytes.end())break;
                if(result)return 0;
                result=s.rva+DWORD(found-s.bytes.begin());cursor=found+1;
            }
        }
        return result;
    }
    DWORD typeTable(DWORD descriptor) const {
        if(!descriptor)return 0;
        if(!indexed_) {
            for(const auto &s:sections_) {
                if(!(s.flags&IMAGE_SCN_MEM_READ) || (s.flags&(IMAGE_SCN_MEM_EXECUTE|IMAGE_SCN_MEM_WRITE)))continue;
                for(DWORD offset=0;offset+4<=s.size;offset+=4) {
                    const DWORD target=relative(value(s.rva+offset));
                    if(target)pointers_.emplace_back(target,s.rva+offset);
                }
            }
            std::sort(pointers_.begin(),pointers_.end());indexed_=true;
        }
        DWORD result=0;
        auto references=[&](DWORD target,auto &&visit) {
            auto it=std::lower_bound(pointers_.begin(),pointers_.end(),std::pair<DWORD,DWORD>{target,0});
            for(;it!=pointers_.end() && it->first==target;++it)visit(it->second);
        };
        bool ambiguous=false;
        references(descriptor,[&](DWORD field) {
            if(field<12)return;
            const DWORD locator=field-12;
            if(value(locator)!=0 || value(locator+4)!=0 || value(locator+8)!=0 ||
               !section(locator,20,IMAGE_SCN_MEM_READ,IMAGE_SCN_MEM_WRITE|IMAGE_SCN_MEM_EXECUTE))return;
            references(locator,[&](DWORD entry) {
                const DWORD table=entry+4,process=relative(value(table+24));
                if(!section(table,28,IMAGE_SCN_MEM_READ,IMAGE_SCN_MEM_WRITE|IMAGE_SCN_MEM_EXECUTE) ||
                   !section(process,1,IMAGE_SCN_MEM_EXECUTE))return;
                if(result && result!=table)ambiguous=true;
                result=table;
            });
        });
        return ambiguous?0:result;
    }
    Layout discover() const {
        Layout l;
        l.gate = find("83 3D ?? ?? ?? ?? 00 8B 3D ?? ?? ?? ?? 74 36 FF D7 8B 0D ?? ?? ?? ?? 2B 0D ?? ?? ?? "
                      "?? 03 C8 A3 ?? ?? ?? ?? 89 0D ?? ?? ?? ?? 81 F9 E8 03 00 00");
        if (!l.gate)
            return {};
        l.flag = relative(value(l.gate + 2));
        l.timeImport = relative(value(l.gate + 9));
        l.elapsed = relative(value(l.gate + 19));
        l.tick = relative(value(l.gate + 25));
        l.sleepImport = relative(value(l.gate + 217));
        if (!l.flag || !l.tick || !l.elapsed || !l.timeImport || !l.sleepImport)
            return {};
        // Find the actual vtable and singleton rather than assuming their old RVAs.
        for (auto &s : sections_)
            if ((s.flags & IMAGE_SCN_MEM_READ) && !(s.flags & (IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_WRITE)))
                for (DWORD at = 20; at + 8 <= s.size; at += 4) {
                    DWORD move = relative(value(s.rva + at)), render = relative(value(s.rva + at + 4));
                    if (!move || !render || !section(move, 32, IMAGE_SCN_MEM_EXECUTE) ||
                        !section(render, 32, IMAGE_SCN_MEM_EXECUTE) ||
                        !bytes(move,
                               "53 8B DC 83 EC 08 83 E4 F8 83 C4 04 55 8B 6B 04 89 6C 24 04 8B EC 6A FF") ||
                        !bytes(render,
                               "55 8B EC 83 E4 F8 83 EC 20 A1 ?? ?? ?? ?? 33 C4 89 44 24 1C 56 8B F1"))
                        continue;
                    DWORD table = s.rva + at - 20;
                    for (auto &data : sections_)
                        if (data.flags & IMAGE_SCN_MEM_WRITE)
                            for (DWORD p = 0; p + 0xf8 <= data.size; p += 4)
                                if (relative(value(data.rva + p)) == table) {
                                    if (l.application)
                                        return {}; // Ambiguous ownership is unsupported.
                                    l.application = data.rva + p;
                                    l.vtable = table;
                                    l.move = move;
                                    l.render = render;
                                }
                }
        if (!l.frames())
            return {};
        l.compute = find("53 8B D9 56 57 83 7B 10 00 0F 85 BB 00 00 00 FF 15 ?? ?? ?? ?? 66 0F 6E C0 C1 E8 "
                         "1F F3 0F E6 C0");
        DWORD clock =
            find("83 3D ?? ?? ?? ?? 00 0F 85 CE 00 00 00 FF 15 ?? ?? ?? ?? 66 0F 6E C0 C1 E8 1F F3 0F E6 C0");
        if (clock) {
            DWORD fixed = relative(value(clock + 2));
            if (fixed >= 16)
                l.clock = fixed - 16;
        }
        DWORD sky = find("B8 ?? ?? ?? ?? C3 CC 55 8B EC 83 B9 38 B0 02 00 00 75 1C 83 7D 0C 01 75 0F");
        if (sky)
            l.sky = relative(value(sky + 1));
        DWORD init =
            find("8B F1 89 75 BC C7 45 C0 00 00 00 00 0F 57 C0 8B 46 20 0F 11 45 CC 0F 11 45 DC 83 F8 04");
        if (init >= 62 && bytes(init - 62, "53 8B DC 83 EC 08 83 E4 F8 83 C4 04 55 8B 6B 04"))
            l.initFall = init - 62;
        DWORD player = find("55 8B EC 8B 15 ?? ?? ?? ?? 53 56 57 0F B6 82 9C 06 00 00 33 FF 8B D9 39 45 08");
        if (player)
            l.player = relative(value(player + 5));
        DWORD weather = find("89 84 24 A4 00 00 00 83 3D ?? ?? ?? ?? 00 56 57 8B F9 0F 84 D3 08 00 00");
        if (weather)
            l.weather = relative(value(weather + 9));
        return l;
    }
};
inline Layout Discover(HMODULE image) {
    return Image(image).discover();
}
inline const Layout &Current() {
    static const auto layout = Discover(GetModuleHandleW(nullptr));
    return layout;
}
} // namespace client_detail
