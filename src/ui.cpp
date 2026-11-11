#include "imgui.h"
#include "runtime.hpp"
#include <cstdio>

namespace {
constexpr ImU32 Background = IM_COL32(29, 30, 33, 255), Raised = IM_COL32(35, 36, 40, 255);
constexpr ImU32 Line = IM_COL32(54, 56, 61, 255), Text = IM_COL32(221, 223, 227, 255);
constexpr ImU32 Muted = IM_COL32(143, 147, 155, 255), Accent = IM_COL32(171, 157, 204, 255);
constexpr float RowHeight = 26;
ImU32 Opacity(ImU32 color) {
    auto a = static_cast<ImU32>((color >> 24) * ImGui::GetStyle().Alpha);
    return (color & 0x00ffffff) | (a << 24);
}
void TextAt(ImDrawList *draw, ImVec2 p, const char *text, ImU32 color = Text, float size = 15) {
    draw->AddText(ImGui::GetFont(), size, {std::floor(p.x), std::floor(p.y)}, Opacity(color), text);
}
void Theme() {
    auto &s = ImGui::GetStyle();
    for (auto &c : s.Colors)
        c = ImVec4(.23f, .24f, .27f, 1);
    s.WindowPadding = {0, 0};
    s.WindowRounding = 5;
    s.WindowBorderSize = 1;
    s.ChildRounding = 0;
    s.ChildBorderSize = 0;
    s.FrameRounding = 2;
    s.FramePadding = {5, 3};
    s.FrameBorderSize = 0;
    s.ItemSpacing = {7, 0};
    s.ItemInnerSpacing = {5, 4};
    s.ScrollbarSize = 5;
    s.ScrollbarRounding = 2;
    s.GrabRounding = 1;
    s.PopupRounding = 3;
    s.PopupBorderSize = 1;
    s.Colors[ImGuiCol_Text] = {.87f, .88f, .90f, 1};
    s.Colors[ImGuiCol_TextDisabled] = {.48f, .50f, .54f, 1};
    s.Colors[ImGuiCol_WindowBg] = ImGui::ColorConvertU32ToFloat4(Background);
    s.Colors[ImGuiCol_ChildBg] = {0, 0, 0, 0};
    s.Colors[ImGuiCol_Border] = ImGui::ColorConvertU32ToFloat4(Line);
    s.Colors[ImGuiCol_FrameBg] = {0, 0, 0, 0};
    s.Colors[ImGuiCol_FrameBgHovered] = {.19f, .20f, .23f, 1};
    s.Colors[ImGuiCol_FrameBgActive] = {.23f, .24f, .28f, 1};
    s.Colors[ImGuiCol_Button] = {0, 0, 0, 0};
    s.Colors[ImGuiCol_ButtonHovered] = {.22f, .23f, .26f, 1};
    s.Colors[ImGuiCol_ButtonActive] = {.26f, .27f, .30f, 1};
    s.Colors[ImGuiCol_CheckMark] = {.79f, .80f, .83f, 1};
    s.Colors[ImGuiCol_SliderGrab] = {.65f, .66f, .69f, 1};
    s.Colors[ImGuiCol_SliderGrabActive] = ImGui::ColorConvertU32ToFloat4(Accent);
    s.Colors[ImGuiCol_ScrollbarBg] = {0, 0, 0, 0};
    s.Colors[ImGuiCol_ScrollbarGrab] = {.30f, .31f, .35f, 1};
    s.Colors[ImGuiCol_PopupBg] = {.13f, .14f, .16f, 1};
    s.Colors[ImGuiCol_Header] = {.24f, .25f, .28f, 1};
    s.Colors[ImGuiCol_HeaderHovered] = {.28f, .29f, .33f, 1};
    s.Colors[ImGuiCol_HeaderActive] = {.32f, .33f, .37f, 1};
    s.Colors[ImGuiCol_TextSelectedBg] = {.40f, .37f, .49f, .65f};
    s.Colors[ImGuiCol_NavCursor] = ImGui::ColorConvertU32ToFloat4(Accent);
}
void Tip(const char *text) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(300);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}
bool SquareSwitch(const char *id, bool &value, ImVec2 size = {18, 18}) {
    auto p = ImGui::GetCursorScreenPos();
    bool changed = ImGui::InvisibleButton(id, size, ImGuiButtonFlags_EnableNav);
    if (changed)
        value = !value;
    auto *d = ImGui::GetWindowDrawList();
    auto center = ImVec2(std::floor(p.x + size.x / 2), std::floor(p.y + size.y / 2));
    d->AddRectFilled({center.x - 6, center.y - 6}, {center.x + 6, center.y + 6},
                     value ? IM_COL32(176, 179, 187, 255) : Raised, 2);
    d->AddRect({center.x - 6, center.y - 6}, {center.x + 6, center.y + 6},
               ImGui::IsItemHovered() ? Accent : IM_COL32(104, 107, 116, 255), 2);
    if (value) {
        d->AddLine({center.x - 3, center.y}, {center.x - 1, center.y + 2}, Background, 1.5f);
        d->AddLine({center.x - 1, center.y + 2}, {center.x + 3, center.y - 3}, Background, 1.5f);
    }
    return changed;
}
void ResetGlyph(ImDrawList *d, ImVec2 p, ImU32 color) {
    d->PathArcTo({p.x + 8, p.y + 9}, 4, .5f, 5.f, 14);
    d->PathStroke(color, 1.f);
    d->AddLine({p.x + 4, p.y + 3}, {p.x + 4, p.y + 7}, color, 1);
    d->AddLine({p.x + 4, p.y + 7}, {p.x + 8, p.y + 7}, color, 1);
}
void Row(const char *label, const char *help, float &value, float low, float high, float neutral,
         const char *format = "%.2f") {
    ImGui::PushID(label);
    auto p = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    auto *d = ImGui::GetWindowDrawList();
    const float labelWidth = 146, valueWidth = 57, resetWidth = 18,
                inputX = width - valueWidth - resetWidth - 7, trackX = labelWidth,
                trackWidth = inputX - trackX - 12;
    ImGui::InvisibleButton("label", {labelWidth - 8, RowHeight});
    Tip(help);
    TextAt(d, {p.x, p.y + 5}, label);
    ImGui::SetCursorScreenPos({p.x + trackX, p.y});
    ImGui::InvisibleButton("track", {trackWidth, RowHeight}, ImGuiButtonFlags_EnableNav);
    bool hover = ImGui::IsItemHovered(), active = ImGui::IsItemActive();
    if (active && ImGui::IsMouseDown(0))
        value =
            low + std::clamp((ImGui::GetIO().MousePos.x - p.x - trackX - 3) / (trackWidth - 6), 0.f, 1.f) *
                      (high - low);
    if (hover && ImGui::IsMouseDoubleClicked(0))
        value = neutral;
    if (ImGui::IsItemFocused()) {
        float step = (high - low) / 200;
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
            value = std::max(low, value - step);
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
            value = std::min(high, value + step);
    }
    float y = std::floor(p.y + RowHeight * .5f), left = p.x + trackX + 3, right = left + trackWidth - 6;
    float knob = std::floor(left + (right - left) * std::clamp((value - low) / (high - low), 0.f, 1.f)),
          zero = std::floor(left + (right - left) * (neutral - low) / (high - low));
    d->AddLine({left, y}, {right, y}, Opacity(IM_COL32(76, 79, 86, 255)), 1);
    d->AddLine({zero, y - 3}, {zero, y + 3}, Opacity(IM_COL32(99, 102, 110, 255)), 1);
    d->AddLine({zero, y}, {knob, y}, Opacity(IM_COL32(145, 148, 157, 255)), 2);
    d->AddRectFilled({knob - 2, y - 5}, {knob + 3, y + 5},
                     Opacity(active ? Accent : IM_COL32(191, 194, 201, 255)), 1);
    if (hover)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    Tip("Перетащи для настройки. Двойной щелчок — исходное значение.");
    ImGui::SetCursorScreenPos({p.x + inputX, p.y + 2});
    ImGui::SetNextItemWidth(valueWidth);
    ImGui::PushFont(nullptr, 14);
    ImGui::DragFloat("##value", &value, (high - low) / 600, low, high, format, ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopFont();
    Tip("Перетащи число или нажми Ctrl+щелчок для точного ввода.");
    ImGui::SetCursorScreenPos({p.x + width - resetWidth, p.y + 4});
    if (ImGui::InvisibleButton("reset", {resetWidth, 18}, ImGuiButtonFlags_EnableNav))
        value = neutral;
    if (value != neutral || ImGui::IsItemHovered() || ImGui::IsItemFocused())
        ResetGlyph(d, ImGui::GetItemRectMin(), Opacity(ImGui::IsItemHovered() ? Text : Muted));
    Tip("Сбросить значение");
    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy({width, RowHeight});
    ImGui::PopID();
}
bool Section(const char *id, const char *title, bool &enabled) {
    ImGui::PushID(id);
    auto p = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    auto *d = ImGui::GetWindowDrawList();
    auto key = ImGui::GetID("expanded");
    auto *state = ImGui::GetStateStorage();
    bool expanded = state->GetBool(key, true);
    d->AddLine(p, {p.x + width, p.y}, Line, 1);
    if (ImGui::InvisibleButton("heading", {width - 30, 28}, ImGuiButtonFlags_EnableNav)) {
        expanded = !expanded;
        state->SetBool(key, expanded);
    }
    ImU32 c = ImGui::IsItemHovered() ? Text : Muted;
    if (expanded)
        d->AddTriangleFilled({p.x + 1, p.y + 12}, {p.x + 8, p.y + 12}, {p.x + 4.5f, p.y + 16}, c);
    else
        d->AddTriangleFilled({p.x + 2, p.y + 10}, {p.x + 2, p.y + 17}, {p.x + 6, p.y + 13.5f}, c);
    TextAt(d, {p.x + 17, p.y + 6}, title);
    ImGui::SetCursorScreenPos({p.x + width - 22, p.y + 5});
    SquareSwitch("enabled", enabled);
    Tip(enabled ? "Выключить эффект" : "Включить эффект");
    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy({width, 28});
    ImGui::PopID();
    return expanded;
}
int MatchingPreset(const Settings &s) {
    for (int i = 0; i < 4; ++i) {
        Settings candidate;
        candidate.preset(i);
        candidate.enabled = s.enabled;
        candidate.openOnStart = s.openOnStart;
        candidate.panelPage = s.panelPage;
        if (candidate == s)
            return i;
    }
    return -1;
}
} // namespace
void DrawPanel(Runtime &r) {
    Theme();
    auto &io = ImGui::GetIO();
    auto &s = r.settings;
    const Settings before = s;
    ImVec2 size{std::max(310.f, std::min(540.f, io.DisplaySize.x - 24)),
                std::max(250.f, std::min(520.f, io.DisplaySize.y - 24))};
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowPos({(io.DisplaySize.x - size.x) * .5f, (io.DisplaySize.y - size.y) * .5f},
                            ImGuiCond_FirstUseEver);
    ImGui::Begin("FENB", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);
    auto origin = ImGui::GetWindowPos();
    auto *d = ImGui::GetWindowDrawList();
    TextAt(d, {origin.x + 16, origin.y + 12}, "FENB");
    d->AddLine({origin.x + 68, origin.y + 12}, {origin.x + 68, origin.y + 27}, Line, 1);
    TextAt(d, {origin.x + 80, origin.y + 12}, "Изображение", Muted, 14);
    ImGui::SetCursorPos({size.x - 151, 8});
    SquareSwitch("master", s.enabled, {20, 24});
    Tip("Включить или выключить всю постобработку");
    TextAt(d, {origin.x + size.x - 125, origin.y + 13}, "Эффекты", s.enabled ? Text : Muted, 13);
    ImGui::SetCursorPos({size.x - 57, 8});
    if (ImGui::InvisibleButton("menu", {24, 24}, ImGuiButtonFlags_EnableNav))
        ImGui::OpenPopup("actions");
    auto mp = ImGui::GetItemRectMin();
    for (int i = 0; i < 3; ++i)
        d->AddCircleFilled({mp.x + 6 + i * 5.f, mp.y + 12}, 1, Muted, 8);
    Tip("Действия");
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 7));
    if (ImGui::BeginPopup("actions")) {
#ifdef FENB_DIAGNOSTICS
        if(ImGui::MenuItem("Записать 3 кадра DX9"))r.requestCapture();
        ImGui::Separator();
#endif
        if (ImGui::MenuItem("Сохранить настройки", "Ctrl+S"))
            r.save();
        if (ImGui::MenuItem("Перечитать FENB.ini"))
            r.reload();
        ImGui::Separator();
        if (ImGui::MenuItem("Сбросить всё")) {
            bool open = s.openOnStart;
            s = Settings{};
            s.openOnStart = open;
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
    ImGui::SetCursorPos({size.x - 30, 8});
    if (ImGui::InvisibleButton("close", {22, 24}, ImGuiButtonFlags_EnableNav)) {
        r.visible = false;
        r.compare = false;
    }
    auto cp = ImGui::GetItemRectMin();
    d->AddLine({cp.x + 7, cp.y + 8}, {cp.x + 15, cp.y + 16}, Muted, 1);
    d->AddLine({cp.x + 15, cp.y + 8}, {cp.x + 7, cp.y + 16}, Muted, 1);
    d->AddLine({origin.x + 1, origin.y + 40}, {origin.x + size.x - 1, origin.y + 40}, Line, 1);
    ImGui::SetCursorPos({16, 48});
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(Muted), "Профиль");
    ImGui::SameLine(88);
    ImGui::SetNextItemWidth(size.x - 234);
    const char *presets[] = {"Нейтральный", "Тёплый", "Холодный", "Мягкий свет"};
    int matching = MatchingPreset(s);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(.16f, .17f, .19f, 1));
    if (ImGui::BeginCombo("##profile", matching < 0 ? "Свой" : presets[matching])) {
        for (int i = 0; i < 4; ++i)
            if (ImGui::Selectable(presets[i], matching == i))
                s.preset(i);
        ImGui::EndCombo();
    }
    ImGui::PopStyleColor();
    ImGui::SameLine(size.x - 124);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(r.compare ? Accent : Muted));
    if (ImGui::Button("Оригинал", {108, 22}))
        r.compare = !r.compare;
    ImGui::PopStyleColor();
    Tip("Показать изображение без постобработки. Повторное нажатие вернёт эффекты.");
    if (r.compare)
        d->AddLine({origin.x + size.x - 114, origin.y + 73}, {origin.x + size.x - 26, origin.y + 73}, Accent,
                   1);
    ImGui::SetCursorPos({16, 80});
    ImGui::BeginChild("adjustments", {size.x - 32, size.y - 116}, ImGuiChildFlags_None);
    const Settings defaults;
    if (Section("grade", "Цветокоррекция", s.color)) {
        ImGui::BeginDisabled(!s.color || !s.enabled);
        Row("Экспозиция", "Изменение яркости в ступенях экспозиции (EV).", s.exposure, -2, 2,
            defaults.exposure, "%+.2f");
        Row("Контраст", "Разница между светлыми и тёмными участками.", s.contrast, .5f, 1.5f,
            defaults.contrast);
        Row("Света", "Настройка ярких участков.", s.highlights, -.5f, .5f, defaults.highlights, "%+.2f");
        Row("Тени", "Настройка тёмных участков.", s.shadows, -.5f, .5f, defaults.shadows, "%+.2f");
        Row("Температура", "От холодных оттенков к тёплым.", s.temperature, -1, 1, defaults.temperature,
            "%+.2f");
        Row("Оттенок", "Баланс зелёного и пурпурного.", s.tint, -1, 1, defaults.tint, "%+.2f");
        Row("Насыщенность", "Интенсивность цвета. Ноль — чёрно-белое изображение.", s.saturation, 0, 2,
            defaults.saturation);
        Row("Гамма", "Яркость средних тонов.", s.gamma, .5f, 2, defaults.gamma);
        ImGui::EndDisabled();
    }
    ImGui::Dummy({0, 4});
    if (Section("bloom", "Bloom", s.bloom)) {
        ImGui::BeginDisabled(!s.bloom || !s.enabled);
        Row("Интенсивность", "Сила свечения ярких деталей.", s.bloomStrength, 0, 2, defaults.bloomStrength);
        Row("Порог яркости", "Минимальная яркость, при которой появляется свечение.", s.threshold, 0, 1,
            defaults.threshold);
        Row("Мягкость порога", "Плавность появления свечения около порога.", s.knee, .001f, .5f,
            defaults.knee);
        Row("Радиус", "Размер ореола вокруг ярких деталей.", s.radius, .5f, 4, defaults.radius);
        auto p = ImGui::GetCursorScreenPos();
        float width = ImGui::GetContentRegionAvail().x;
        TextAt(ImGui::GetWindowDrawList(), {p.x, p.y + 5}, "Качество");
        ImGui::SetCursorScreenPos({p.x + 146, p.y + 2});
        ImGui::SetNextItemWidth(width - 146 - 25);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(.14f, .15f, .17f, 1));
        int quality = s.bloomScale == 4 ? 0 : 1;
        const char *options[] = {"Экономное", "Высокое"};
        if (ImGui::Combo("##quality", &quality, options, 2))
            s.bloomScale = quality ? 2 : 4;
        ImGui::PopStyleColor();
        Tip("Экономное: текстуры в четверть ширины и высоты кадра. Высокое: в половину.");
        ImGui::SetCursorScreenPos(p);
        ImGui::Dummy({width, RowHeight});
        ImGui::EndDisabled();
    }
    ImGui::EndChild();
    float footer = size.y - 28;
    d->AddLine({origin.x + 1, origin.y + footer}, {origin.x + size.x - 1, origin.y + footer}, Line, 1);
    const char *status = !r.message.empty() ? r.message.c_str()
                         : r.pendingSave()  ? "Сохраняется…"
                                            : "Автосохранение";
    TextAt(d, {origin.x + 16, origin.y + footer + 7}, status, Muted, 12);
    TextAt(d, {origin.x + size.x - 113, origin.y + footer + 7}, "Shift + Enter", Muted, 12);
    s.sanitize();
    if (!(s == before))
        r.settingsChanged();
    if (!r.visible && r.pendingSave())
        r.save();
    ImGui::End();
}
