#pragma once

// Header privado de drymedia: los kernels sobre arreglos contiguos de n celdas (n
// múltiplo de 16). Las dos rutas tienen que dar resultados idénticos bit a bit; ver la
// definición de la aritmética en Contact.h.

#include <cstdint>

namespace drymedia::kernel {

struct Impl {
    void (*surface)(uint16_t* out, const uint16_t* relief, const uint16_t* deposit, int n, uint16_t base);
    uint32_t (*force)(const uint16_t* surface, const uint16_t* tip, int n, uint16_t d);
    void (*penetration)(uint16_t* out, const uint16_t* surface, const uint16_t* tip, int n, uint16_t d);
};

extern const Impl kScalar;
extern const Impl kAvx2;

} // namespace drymedia::kernel
