#pragma once

#include <cstdint>
#include <deque>
#include <vector>

namespace drymedia {

class Paper;

// Deshacer y rehacer por trazo (HU-53). Guarda, para cada trazo, el estado de los tiles
// que tocó antes y después; deshacer y rehacer los vuelven a escribir tal cual, así el
// papel queda idéntico (mismo hash) al que había.
//
// HU-76: de cada tile se guardan solo los planos que usa (los otros están en cero), y el
// estado "después" no se copia al soltar el trazo sino al deshacerlo: como se deshace en
// orden, en ese momento los tiles están exactamente como los dejó el trazo. Copiar todo al
// soltar frenaba la simulación al final de cada trazo largo.
//
// Uso: beginStroke(); beforeTileWrite() lo llama Pencil (TileObserver) antes de la primera
// escritura de cada tile; endStroke() al soltar.
class History {
public:
    explicit History(int limit = 100);

    // tag: número de orden del trazo (0 si no importa). Con una pila de hojas (HU-87) el
    // deshacer compara el del último trazo de la hoja con el de la última operación de la
    // pila para deshacer lo más reciente.
    void beginStroke(uint64_t tag = 0);
    // before: nullptr si el tile no existía; planes: Paper::tilePlanes del tile.
    void beforeTileWrite(int tileIndex, const uint16_t* before, uint8_t planes);
    void endStroke(const Paper& paper);

    bool canUndo() const { return !m_undo.empty(); }
    // El tag de lo próximo que se deshace o se rehace; 0 si no hay.
    uint64_t undoTag() const { return m_undo.empty() ? 0 : m_undo.back().tag; }
    uint64_t redoTag() const { return m_redo.empty() ? 0 : m_redo.back().tag; }
    void clearRedo() { m_redo.clear(); }
    bool canRedo() const { return !m_redo.empty(); }
    size_t undoCount() const { return m_undo.size(); }

    // Devuelven los tiles afectados (índice ty · tilesX + tx), para volver a pintarlos.
    std::vector<int> undo(Paper& paper);
    std::vector<int> redo(Paper& paper);

    void clear();

private:
    struct TileState {
        int index = 0;
        bool existed = false;
        uint8_t planes = 0;          // planos guardados además del depósito
        std::vector<uint16_t> data;  // el depósito y los planos guardados, en orden; vacío si no existía
    };
    static TileState capture(int index, const uint16_t* tile, uint8_t planes);
    static std::vector<TileState> captureCurrent(const Paper& paper, const std::vector<TileState>& like);
    struct Entry {
        std::vector<TileState> before, after;
        uint64_t tag = 0;
    };
    static std::vector<int> apply(Paper& paper, const std::vector<TileState>& states);

    int m_limit;
    bool m_recording = false;
    Entry m_current;
    std::deque<Entry> m_undo;
    std::vector<Entry> m_redo;
};

} // namespace drymedia
