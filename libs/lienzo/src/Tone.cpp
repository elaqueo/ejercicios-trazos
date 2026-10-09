#include "lienzo/Tone.h"

#include <drymedia/Paper.h>

#include <algorithm>
#include <cmath>

namespace lienzo {

namespace {

uint32_t channel(uint32_t color, int shift)
{
    return (color >> shift) & 0xFF;
}

// La hoja con más o menos luz (factor 4096 = igual).
uint32_t lit(uint32_t color, uint32_t factor)
{
    uint32_t out = 0xFF000000u;
    for (const int shift : {0, 8, 16})
        out |= std::min<uint32_t>(255, (channel(color, shift) * factor + 2048) >> 12) << shift;
    return out;
}

int16_t normalized(double v, double deviation)
{
    return int16_t(std::clamp(std::lround(v / deviation * 4096.0), -32767L, 32767L));
}

} // namespace

uint32_t toneOf(uint32_t averageDeposit, uint32_t base)
{
    const uint32_t g = std::min<uint32_t>(averageDeposit, 65535);
    uint32_t out = 0xFF000000u;
    for (const int shift : {0, 8, 16}) {
        const uint32_t paper = channel(base, shift), graphite = channel(kGraphiteColor, shift);
        // base + (grafito − base) · g, con redondeo, en enteros.
        const uint32_t v = (paper * (65535 - g) + graphite * g + 32767) / 65535;
        out |= v << shift;
    }
    return out;
}

uint32_t paperWithGuide(uint32_t guide)
{
    const uint32_t alpha = guide >> 24;
    if (alpha == 0)
        return kPaperColor;
    uint32_t out = 0xFF000000u;
    for (const int shift : {0, 8, 16}) // guía premultiplicada + hoja · (1 − alfa)
        out |= ((channel(guide, shift) * 255 + channel(kPaperColor, shift) * (255 - alpha) + 127) / 255) << shift;
    return out;
}

void PaperTexture::build(const drymedia::Paper& paper, const SheetMapping& m)
{
    m_mapping = m;
    const int w = m.sheetWidth, h = m.sheetHeight;
    const size_t n = size_t(std::max(w, 0)) * size_t(std::max(h, 0));
    m_gx.assign(n, 0);
    m_gy.assign(n, 0);
    m_height.assign(n, 0);
    m_shade.assign(size_t(m.clientWidth) * size_t(m.clientHeight), 4096);
    m_on = false;
    if (n == 0)
        return;

    // Relieve medio de cada píxel de la hoja.
    std::vector<float> mean(n);
    const uint16_t* relief = paper.relief();
    double sum = 0;
    for (int y = 0; y < h; ++y) {
        int cy0, cy1;
        m.cellsOfRow(m.sheetY + y, cy0, cy1);
        for (int x = 0; x < w; ++x) {
            int cx0, cx1;
            m.cellsOfColumn(m.sheetX + x, cx0, cx1);
            uint64_t s = 0;
            for (int cy = cy0; cy < cy1; ++cy) {
                const uint16_t* r = relief + size_t(cy) * size_t(paper.width());
                for (int cx = cx0; cx < cx1; ++cx)
                    s += r[cx];
            }
            const float v = float(double(s) / (double(cx1 - cx0) * double(cy1 - cy0)));
            mean[size_t(y) * size_t(w) + size_t(x)] = v;
            sum += v;
        }
    }
    (void)sum;

    // Solo el detalle: el relieve menos su promedio en un cuadrado de (2·kDetailRadius + 1)
    // píxeles de lado. Saca la ondulación ancha y deja el grano más fino (pedido del usuario).
    {
        std::vector<double> table(size_t(w + 1) * size_t(h + 1), 0.0); // sumas acumuladas
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                table[size_t(y + 1) * size_t(w + 1) + size_t(x + 1)] = mean[size_t(y) * size_t(w) + size_t(x)] +
                                                                       table[size_t(y) * size_t(w + 1) + size_t(x + 1)] +
                                                                       table[size_t(y + 1) * size_t(w + 1) + size_t(x)] -
                                                                       table[size_t(y) * size_t(w + 1) + size_t(x)];
        const int r = kDetailRadius;
        for (int y = 0; y < h; ++y) {
            const int y0 = std::max(y - r, 0), y1 = std::min(y + r + 1, h);
            for (int x = 0; x < w; ++x) {
                const int x0 = std::max(x - r, 0), x1 = std::min(x + r + 1, w);
                const double box = table[size_t(y1) * size_t(w + 1) + size_t(x1)] - table[size_t(y0) * size_t(w + 1) + size_t(x1)] -
                                   table[size_t(y1) * size_t(w + 1) + size_t(x0)] + table[size_t(y0) * size_t(w + 1) + size_t(x0)];
                mean[size_t(y) * size_t(w) + size_t(x)] -= float(box / double((x1 - x0) * (y1 - y0)));
            }
        }
    }
    double detailSum = 0;
    for (const float v : mean)
        detailSum += v;
    const double average = detailSum / double(n);

    // Pendiente (diferencias centradas entre píxeles) y altura respecto del promedio,
    // normalizadas por su desvío típico: así la textura se ve igual con cualquier escala.
    std::vector<float> gx(n), gy(n);
    double slope2 = 0, height2 = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const size_t i = size_t(y) * size_t(w) + size_t(x);
            const float* row = mean.data() + size_t(y) * size_t(w);
            const float dx = (row[std::min(x + 1, w - 1)] - row[std::max(x - 1, 0)]) * 0.5f;
            const float dy = (mean[size_t(std::min(y + 1, h - 1)) * size_t(w) + size_t(x)] -
                              mean[size_t(std::max(y - 1, 0)) * size_t(w) + size_t(x)]) * 0.5f;
            gx[i] = dx;
            gy[i] = dy;
            slope2 += double(dx) * dx + double(dy) * dy;
            const double dh = mean[i] - average;
            height2 += dh * dh;
        }
    const double slopeDev = std::max(std::sqrt(slope2 / (2.0 * double(n))), 1e-6);
    const double heightDev = std::max(std::sqrt(height2 / double(n)), 1e-6);
    for (size_t i = 0; i < n; ++i) {
        m_gx[i] = normalized(gx[i], slopeDev);
        m_gy[i] = normalized(gy[i], slopeDev);
        m_height[i] = normalized(mean[i] - average, heightDev);
    }
}

void PaperTexture::setLight(double lightDegrees, double intensity)
{
    m_on = intensity > 0 && !m_gx.empty();
    if (!m_on)
        return;
    const double a = lightDegrees * 3.14159265358979323846 / 180.0;
    // La cara que mira a la luz se aclara: normal ∝ (−gx, −gy, 1), luz hacia (lx, ly).
    const double lx = std::cos(a), ly = std::sin(a);
    const double gain = intensity * kContrast;
    const int w = m_mapping.sheetWidth, h = m_mapping.sheetHeight;
    for (int y = 0; y < h; ++y) {
        uint16_t* out = m_shade.data() + size_t(m_mapping.sheetY + y) * size_t(m_mapping.clientWidth) + size_t(m_mapping.sheetX);
        for (int x = 0; x < w; ++x) {
            const size_t i = size_t(y) * size_t(w) + size_t(x);
            const double t = (-(m_gx[i] * lx + m_gy[i] * ly) + kHeightWeight * m_height[i]) / 4096.0;
            out[x] = uint16_t(std::lround(4096.0 * (1.0 + gain * std::clamp(t, -3.0, 3.0))));
        }
    }
}

void renderTone(const drymedia::Paper& paper, const SheetMapping& m, int px0, int py0, int px1, int py1, uint32_t* image,
                const uint32_t* guides, const uint16_t* shade)
{
    for (int py = py0; py < py1; ++py) {
        uint32_t* row = image + size_t(py) * size_t(m.clientWidth);
        const uint32_t* guideRow = guides ? guides + size_t(py) * size_t(m.clientWidth) : nullptr;
        const uint16_t* shadeRow = shade ? shade + size_t(py) * size_t(m.clientWidth) : nullptr;
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
            uint32_t base = guideRow ? paperWithGuide(guideRow[px]) : kPaperColor;
            if (shadeRow)
                base = lit(base, shadeRow[px]);
            row[px] = toneOf(uint32_t(sum / cells), base);
        }
    }
}

} // namespace lienzo
