#pragma once

#include "lienzo/FixedSheet.h"
#include "lienzo/Renderer.h"
#include "lienzo/SheetRole.h"
#include "lienzo/SheetStack.h"

#include <algorithm>
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
// lejanas primero. La hoja fija (HU-89) no cuenta: se saltea. circle (HU-94): da la vuelta, sin
// repetir hojas ni mostrar la activa (las de atrás tienen prioridad).
inline std::vector<SheetLayer> lightTableLayers(const SheetOrder& order, const LightTable& table,
                                                const SheetRoles& roles = {}, uint64_t fixed = 0, bool circle = false)
{
    const int skip = fixedIndex(order, fixed);
    // Las posiciones que cuentan hacia cada lado, de la más cercana a la más lejana.
    const auto side = [&](int step, size_t count, const std::vector<int>& taken) {
        std::vector<int> found;
        int i = order.active;
        for (int walked = 1; walked < order.count && found.size() < count; ++walked) {
            i += step;
            if (circle)
                i = (i % order.count + order.count) % order.count;
            if (!order.valid(i))
                break;
            if (i == skip || std::find(taken.begin(), taken.end(), i) != taken.end())
                continue;
            if (!table.keysOnly || roleOf(roles, order.ids[size_t(i)]) == SheetRole::Key)
                found.push_back(i);
        }
        return found;
    };
    const std::vector<int> before = side(-1, table.before.size(), {});
    const std::vector<int> after = side(1, table.after.size(), before);
    std::vector<SheetLayer> layers;
    for (size_t i = before.size(); i-- > 0;)
        layers.push_back({order.ids[size_t(before[i])], table.before[i].color, table.before[i].opacity});
    for (size_t i = after.size(); i-- > 0;)
        layers.push_back({order.ids[size_t(after[i])], table.after[i].color, table.after[i].opacity});
    return layers;
}

} // namespace lienzo
