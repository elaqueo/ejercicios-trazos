#pragma once

#include "lienzo/SheetMapping.h"

#include <cstdint>

namespace drymedia {
class Paper;
}

namespace lienzo {

// Colores en BGRA de 32 bits (como DXGI_FORMAT_B8G8R8A8_UNORM en memoria).
constexpr uint32_t bgra(int r, int g, int b)
{
    return 0xFF000000u | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}

constexpr uint32_t kPaperColor = bgra(0xF5, 0xF0, 0xE6);    // theme::kHoja
constexpr uint32_t kOutsideColor = bgra(0x30, 0x33, 0x38);  // theme::kFuera
constexpr uint32_t kGraphiteColor = bgra(0x3A, 0x3A, 0x3E); // grafito saturado

// Valor tonal (Fase 1, sin iluminación): la hoja mezclada con el grafito según el
// depósito promedio de las celdas que cubre cada píxel (0 = hoja, 65535 = grafito).
uint32_t toneOf(uint32_t averageDeposit);

// Recalcula los píxeles [px0, px1) × [py0, py1) del cliente en image (stride =
// clientWidth). Fuera de la hoja pinta kOutsideColor.
void renderTone(const drymedia::Paper& paper, const SheetMapping& mapping, int px0, int py0, int px1, int py1,
                uint32_t* image);

} // namespace lienzo
