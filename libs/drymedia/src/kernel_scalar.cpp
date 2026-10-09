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

inline uint32_t contribution(uint32_t p, uint32_t k)
{
    return std::min<uint32_t>((p * k) >> 12, 65535u);
}

void surface(uint16_t* out, const uint16_t* relief, const uint16_t* crest, const uint16_t* deposit,
             const uint16_t* burnish, int n, uint16_t base, int shift)
{
    for (int i = 0; i < n; ++i) {
        const uint32_t top = crest[i] >> shift, level = subSat(top, kHalfGrain >> shift);
        uint32_t r = relief[i] >> shift;
        r -= (uint32_t(subSat(r, level)) * burnish[i]) >> 16;
        out[i] = addSat(addSat(r, base), (uint32_t(subSat(top, r)) * deposit[i]) >> 16);
    }
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

void deposit(uint16_t* dep, const uint16_t* burn, const uint16_t* pen, int n, uint16_t k, uint16_t ceiling)
{
    for (int i = 0; i < n; ++i) {
        uint32_t a = contribution(pen[i], k);
        a -= (a * burn[i]) >> 16;
        const uint32_t delta = (a * subSat(ceiling, dep[i])) >> 16;
        dep[i] = addSat(dep[i], delta);
    }
}

void erase(uint16_t* dep, const uint16_t* burn, const uint16_t* pen, int n, uint16_t k)
{
    for (int i = 0; i < n; ++i) {
        uint32_t a = contribution(pen[i], k);
        a -= (a * (burn[i] >> 1u)) >> 16;
        const uint32_t delta = ((a * dep[i]) >> 16) + (a > 0 ? 1u : 0u);
        dep[i] = subSat(dep[i], delta);
    }
}

void burnish(uint16_t* dep, uint16_t* burn, const uint16_t* pen, int n, uint16_t kb, uint16_t target)
{
    for (int i = 0; i < n; ++i) {
        const uint32_t h = contribution(pen[i], kb);
        const uint32_t d = dep[i];
        burn[i] = addSat(burn[i], (h * d) >> 16);
        dep[i] = uint16_t(d + ((uint32_t(subSat(target, d)) * h) >> 16));
    }
}

uint32_t contactDeposit(const uint16_t* dep, const uint16_t* pen, int n, uint32_t* count)
{
    uint32_t sum = 0, cells = 0;
    for (int i = 0; i < n; ++i)
        if (pen[i] > 0) {
            sum += dep[i];
            ++cells;
        }
    *count = cells;
    return sum;
}

} // namespace

const Impl kScalar{surface, force, penetration, deposit, erase, burnish, contactDeposit};

} // namespace drymedia::kernel
