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

void History::beforeTileWrite(int tileIndex, const uint16_t* before)
{
    if (!m_recording)
        return;
    TileState state;
    state.index = tileIndex;
    state.existed = before != nullptr;
    if (before)
        state.data.assign(before, before + kTileStride);
    m_current.before.push_back(std::move(state));
}

void History::endStroke(const Paper& paper)
{
    if (!m_recording)
        return;
    m_recording = false;
    if (m_current.before.empty())
        return; // el trazo no cambió nada: no ocupa lugar ni descarta lo rehacible

    for (const TileState& b : m_current.before) {
        TileState after;
        after.index = b.index;
        const uint16_t* tile = paper.findDepositTile(b.index % paper.tilesX(), b.index / paper.tilesX());
        after.existed = tile != nullptr;
        if (tile)
            after.data.assign(tile, tile + kTileStride);
        m_current.after.push_back(std::move(after));
    }
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
        if (s.existed)
            std::copy(s.data.begin(), s.data.end(), paper.depositTile(tx, ty));
        else
            paper.releaseTile(tx, ty);
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
