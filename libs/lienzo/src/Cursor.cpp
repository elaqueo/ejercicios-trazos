#include "lienzo/Cursor.h"

#include <cstring>

namespace lienzo {

namespace {

constexpr uint32_t kInk = 0xFF111111u;  // theme::kTinta
constexpr uint32_t kHalo = 0xFFF5F0E6u; // theme::kHoja
constexpr int kGap = 4;                 // centro libre: radio en píxeles
constexpr int kArm = 7;                 // largo de cada trazo

} // namespace

std::array<uint32_t, kCursorSize * kCursorSize> crossCursorPixels()
{
    std::array<uint32_t, kCursorSize * kCursorSize> px{};
    const auto set = [&](int x, int y, uint32_t c) {
        if (x >= 0 && y >= 0 && x < kCursorSize && y < kCursorSize)
            px[size_t(y) * kCursorSize + size_t(x)] = c;
    };
    const int c = kCursorHotspot;
    // Primero el borde claro (un píxel alrededor de cada trazo), después la tinta encima.
    for (const uint32_t color : {kHalo, kInk}) {
        const int pad = color == kHalo ? 1 : 0;
        for (int d = kGap - pad; d <= kGap + kArm + pad; ++d)
            for (int w = -pad; w <= pad; ++w) {
                set(c + d, c + w, color);
                set(c - d, c + w, color);
                set(c + w, c + d, color);
                set(c + w, c - d, color);
            }
    }
    // Punto central de 1 px, como los trazos: tinta con su borde claro alrededor.
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
            set(c + dx, c + dy, kHalo);
    set(c, c, kInk);
    return px;
}

HCURSOR createCrossCursor()
{
    const auto pixels = crossCursorPixels();
    BITMAPV5HEADER header{};
    header.bV5Size = sizeof(header);
    header.bV5Width = kCursorSize;
    header.bV5Height = -kCursorSize; // de arriba hacia abajo
    header.bV5Planes = 1;
    header.bV5BitCount = 32;
    header.bV5Compression = BI_BITFIELDS;
    header.bV5RedMask = 0x00FF0000;
    header.bV5GreenMask = 0x0000FF00;
    header.bV5BlueMask = 0x000000FF;
    header.bV5AlphaMask = 0xFF000000;
    void* bits = nullptr;
    HDC screen = GetDC(nullptr);
    HBITMAP color = CreateDIBSection(screen, reinterpret_cast<BITMAPINFO*>(&header), DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!color)
        return LoadCursorW(nullptr, IDC_CROSS);
    std::memcpy(bits, pixels.data(), pixels.size() * sizeof(uint32_t));
    HBITMAP mask = CreateBitmap(kCursorSize, kCursorSize, 1, 1, nullptr); // con alfa, la máscara no se usa
    ICONINFO info{};
    info.fIcon = FALSE;
    info.xHotspot = kCursorHotspot;
    info.yHotspot = kCursorHotspot;
    info.hbmMask = mask;
    info.hbmColor = color;
    HCURSOR cursor = CreateIconIndirect(&info);
    DeleteObject(color);
    DeleteObject(mask);
    return cursor ? cursor : LoadCursorW(nullptr, IDC_CROSS);
}

} // namespace lienzo
