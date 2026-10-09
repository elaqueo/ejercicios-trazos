// Ruta escalar: la referencia. Compilada sin /arch, y escrita para que el compilador no
// la vectorice (así mide de verdad lo escalar).
#include "kernel.h"

#include <algorithm>

namespace kernel {

namespace {

inline uint16_t addSat(uint32_t a, uint32_t b)
{
    return static_cast<uint16_t>(std::min<uint32_t>(a + b, 65535u));
}

inline uint16_t subSat(uint32_t a, uint32_t b)
{
    return static_cast<uint16_t>(a > b ? a - b : 0u);
}

void surface(uint16_t* out, const uint16_t* relief, const uint16_t* deposit, int n)
{
#pragma loop(no_vector)
    for (int i = 0; i < n; ++i)
        out[i] = addSat(addSat(relief[i], kBase), deposit[i] >> 4);
}

uint32_t force(const uint16_t* surf, const uint16_t* tip, int n, uint16_t d)
{
    uint32_t sum = 0;
#pragma loop(no_vector)
    for (int i = 0; i < n; ++i)
        sum += subSat(surf[i], addSat(tip[i], d));
    return sum;
}

void deposit(uint16_t* dep, const uint16_t* surf, const uint16_t* tip, int n, uint16_t d, uint16_t k)
{
#pragma loop(no_vector)
    for (int i = 0; i < n; ++i) {
        const uint32_t p = subSat(surf[i], addSat(tip[i], d));
        const uint32_t a = (p * k) >> 16;
        const uint32_t delta = (a * (65535u - dep[i])) >> 16;
        dep[i] = addSat(dep[i], delta);
    }
}

} // namespace

const Impl kScalar{"escalar", surface, force, deposit};

} // namespace kernel
