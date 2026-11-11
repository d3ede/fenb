#include <windows.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    FILE* input = fopen(argv[1], "rb"); if (!input) return 3;
    fseek(input, 0, SEEK_END); long size = ftell(input); rewind(input);
    std::vector<char> source(size); fread(source.data(), 1, size, input); fclose(input);
    FILE* output = fopen(argv[2], "wb"); if (!output) return 4;
    fprintf(output, "#pragma once\n#include <windows.h>\n");
    const char* entries[] = {"Bright", "Blur", "Composite"};
    for (auto entry : entries) {
        ID3DBlob* code = nullptr; ID3DBlob* error = nullptr;
        HRESULT hr = D3DCompile(source.data(), source.size(), argv[1], nullptr, nullptr,
            entry, "ps_3_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &error);
        if (error) { fprintf(stderr, "%s", (char*)error->GetBufferPointer()); error->Release(); }
        if (FAILED(hr)) { fclose(output); return 5; }
        fprintf(output, "inline constexpr DWORD Shader%s[] = {", entry);
        auto* data = (DWORD*)code->GetBufferPointer();
        for (size_t i=0; i<code->GetBufferSize()/4; ++i) fprintf(output, "%s0x%08lx", i?",":"", data[i]);
        fprintf(output, "};\n"); code->Release();
    }
    fclose(output); return 0;
}

