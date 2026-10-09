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
// Uso: beginStroke(); beforeTileWrite() lo llama Pencil (TileObserver) antes de la primera
// escritura de cada tile; endStroke() al soltar.
class History {
public:
    explicit History(int limit = 100);

    void beginStroke();
    void beforeTileWrite(int tileIndex, const uint16_t* before); // before: nullptr si el tile no existía
    void endStroke(const Paper& paper);

    bool canUndo() const { return !m_undo.empty(); }
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
        std::vector<uint16_t> data; // vacío si no existía
    };
    struct Entry {
        std::vector<TileState> before, after;
    };
    static std::vector<int> apply(Paper& paper, const std::vector<TileState>& states);

    int m_limit;
    bool m_recording = false;
    Entry m_current;
    std::deque<Entry> m_undo;
    std::vector<Entry> m_redo;
};

} // namespace drymedia
