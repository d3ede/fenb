#include "optimization.cpp"
#include <cassert>
#include <cstdio>
void Log(const char *) {}
int main() {
    using namespace optimization_detail;
    alignas(4) std::array<unsigned char,0x16c> player{},nearby{},distant{},invalid{};
    field<float>(nearby.data(),0x160)=10;
    field<float>(distant.data(),0x160)=80;
    field<float>(invalid.data(),0x160)=std::numeric_limits<float>::quiet_NaN();
    alignas(4) std::array<unsigned char,0x7c> texts[4]{};
    void *objects[]={nearby.data(),distant.data(),player.data(),invalid.data()};
    void *entries[4]{};
    for(int i=0;i<4;++i){field<void *>(texts[i].data(),0x78)=objects[i];entries[i]=texts[i].data();}
    void **data=entries;int count=4;void *active=player.data();unsigned char instruction=0;
    Binding binding{&instruction,reinterpret_cast<Render>(1),&data,&count,&active};
    Settings settings;
    {Filter scope(binding,settings);assert(data==entries&&count==4);}
    settings.limitShopSigns=true;settings.shopSignDistance=40;
    {Filter scope(binding,settings);assert(data!=entries&&count==3);assert(data[0]==entries[0]&&data[1]==entries[2]&&data[2]==entries[3]);}
    assert(data==entries&&count==4);
    assert(field<float>(distant.data(),0x160)==80); // only the render list changes
    settings.shopSignDistance=100;
    {Filter scope(binding,settings);assert(data==entries&&count==4);}
    active=nullptr;
    {Filter scope(binding,settings);assert(data==entries&&count==4);}
    active=player.data();count=4097;
    {Filter scope(binding,settings);assert(data==entries&&count==4097);}
    count=4;field<float>(player.data(),0x160)=std::numeric_limits<float>::infinity();
    {Filter scope(binding,settings);assert(data==entries&&count==4);}
    for(int i=0;i<4;++i)assert(field<void *>(texts[i].data(),0x78)==objects[i]);
    puts("PASS: shop-sign range, own sign, stable order, invalid-position fallback, bounded list and exact restoration.");
}
