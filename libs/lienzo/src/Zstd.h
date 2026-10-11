#pragma once

// zstd para la toma (HU-85): nivel 1, el mejor equilibrio medido (docs/mesa-de-animacion/
// spikes/toma.md), con la suma de verificación del formato para reconocer un archivo dañado.

#include <zstd.h>

#include <cstdint>
#include <vector>

namespace lienzo::zstd {

inline std::vector<uint8_t> compress(const void* data, size_t size)
{
    std::vector<uint8_t> out(ZSTD_compressBound(size));
    ZSTD_CCtx* context = ZSTD_createCCtx();
    ZSTD_CCtx_setParameter(context, ZSTD_c_compressionLevel, 1);
    ZSTD_CCtx_setParameter(context, ZSTD_c_checksumFlag, 1);
    const size_t n = ZSTD_compress2(context, out.data(), out.size(), data, size);
    ZSTD_freeCCtx(context);
    out.resize(ZSTD_isError(n) ? 0 : n);
    return out;
}

// false si los datos están dañados o no son exactamente `size` bytes.
inline bool decompress(const uint8_t* data, size_t packedSize, void* out, size_t size)
{
    const size_t n = ZSTD_decompress(out, size, data, packedSize);
    return !ZSTD_isError(n) && n == size;
}

} // namespace lienzo::zstd
