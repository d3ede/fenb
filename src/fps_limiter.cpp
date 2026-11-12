#include "fps_limiter.hpp"
#include <bcrypt.h>
#include <array>
#include <cstring>
#include <cstdint>

namespace {
constexpr DWORD Timestamp = 1794424444, ImageSize = 11538432, EntryPoint = 5918976;
constexpr DWORD Flag = 0x829c4c, Tick = 0x844ae0, Elapsed = 0x844ae4;
constexpr DWORD TimeImport = 0x6d5c10, SleepImport = 0x6d5368;
// SHA-256 of the inspected Homesickness Neuz revision, not a general Flyff signature.
constexpr unsigned char Digest[] = {0xce, 0x32, 0xe3, 0x50, 0xc6, 0x00, 0x24, 0x96, 0xac, 0xf9, 0xe8,
                                    0x77, 0x42, 0x75, 0x78, 0x49, 0x71, 0x18, 0x6e, 0x61, 0x44, 0x90,
                                    0x1f, 0x7c, 0x2c, 0x7d, 0x9b, 0x37, 0xa4, 0xaa, 0x41, 0xa1};

bool Read(const void *address, void *out, size_t size) {
    SIZE_T read = 0;
    return ReadProcessMemory(GetCurrentProcess(), address, out, size, &read) && read == size;
}
bool Region(HMODULE image, DWORD offset, bool executable) {
    MEMORY_BASIC_INFORMATION info{};
    auto *p = reinterpret_cast<unsigned char *>(image) + offset;
    if (!VirtualQuery(p, &info, sizeof(info)) || info.AllocationBase != image || info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
        return false;
    const DWORD protection = info.Protect & 0xff;
    return executable ? protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE
                      : protection == PAGE_READWRITE;
}
bool Headers(HMODULE image) {
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS32 nt{};
    return image && Read(image, &dos, sizeof(dos)) && dos.e_magic == IMAGE_DOS_SIGNATURE &&
           dos.e_lfanew > 0 && dos.e_lfanew < 4096 &&
           Read(reinterpret_cast<unsigned char *>(image) + dos.e_lfanew, &nt, sizeof(nt)) &&
           nt.Signature == IMAGE_NT_SIGNATURE && nt.FileHeader.Machine == IMAGE_FILE_MACHINE_I386 &&
           nt.FileHeader.TimeDateStamp == Timestamp &&
           nt.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
           nt.OptionalHeader.SizeOfImage == ImageSize && nt.OptionalHeader.AddressOfEntryPoint == EntryPoint;
}
bool Bytes(HMODULE image, DWORD offset, const unsigned char *expected, size_t count) {
    std::array<unsigned char, 32> actual{};
    return count <= actual.size() &&
           Read(reinterpret_cast<unsigned char *>(image) + offset, actual.data(), count) &&
           !memcmp(actual.data(), expected, count);
}
bool Address(HMODULE image, DWORD offset, DWORD target) {
    DWORD actual = 0;
    const auto base = reinterpret_cast<uintptr_t>(image);
    return base <= UINT32_MAX - ImageSize &&
           Read(reinterpret_cast<unsigned char *>(image) + offset, &actual, sizeof(actual)) &&
           actual == base + target;
}
} // namespace

fps_detail::Binding fps_detail::InspectImage(HMODULE image) {
    if (!Headers(image) || !Region(image, 0xebdcf, true) || !Region(image, 0xebea6, true) ||
        !Region(image, Flag, false) || !Region(image, Tick, false) || !Region(image, Elapsed, false))
        return {};
    // Both frame gates must refer to the same flag, including ASLR relocations.
    const unsigned char compare[] = {0x83, 0x3d}, zero[] = {0}, clock[] = {0x8b, 0x3d};
    const unsigned char gate[] = {0x74, 0x36}, afterGate[] = {0x74, 0x73};
    const unsigned char wait[] = {0x83, 0xf9, 0x10, 0x73, 0x2d, 0xb8, 0x10, 0,
                                  0,    0,    0x2b, 0xc1, 0x50, 0xff, 0x15};
    if (!Bytes(image, 0xebdcf, compare, sizeof(compare)) || !Address(image, 0xebdd1, Flag) ||
        !Bytes(image, 0xebdd5, zero, 1) || !Bytes(image, 0xebdd6, clock, sizeof(clock)) ||
        !Address(image, 0xebdd8, TimeImport) || !Bytes(image, 0xebddc, gate, sizeof(gate)) ||
        !Bytes(image, 0xebe57, compare, sizeof(compare)) || !Address(image, 0xebe59, Flag) ||
        !Bytes(image, 0xebe5d, zero, 1) || !Bytes(image, 0xebe5e, afterGate, sizeof(afterGate)) ||
        !Bytes(image, 0xebe99, wait, sizeof(wait)) || !Address(image, 0xebea8, SleepImport))
        return {};
    auto *base = reinterpret_cast<unsigned char *>(image);
    LONG enabled = 0;
    if (!Read(base + Flag, &enabled, sizeof(enabled)) || (enabled != 0 && enabled != 1))
        return {};
    return {reinterpret_cast<LONG *>(base + Flag), reinterpret_cast<DWORD *>(base + Tick),
            reinterpret_cast<LONG *>(base + Elapsed)};
}

bool fps_detail::KnownClientFile(const wchar_t *path) {
    HANDLE file =
        CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart != 9274368) {
        CloseHandle(file);
        return false;
    }
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    std::array<unsigned char, 65536> buffer{};
    unsigned char digest[32]{};
    bool ok = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
              BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
    DWORD count = 0;
    size_t total = 0;
    while (ok) {
        if (!ReadFile(file, buffer.data(), DWORD(buffer.size()), &count, nullptr)) {
            ok = false;
            break;
        }
        if (!count)
            break;
        total += count;
        if (total > 9274368) {
            ok = false;
            break;
        }
        ok = BCryptHashData(hash, buffer.data(), count, 0) >= 0;
    }
    if (ok)
        ok =
            BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0 && !memcmp(digest, Digest, sizeof(Digest));
    if (hash)
        BCryptDestroyHash(hash);
    if (algorithm)
        BCryptCloseAlgorithmProvider(algorithm, 0);
    CloseHandle(file);
    return ok;
}

bool FpsLimiter::supported() {
    // The pinned client advances gameplay with frames. A verified address is not
    // evidence that rendering can be uncapped without accelerating simulation.
    return false;
}
void FpsLimiter::update(bool) {
    // Legacy settings and callers must never alter the client's limiter or clocks.
}
