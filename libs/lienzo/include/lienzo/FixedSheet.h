#pragma once

#include "lienzo/SheetStack.h"

#include <cstdint>

namespace lienzo {

// La hoja fija (HU-89, Mesa de animación; mesa "Hoja fija" del canvas): una hoja de la pila,
// por id (0: ninguna), que se ve debajo de todas y no participa del flip. No se mueve de su
// lugar en la pila: el flip, Inicio, Fin, ↑ ↓ y la numeración la saltean. Si no está en la pila
// (se borró; deshacer la trae de vuelta, fija) no cuenta.

// Su posición, o -1.
inline int fixedIndex(const SheetOrder& order, uint64_t fixed)
{
    return fixed ? order.indexOf(fixed) : -1;
}

// La posición a un paso (step = ±1) de `from` salteando la fija, o -1 si no hay (un extremo).
// circle (HU-94, pila en círculo): después de la última viene la primera, y al revés; -1 solo
// si no hay otra hoja que flipee.
inline int flipStep(const SheetOrder& order, uint64_t fixed, int from, int step, bool circle = false)
{
    const int skip = fixedIndex(order, fixed);
    const auto next = [&](int i) {
        i += step;
        return circle && order.count > 0 ? (i % order.count + order.count) % order.count : i;
    };
    int i = next(from);
    if (i == skip)
        i = next(i);
    return order.valid(i) && i != from ? i : -1;
}

// La primera o la última hoja del flip, o -1 si la única es la fija.
inline int flipEdge(const SheetOrder& order, uint64_t fixed, bool last)
{
    return last ? flipStep(order, fixed, order.count, -1) : flipStep(order, fixed, -1, 1);
}

// El número de la hoja en `index` (desde 1, sin contar la fija); 0 si es la fija.
inline int sheetNumber(const SheetOrder& order, uint64_t fixed, int index)
{
    const int f = fixedIndex(order, fixed);
    if (index == f)
        return 0;
    return index + 1 - (f >= 0 && f < index ? 1 : 0);
}

// Cuántas hojas flipean (todas menos la fija).
inline int flipCount(const SheetOrder& order, uint64_t fixed)
{
    return order.count - (fixedIndex(order, fixed) >= 0 ? 1 : 0);
}

} // namespace lienzo
