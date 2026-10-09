#pragma once

#include "lienzo/SheetMapping.h"

#include <cstdint>
#include <vector>

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

// Valor tonal (Fase 1, sin iluminación): la base (la hoja, o la hoja con la guía encima)
// mezclada con el grafito según el depósito promedio de las celdas que cubre cada píxel
// (0 = base, 65535 = grafito).
uint32_t toneOf(uint32_t averageDeposit, uint32_t base = kPaperColor);

// La hoja con la guía encima: `guide` en BGRA premultiplicado (como
// QImage::Format_ARGB32_Premultiplied en memoria); alfa 0 = solo hoja.
uint32_t paperWithGuide(uint32_t guide);

// Textura del papel (HU-75): luz rasante sobre el mismo relieve al que responde el lápiz.
// Cada píxel promedia las celdas que cubre (~4 por lado) y se muestra solo el detalle (la
// ondulación ancha se resta), así se ve el grano. La pendiente y la altura de cada píxel se
// calculan una vez por mapeo (build); con la dirección de la luz y la intensidad sale un
// factor de brillo por píxel (setLight, barato: se rehace al girar la vista, porque la luz
// está fija al escritorio). El grafito tapa la textura solo: el factor aclara u oscurece la
// hoja, y renderTone mezcla esa hoja con el grafito según el depósito.
class PaperTexture {
public:
    // Brillo por desvío típico del relieve con la intensidad al 100 %.
    static constexpr double kContrast = 0.08;
    // Peso de la altura (crestas más claras, valles más oscuros) frente a la pendiente.
    static constexpr double kHeightWeight = 0.5;
    // Radio en píxeles del promedio que se resta al relieve: más chico, grano más fino.
    static constexpr int kDetailRadius = 2;

    void build(const drymedia::Paper& paper, const SheetMapping& mapping);
    // lightDegrees: hacia dónde está la luz en la imagen sin rotar (0 = derecha, 90 = abajo).
    // intensity de 0 a 1; con 0 no hay textura (shade() da nullptr).
    void setLight(double lightDegrees, double intensity);
    // Factor de brillo por píxel del cliente (4096 = 1), mismo tamaño y stride que la imagen;
    // nullptr si está apagada.
    const uint16_t* shade() const { return m_on ? m_shade.data() : nullptr; }

private:
    SheetMapping m_mapping;
    std::vector<int16_t> m_gx, m_gy, m_height; // por píxel de la hoja, 4096 = un desvío típico
    std::vector<uint16_t> m_shade;
    bool m_on = false;
};

// Recalcula los píxeles [px0, px1) × [py0, py1) del cliente en image (stride =
// clientWidth). Fuera de la hoja pinta kOutsideColor. `guides` (opcional, mismo tamaño y
// stride que image) es la capa de guías de los ejercicios (HU-64): va bajo el grafito.
// `shade` (opcional, PaperTexture::shade) aclara u oscurece la hoja antes del grafito.
void renderTone(const drymedia::Paper& paper, const SheetMapping& mapping, int px0, int py0, int px1, int py1,
                uint32_t* image, const uint32_t* guides = nullptr, const uint16_t* shade = nullptr);

} // namespace lienzo
