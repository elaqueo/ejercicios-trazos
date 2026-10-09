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
    // aporte a = min((p · k) >> 12, 65535); delta = (a · sat0(techo − depósito)) >> 16;
    // depósito = sat(depósito + delta). (>> 12 y no >> 16: con >> 16 ni la blandura máxima
    // llegaba al tono de una HB; calibración del 10 de octubre en HU-51.) El depósito nunca
    // pasa el techo, y si ya está por encima (de una mina más blanda) no cambia.
    void (*deposit)(uint16_t* deposit, const uint16_t* penetration, int n, uint16_t k, uint16_t ceiling);
};

extern const Impl kScalar;
extern const Impl kAvx2;

} // namespace drymedia::kernel
