// Ruta escalar: la referencia, y la que se usa si la CPU no tiene AVX2.
#include "kernel.h"

#include <algorithm>

namespace drymedia::kernel {

namespace {

inline uint16_t addSat(uint32_t a, uint32_t b)
{
    return static_cast<uint16_t>(std::min<uint32_t>(a + b, 65535u));
}

inline uint16_t subSat(uint32_t a, uint32_t b)
{
    return static_cast<uint16_t>(a > b ? a - b : 0u);
}

void surface(uint16_t* out, const uint16_t* relief, const uint16_t* deposit, int n, uint16_t base)
{
    for (int i = 0; i < n; ++i)
        out[i] = addSat(addSat(relief[i], base), deposit[i] >> 4);
}

uint32_t force(const uint16_t* surf, const uint16_t* tip, int n, uint16_t d)
{
    uint32_t sum = 0;
    for (int i = 0; i < n; ++i)
        sum += subSat(surf[i], addSat(tip[i], d));
    return sum;
}

void penetration(uint16_t* out, const uint16_t* surf, const uint16_t* tip, int n, uint16_t d)
{
    for (int i = 0; i < n; ++i)
        out[i] = subSat(surf[i], addSat(tip[i], d));
}

void deposit(uint16_t* dep, const uint16_t* pen, int n, uint16_t k, uint16_t ceiling)
{
    for (int i = 0; i < n; ++i) {
        const uint32_t a = std::min<uint32_t>((uint32_t(pen[i]) * k) >> 12, 65535u);
        const uint32_t delta = (a * subSat(ceiling, dep[i])) >> 16;
        dep[i] = addSat(dep[i], delta);
    }
}

} // namespace

const Impl kScalar{surface, force, penetration, deposit};

} // namespace drymedia::kernel
