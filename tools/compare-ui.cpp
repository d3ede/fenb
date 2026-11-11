#include <windows.h>
#include <objidl.h>
#include <wtypes.h>

#include <gdiplus.h>
#include <cstdio>
#include <vector>
int main() {
    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok)
        return 1;
    int result = 0;
    {
        Gdiplus::Bitmap oldImage(L"artifacts/ui-comparison/v1/overview.png"),
            newImage(L"artifacts/ui-comparison/v2/overview.png");
        Gdiplus::Bitmap canvas(1472, 786, PixelFormat32bppRGB);
        Gdiplus::Graphics g(&canvas);
        g.Clear(Gdiplus::Color(255, 19, 20, 23));
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
        Gdiplus::Font label(L"Arial", 18, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::Font note(L"Arial", 14, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush white(Gdiplus::Color(255, 224, 226, 230)),
            muted(Gdiplus::Color(255, 157, 161, 170));
        g.DrawString(L"Версия 1 · 860 × 660", -1, &label, Gdiplus::PointF(16, 18), &white);
        g.DrawString(L"Версия 2 · 540 × 520", -1, &label, Gdiplus::PointF(908, 18), &white);
        // Both panels are exact crops from real DX9 frames, drawn at 1:1 without resizing.
        g.DrawImage(&oldImage, Gdiplus::Rect(16, 56, 860, 660), 150, 70, 860, 660, Gdiplus::UnitPixel);
        g.DrawImage(&newImage, Gdiplus::Rect(908, 56, 540, 520), 310, 140, 540, 520, Gdiplus::UnitPixel);
        g.DrawString(L"3 настройки на первом экране. Остальные — в разделах.", -1, &note,
                     Gdiplus::PointF(16, 735), &muted);
        g.DrawString(L"12 параметров и качество на одном экране.", -1, &note, Gdiplus::PointF(908, 600),
                     &white);
        g.DrawString(L"Площадь панели меньше на 50,5%.", -1, &note, Gdiplus::PointF(908, 627), &muted);
        g.DrawString(L"Inter · нейтральная палитра · автосохранение", -1, &note, Gdiplus::PointF(908, 654),
                     &muted);
        g.DrawString(L"Снимки реального DX9-рендера. Масштаб обеих панелей — 1:1.", -1, &note,
                     Gdiplus::PointF(16, 760), &muted);
        UINT count = 0, size = 0;
        Gdiplus::GetImageEncodersSize(&count, &size);
        std::vector<unsigned char> buffer(size);
        auto *enc = reinterpret_cast<Gdiplus::ImageCodecInfo *>(buffer.data());
        Gdiplus::GetImageEncoders(count, size, enc);
        CLSID png{};
        for (UINT i = 0; i < count; ++i)
            if (wcscmp(enc[i].MimeType, L"image/png") == 0)
                png = enc[i].Clsid;
        if (canvas.Save(L"artifacts/ui-comparison/before-after.png", &png, nullptr) != Gdiplus::Ok)
            result = 2;
    }
    Gdiplus::GdiplusShutdown(token);
    return result;
}
