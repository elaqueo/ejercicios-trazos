#pragma once

#include "lienzo/SheetRole.h"

#include <drymedia/Paper.h>

#include <QString>

#include <atomic>
#include <cstdint>
#include <functional>
#include <vector>

namespace lienzo {

// Exportar la toma como secuencia PNG (HU-109, Mesa de animación): cada hoja a la resolución
// de la simulación (un píxel por celda: 300 dpi), con el grafito y los colores sobre el color
// del papel, sin tinte, guías, carta ni marcas (van en el render, no en la hoja). Las hojas
// del flip van numeradas en orden (0001.png...); la fija, aparte (fija.png). Al lado,
// toma.json con el rol y el número de cada una y el audio con sus marcas (copiado), para la
// app de pencil test futura. Formato en docs/mesa-de-animacion/arquitectura.md.
//
// Trabaja con una copia del depósito de cada hoja (SheetStack::serialize) y un papel propio:
// corre en su hilo sin tocar la simulación.

struct ExportSheet {
    std::vector<uint8_t> state; // Paper::serializeTiles
    int number = 0;             // el de la app (lienzo/FixedSheet.h); 0: la fija
    SheetRole role = SheetRole::None;
};

struct ExportJob {
    QString dir; // la carpeta de la exportación (<toma>/export)
    drymedia::PaperSpec paper;
    uint32_t paperColor = 0; // BGRA, como los de Tone.h
    std::vector<ExportSheet> sheets; // en el orden de la pila, la fija incluida
    QString audio;                   // la ruta del audio de la toma (vacío: sin audio)
    std::vector<double> marks;       // sus marcas, en segundos
};

struct ExportResult {
    bool ok = false;
    QString error;
    int sheets = 0; // las del flip escritas
};

constexpr const char* kExportFolder = "export";

// "0001.png" (number desde 1) o "fija.png" (0).
QString exportFileName(int number);

// Escribe la exportación: primero borra los PNG y el toma.json que haya en dir (una hoja
// borrada no queda), después las hojas (progress(hechas, total) después de cada una, desde
// el hilo que llama) y al final toma.json. cancel (opcional) la corta entre hojas.
ExportResult exportTake(const ExportJob& job, const std::function<void(int done, int total)>& progress = {},
                        const std::atomic<bool>* cancel = nullptr);

} // namespace lienzo
