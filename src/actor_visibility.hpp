#pragma once
#include "settings.hpp"

// Called only for the guarded native render-list layout, never world updates.
namespace actor_visibility {
template<class T> const T &field(const void *object,size_t offset) {
    return *reinterpret_cast<const T *>(static_cast<const unsigned char *>(object)+offset);
}
enum class Kind { Other,Player,Monster,Pet };
inline bool changed(const Settings &s) {
    return !s.renderPlayers || !s.renderMonsters || !s.renderPets ||
        s.playerDrawDistance>0 || s.monsterDrawDistance>0 || s.petDrawDistance>0;
}
inline Kind kind(const void *object) {
    if(!object || field<DWORD>(object,0x170)!=5)return Kind::Other;
    if(field<int>(object,0x31c))return Kind::Player;
    const auto ai=field<DWORD>(object,0x18c);
    if(ai==5 || ai==9)return Kind::Pet;
    // Same field/value as the native IsPeaceful predicate. This includes
    // passive combat mobs and bosses while retaining peaceful town NPCs.
    return field<DWORD>(object,0x6e8)==1 ? Kind::Other : Kind::Monster;
}
inline bool visible(const Settings &s,const void *object,const void *player) {
    if(!object || !player || object==player)return true;
    bool enabled=true;float limit=0;
    switch(kind(object)) {
    case Kind::Player:enabled=s.renderPlayers;limit=s.playerDrawDistance;break;
    case Kind::Monster:enabled=s.renderMonsters;limit=s.monsterDrawDistance;break;
    case Kind::Pet:enabled=s.renderPets;limit=s.petDrawDistance;break;
    case Kind::Other:return true;
    }
    if(!enabled)return false;
    // Native camera distance is computed before the guarded sort call. Zero
    // retains native visibility and invalid values fail open.
    const float distance=field<float>(object,0x2cc);
    return !(limit>0) || !std::isfinite(distance) || distance<0 || distance<=limit;
}
inline size_t compact(void **objects,size_t count,const Settings &s,const void *player) {
    if(!objects || count>5000 || !player || !changed(s))return count;
    size_t kept=0;
    for(size_t i=0;i<count;++i)if(visible(s,objects[i],player))objects[kept++]=objects[i];
    return kept;
}
}
