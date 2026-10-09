#include "lienzo/Tone.h"

#include <drymedia/Paper.h>

#include <algorithm>

namespace lienzo {

namespace {

uint32_t channel(uint32_t color, int shift)
{
    return (color >> shift) & 0xFF;
}

} // namespace

uint32_t toneOf(uint32_t averageDeposit)
{
    const uint32_t g = std::min<uint32_t>(averageDeposit, 65535);
    uint32_t out = 0xFF000000u;
    for (const int shift : {0, 8, 16}) {
        const uint32_t paper = channel(kPaperColor, shift), graphite = channel(kGraphiteColor, shift);
        // paper + (graphite − paper) · g, con redondeo, en enteros.
        const uint32_t v = (paper * (65535 - g) + graphite * g + 32767) / 65535;
        out |= v << shift;
    }
    return out;
}

void renderTone(const drymedia::Paper& paper, const SheetMapping& m, int px0, int py0, int px1, int py1, uint32_t* image)
{
    for (int py = py0; py < py1; ++py) {
        uint32_t* row = image + size_t(py) * size_t(m.clientWidth);
        int cy0 = 0, cy1 = 0;
        m.cellsOfRow(py, cy0, cy1);
        for (int px = px0; px < px1; ++px) {
            if (!m.contains(px, py)) {
                row[px] = kOutsideColor;
                continue;
            }
            int cx0, cx1;
            m.cellsOfColumn(px, cx0, cx1);
            uint64_t sum = 0;
            for (int cy = cy0; cy < cy1; ++cy) {
                int cx = cx0;
                while (cx < cx1) {
                    const int tx = cx / drymedia::kTileSize, ty = cy / drymedia::kTileSize;
                    const int run = std::min(cx1 - cx, drymedia::kTileSize - cx % drymedia::kTileSize);
                    if (const uint16_t* tile = paper.findDepositTile(tx, ty)) {
                        const uint16_t* p = tile + size_t(cy % drymedia::kTileSize) * drymedia::kTileSize +
                                            size_t(cx % drymedia::kTileSize);
                        for (int i = 0; i < run; ++i)
                            sum += p[i];
                    }
                    cx += run;
                }
            }
            const uint64_t cells = uint64_t(cx1 - cx0) * uint64_t(cy1 - cy0);
            row[px] = toneOf(uint32_t(sum / cells));
        }
    }
}

} // namespace lienzo
