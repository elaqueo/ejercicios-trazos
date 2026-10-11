#pragma once

#include "lienzo/Renderer.h"
#include "lienzo/SheetRole.h"
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
// keysOnly (HU-93): en vez de las vecinas, las keys más cercanas (HU-104), contadas igual: before[0]
// es la key anterior más cercana, after[0] la siguiente. La activa no cuenta aunque sea key.
struct LightTable {
    std::vector<Tint> before, after;
    bool keysOnly = false;
};

// Las capas para el render con la pila como está (en los extremos, las que haya), las más
// lejanas primero.
inline std::vector<SheetLayer> lightTableLayers(const SheetOrder& order, const LightTable& table,
                                                const SheetRoles& roles = {})
{
    // Las posiciones que cuentan hacia cada lado, de la más cercana a la más lejana.
    const auto side = [&](int step, size_t count) {
        std::vector<int> found;
        for (int i = order.active + step; order.valid(i) && found.size() < count; i += step)
            if (!table.keysOnly || roleOf(roles, order.ids[size_t(i)]) == SheetRole::Key)
                found.push_back(i);
        return found;
    };
    const std::vector<int> before = side(-1, table.before.size()), after = side(1, table.after.size());
    std::vector<SheetLayer> layers;
    for (size_t i = before.size(); i-- > 0;)
        layers.push_back({order.ids[size_t(before[i])], table.before[i].color, table.before[i].opacity});
    for (size_t i = after.size(); i-- > 0;)
        layers.push_back({order.ids[size_t(after[i])], table.after[i].color, table.after[i].opacity});
    return layers;
}

} // namespace lienzo
