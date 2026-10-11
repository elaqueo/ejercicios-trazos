#include "lienzo/SheetStack.h"

#include <algorithm>

namespace lienzo {

int SheetOrder::indexOf(uint64_t id) const
{
    const auto it = std::find(ids.begin(), ids.end(), id);
    return it == ids.end() ? -1 : int(it - ids.begin());
}

void SheetOrder::insert(int at, uint64_t id)
{
    at = std::clamp(at, 0, count);
    ids.insert(ids.begin() + at, id);
    ++count;
    if (at <= active)
        ++active;
}

bool SheetOrder::remove(int index)
{
    if (count <= 1 || !valid(index))
        return false;
    ids.erase(ids.begin() + index);
    --count;
    if (index < active || active == count) // antes de la activa, o era la activa y la última
        --active;
    return true;
}

void SheetOrder::move(int from, int to)
{
    if (!valid(from) || !valid(to) || from == to)
        return;
    const uint64_t id = ids[size_t(from)];
    ids.erase(ids.begin() + from);
    ids.insert(ids.begin() + to, id);
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

void SheetStack::leave()
{
    if (onLeave)
        onLeave(m_order.activeId());
}

void SheetStack::insertSheet(drymedia::Paper& paper, int at, uint64_t id, Sheet sheet, bool activateIt)
{
    at = std::clamp(at, 0, m_order.count);
    m_sheets.insert(m_sheets.begin() + at, std::move(sheet));
    m_order.insert(at, id);
    if (activateIt)
        activate(paper, at);
}

SheetStack::Sheet SheetStack::takeSheet(drymedia::Paper& paper, int index)
{
    const bool wasActive = index == m_order.active;
    if (wasActive)
        paper.swapTiles(m_sheets[size_t(index)].tiles); // sus tiles vuelven a su lugar; el papel, vacío
    Sheet sheet = std::move(m_sheets[size_t(index)]);
    m_sheets.erase(m_sheets.begin() + index);
    m_order.remove(index);
    if (wasActive)
        paper.swapTiles(m_sheets[size_t(m_order.active)].tiles);
    return sheet;
}

SheetStack::Op SheetStack::makeOp(Op::Kind kind, int index, uint64_t id, uint64_t activeId, bool activated, uint64_t tag)
{
    Op op;
    op.kind = kind;
    op.index = index;
    op.id = id;
    op.activeId = activeId;
    op.activated = activated;
    op.tag = tag;
    return op;
}

void SheetStack::record(Op op)
{
    if (!op.tag)
        return;
    m_undoOps.push_back(std::move(op));
    if (int(m_undoOps.size()) > m_undoLimit)
        m_undoOps.pop_front();
    m_redoOps.clear(); // una operación nueva descarta lo rehacible
}

bool SheetStack::add(drymedia::Paper& paper, int at, uint64_t tag)
{
    at = std::clamp(at, 0, m_order.count);
    const uint64_t before = m_order.activeId(), id = m_order.nextId++;
    insertSheet(paper, at, id, blank(paper), false);
    record(makeOp(Op::Insert, at, id, before, false, tag));
    return false;
}

bool SheetStack::insertAfterActive(drymedia::Paper& paper, uint64_t tag)
{
    const int at = m_order.active + 1;
    const uint64_t before = m_order.activeId(), id = m_order.nextId++;
    insertSheet(paper, at, id, blank(paper), true);
    record(makeOp(Op::Insert, at, id, before, true, tag));
    return true;
}

bool SheetStack::duplicateActive(drymedia::Paper& paper, uint64_t tag)
{
    const int at = m_order.active + 1;
    const uint64_t before = m_order.activeId(), id = m_order.nextId++;
    // La copia arranca sin deshacer propio: lo que se hizo antes es de la original.
    insertSheet(paper, at, id, {paper.copyTiles(), drymedia::History(m_undoLimit)}, true);
    record(makeOp(Op::Insert, at, id, before, true, tag));
    return true;
}

bool SheetStack::clearActive(drymedia::Paper& paper, uint64_t tag)
{
    drymedia::History& h = history();
    if (!tag) { // sin deshacer: como la hoja nueva de Cartuchera
        paper.clear();
        h.clear();
        return true;
    }
    // Como un trazo que toca todos los tiles: deshacerlo los vuelve a poner.
    h.beginStroke(tag);
    for (int ty = 0; ty < paper.tilesY(); ++ty)
        for (int tx = 0; tx < paper.tilesX(); ++tx)
            if (const uint16_t* tile = paper.findDepositTile(tx, ty))
                h.beforeTileWrite(ty * paper.tilesX() + tx, tile, paper.tilePlanes(tx, ty));
    paper.clear();
    h.endStroke(paper);
    m_redoOps.clear();
    return true;
}

bool SheetStack::remove(drymedia::Paper& paper, int index, uint64_t tag)
{
    if (m_order.count <= 1 || !m_order.valid(index))
        return false;
    const bool wasActive = index == m_order.active;
    const uint64_t before = m_order.activeId(), id = m_order.ids[size_t(index)];
    Sheet sheet = takeSheet(paper, index);
    if (tag) {
        Op op = makeOp(Op::Remove, index, id, before, wasActive, tag);
        op.sheet = std::move(sheet);
        record(std::move(op));
    }
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
    leave();
    switchTo(paper, index);
    return true;
}

void SheetStack::switchTo(drymedia::Paper& paper, int index)
{
    if (!m_order.valid(index) || index == m_order.active)
        return;
    paper.swapTiles(m_sheets[size_t(m_order.active)].tiles); // la que sale vuelve a su lugar
    m_order.activate(index);
    paper.swapTiles(m_sheets[size_t(index)].tiles); // y entra la nueva
}

void SheetStack::undoOp(drymedia::Paper& paper, Op& op)
{
    if (op.kind == Op::Insert) {
        // Quitar la hoja insertada (se guarda para rehacer) y volver a la activa de antes.
        const int index = m_order.indexOf(op.id);
        if (index < 0 || m_order.count <= 1)
            return;
        op.index = index;
        op.sheet = takeSheet(paper, index);
        // Sin avisar: si se quitó la activa, la imagen todavía es la de ella, no la de la que
        // quedó en el papel (la simulación repinta después).
        switchTo(paper, m_order.indexOf(op.activeId));
    } else {
        // Volver a poner la hoja quitada en su lugar; si era la activa, vuelve a serlo.
        insertSheet(paper, op.index, op.id, std::move(op.sheet), op.activated);
    }
}

void SheetStack::redoOp(drymedia::Paper& paper, Op& op)
{
    if (op.kind == Op::Insert) {
        insertSheet(paper, op.index, op.id, std::move(op.sheet), op.activated);
    } else {
        const int index = m_order.indexOf(op.id);
        if (index < 0 || m_order.count <= 1)
            return;
        op.index = index;
        op.sheet = takeSheet(paper, index);
    }
}

SheetStack::UndoResult SheetStack::undo(drymedia::Paper& paper)
{
    UndoResult result;
    drymedia::History& h = history();
    const uint64_t stackTag = m_undoOps.empty() ? 0 : m_undoOps.back().tag;
    if (stackTag && (!h.canUndo() || stackTag > h.undoTag())) {
        Op op = std::move(m_undoOps.back());
        m_undoOps.pop_back();
        undoOp(paper, op);
        m_redoOps.push_back(std::move(op));
        result.sheetChanged = true;
    } else if (h.canUndo()) {
        result.tiles = h.undo(paper);
    }
    return result;
}

SheetStack::UndoResult SheetStack::redo(drymedia::Paper& paper)
{
    UndoResult result;
    drymedia::History& h = history();
    const uint64_t stackTag = m_redoOps.empty() ? 0 : m_redoOps.back().tag;
    if (stackTag && (!h.canRedo() || stackTag < h.redoTag())) {
        Op op = std::move(m_redoOps.back());
        m_redoOps.pop_back();
        redoOp(paper, op);
        m_undoOps.push_back(std::move(op));
        result.sheetChanged = true;
    } else if (h.canRedo()) {
        result.tiles = h.redo(paper);
    }
    return result;
}

uint64_t SheetStack::hash(drymedia::Paper& paper, int index)
{
    uint64_t h = 0;
    withSheet(paper, index, [&] { h = paper.hash(); });
    return h;
}

void SheetStack::withSheet(drymedia::Paper& paper, int index, const std::function<void()>& work)
{
    if (!m_order.valid(index) || index == m_order.active) {
        work();
        return;
    }
    drymedia::TileSet& tiles = m_sheets[size_t(index)].tiles;
    paper.swapTiles(tiles);
    work();
    paper.swapTiles(tiles);
}

std::vector<uint8_t> SheetStack::serialize(const drymedia::Paper& paper, int index) const
{
    if (!m_order.valid(index) || index == m_order.active)
        return paper.serializeTiles(); // la activa está en el papel
    return paper.serializeTiles(&m_sheets[size_t(index)].tiles);
}

void SheetStack::replace(drymedia::Paper& paper, std::vector<std::pair<uint64_t, drymedia::TileSet>> sheets, int active)
{
    if (sheets.empty())
        return;
    m_sheets.clear();
    m_undoOps.clear();
    m_redoOps.clear();
    m_order = SheetOrder{};
    m_order.ids.clear();
    m_order.count = 0;
    m_order.nextId = 1;
    for (auto& [id, tiles] : sheets) {
        m_sheets.push_back({std::move(tiles), drymedia::History(m_undoLimit)});
        m_order.ids.push_back(id);
        ++m_order.count;
        m_order.nextId = std::max(m_order.nextId, id + 1);
    }
    m_order.active = std::clamp(active, 0, m_order.count - 1);
    // La activa entra al papel; lo que había en él se descarta (la de la activa queda vacía).
    drymedia::TileSet& tiles = m_sheets[size_t(m_order.active)].tiles;
    paper.swapTiles(tiles);
    tiles = drymedia::TileSet{};
}

} // namespace lienzo
