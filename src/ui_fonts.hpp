#pragma once
#include "common.hpp"
#include "imgui.h"
#include "generated/ui_font.hpp"
#include <cstdio>
#include <cstring>

struct PanelFonts {
    ImFont *regular = nullptr;
    ImFont *emphasis = nullptr;
};

// Fonts belong to the current ImGui atlas. Windows supplies Tahoma; Inter remains
// embedded so a missing system font never prevents opening the settings panel.
inline PanelFonts LoadPanelFonts(const wchar_t *fontPath = nullptr) {
    std::wstring systemPath;
    if (!fontPath) {
        wchar_t directory[MAX_PATH]{};
        const UINT length = GetWindowsDirectoryW(directory, MAX_PATH);
        if (length && length < MAX_PATH)
            systemPath = std::wstring(directory) + L"\\Fonts\\tahoma.ttf";
        fontPath = systemPath.c_str();
    }
    std::vector<unsigned char> data;
    if (FILE *file = _wfopen(fontPath, L"rb")) {
        if (fseek(file, 0, SEEK_END) == 0) {
            const long size = ftell(file);
            if (size > 100 && size <= 8 * 1024 * 1024 && fseek(file, 0, SEEK_SET) == 0) {
                data.resize(static_cast<std::size_t>(size));
                if (fread(data.data(), 1, data.size(), file) != data.size())
                    data.clear();
            }
        }
        fclose(file);
    }
    auto &io = ImGui::GetIO();
    auto add = [&](const char *name, float coverage) {
        ImFontConfig config{};
        config.OversampleH = 2;
        config.OversampleV = 1;
        config.RasterizerMultiply = coverage;
        std::snprintf(config.Name, sizeof(config.Name), "%s", name);
        void *bytes = const_cast<unsigned char *>(UI_FONT);
        int size = sizeof(UI_FONT);
        config.FontDataOwnedByAtlas = !data.empty();
        if (!data.empty()) {
            size = static_cast<int>(data.size());
            bytes = IM_ALLOC(data.size());
            std::memcpy(bytes, data.data(), data.size());
        }
        return io.Fonts->AddFontFromMemoryTTF(bytes, size, 15, &config,
                                             io.Fonts->GetGlyphRangesCyrillic());
    };
    PanelFonts fonts{add("FENB UI Regular", 1.f), add("FENB UI Emphasis", 1.35f)};
    io.FontDefault = fonts.regular;
    return fonts;
}

inline ImFont *PanelEmphasisFont() {
    for (ImFont *font : ImGui::GetIO().Fonts->Fonts)
        if (!std::strcmp(font->GetDebugName(), "FENB UI Emphasis"))
            return font;
    return ImGui::GetFont();
}
