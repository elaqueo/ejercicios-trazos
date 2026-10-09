// Ruta AVX2: 16 celdas por instrucción. Compilada con /arch:AVX2; solo se usa si la CPU
// lo soporta (Contact::avx2Available).
#include "kernel.h"

#include <immintrin.h>

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

void surface(uint16_t* out, const uint16_t* relief, const uint16_t* deposit, int n, uint16_t base)
{
    const __m256i b = _mm256_set1_epi16(static_cast<short>(base));
    for (int i = 0; i < n; i += 16) {
        const __m256i s = _mm256_adds_epu16(_mm256_adds_epu16(load(relief + i), b), _mm256_srli_epi16(load(deposit + i), 4));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(out + i), s);
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

} // namespace

const Impl kAvx2{surface, force, penetration};

} // namespace drymedia::kernel
