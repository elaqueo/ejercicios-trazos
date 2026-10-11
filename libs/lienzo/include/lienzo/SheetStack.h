#pragma once

#include <drymedia/History.h>
#include <drymedia/Paper.h>

#include <cstdint>
#include <deque>
#include <functional>
#include <utility>
#include <vector>

namespace lienzo {

// Cuántas hojas hay y cuál es la activa, y cómo se mueve la activa al agregar, quitar o
// mover hojas (HU-82). Solo cuentas, sin hilos.
// Cada hoja tiene además un id que no cambia al moverla ni al agregar otras (HU-83: el render
// guarda la textura de cada hoja por id); deshacer un borrado le devuelve el suyo (HU-87).
struct SheetOrder {
    int count = 1;
    int active = 0;
    std::vector<uint64_t> ids{1}; // por posición
    uint64_t nextId = 2;

    uint64_t activeId() const { return ids[size_t(active)]; }
    int indexOf(uint64_t id) const;

    // Hoja en blanco en `at` (0..count), con un id nuevo; la activa sigue siendo la misma hoja.
    void add(int at) { insert(at, nextId++); }
    // Lo mismo con un id que ya tuvo (deshacer, rehacer).
    void insert(int at, uint64_t id);
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
//
// Deshacer (HU-87): los trazos van al deshacer de su hoja y las operaciones de la pila
// (insertar, duplicar, borrar, mover) a uno de la pila; limpiar una hoja es como un trazo. Cada
// cosa lleva un número de orden (tag, creciente) y Z deshace la más reciente entre el último
// trazo de la hoja activa y la última operación de la pila; Ctrl+Y rehace en el orden
// inverso. Con tag 0 la operación no se guarda (sin deshacer, como en Ejercicios).
class SheetStack {
public:
    explicit SheetStack(int undoLimit);

    const SheetOrder& order() const { return m_order; }
    int count() const { return m_order.count; }
    int active() const { return m_order.active; }

    // El deshacer de la hoja activa.
    drymedia::History& history() { return m_sheets[size_t(m_order.active)].history; }

    // Antes de que la activa deje su lugar a otra y siga existiendo: la simulación guarda su
    // imagen para el render (HU-83). La imagen de pantalla tiene que ser la de la activa:
    // después de cada operación que cambia la hoja del papel, la simulación repinta antes de
    // la siguiente.
    std::function<void(uint64_t id)> onLeave;

    // Devuelven true si cambió la hoja que está en el papel o su contenido entero (hay que
    // repintar todo).
    bool add(drymedia::Paper& paper, int at, uint64_t tag = 0); // en blanco, no la activa
    bool insertAfterActive(drymedia::Paper& paper, uint64_t tag); // en blanco, y pasa a ser la activa
    bool duplicateActive(drymedia::Paper& paper, uint64_t tag);   // una copia después, y la activa
    bool clearActive(drymedia::Paper& paper, uint64_t tag);
    bool remove(drymedia::Paper& paper, int index, uint64_t tag = 0);
    bool move(int from, int to, uint64_t tag = 0); // HU-88: con tag, se deshace
    bool activate(drymedia::Paper& paper, int index);

    // Deshacer y rehacer (Z, Ctrl+Y): lo de un trazo devuelve los tiles a repintar; lo de la
    // pila, sheetChanged (salvo mover, que no cambia la hoja del papel).
    struct UndoResult {
        bool sheetChanged = false;
        std::vector<int> tiles;
    };
    UndoResult undo(drymedia::Paper& paper);
    UndoResult redo(drymedia::Paper& paper);
    // Algo nuevo (un trazo) descarta lo rehacible de la pila.
    void clearRedo() { m_redoOps.clear(); }
    size_t stackUndoCount() const { return m_undoOps.size(); }

    // Hash de una hoja (pruebas y diagnóstico). Pasa la hoja por el papel y la devuelve:
    // llamar solo desde el hilo que escribe el papel, o con la simulación detenida.
    uint64_t hash(drymedia::Paper& paper, int index);
    // Pasa la hoja por el papel mientras dura `work` (redibujar su imagen) y la devuelve. Ídem.
    void withSheet(drymedia::Paper& paper, int index, const std::function<void()>& work);

    // La toma (HU-85): el depósito de una hoja como bytes (Paper::serializeTiles), sin pasarla
    // por el papel. Solo lee: varias hojas a la vez, con la simulación detenida (o desde ella,
    // con keepGoing: ver Paper::serializeTiles).
    std::vector<uint8_t> serialize(const drymedia::Paper& paper, int index,
                                   const std::function<bool()>& keepGoing = {}) const;
    // Reemplaza la pila entera (abrir una toma): estas hojas con sus ids, en orden, y la
    // activa; sin deshacer. No avisa onLeave. Quedan como guardadas.
    void replace(drymedia::Paper& paper, std::vector<std::pair<uint64_t, drymedia::TileSet>> sheets, int active);

    // Guardado automático (HU-86): cada hoja lleva una versión que cambia con cada trazo,
    // deshacer, rehacer o limpieza, y la última guardada. Una hoja nueva, o una que vuelve a
    // la pila (deshacer un borrado, rehacer un insertar), no está guardada: su archivo se
    // borró o nunca existió.
    void touchActive() { ++m_sheets[size_t(m_order.active)].version; } // la activa cambió (un trazo)
    std::vector<int> unsaved() const; // por posición
    void markSaved(int index) { m_sheets[size_t(index)].savedVersion = m_sheets[size_t(index)].version; }

private:
    struct Sheet {
        drymedia::TileSet tiles; // de la activa: un conjunto vacío (el suyo está en el papel)
        drymedia::History history;
        uint64_t version = 1, savedVersion = 0; // HU-86
    };
    // Una operación de la pila, para deshacerla. Insert: la hoja quedó en `index`; deshacer la
    // quita (y la guarda en `sheet` para rehacer). Remove: se quitó de `index`; `sheet` es la
    // hoja quitada, y deshacer la vuelve a poner. Move (HU-88): pasó de `index` a `to`.
    struct Op {
        enum Kind { Insert, Remove, Move } kind = Insert;
        int index = 0;
        int to = 0;
        uint64_t id = 0;       // la hoja insertada o quitada
        uint64_t activeId = 0; // la activa antes de la operación
        bool activated = false; // la hoja insertada (o la quitada) era la activa después (o antes)
        uint64_t tag = 0;
        Sheet sheet;
    };

    void leave(); // avisa que la activa deja su lugar (onLeave)
    void switchTo(drymedia::Paper& paper, int index); // activa sin avisar
    void moveSheet(int from, int to); // sin deshacer
    void insertSheet(drymedia::Paper& paper, int at, uint64_t id, Sheet sheet, bool activateIt);
    Sheet takeSheet(drymedia::Paper& paper, int index); // la quita de la pila, con sus tiles
    static Op makeOp(Op::Kind kind, int index, uint64_t id, uint64_t activeId, bool activated, uint64_t tag);
    void record(Op op);
    void undoOp(drymedia::Paper& paper, Op& op);
    void redoOp(drymedia::Paper& paper, Op& op);
    Sheet blank(drymedia::Paper& paper) { return {paper.newTileSet(), drymedia::History(m_undoLimit)}; }

    int m_undoLimit;
    std::vector<Sheet> m_sheets;
    SheetOrder m_order;
    std::deque<Op> m_undoOps;
    std::vector<Op> m_redoOps;
};

} // namespace lienzo
