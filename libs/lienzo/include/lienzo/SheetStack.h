#pragma once

#include <drymedia/History.h>
#include <drymedia/Paper.h>

#include <cstdint>
#include <vector>

namespace lienzo {

// Cuántas hojas hay y cuál es la activa, y cómo se mueve la activa al agregar, quitar o
// mover hojas (HU-82). Solo cuentas, sin hilos: la usan la pila (en el hilo de simulación) y
// el Lienzo (en el de la interfaz), que así saben lo mismo sin preguntarse.
// Cada hoja tiene además un id que no cambia al moverla ni al agregar otras (HU-83: el render
// guarda la textura de cada hoja por id). Los dos lados hacen las mismas operaciones en el
// mismo orden, así que dan los mismos ids.
struct SheetOrder {
    int count = 1;
    int active = 0;
    std::vector<uint64_t> ids{1}; // por posición
    uint64_t nextId = 2;

    uint64_t activeId() const { return ids[size_t(active)]; }

    // Hoja en blanco en `at` (0..count); la activa sigue siendo la misma hoja.
    void add(int at);
    // Quita la hoja `index`; si era la activa, pasa a serlo la que queda en su lugar (o la
    // anterior, si era la última). Con una sola hoja no hace nada. Devuelve si quitó.
    bool remove(int index);
    // Lleva la hoja `from` al lugar `to`; la activa sigue siendo la misma hoja.
    void move(int from, int to);
    void activate(int index);

    bool valid(int index) const { return index >= 0 && index < count; }
};

// La pila de hojas (HU-82): el depósito de cada hoja (drymedia::TileSet) y su deshacer. El
// papel trabaja con el de la activa; las demás guardan el suyo acá, sin copias: activar otra
// hoja cambia de conjunto de tiles (Paper::swapTiles) y el relieve es uno solo. Vive junto
// al papel, en el Lienzo, para sobrevivir a recrear la simulación (F9); la cambia solo el
// hilo de simulación, entre trazos. Con una hoja (Cartuchera, Ejercicios) es lo de siempre.
class SheetStack {
public:
    explicit SheetStack(int undoLimit);

    const SheetOrder& order() const { return m_order; }
    int count() const { return m_order.count; }
    int active() const { return m_order.active; }

    // El deshacer de la hoja activa.
    drymedia::History& history() { return m_sheets[size_t(m_order.active)].history; }

    // Devuelven true si cambió la hoja que está en el papel (hay que repintar).
    bool add(drymedia::Paper& paper, int at);
    bool remove(drymedia::Paper& paper, int index);
    bool move(int from, int to);
    bool activate(drymedia::Paper& paper, int index);

    // Hash de una hoja (pruebas y diagnóstico). Pasa la hoja por el papel y la devuelve:
    // llamar solo desde el hilo que escribe el papel, o con la simulación detenida.
    uint64_t hash(drymedia::Paper& paper, int index);

private:
    struct Sheet {
        drymedia::TileSet tiles; // de la activa: un conjunto vacío (el suyo está en el papel)
        drymedia::History history;
    };

    int m_undoLimit;
    std::vector<Sheet> m_sheets;
    SheetOrder m_order;
};

} // namespace lienzo
