#pragma once

#include <cstdint>

namespace drymedia {

// Parámetros de un medio seco. En la Fase 1 hay uno solo, la mina HB, con valores fijos;
// los archivos de parámetros recargables en caliente llegan después (plan, "Herramienta").
struct Medium {
    const char* name = "HB";
    double leadDiameterMm = 0.5;  // diámetro de la mina afilada
    uint16_t coneSlope = 600;     // altura de la punta por celda de distancia al centro
                                  // (unidades de relieve: el relieve va de 0 a ~4095)
    uint32_t forceScale = 120000; // fuerza buscada con presión 1 (suma de penetraciones)
    uint16_t softness = 16;       // blandura: depósito por 1/256 de celda deslizada
                                  // (k = distancia × softness, ≤ 4096 por celda)

    static Medium hb() { return {}; }
};

} // namespace drymedia
