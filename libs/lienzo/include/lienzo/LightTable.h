#pragma once

#include "lienzo/Renderer.h"
#include "lienzo/SheetStack.h"

#include <cstdint>
#include <vector>

namespace lienzo {

// Mesa de luz (HU-91): las hojas que se ven debajo de la activa, contadas desde ella.
// before[0] es la inmediata anterior y after[0] la siguiente, cada una con su color (BGRA,
// como los de Tone.h; se toma solo el grafito de la hoja) y su opacidad. Vacía = apagada.
struct Tint {
    uint32_t color = 0;
    float opacity = 1;
};
struct LightTable {
    std::vector<Tint> before, after;
};

// Las capas para el render con la pila como está (en los extremos, las que haya), las más
// lejanas primero.
inline std::vector<SheetLayer> lightTableLayers(const SheetOrder& order, const LightTable& table)
{
    std::vector<SheetLayer> layers;
    const auto add = [&](int index, const Tint& tint) {
        if (order.valid(index))
            layers.push_back({order.ids[size_t(index)], tint.color, tint.opacity});
    };
    for (size_t i = table.before.size(); i-- > 0;)
        add(order.active - 1 - int(i), table.before[i]);
    for (size_t i = table.after.size(); i-- > 0;)
        add(order.active + 1 + int(i), table.after[i]);
    return layers;
}

} // namespace lienzo
