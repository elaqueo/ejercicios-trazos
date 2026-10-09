// Ruta AVX-512 (F + BW): 32 celdas por instrucción. Compilada con /arch:AVX512; solo
// se llama si la CPU y el sistema lo soportan.
#include "kernel.h"

#include <immintrin.h>

namespace kernel {

namespace {

inline __m512i penetration(__m512i surf, __m512i tip, __m512i d)
{
    return _mm512_subs_epu16(surf, _mm512_adds_epu16(tip, d));
}

void surface(uint16_t* out, const uint16_t* relief, const uint16_t* deposit, int n)
{
    const __m512i base = _mm512_set1_epi16(static_cast<short>(kBase));
    for (int i = 0; i < n; i += 32) {
        const __m512i r = _mm512_loadu_si512(relief + i);
        const __m512i dep = _mm512_loadu_si512(deposit + i);
        _mm512_storeu_si512(out + i, _mm512_adds_epu16(_mm512_adds_epu16(r, base), _mm512_srli_epi16(dep, 4)));
    }
}

uint32_t force(const uint16_t* surf, const uint16_t* tip, int n, uint16_t d)
{
    const __m512i dv = _mm512_set1_epi16(static_cast<short>(d));
    __m512i acc = _mm512_setzero_si512(); // 16 sumas de 32 bits
    for (int i = 0; i < n; i += 32) {
        const __m512i p = penetration(_mm512_loadu_si512(surf + i), _mm512_loadu_si512(tip + i), dv);
        acc = _mm512_add_epi32(acc, _mm512_cvtepu16_epi32(_mm512_castsi512_si256(p)));
        acc = _mm512_add_epi32(acc, _mm512_cvtepu16_epi32(_mm512_extracti64x4_epi64(p, 1)));
    }
    return static_cast<uint32_t>(_mm512_reduce_add_epi32(acc));
}

void deposit(uint16_t* dep, const uint16_t* surf, const uint16_t* tip, int n, uint16_t d, uint16_t k)
{
    const __m512i dv = _mm512_set1_epi16(static_cast<short>(d));
    const __m512i kv = _mm512_set1_epi16(static_cast<short>(k));
    const __m512i full = _mm512_set1_epi16(-1);
    for (int i = 0; i < n; i += 32) {
        const __m512i dcur = _mm512_loadu_si512(dep + i);
        const __m512i p = penetration(_mm512_loadu_si512(surf + i), _mm512_loadu_si512(tip + i), dv);
        const __m512i a = _mm512_mulhi_epu16(p, kv);
        const __m512i delta = _mm512_mulhi_epu16(a, _mm512_sub_epi16(full, dcur));
        _mm512_storeu_si512(dep + i, _mm512_adds_epu16(dcur, delta));
    }
}

} // namespace

const Impl kAvx512{"AVX-512", surface, force, deposit};

} // namespace kernel
