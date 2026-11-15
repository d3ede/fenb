#pragma once
#include "client_layout.hpp"

// Presentation-only fallback discovery. Keep the model ABI and vtable ownership
// checks while allowing a rebuilt client to move code and read-only data.
namespace presentation_detail {
inline DWORD player(const client_detail::Image &scan,DWORD operand) {
    const DWORD slot=scan.relative(scan.value(operand));
    return slot && scan.section(slot,4,IMAGE_SCN_MEM_READ|IMAGE_SCN_MEM_WRITE)?slot:0;
}
struct Animation { DWORD table=0,destroyTable=0;explicit operator bool()const{return table && destroyTable;} };
inline Animation animation(const client_detail::Image &scan) {
    const DWORD render=scan.find("55 8B EC 83 E4 F0 81 EC 28 02 00 00 A1 ?? ?? ?? ?? 33 C4 89 84 24 24 02");
    const DWORD move=scan.find("55 8B EC 53 56 8B F1 57 83 BE 08 01 00 00 00 0F 84 E5 00 00 00");
    const DWORD table=scan.table(0,render);
    if(table<19*4 || !move || scan.relative(scan.value(table+8))!=move)return {};
    const DWORD destroyTable=table-19*4,destroy=scan.relative(scan.value(destroyTable));
    if(!destroy || !scan.bytes(destroy,"55 8B EC 56 8B F1 E8 ?? ?? ?? ?? F6 45 08 01 74 0E 68 5C 01 00 00 56 E8"))return {};
    return {table,destroyTable};
}
struct Camera { DWORD table=0,vectors=0,player=0;explicit operator bool()const{return table && vectors && player;} };
inline Camera camera(const client_detail::Image &scan) {
    const DWORD transform=scan.find("55 8B EC 83 EC 2C A1 ?? ?? ?? ?? 33 C5 89 45 FC 53 8B D9 56 8B 75 08 57");
    const DWORD table=scan.table(1,transform,"55 8B EC 83 E4 F0 81 EC 98 00 00 00 A1 ?? ?? ?? ?? 33 C4"),process=table?scan.relative(scan.value(table)):0;
    if(!process || !scan.bytes(process,"55 8B EC 83 E4 F0 81 EC 98 00 00 00 A1 ?? ?? ?? ?? 33 C4"))return {};
    // The verified Process publishes three adjacent vectors. Follow its stores,
    // checking their relationship rather than assuming the former global RVA.
    if(!scan.bytes(process+0x489,"F3 0F 11 0D ?? ?? ?? ?? F3 0F 11 1D ?? ?? ?? ?? F3 0F 11 25"))return {};
    if(!scan.bytes(process+0x1a,"A1 ?? ?? ?? ?? 56 57 8B F9 89 7C 24 2C 85 C0"))return {};
    const DWORD vectors=scan.relative(scan.value(process+0x495)),activePlayer=player(scan,process+0x1b);
    if(!vectors || !activePlayer || !scan.section(vectors,36,IMAGE_SCN_MEM_READ|IMAGE_SCN_MEM_WRITE) ||
       scan.relative(scan.value(process+0x48d))!=vectors+4 || scan.relative(scan.value(process+0x49d))!=vectors+8)return {};
    return {table,vectors,activePlayer};
}
struct Mover { DWORD table=0,player=0;explicit operator bool()const{return table && player;} };
inline Mover mover(const client_detail::Image &scan) {
    const DWORD render=scan.find("55 8B EC 81 EC 04 02 00 00 A1 ?? ?? ?? ?? 33 C5 89 45 FC 56 8B 75 08 57");
    const DWORD table=scan.table(9,render);
    const DWORD process=table?scan.relative(scan.value(table+6*4)):0,destroy=table?scan.relative(scan.value(table)):0;
    const DWORD activePlayer=process?player(scan,process+0x5f):0;
    if(!process || !destroy || !activePlayer ||
       !scan.bytes(process,"55 8B EC 6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00 50 81 EC D0 01 00 00") ||
       !scan.bytes(destroy,"55 8B EC 56 8B F1 E8 ?? ?? ?? ?? 8B 45 08 A8 01 74 56 A8 04 75 44 53 57") ||
       !scan.bytes(process+0x5d,"39 3D ?? ?? ?? ?? 75 4B F6 87 C8 11 00 00 1F") ||
       scan.relative(scan.value(process+0xc2))!=activePlayer)return {};
    return {table,activePlayer};
}
}
