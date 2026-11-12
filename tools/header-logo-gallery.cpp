#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

int main() {
    Gdiplus::GdiplusStartupInput startup;
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &startup, nullptr) != Gdiplus::Ok)
        return 1;
    int result = 0;
    {
        UINT count = 0, size = 0;
        Gdiplus::GetImageEncodersSize(&count, &size);
        std::vector<unsigned char> codecs(size);
        auto *encoders = reinterpret_cast<Gdiplus::ImageCodecInfo *>(codecs.data());
        Gdiplus::GetImageEncoders(count, size, encoders);
        CLSID png{};
        for (UINT i = 0; i < count; ++i)
            if (!wcscmp(encoders[i].MimeType, L"image/png"))
                png = encoders[i].Clsid;
        Gdiplus::Bitmap gallery(1768, 940, PixelFormat32bppRGB);
        Gdiplus::Graphics graphics(&gallery);
        graphics.Clear(Gdiplus::Color(255, 19, 20, 23));
        graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        Gdiplus::SolidBrush white(Gdiplus::Color(255, 225, 224, 234));
        Gdiplus::SolidBrush muted(Gdiplus::Color(255, 151, 151, 166));
        Gdiplus::Font heading(L"Arial", 25, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::Font label(L"Arial", 20, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::Font note(L"Arial", 15, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        graphics.DrawString(L"FENB / три варианта логотипа в шапке", -1, &heading,
                            Gdiplus::PointF(32, 20), &white);
        const wchar_t *names[] = {L"01-symbol-text", L"02-horizontal", L"03-symbol-only"};
        const wchar_t *titles[] = {L"1 / Знак + обычная надпись FENB", L"2 / Фирменный логотип целиком",
                                  L"3 / Только знак"};
        for (int i = 0; i < 3; ++i) {
            float x = float(32 + i * 576);
            graphics.DrawString(titles[i], -1, &label, Gdiplus::PointF(x, 58), &white);
            graphics.DrawString(L"Русский · реальный размер 1:1", -1, &note,
                                Gdiplus::PointF(x, 88), &muted);
            std::wstring prefix = std::wstring(L"artifacts/header-logo-options/") + names[i];
            Gdiplus::Bitmap russian((prefix + L"-ru.png").c_str());
            Gdiplus::Bitmap english((prefix + L"-en.png").c_str());
            if (russian.GetLastStatus() != Gdiplus::Ok || english.GetLastStatus() != Gdiplus::Ok ||
                russian.GetWidth() != 800 || english.GetWidth() != 800) {
                result = 2;
                break;
            }
            // Exact crops from actual DX9 backbuffers. No logo is composited onto a screenshot.
            graphics.DrawImage(&russian, Gdiplus::Rect(int(x), 116, 540, 520), 130, 40, 540, 520,
                               Gdiplus::UnitPixel);
            graphics.DrawString(L"English · шапка 1:1", -1, &note,
                                Gdiplus::PointF(x, 652), &muted);
            graphics.DrawImage(&english, Gdiplus::Rect(int(x), 678, 540, 42), 130, 40, 540, 42,
                               Gdiplus::UnitPixel);
            graphics.DrawString(L"Русский · увеличено ×3 для деталей", -1, &note,
                                Gdiplus::PointF(x, 738), &muted);
            graphics.DrawImage(&russian, Gdiplus::Rect(int(x), 766, 510, 120), 130, 40, 170, 40,
                               Gdiplus::UnitPixel);
        }
        graphics.DrawString(L"Реальные кадры родной панели DirectX 9. Панель и все кнопки сохраняют прежнюю высоту.",
                            -1, &note, Gdiplus::PointF(32, 910), &muted);
        if (!result && gallery.Save(L"artifacts/header-logo-options/comparison.png", &png, nullptr) !=
                           Gdiplus::Ok)
            result = 3;
    }
    Gdiplus::GdiplusShutdown(token);
    if (!result)
        puts("PASS: native header branding comparison assembled from exact DX9 captures.");
    return result;
}
