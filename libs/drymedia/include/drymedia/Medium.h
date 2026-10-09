#pragma once

#include <cstdint>

namespace drymedia {

// Parámetros de un medio seco o de la goma. Las minas calibradas viven en la app
// (medios.json de Cartuchera); acá están la HB de referencia y la forma de armarlos.
struct Medium {
    // La mina deposita grafito; la goma lo quita (HU-58) con una punta plana y ancha.
    enum class Kind : uint8_t { Lead, Eraser };

    static constexpr double kReferenceDiameterMm = 0.5;
    static constexpr uint32_t kReferenceForce = 120000;

    const char* name = "HB";
    Kind kind = Kind::Lead;
    int maxStepCells = 1; // largo máximo de cada subpaso del barrido, en celdas
    int reliefShift = 0;  // la punta ve el relieve dividido por 2^reliefShift: una goma
                          // blanda se mete en el diente del papel; la mina, no (0)
    double leadDiameterMm = kReferenceDiameterMm; // diámetro de la mina afilada
    uint16_t coneSlope = 600;            // altura de la punta por celda de distancia al centro
                                         // (unidades de relieve: el relieve va de 0 a ~4095)
    uint32_t forceScale = kReferenceForce; // fuerza buscada con presión 1 (suma de penetraciones)
    uint16_t softness = 20;              // blandura 1..255: depósito por 1/256 de celda deslizada
                                         // (k = distancia × softness, ≤ 65280 por celda)
    uint16_t ceiling = 65535;            // techo: el depósito máximo (negro) que alcanza esta
                                         // mina; las duras tienen más arcilla y quedan en gris

    // HB calibrada con la tableta el 10 de octubre de 2026 (HU-51): mina de 0,87 mm y
    // blandura 20, mirando el tono, el grano y las pasadas repetidas.
    static Medium hb()
    {
        Medium m = Medium{}.withLeadDiameter(0.87);
        m.softness = 20;
        return m;
    }

    // Goma (HU-58): punta plana de `diameterMm` con el borde redondeado; `strength` (1..255)
    // es cuánto quita por distancia, como la blandura de una mina. Subpasos de hasta 4
    // celdas: la huella es de cientos de celdas de ancho y lo que quita ∝ distancia, así
    // que el resultado casi no cambia y cuesta 4 veces menos.
    static Medium eraser(double diameterMm, uint16_t strength)
    {
        Medium m = Medium{}.withLeadDiameter(diameterMm);
        m.name = "goma";
        m.kind = Kind::Eraser;
        m.softness = strength;
        m.maxStepCells = 4;
        m.reliefShift = 2; // ve el diente a un cuarto de su profundidad: con presión llega al fondo
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
