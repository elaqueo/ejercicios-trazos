#include "lienzo/SheetStack.h"

#include <algorithm>

namespace lienzo {

void SheetOrder::add(int at)
{
    at = std::clamp(at, 0, count);
    ++count;
    if (at <= active)
        ++active;
}

bool SheetOrder::remove(int index)
{
    if (count <= 1 || !valid(index))
        return false;
    --count;
    if (index < active || active == count) // antes de la activa, o era la activa y la última
        --active;
    return true;
}

void SheetOrder::move(int from, int to)
{
    if (!valid(from) || !valid(to) || from == to)
        return;
    if (active == from)
        active = to;
    else if (from < active && active <= to)
        --active;
    else if (to <= active && active < from)
        ++active;
}

void SheetOrder::activate(int index)
{
    if (valid(index))
        active = index;
}

SheetStack::SheetStack(int undoLimit)
    : m_undoLimit(undoLimit)
{
    m_sheets.push_back({drymedia::TileSet{}, drymedia::History(undoLimit)});
}

bool SheetStack::add(drymedia::Paper& paper, int at)
{
    at = std::clamp(at, 0, m_order.count);
    m_sheets.insert(m_sheets.begin() + at, Sheet{paper.newTileSet(), drymedia::History(m_undoLimit)});
    m_order.add(at);
    return false;
}

bool SheetStack::remove(drymedia::Paper& paper, int index)
{
    if (m_order.count <= 1 || !m_order.valid(index))
        return false;
    const bool wasActive = index == m_order.active;
    if (wasActive)
        paper.swapTiles(m_sheets[size_t(index)].tiles); // el papel queda con un conjunto vacío
    m_sheets.erase(m_sheets.begin() + index);
    m_order.remove(index);
    if (wasActive)
        paper.swapTiles(m_sheets[size_t(m_order.active)].tiles);
    return wasActive;
}

bool SheetStack::move(int from, int to)
{
    if (!m_order.valid(from) || !m_order.valid(to) || from == to)
        return false;
    Sheet sheet = std::move(m_sheets[size_t(from)]);
    m_sheets.erase(m_sheets.begin() + from);
    m_sheets.insert(m_sheets.begin() + to, std::move(sheet));
    m_order.move(from, to);
    return false; // la activa sigue en el papel
}

bool SheetStack::activate(drymedia::Paper& paper, int index)
{
    if (!m_order.valid(index) || index == m_order.active)
        return false;
    paper.swapTiles(m_sheets[size_t(m_order.active)].tiles); // la que sale vuelve a su lugar
    m_order.activate(index);
    paper.swapTiles(m_sheets[size_t(index)].tiles); // y entra la nueva
    return true;
}

uint64_t SheetStack::hash(drymedia::Paper& paper, int index)
{
    if (!m_order.valid(index) || index == m_order.active)
        return paper.hash();
    drymedia::TileSet& tiles = m_sheets[size_t(index)].tiles;
    paper.swapTiles(tiles);
    const uint64_t h = paper.hash();
    paper.swapTiles(tiles);
    return h;
}

} // namespace lienzo
