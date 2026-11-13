#include <windows.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <vector>
#include <cstring>
#include <string>

int main(int argc, char** argv) {
    if (argc != 3 && argc != 4 && argc != 5) return 2;
    FILE* input = fopen(argv[1], "rb"); if (!input) return 3;
    fseek(input, 0, SEEK_END); long size = ftell(input); rewind(input);
    std::vector<char> source(size); fread(source.data(), 1, size, input); fclose(input);
    // Publish only complete headers, including when another local build reads them concurrently.
    const std::string temporary=std::string(argv[2])+".tmp."+std::to_string(GetCurrentProcessId());
    FILE* output = fopen(temporary.c_str(), "wb"); if (!output) return 4;
    auto fail=[&](int status){fclose(output);DeleteFileA(temporary.c_str());return status;};
    fprintf(output, "#pragma once\n#include <windows.h>\n");
    const char* entries[] = {"Bright", "Blur", "Composite"};
    const char* sceneEntries[]={"SceneAO","SceneRays","SceneComposite","Luminance","Reduce","Adapt","SkyVS","SkyPS","FlarePS","EmitterVS","EmitterPS","EmitterSum","EmitterLighting","FXAA","SkyMean","SceneRaySource","SceneDoF"};
    const char* smaaEntries[]={"SMAAEdges","SMAAWeights","SMAABlend"};
    const char* sfxEntries[]={"SfxVS","SfxPS","AuraPS"};
    const bool sfx=argc==4 && !strcmp(argv[3],"sfx");
    const bool smaa=argc==4 && !strcmp(argv[3],"smaa");
    const int count=argc==5?1:sfx?3:smaa?3:argc==4?17:3;
    for (int index=0;index<count;++index) {
        const char* entry=argc==5?argv[3]:sfx?sfxEntries[index]:smaa?smaaEntries[index]:argc==4?sceneEntries[index]:entries[index];
        ID3DBlob* code = nullptr; ID3DBlob* error = nullptr;
        HRESULT hr = D3DCompile(source.data(), source.size(), argv[1], nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entry, argc==5?argv[4]:argc==4&&std::strlen(entry)>2&&!std::strcmp(entry+std::strlen(entry)-2,"VS")?"vs_3_0":"ps_3_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &error);
        if (error) { fprintf(stderr, "%s", (char*)error->GetBufferPointer()); error->Release(); }
        if (FAILED(hr)) return fail(5);
        ID3DBlob* assembly=nullptr;
        if(FAILED(D3DDisassemble(code->GetBufferPointer(),code->GetBufferSize(),0,nullptr,&assembly))){code->Release();return fail(6);}
        const char* budget=std::strstr(static_cast<const char*>(assembly->GetBufferPointer()),"approximately ");
        unsigned slots=0;if(budget)std::sscanf(budget,"approximately %u",&slots);
        assembly->Release();
        if(slots>512){fprintf(stderr,"%s: %s exceeds the 512-slot Shader Model 3 budget (%u).\n",argv[1],entry,slots);code->Release();return fail(7);}
        fprintf(output, "inline constexpr DWORD Shader%s[] = {", entry);
        auto* data = (DWORD*)code->GetBufferPointer();
        for (size_t i=0; i<code->GetBufferSize()/4; ++i) fprintf(output, "%s0x%08lx", i?",":"", data[i]);
        fprintf(output, "};\n"); code->Release();
    }
    const bool written=!ferror(output);const bool closed=fclose(output)==0;
    if(!written||!closed||!MoveFileExA(temporary.c_str(),argv[2],MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        DeleteFileA(temporary.c_str());return 8;
    }
    return 0;
}

