#pragma once

#include <cstdint>

namespace drymedia {

// Parámetros de un medio seco. En la Fase 1 hay uno solo, la mina HB, con valores fijos;
// los archivos de parámetros recargables en caliente llegan después (plan, "Herramienta").
struct Medium {
    static constexpr double kReferenceDiameterMm = 0.5;
    static constexpr uint32_t kReferenceForce = 120000;

    const char* name = "HB";
    double leadDiameterMm = kReferenceDiameterMm; // diámetro de la mina afilada
    uint16_t coneSlope = 600;            // altura de la punta por celda de distancia al centro
                                         // (unidades de relieve: el relieve va de 0 a ~4095)
    uint32_t forceScale = kReferenceForce; // fuerza buscada con presión 1 (suma de penetraciones)
    uint16_t softness = 20;              // blandura 1..255: depósito por 1/256 de celda deslizada
                                         // (k = distancia × softness, ≤ 65280 por celda)

    // HB calibrada con la tableta el 10 de octubre de 2026 (HU-51): mina de 0,87 mm y
    // blandura 20, mirando el tono, el grano y las pasadas repetidas.
    static Medium hb()
    {
        Medium m = Medium{}.withLeadDiameter(0.87);
        m.softness = 20;
        return m;
    }

    // La misma mina con otro diámetro. La fuerza se escala con el área de la punta: si no,
    // la misma presión repartida en más celdas dejaría un trazo más claro.
    Medium withLeadDiameter(double mm) const
    {
        Medium m = *this;
        m.leadDiameterMm = mm;
        const double ratio = mm / kReferenceDiameterMm;
        m.forceScale = uint32_t(double(kReferenceForce) * ratio * ratio + 0.5);
        return m;
    }
};

} // namespace drymedia
