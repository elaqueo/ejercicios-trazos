#pragma once

#include "drymedia/Medium.h"

#include <cstdint>
#include <vector>

namespace drymedia {

// La punta como mapa de alturas en coordenadas locales: size() × size() celdas, centrada
// en (center(), center()). 0 es el punto más bajo; kNoContact marca las celdas fuera de
// la mina, que nunca tocan el papel. Las minas usan kTipSize; la goma, un mapa más grande
// según su diámetro (siempre múltiplo de 4, así las celdas son múltiplo de 16).
//
// Fase 1: cono de la mina afilada, estirado en la dirección del azimut por 1/sen(altitud)
// para que al acostar el lápiz la huella se alargue hacia donde apunta. Es una
// aproximación simétrica; la forma exacta del cono inclinado y el desgaste son de la
// Fase 2.
class Tip {
public:
    static constexpr int kTipSize = 48; // minas: hasta ~2 mm de diámetro con el lápiz vertical
    static constexpr int kTipCenter = kTipSize / 2;
    static constexpr int kTipCells = kTipSize * kTipSize;
    static constexpr uint16_t kNoContact = 65535;

    // azimut y altitud en grados, como los entrega tabletinput (la goma los ignora).
    static Tip make(const Medium& medium, float azimuthDeg, float altitudeDeg);

    const uint16_t* heights() const { return m_heights.data(); }
    int size() const { return m_size; }
    int center() const { return m_size / 2; }
    int cells() const { return m_size * m_size; }
    int cellsInside() const; // celdas de la mina (huella máxima posible)

private:
    static Tip makeEraser(const Medium& medium);

    int m_size = kTipSize;
    std::vector<uint16_t> m_heights;
};

} // namespace drymedia
