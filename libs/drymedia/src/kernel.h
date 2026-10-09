#pragma once

// Header privado de drymedia: los kernels sobre arreglos contiguos de n celdas (n
// múltiplo de 16). Las dos rutas tienen que dar resultados idénticos bit a bit; ver la
// definición de la aritmética en Contact.h.

#include <cstdint>

namespace drymedia::kernel {

// El nivel medio de una zona del papel: la cresta menos medio grano (Paper: grano 0..2047).
// El bruñido aplasta las crestas hacia ese nivel.
constexpr uint16_t kHalfGrain = 1024;

struct Impl {
    // r = relieve >> shift; top = cresta >> shift; nivel = sat0(top − kHalfGrain >> shift);
    // r −= (sat0(r − nivel) · bruñido) >> 16 (el bruñido aplasta las crestas, HU-60);
    // superficie = sat(r + base + ((sat0(top − r) · depósito) >> 16)): el grafito llena el
    // diente hasta las crestas de la zona, sin subir por encima (HU-59).
    void (*surface)(uint16_t* out, const uint16_t* relief, const uint16_t* crest, const uint16_t* deposit,
                    const uint16_t* burnish, int n, uint16_t base, int shift);
    uint32_t (*force)(const uint16_t* surface, const uint16_t* tip, int n, uint16_t d);
    void (*penetration)(uint16_t* out, const uint16_t* surface, const uint16_t* tip, int n, uint16_t d);
    // aporte a = min((p · k) >> 12, 65535); delta = (a · sat0(techo − depósito)) >> 16;
    // depósito = sat(depósito + delta). (>> 12 y no >> 16: con >> 16 ni la blandura máxima
    // llegaba al tono de una HB; calibración del 10 de octubre en HU-51.) El depósito nunca
    // pasa el techo, y si ya está por encima (de una mina más blanda) no cambia.
    // Con bruñido (HU-60), antes: a −= (a · bruñido) >> 16 (la capa bruñida rechaza grafito).
    void (*deposit)(uint16_t* deposit, const uint16_t* burnish, const uint16_t* penetration, int n, uint16_t k,
                    uint16_t ceiling);
    // Goma (HU-58): con el mismo aporte a, delta = ((a · depósito) >> 16) + (a > 0 ? 1 : 0);
    // depósito = sat0(depósito − delta). El +1 hace que llegue a 0 (hoja limpia) en vez de
    // quedarse en un resto que la proporción redondea a cero.
    // Con bruñido, antes: a −= (a · (bruñido >> 1)) >> 16 (el grafito bruñido resiste la goma).
    void (*erase)(uint16_t* deposit, const uint16_t* burnish, const uint16_t* penetration, int n, uint16_t k);
    // Bruñido (HU-60): h = min((p · kb) >> 12, 65535) (kb ya trae presión³: casi nada con
    // poca presión); bruñido = sat(bruñido + ((h · depósito) >> 16)) (solo donde hay grafito); y
    // el depósito por debajo de `target` sube hacia él: + ((sat0(target − depósito) · h) >> 16)
    // (el grafito se arrastra a los valles y el tono se empareja; nunca aclara). Todo con el
    // depósito de antes de este paso.
    void (*burnish)(uint16_t* deposit, uint16_t* burnish, const uint16_t* penetration, int n, uint16_t kb,
                    uint16_t target);
    // Suma del depósito y cantidad de celdas con penetración > 0.
    uint32_t (*contactDeposit)(const uint16_t* deposit, const uint16_t* penetration, int n, uint32_t* count);
};

extern const Impl kScalar;
extern const Impl kAvx2;

} // namespace drymedia::kernel
