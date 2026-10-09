#pragma once

#include "drymedia/Medium.h"

#include <cstdint>
#include <vector>

namespace drymedia {

// La punta como mapa de alturas en coordenadas locales: width() × height() celdas, con el
// vértice (el punto que reporta la tableta) en la esquina de celdas (originX(), originY()).
// 0 es el punto más bajo; kNoContact marca las celdas fuera de la mina, que nunca tocan
// el papel. Las minas verticales usan un mapa de kTipSize centrado; inclinadas y la goma,
// un mapa ajustado a la huella (lados múltiplo de 4, así las celdas son múltiplo de 16).
//
// Mina (HU-59): el cono afilado inclinado de verdad, con el eje según azimut y altitud.
// La huella es asimétrica: empieza en el vértice y crece hacia donde se inclina el lápiz;
// acostado, apoya el costado del cono (sombreado). Con el lápiz vertical es el cono
// simétrico de la Fase 1.
class Tip {
public:
    static constexpr int kTipSize = 48; // minas verticales de hasta ~2 mm de diámetro
    static constexpr int kTipCenter = kTipSize / 2;
    static constexpr int kTipCells = kTipSize * kTipSize;
    static constexpr uint16_t kNoContact = 65535;

    // azimut y altitud en grados, como los entrega tabletinput (la goma los ignora).
    static Tip make(const Medium& medium, float azimuthDeg, float altitudeDeg);
    // La altitud del lápiz para una altitud de la tableta: 90° queda 90°, y la máxima
    // inclinación de la tableta (medium.minTabletAltitudeDeg) es el lápiz acostado.
    static double effectiveAltitude(const Medium& medium, double tabletAltitudeDeg);

    const uint16_t* heights() const { return m_heights.data(); }
    int width() const { return m_width; }
    int height() const { return m_height; }
    int originX() const { return m_originX; }
    int originY() const { return m_originY; }
    int cells() const { return m_width * m_height; }
    int cellsInside() const; // celdas de la mina (huella máxima posible)

private:
    static Tip makeEraser(const Medium& medium);

    int m_width = kTipSize, m_height = kTipSize;
    int m_originX = kTipCenter, m_originY = kTipCenter;
    std::vector<uint16_t> m_heights;
};

} // namespace drymedia
