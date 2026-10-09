// Ruta AVX2: 16 celdas por instrucción. Compilada con /arch:AVX2; solo se usa si la CPU
// lo soporta (Contact::avx2Available).
#include "kernel.h"

#include <immintrin.h>
#include <intrin.h>

namespace drymedia::kernel {

namespace {

inline __m256i load(const uint16_t* p)
{
    return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));
}

inline __m256i penetrationOf(__m256i surf, __m256i tip, __m256i d)
{
    return _mm256_subs_epu16(surf, _mm256_adds_epu16(tip, d));
}

// a = min((p·k) >> 12, 65535): el producto de 32 bits armado con su mitad alta (mulhi) y
// baja (mullo); si la alta pasa de 4095, el resultado no entra en 16 bits.
inline __m256i contribution(__m256i p, __m256i kv)
{
    const __m256i hi = _mm256_mulhi_epu16(p, kv), lo = _mm256_mullo_epi16(p, kv);
    const __m256i shifted = _mm256_or_si256(_mm256_slli_epi16(hi, 4), _mm256_srli_epi16(lo, 12));
    const __m256i fits = _mm256_cmpeq_epi16(_mm256_min_epu16(hi, _mm256_set1_epi16(4095)), hi);
    return _mm256_blendv_epi8(_mm256_set1_epi16(-1), shifted, fits);
}

void surface(uint16_t* out, const uint16_t* relief, const uint16_t* crest, const uint16_t* deposit,
             const uint16_t* burnish, const uint16_t* deform, const uint16_t* damage, int n, uint16_t base, int shift)
{
    const __m256i b = _mm256_set1_epi16(static_cast<short>(base));
    const __m256i half = _mm256_set1_epi16(static_cast<short>(kHalfGrain >> shift));
    const __m128i sh = _mm_cvtsi32_si128(shift);
    for (int i = 0; i < n; i += 16) {
        const __m256i top = _mm256_srl_epi16(load(crest + i), sh);
        const __m256i level = _mm256_subs_epu16(top, half);
        const __m256i r0 = _mm256_srl_epi16(load(relief + i), sh);
        const __m256i dmg = load(damage + i);
        const __m256i up = _mm256_mulhi_epu16(_mm256_subs_epu16(r0, level), dmg);
        const __m256i down = _mm256_mulhi_epu16(_mm256_subs_epu16(level, r0), dmg);
        __m256i r = _mm256_subs_epu16(_mm256_adds_epu16(r0, up), down);
        r = _mm256_sub_epi16(r, _mm256_mulhi_epu16(_mm256_subs_epu16(r, level), load(burnish + i)));
        const __m256i fill = _mm256_mulhi_epu16(_mm256_subs_epu16(top, r), load(deposit + i));
        const __m256i s = _mm256_adds_epu16(_mm256_adds_epu16(r, b), fill);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(out + i), _mm256_subs_epu16(s, _mm256_srl_epi16(load(deform + i), sh)));
    }
}

uint32_t force(const uint16_t* surf, const uint16_t* tip, int n, uint16_t d)
{
    const __m256i dv = _mm256_set1_epi16(static_cast<short>(d));
    const __m256i zero = _mm256_setzero_si256();
    __m256i acc = zero;
    for (int i = 0; i < n; i += 16) {
        const __m256i p = penetrationOf(load(surf + i), load(tip + i), dv);
        acc = _mm256_add_epi32(acc, _mm256_unpacklo_epi16(p, zero));
        acc = _mm256_add_epi32(acc, _mm256_unpackhi_epi16(p, zero));
    }
    __m128i s = _mm_add_epi32(_mm256_castsi256_si128(acc), _mm256_extracti128_si256(acc, 1));
    s = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(1, 0, 3, 2)));
    s = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(2, 3, 0, 1)));
    return static_cast<uint32_t>(_mm_cvtsi128_si32(s));
}

void penetration(uint16_t* out, const uint16_t* surf, const uint16_t* tip, int n, uint16_t d)
{
    const __m256i dv = _mm256_set1_epi16(static_cast<short>(d));
    for (int i = 0; i < n; i += 16)
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(out + i), penetrationOf(load(surf + i), load(tip + i), dv));
}

void deposit(uint16_t* dep, const uint16_t* burn, const uint16_t* damage, const uint16_t* pen, int n, uint16_t k,
             uint16_t ceiling)
{
    const __m256i kv = _mm256_set1_epi16(static_cast<short>(k));
    const __m256i top = _mm256_set1_epi16(static_cast<short>(ceiling));
    for (int i = 0; i < n; i += 16) {
        __m256i* dp = reinterpret_cast<__m256i*>(dep + i);
        const __m256i dcur = _mm256_loadu_si256(dp);
        __m256i a = contribution(load(pen + i), kv);
        a = _mm256_adds_epu16(a, _mm256_mulhi_epu16(a, _mm256_srli_epi16(load(damage + i), 1)));
        a = _mm256_sub_epi16(a, _mm256_mulhi_epu16(a, load(burn + i)));
        const __m256i delta = _mm256_mulhi_epu16(a, _mm256_subs_epu16(top, dcur)); // (a·sat0(techo−dep)) >> 16
        _mm256_storeu_si256(dp, _mm256_adds_epu16(dcur, delta));
    }
}

void erase(uint16_t* dep, const uint16_t* burn, const uint16_t* pen, int n, uint16_t k)
{
    const __m256i kv = _mm256_set1_epi16(static_cast<short>(k));
    const __m256i zero = _mm256_setzero_si256(), one = _mm256_set1_epi16(1);
    for (int i = 0; i < n; i += 16) {
        __m256i* dp = reinterpret_cast<__m256i*>(dep + i);
        const __m256i dcur = _mm256_loadu_si256(dp);
        __m256i a = contribution(load(pen + i), kv);
        a = _mm256_sub_epi16(a, _mm256_mulhi_epu16(a, _mm256_srli_epi16(load(burn + i), 1)));
        const __m256i touched = _mm256_andnot_si256(_mm256_cmpeq_epi16(a, zero), one); // a > 0 ? 1 : 0
        const __m256i delta = _mm256_adds_epu16(_mm256_mulhi_epu16(a, dcur), touched);
        _mm256_storeu_si256(dp, _mm256_subs_epu16(dcur, delta));
    }
}

bool burnish(uint16_t* dep, uint16_t* burn, const uint16_t* pen, int n, uint16_t kb, uint16_t target)
{
    __m256i changed = _mm256_setzero_si256();
    const __m256i kbv = _mm256_set1_epi16(static_cast<short>(kb));
    const __m256i tv = _mm256_set1_epi16(static_cast<short>(target));
    for (int i = 0; i < n; i += 16) {
        const __m256i h = contribution(load(pen + i), kbv);
        __m256i* dp = reinterpret_cast<__m256i*>(dep + i);
        __m256i* bp = reinterpret_cast<__m256i*>(burn + i);
        const __m256i d = _mm256_loadu_si256(dp);
        const __m256i db = _mm256_mulhi_epu16(h, d);
        changed = _mm256_or_si256(changed, db);
        _mm256_storeu_si256(bp, _mm256_adds_epu16(_mm256_loadu_si256(bp), db));
        _mm256_storeu_si256(dp, _mm256_add_epi16(d, _mm256_mulhi_epu16(_mm256_subs_epu16(tv, d), h)));
    }
    return !_mm256_testz_si256(changed, changed);
}

void grow(uint16_t* field, const uint16_t* pen, int n, uint16_t threshold, uint16_t rate, uint16_t cap)
{
    const __m256i tv = _mm256_set1_epi16(static_cast<short>(threshold));
    const __m256i rv = _mm256_set1_epi16(static_cast<short>(rate));
    const __m256i cv = _mm256_set1_epi16(static_cast<short>(cap));
    for (int i = 0; i < n; i += 16) {
        __m256i* fp = reinterpret_cast<__m256i*>(field + i);
        const __m256i add = contribution(_mm256_subs_epu16(load(pen + i), tv), rv);
        _mm256_storeu_si256(fp, _mm256_min_epu16(_mm256_adds_epu16(_mm256_loadu_si256(fp), add), cv));
    }
}

int contactIndices(const uint16_t* pen, int n, uint32_t* out)
{
    const __m256i zero = _mm256_setzero_si256();
    int count = 0;
    for (int i = 0; i < n; i += 16) {
        // Dos bits por celda de 16 bits: se toma uno de cada par.
        uint32_t m = ~uint32_t(_mm256_movemask_epi8(_mm256_cmpeq_epi16(load(pen + i), zero))) & 0x55555555u;
        while (m) {
            unsigned long bit;
            _BitScanForward(&bit, m);
            out[count++] = uint32_t(i) + bit / 2;
            m &= m - 1;
        }
    }
    return count;
}

uint32_t contactDeposit(const uint16_t* dep, const uint16_t* pen, int n, uint32_t* count)
{
    const __m256i zero = _mm256_setzero_si256();
    __m256i acc = zero;
    uint32_t cells = 0;
    for (int i = 0; i < n; i += 16) {
        const __m256i none = _mm256_cmpeq_epi16(load(pen + i), zero);
        const __m256i d = _mm256_andnot_si256(none, load(dep + i));
        acc = _mm256_add_epi32(acc, _mm256_unpacklo_epi16(d, zero));
        acc = _mm256_add_epi32(acc, _mm256_unpackhi_epi16(d, zero));
        cells += uint32_t(__popcnt(~uint32_t(_mm256_movemask_epi8(none)))) / 2;
    }
    __m128i s = _mm_add_epi32(_mm256_castsi256_si128(acc), _mm256_extracti128_si256(acc, 1));
    s = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(1, 0, 3, 2)));
    s = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(2, 3, 0, 1)));
    *count = cells;
    return static_cast<uint32_t>(_mm_cvtsi128_si32(s));
}

} // namespace

const Impl kAvx2{surface, force, penetration, deposit, erase, burnish, grow, contactIndices, contactDeposit};

} // namespace drymedia::kernel
