#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace drymedia {

// Resolución de referencia: 600 dpi (celda de ~42 µm), la de Cartuchera y Ejercicios. El
// papel puede tener otra (HU-112, Mesa de animación: 300 dpi); ver Paper::cellsPerMm().
constexpr double kCellsPerMm = 600.0 / 25.4;

// Parámetros de un medio seco o de la goma. Las minas calibradas viven en la app
// (medios.json de Cartuchera); acá están la HB de referencia y la forma de armarlos.
struct Medium {
    // La mina deposita grafito; la goma lo quita (HU-58) con una punta plana y ancha; el
    // lápiz de color (HU-99) deposita en el plano de color, no en el del grafito.
    enum class Kind : uint8_t { Lead, Eraser, Color };

    static constexpr double kReferenceDiameterMm = 0.5;
    static constexpr uint32_t kReferenceForce = 120000;

    const char* name = "HB";
    Kind kind = Kind::Lead;
    uint8_t color = 0; // lápiz de color: cuál (0..kColorCount − 1)
    int8_t eraseColor = -1; // goma (HU-99): -1 borra solo el grafito; 0..3, solo ese color
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
    // Forma de la punta afilada (HU-59): el cono de madera y mina tiene este semiángulo; la
    // mina es la parte del cono dentro de su radio. Y la inclinación de la tableta: a esta
    // altitud (la máxima inclinación que reporta; la Intuos4 llega a ~30°) el lápiz se toma
    // como acostado, con el costado del cono sobre el papel.
    double coneHalfAngleDeg = 12.0;
    // Bruñido (HU-60): cuánto aplasta la capa de grafito por celda deslizada (0 = nada), con
    // presión 1. Va con presión³: un trazo normal casi no bruñe; apretando, sí (la penetración
    // casi no crece con la presión: crece el área). Las minas duras bruñen más.
    uint16_t burnishRate = 0;
    // Deformación (HU-61): cuánto se hunde el papel por exceso de penetración sobre el umbral
    // de Contact (cualquier punta que apriete de más). Daño de fibra: solo la goma, con
    // presión³, por celda deslizada.
    uint16_t deformRate = 3000;
    uint16_t damageRate = 0;
    // Desgaste (HU-62): cada punto de la mina pierde material ∝ penetración × k (lo que
    // deposita) × blandura × wearRate. La blandura entra dos veces: las blandas tienen menos
    // arcilla que las sostenga y se gastan mucho más que lo que depositan de más (y las duras,
    // de cono chico, si no se gastaban casi como ellas). 0 = no se gasta.
    uint16_t wearRate = 0;
    double minTabletAltitudeDeg = 30.0;
    // La resolución del papel para la que están los valores por celda (HU-112).
    double cellsPerMm = kCellsPerMm;

    // La misma mina en un papel de otra resolución (HU-112): lo que va por celda se
    // convierte para que la línea tenga el mismo tamaño y el mismo tono físicos. Con celdas
    // s veces más grandes, el cono sube s veces más por celda, la fuerza (suma de
    // penetraciones) se reparte en s² veces menos celdas y cada celda deslizada vale s veces
    // más distancia. En la misma resolución no cambia nada (Cartuchera: mismos hashes).
    Medium atResolution(double paperCellsPerMm) const
    {
        if (paperCellsPerMm == cellsPerMm)
            return *this;
        Medium m = *this;
        const double s = cellsPerMm / paperCellsPerMm;
        const auto u16 = [](double v) { return uint16_t(std::clamp(std::round(v), 0.0, 65535.0)); };
        m.coneSlope = u16(coneSlope * s);
        m.forceScale = uint32_t(std::max(1.0, std::round(forceScale / (s * s))));
        m.softness = u16(softness * s);
        m.burnishRate = u16(burnishRate * s);
        m.damageRate = u16(damageRate * s);
        m.maxStepCells = std::max(1, int(std::lround(maxStepCells / s)));
        m.cellsPerMm = paperCellsPerMm;
        return m;
    }

    // HB calibrada con la tableta el 10 de octubre de 2026 (HU-51): mina de 0,87 mm y
    // blandura 20, mirando el tono, el grano y las pasadas repetidas.
    static Medium hb()
    {
        Medium m = Medium{}.withLeadDiameter(0.87);
        m.softness = 20;
        m.burnishRate = 3000; // HU-60, a calibrar con la tableta
        m.wearRate = 6;       // HU-62, a calibrar con la tableta
        return m;
    }

    // Lápiz de color (HU-99), como un col-erase o un lápiz de color de animación: mina de cera
    // de 0,7 mm, más tenue que la HB (techo más bajo) y que deposita algo menos por pasada. La
    // goma (con ese color elegido) lo borra 4 veces más despacio que al grafito.
    static Medium colored(int color)
    {
        Medium m = Medium{}.withLeadDiameter(0.7);
        m.name = "color";
        m.kind = Kind::Color;
        m.color = uint8_t(color & 3);
        m.softness = 16;
        m.ceiling = 44000;
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
        m.deformRate = 0;  // blanda: no hunde el papel
        m.damageRate = 3000;
        return m;
    }

    // Punta seca (HU-61): un estilete fino que no deposita, para marcar surcos que quedan
    // blancos al sombrear encima. La fuerza no se escala con su diámetro: la misma mano sobre
    // una punta más fina aprieta más el papel.
    static Medium stylus()
    {
        Medium m = hb();
        m.name = "punta seca";
        m.leadDiameterMm = 0.5;
        m.softness = 0;
        m.wearRate = 0; // de acero
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
