#include "drymedia/History.h"

#include "drymedia/Paper.h"

#include <algorithm>

namespace drymedia {

History::History(int limit)
    : m_limit(std::max(1, limit))
{
}

void History::beginStroke()
{
    m_current = {};
    m_recording = true;
}

History::TileState History::capture(int index, const uint16_t* tile, uint8_t planes)
{
    TileState state;
    state.index = index;
    state.existed = tile != nullptr;
    if (!tile)
        return state;
    state.planes = uint8_t(planes & ~1);
    state.data.reserve(size_t(kTileStride));
    for (int pl = 0; pl < kTilePlanes; ++pl)
        if (pl == 0 || (state.planes >> pl) & 1)
            state.data.insert(state.data.end(), tile + size_t(pl) * kTileCells, tile + size_t(pl + 1) * kTileCells);
    return state;
}

std::vector<History::TileState> History::captureCurrent(const Paper& paper, const std::vector<TileState>& like)
{
    std::vector<TileState> states;
    states.reserve(like.size());
    for (const TileState& s : like) {
        const int tx = s.index % paper.tilesX(), ty = s.index / paper.tilesX();
        states.push_back(capture(s.index, paper.findDepositTile(tx, ty), paper.tilePlanes(tx, ty)));
    }
    return states;
}

void History::beforeTileWrite(int tileIndex, const uint16_t* before, uint8_t planes)
{
    if (!m_recording)
        return;
    m_current.before.push_back(capture(tileIndex, before, planes));
}

void History::endStroke(const Paper&)
{
    if (!m_recording)
        return;
    m_recording = false;
    if (m_current.before.empty())
        return; // el trazo no cambió nada: no ocupa lugar ni descarta lo rehacible
    m_undo.push_back(std::move(m_current));
    m_current = {};
    if (int(m_undo.size()) > m_limit)
        m_undo.pop_front();
    m_redo.clear(); // un trazo nuevo descarta lo rehacible
}

std::vector<int> History::apply(Paper& paper, const std::vector<TileState>& states)
{
    std::vector<int> touched;
    for (const TileState& s : states) {
        const int tx = s.index % paper.tilesX(), ty = s.index / paper.tilesX();
        if (s.existed) {
            uint16_t* tile = paper.depositTile(tx, ty);
            auto from = s.data.begin();
            for (int pl = 0; pl < kTilePlanes; ++pl) {
                uint16_t* plane = tile + size_t(pl) * kTileCells;
                if (pl == 0 || (s.planes >> pl) & 1) {
                    std::copy(from, from + kTileCells, plane);
                    from += kTileCells;
                } else {
                    std::fill(plane, plane + kTileCells, uint16_t(0));
                }
            }
            paper.setTilePlanes(tx, ty, s.planes);
        } else {
            paper.releaseTile(tx, ty);
        }
        touched.push_back(s.index);
    }
    return touched;
}

std::vector<int> History::undo(Paper& paper)
{
    if (m_undo.empty())
        return {};
    Entry e = std::move(m_undo.back());
    m_undo.pop_back();
    e.after = captureCurrent(paper, e.before); // así lo dejó el trazo (ver arriba)
    std::vector<int> touched = apply(paper, e.before);
    m_redo.push_back(std::move(e));
    return touched;
}

std::vector<int> History::redo(Paper& paper)
{
    if (m_redo.empty())
        return {};
    Entry e = std::move(m_redo.back());
    m_redo.pop_back();
    std::vector<int> touched = apply(paper, e.after);
    m_undo.push_back(std::move(e));
    return touched;
}

void History::clear()
{
    m_undo.clear();
    m_redo.clear();
    m_current = {};
    m_recording = false;
}

} // namespace drymedia
