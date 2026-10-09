#pragma once

// Spike HU-45: kernel de contacto y depósito de Cartuchera, en enteros, con tres
// implementaciones que tienen que dar resultados idénticos bit a bit.
//
// Todas las funciones trabajan sobre arreglos contiguos de n celdas (la huella de la
// punta, copiada del papel), con n múltiplo de 32. Aritmética de 16 bits sin signo con
// saturación, igual en las tres rutas:
//   superficie = sat(relieve + kBase + depósito / 16)
//   penetración p = sat0(superficie - sat(punta + D))      (D: altura de la punta)
//   fuerza(D) = Σ p                                        (decrece con D)
//   aporte a = (p · k) >> 16; delta = (a · (65535 - depósito)) >> 16
//   depósito = sat(depósito + delta)                       (satura: el diente se llena)

#include <cstdint>

namespace kernel {

constexpr uint16_t kBase = 16384; // desplaza la superficie para que D tenga rango hacia abajo

struct Impl {
    const char* name;
    void (*surface)(uint16_t* surface, const uint16_t* relief, const uint16_t* deposit, int n);
    uint32_t (*force)(const uint16_t* surface, const uint16_t* tip, int n, uint16_t d);
    void (*deposit)(uint16_t* deposit, const uint16_t* surface, const uint16_t* tip, int n, uint16_t d, uint16_t k);
};

extern const Impl kScalar;
extern const Impl kAvx2;
extern const Impl kAvx512;

} // namespace kernel
