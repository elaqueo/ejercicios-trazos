// Ruta AVX2: 16 celdas por instrucción. Compilada con /arch:AVX2; solo se llama si la
// CPU lo soporta.
#include "kernel.h"

#include <immintrin.h>

namespace kernel {

namespace {

inline __m256i penetration(__m256i surf, __m256i tip, __m256i d)
{
    return _mm256_subs_epu16(surf, _mm256_adds_epu16(tip, d));
}

void surface(uint16_t* out, const uint16_t* relief, const uint16_t* deposit, int n)
{
    const __m256i base = _mm256_set1_epi16(static_cast<short>(kBase));
    for (int i = 0; i < n; i += 16) {
        const __m256i r = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(relief + i));
        const __m256i dep = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(deposit + i));
        const __m256i s = _mm256_adds_epu16(_mm256_adds_epu16(r, base), _mm256_srli_epi16(dep, 4));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(out + i), s);
    }
}

uint32_t force(const uint16_t* surf, const uint16_t* tip, int n, uint16_t d)
{
    const __m256i dv = _mm256_set1_epi16(static_cast<short>(d));
    const __m256i zero = _mm256_setzero_si256();
    __m256i acc = zero; // 8 sumas de 32 bits
    for (int i = 0; i < n; i += 16) {
        const __m256i p = penetration(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(surf + i)),
                                      _mm256_loadu_si256(reinterpret_cast<const __m256i*>(tip + i)), dv);
        acc = _mm256_add_epi32(acc, _mm256_unpacklo_epi16(p, zero));
        acc = _mm256_add_epi32(acc, _mm256_unpackhi_epi16(p, zero));
    }
    __m128i s = _mm_add_epi32(_mm256_castsi256_si128(acc), _mm256_extracti128_si256(acc, 1));
    s = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(1, 0, 3, 2)));
    s = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(2, 3, 0, 1)));
    return static_cast<uint32_t>(_mm_cvtsi128_si32(s));
}

void deposit(uint16_t* dep, const uint16_t* surf, const uint16_t* tip, int n, uint16_t d, uint16_t k)
{
    const __m256i dv = _mm256_set1_epi16(static_cast<short>(d));
    const __m256i kv = _mm256_set1_epi16(static_cast<short>(k));
    const __m256i full = _mm256_set1_epi16(-1); // 65535
    for (int i = 0; i < n; i += 16) {
        __m256i* dp = reinterpret_cast<__m256i*>(dep + i);
        const __m256i dcur = _mm256_loadu_si256(dp);
        const __m256i p = penetration(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(surf + i)),
                                      _mm256_loadu_si256(reinterpret_cast<const __m256i*>(tip + i)), dv);
        const __m256i a = _mm256_mulhi_epu16(p, kv);                                  // (p·k) >> 16
        const __m256i delta = _mm256_mulhi_epu16(a, _mm256_sub_epi16(full, dcur));    // (a·(65535−dep)) >> 16
        _mm256_storeu_si256(dp, _mm256_adds_epu16(dcur, delta));
    }
}

} // namespace

const Impl kAvx2{"AVX2", surface, force, deposit};

} // namespace kernel
