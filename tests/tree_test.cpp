#include "trees.hpp"
#include "water.hpp"
#include <cstdio>
#define CHECK(x) do{if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char**argv){
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"FENBTreeTest";
    RegisterClassW(&wc);HWND window=CreateWindowW(wc.lpszClassName,L"Tree replacement fixture",WS_OVERLAPPEDWINDOW,0,0,128,128,nullptr,nullptr,wc.hInstance,nullptr);
    Com<IDirect3D9> root;root.p=Direct3DCreate9(D3D_SDK_VERSION);CHECK(root.p);
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.hDeviceWindow=window;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    pp.BackBufferWidth=pp.BackBufferHeight=128;pp.BackBufferFormat=D3DFMT_X8R8G8B8;
    Com<IDirect3DDevice9> device;CHECK(SUCCEEDED(root->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,device.out())));
    auto*d=device.p;TreeFX trees(d);Settings settings;CHECK(!settings.trees);settings.trees=true;
    D3DMATRIX projection{};projection._11=projection._22=1;projection._33=1.01f;projection._34=1;projection._43=-.101f;
    CHECK(SUCCEEDED(d->SetTransform(D3DTS_PROJECTION,&projection)));d->SetFVF(0x112);d->SetRenderState(D3DRS_ZENABLE,TRUE);
    d->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT);d->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);
    Com<IDirect3DTexture9> original,second;
    CHECK(SUCCEEDED(d->CreateTexture(TreeAssets[0].width,TreeAssets[0].height,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,original.out(),nullptr)));
    CHECK(SUCCEEDED(d->CreateTexture(TreeAssets[0].width,TreeAssets[0].height,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,second.out(),nullptr)));
    D3DLOCKED_RECT lock{};CHECK(SUCCEEDED(original->LockRect(0,&lock,nullptr,0)));
    for(UINT y=0;y<TreeAssets[0].height;y++)std::fill_n((DWORD*)((BYTE*)lock.pBits+y*lock.Pitch),TreeAssets[0].width,0xff326489);
    original->UnlockRect(0);CHECK(SUCCEEDED(d->SetTexture(0,original)));
    trees.beginFrame(true);CHECK(!trees.begin(settings));CHECK(trees.bytes()==0); // unknown uploads stay native
    unsigned id=1;original->SetPrivateData(TreeTag,&id,sizeof(id),0);second->SetPrivateData(TreeTag,&id,sizeof(id),0);
    CHECK(trees.begin(settings));Com<IDirect3DBaseTexture9> replacement;CHECK(SUCCEEDED(d->GetTexture(0,replacement.out())));
    CHECK(replacement.p&&replacement.p!=original.p);CHECK(trees.bytes()==TreeAssets[0].size);
    auto*tex=static_cast<IDirect3DTexture9*>(replacement.p);D3DSURFACE_DESC desc{};tex->GetLevelDesc(0,&desc);
    CHECK(desc.Width==TreeAssets[0].width&&desc.Height==TreeAssets[0].height&&desc.Format==D3DFORMAT(TreeAssets[0].format));
    CHECK(tex->GetLevelCount()==TreeAssets[0].levels);CHECK(SUCCEEDED(tex->LockRect(0,&lock,nullptr,D3DLOCK_READONLY)));
    CHECK(!memcmp(lock.pBits,TreeTextureData+TreeAssets[0].offset,std::max(1u,(TreeAssets[0].width+3)/4)*8));tex->UnlockRect(0);
    CHECK(!trees.begin(settings));trees.end();Com<IDirect3DBaseTexture9> restored;d->GetTexture(0,restored.out());CHECK(restored.p==original.p);
    DWORD sampler=0;d->GetSamplerState(0,D3DSAMP_MINFILTER,&sampler);CHECK(sampler==D3DTEXF_POINT);
    d->GetSamplerState(0,D3DSAMP_ADDRESSU,&sampler);CHECK(sampler==D3DTADDRESS_CLAMP);
    d->SetTexture(0,second);CHECK(trees.begin(settings));Com<IDirect3DBaseTexture9> shared;d->GetTexture(0,shared.out());CHECK(shared.p==replacement.p);trees.end();
    settings.trees=false;CHECK(!trees.begin(settings));trees.beginFrame(false);CHECK(trees.bytes()==0);
    settings.trees=true;settings.enabled=false;CHECK(!trees.begin(settings));settings.enabled=true;
    d->SetFVF(0x104);CHECK(!trees.begin(settings));d->SetFVF(0x112);
    projection._34=0;projection._44=1;d->SetTransform(D3DTS_PROJECTION,&projection);CHECK(!trees.begin(settings));
    projection._34=1;projection._44=0;d->SetTransform(D3DTS_PROJECTION,&projection);
    CHECK(trees.begin(settings));trees.invalidate();CHECK(trees.bytes()==0);d->GetTexture(0,restored.out());CHECK(restored.p==second.p);
    CHECK(trees.begin(settings));trees.end(); // lazily recreated after reset/invalidation
    InvalidateWaterTag(second);CHECK(!trees.begin(settings));
    d->SetTexture(0,original);
    settings.treeFoliage = true;
    for(unsigned i=0;i<std::size(TreeAssets);i++){
        id=i+1;original->SetPrivateData(TreeTag,&id,sizeof(id),0);CHECK(trees.begin(settings));trees.end();
    }
    size_t total=0;for(const auto&a:TreeAssets)total+=a.size;CHECK(trees.bytes()==total);
    trees.beginFrame(false);CHECK(!trees.bytes());
    // Shared native textures pick two stable replacements without changing the source tag.
    unsigned variantCount = 0;
    for (unsigned i = 0; i < std::size(TreeAssets); ++i) {
        if (TreeAssets[i].alternate == i) continue;
        ++variantCount;
        id = i + 1;
        CHECK(SUCCEEDED(original->SetPrivateData(TreeTag, &id, sizeof(id), 0)));
        D3DMATRIX world{};
        world._11 = world._22 = world._33 = world._44 = 1;
        CHECK(SUCCEEDED(d->SetTransform(D3DTS_WORLD, &world)));
        CHECK(trees.begin(settings));
        Com<IDirect3DBaseTexture9> primary;
        CHECK(SUCCEEDED(d->GetTexture(0, primary.out())));
        trees.end();
        bool foundAlternate = false;
        for (int x = 1; x <= 64 && !foundAlternate; ++x) {
            world._41 = float(x);
            CHECK(SUCCEEDED(d->SetTransform(D3DTS_WORLD, &world)));
            CHECK(trees.begin(settings));
            Com<IDirect3DBaseTexture9> chosen;
            CHECK(SUCCEEDED(d->GetTexture(0, chosen.out())));
            trees.end();
            if (chosen.p != primary.p) {
                CHECK(trees.begin(settings));
                CHECK(SUCCEEDED(d->GetTexture(0, restored.out())) && restored.p == chosen.p);
                trees.end();
                foundAlternate = true;
            }
        }
        CHECK(foundAlternate);
        CHECK(SUCCEEDED(d->GetTexture(0, restored.out())) && restored.p == original.p);
        trees.beginFrame(false);
    }
    CHECK(variantCount == 4);
    CHECK(SUCCEEDED(d->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE)));
    CHECK(SUCCEEDED(d->SetRenderState(D3DRS_ALPHAREF, 128)));
    CHECK(SUCCEEDED(d->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE)));
    unsigned foliageCount = 0;
    for (unsigned i = 0; i < std::size(TreeAssets); ++i) {
        const auto &asset = TreeAssets[i];
        if (asset.foliage) {
            ++foliageCount;
            CHECK(asset.format == D3DFMT_DXT5);
        }
        id = i + 1;
        CHECK(SUCCEEDED(original->SetPrivateData(TreeTag, &id, sizeof(id), 0)));
        settings.treeBark = !asset.foliage;
        settings.treeFoliage = asset.foliage;
        CHECK(trees.begin(settings));
        CHECK(trees.nativeFoliage() == (asset.foliage ? original.p : nullptr));
        trees.end();
        CHECK(!trees.nativeFoliage());
        CHECK(SUCCEEDED(d->GetTexture(0, restored.out())) && restored.p == original.p);
        settings.treeBark = asset.foliage;
        settings.treeFoliage = !asset.foliage;
        CHECK(!trees.begin(settings));
        CHECK(SUCCEEDED(d->GetTexture(0, restored.out())) && restored.p == original.p);
    }
    CHECK(foliageCount == 12);
    DWORD alphaState = 0;
    CHECK(SUCCEEDED(d->GetRenderState(D3DRS_ALPHATESTENABLE, &alphaState)) && alphaState == TRUE);
    CHECK(SUCCEEDED(d->GetRenderState(D3DRS_ALPHAREF, &alphaState)) && alphaState == 128);
    CHECK(SUCCEEDED(d->GetRenderState(D3DRS_ALPHABLENDENABLE, &alphaState)) && alphaState == TRUE);
    trees.beginFrame(false);
    settings.treeBark = settings.treeFoliage = true;
    if(argc>1){
        // Optional read-only integration check against explicitly supplied local artwork.
        HMODULE library=LoadLibraryExW(L"d3dx9_43.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);CHECK(library);
        using Load=HRESULT(WINAPI*)(IDirect3DDevice9*,LPCWSTR,UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,DWORD,DWORD,D3DCOLOR,void*,PALETTEENTRY*,IDirect3DTexture9**);
        auto load=reinterpret_cast<Load>(GetProcAddress(library,"D3DXCreateTextureFromFileExW"));CHECK(load);
        unsigned accepted=0;
        for(int i=1;i<argc;i++)for(D3DFORMAT format:{D3DFMT_UNKNOWN,D3DFMT_A8R8G8B8}){
            const int count=MultiByteToWideChar(CP_UTF8,0,argv[i],-1,nullptr,0);CHECK(count>0);
            std::wstring file(count,L'\0');MultiByteToWideChar(CP_UTF8,0,argv[i],-1,file.data(),count);
            Com<IDirect3DTexture9> source;
            CHECK(SUCCEEDED(load(d,file.c_str(),UINT(-1),UINT(-1),0,0,format,D3DPOOL_MANAGED,1,1,0,nullptr,nullptr,source.out())));
            d->SetTexture(0,source);CHECK(trees.begin(settings));d->GetTexture(0,restored.out());CHECK(restored.p!=source.p);
            trees.end();d->GetTexture(0,restored.out());CHECK(restored.p==source.p);accepted++;
        }
        FreeLibrary(library);trees.beginFrame(false);CHECK(!trees.bytes());
        printf("PASS: %u original compressed/ARGB8 tree uploads identified and replaced read-only.\n",accepted);
    }
    puts("PASS: exact-source rejection, all embedded mip chains, shared cache, native texture/sampler restoration, off/master gates, invalidation and lazy recreation.");
    return 0;
}
