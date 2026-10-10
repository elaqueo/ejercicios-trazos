#pragma once

#include <windows.h>

#include <cstdint>
#include <mutex>
#include <vector>

namespace lienzo {

// La imagen de una hoja que dejó de ser la activa (HU-83): solo la región de la hoja, para
// que el render la suba a la textura de esa hoja.
struct RetiredSheet {
    uint64_t sheet = 0; // id (SheetOrder)
    int width = 0, height = 0;
    std::vector<uint32_t> pixels;
};

// Imagen de pantalla (BGRA, tamaño del cliente) que escribe la simulación y lee el
// render. Lleva la unión de lo que cambió desde la última vez que el render la tomó y
// el momento de la muestra más nueva que ya está dibujada (para medir latencia).
struct DisplayImage {
    std::mutex mutex;
    int width = 0, height = 0;
    std::vector<uint32_t> pixels;
    RECT dirty{};
    bool hasDirty = false;
    int64_t newestSampleUs = 0; // tiempo (µs de QPC) de la muestra más nueva dibujada
    uint64_t sampleVersion = 0; // sube cada vez que entra una muestra nueva
    std::vector<RetiredSheet> retired; // hojas que salieron, para el render (HU-83)

    void markDirty(int x0, int y0, int x1, int y1)
    {
        if (!hasDirty) {
            dirty = {x0, y0, x1, y1};
            hasDirty = true;
            return;
        }
        dirty.left = std::min<LONG>(dirty.left, x0);
        dirty.top = std::min<LONG>(dirty.top, y0);
        dirty.right = std::max<LONG>(dirty.right, x1);
        dirty.bottom = std::max<LONG>(dirty.bottom, y1);
    }
};

} // namespace lienzo
