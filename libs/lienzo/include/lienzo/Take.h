#pragma once

#include "lienzo/SheetImages.h"
#include "lienzo/SheetRole.h"

#include <QString>

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace drymedia {
class Paper;
}

namespace lienzo {

class SheetStack;

// La toma en disco (HU-85, Mesa de animación): una carpeta con toma.json (el papel, el orden
// de las hojas, la activa, la vista, y lugar para los roles y el audio de las historias que
// siguen) y un archivo por hoja, hoja-<id>.hoja, con su depósito y su imagen de pantalla
// comprimidos con zstd. Formato en docs/mesa-de-animacion/arquitectura.md.
//
// Guardar y abrir trabajan con el papel quieto (la simulación detenida) y reparten las hojas
// entre los núcleos.
constexpr int kTakeVersion = 1;

struct TakeView {
    double degrees = 0; // giro de la vista
    double zoom = 1;    // HU-101
    double panX = 0, panY = 0; // HU-102: el corrimiento, en fracción del ancho y el alto de la hoja
    SheetRoles roles;   // HU-104: key o breakdown, por id (solo las que tienen)
    uint64_t fixed = 0; // HU-89: la hoja fija, por id (0: ninguna)
    bool circle = false; // HU-94: la pila en círculo
    QString audio;       // HU-106: el archivo del audio, en la carpeta de la toma (vacío: sin audio)
    std::vector<double> marks; // HU-108: las marcas del audio, en segundos, en orden
};

struct TakeResult {
    bool ok = false; // se guardó; o había una toma y se abrió
    QString error;   // por qué no
    // Al abrir: las hojas (por posición) que quedaron en blanco porque su archivo falta o
    // está dañado, con el motivo (también va al log).
    std::vector<std::pair<int, QString>> blank;
};

// Las piezas de guardar, para guardar de a una hoja (HU-86, lienzo/TakeSaver.h). Crean la
// carpeta si no está y devuelven el error, o vacío.
// El archivo de una hoja: su depósito (Paper::serializeTiles) y su imagen (vacía: no tiene).
QString writeSheetFile(const QString& dir, const drymedia::Paper& paper, uint64_t id, const std::vector<uint8_t>& state,
                       const PackedImage& image);
// toma.json con estas hojas en orden, la activa (posición) y la vista; después borra los
// archivos de las hojas que ya no están.
QString writeTakeIndex(const QString& dir, const drymedia::Paper& paper, const std::vector<uint64_t>& ids, int active,
                       const TakeView& view);

// En qué carpeta trabajar (HU-86): dir si open(dir) la abre o si no tiene toma. Si tiene una
// que no se abre (otro papel, más nueva, dañada), no se pisa: la primera de dir-2, dir-3...
// que open abre, o la primera que no existe (una toma nueva).
QString takeFolder(const QString& dir, const std::function<bool(const QString&)>& open);

// Guarda toda la pila. image(id) da la imagen de pantalla de cada hoja (vacía si no la tiene:
// al abrir se redibuja); se llama desde varios hilos. Las hojas se escriben antes que
// toma.json, cada archivo de una vez (QSaveFile); al final se borran los archivos de las
// hojas que ya no están.
TakeResult saveTake(const QString& dir, const drymedia::Paper& paper, const SheetStack& sheets, const TakeView& view,
                    const std::function<PackedImage(uint64_t sheet)>& image);

// Abre la toma y reemplaza la pila (sin deshacer). Una hoja que falta o está dañada queda en
// blanco; sin toma.json, o con otro papel, no abre y la pila no cambia. images (opcional)
// recibe la imagen guardada de cada hoja que la tiene, por id.
TakeResult openTake(const QString& dir, drymedia::Paper& paper, SheetStack& sheets, TakeView& view,
                    std::vector<std::pair<uint64_t, PackedImage>>* images = nullptr);

} // namespace lienzo
