#pragma once

// drymedia: motor de medios secos de Cartuchera (docs/medios-secos/arquitectura.md).
// Sin dependencias de Qt ni de D3D. Todo el estado es entero y determinista: la misma
// semilla y la misma secuencia de muestras dan el mismo papel en cualquier máquina.

#include <cstdint>
#include <memory>

namespace drymedia {

// Resolución de simulación: 600 dpi (celda de ~42 µm).
constexpr double kCellsPerMm = 600.0 / 25.4;
// Tiles de depósito: kTileSize × kTileSize celdas.
constexpr int kTileSize = 64;
constexpr int kTileCells = kTileSize * kTileSize;

struct PaperSpec {
    uint32_t seed = 1;
    double widthMm = 297.0;  // A4 apaisado recortado a la altura útil de la tableta
    double heightMm = 203.0; // (decisión del 10 de octubre de 2026)
};

// La hoja. Dos capas, como en el plan:
//  - Relieve: el diente del papel, u16 por celda, para toda la hoja. Se genera al crear
//    el papel (en varios hilos, con enteros) y después es de solo lectura.
//  - Depósito: u16 por celda, en tiles de 64 × 64 que existen solo donde se tocó. Salen
//    de un pool que reserva bloques por adelantado, para que tocar un tile nuevo durante
//    un trazo no genere ni reserve nada (los picos del spike HU-45 venían de ahí).
class Paper {
public:
    explicit Paper(const PaperSpec& spec = {});
    ~Paper();
    Paper(const Paper&) = delete;
    Paper& operator=(const Paper&) = delete;

    const PaperSpec& spec() const;

    // Tamaño en celdas y en tiles (el último tile de cada fila o columna puede quedar
    // parcialmente fuera de la hoja).
    int width() const;
    int height() const;
    int tilesX() const;
    int tilesY() const;

    // Relieve de toda la hoja, fila por fila (width() × height()).
    const uint16_t* relief() const;
    uint16_t reliefAt(int x, int y) const;

    // Tile de depósito (kTileCells valores, fila por fila); lo crea en cero si no
    // existía. nullptr fuera de la hoja.
    uint16_t* depositTile(int tx, int ty);
    // El tile si ya existe; nullptr si nunca se tocó o está fuera de la hoja.
    const uint16_t* findDepositTile(int tx, int ty) const;
    size_t tileCount() const;

    // Hoja nueva con el mismo relieve: borra todo el depósito y devuelve los tiles al
    // pool (no libera memoria ni regenera el relieve).
    void clear();

    // Hash del relieve (se calcula una vez) y del estado completo (relieve + depósito
    // de los tiles tocados, en orden de tile).
    uint64_t reliefHash() const;
    uint64_t hash() const;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace drymedia
