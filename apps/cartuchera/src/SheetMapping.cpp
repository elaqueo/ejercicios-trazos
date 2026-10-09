#include "SheetMapping.h"

#include <drymedia/Paper.h>

#include <algorithm>
#include <cmath>

namespace cartuchera {

namespace {

void finish(SheetMapping& m)
{
    m.sheetX = int(std::ceil(m.originX));
    m.sheetY = int(std::ceil(m.originY));
    m.sheetWidth = std::min(int(std::floor(m.originX + m.cellsWidth * m.pixelsPerCellX)), m.clientWidth) - m.sheetX;
    m.sheetHeight = std::min(int(std::floor(m.originY + m.cellsHeight * m.pixelsPerCellY)), m.clientHeight) - m.sheetY;
    m.sheetX = std::max(0, m.sheetX);
    m.sheetY = std::max(0, m.sheetY);
}

void cellsOf(int p, double origin, double ppc, int cells, int& first, int& last)
{
    first = std::clamp(int(std::floor((p - origin) / ppc)), 0, cells - 1);
    last = std::clamp(int(std::floor((p + 1 - origin) / ppc)), first + 1, cells);
}

} // namespace

SheetMapping SheetMapping::fit(int clientWidth, int clientHeight, int cellsWidth, int cellsHeight)
{
    SheetMapping m;
    m.clientWidth = clientWidth;
    m.clientHeight = clientHeight;
    m.cellsWidth = cellsWidth;
    m.cellsHeight = cellsHeight;
    const double ppc = std::min(double(clientWidth) / cellsWidth, double(clientHeight) / cellsHeight);
    m.pixelsPerCellX = m.pixelsPerCellY = ppc;
    m.originX = std::floor((clientWidth - cellsWidth * ppc) / 2);
    m.originY = std::floor((clientHeight - cellsHeight * ppc) / 2);
    finish(m);
    return m;
}

SheetMapping SheetMapping::onTablet(int clientWidth, int clientHeight, int areaX, int areaY, int areaWidth,
                                    int areaHeight, double tabletWidthMm, double tabletHeightMm, double sheetWidthMm,
                                    double sheetHeightMm, int cellsWidth, int cellsHeight)
{
    SheetMapping m;
    m.clientWidth = clientWidth;
    m.clientHeight = clientHeight;
    m.cellsWidth = cellsWidth;
    m.cellsHeight = cellsHeight;
    const double pxPerMmX = areaWidth / tabletWidthMm, pxPerMmY = areaHeight / tabletHeightMm;
    m.pixelsPerCellX = pxPerMmX / drymedia::kCellsPerMm;
    m.pixelsPerCellY = pxPerMmY / drymedia::kCellsPerMm;
    // La hoja centrada sobre la superficie activa.
    m.originX = areaX + (tabletWidthMm - sheetWidthMm) / 2 * pxPerMmX;
    m.originY = areaY + (tabletHeightMm - sheetHeightMm) / 2 * pxPerMmY;
    finish(m);
    return m;
}

void SheetMapping::cellsOfColumn(int px, int& first, int& last) const
{
    cellsOf(px, originX, pixelsPerCellX, cellsWidth, first, last);
}

void SheetMapping::cellsOfRow(int py, int& first, int& last) const
{
    cellsOf(py, originY, pixelsPerCellY, cellsHeight, first, last);
}

bool SheetMapping::pixelsOfCells(int x0, int y0, int x1, int y1, int& px0, int& py0, int& px1, int& py1) const
{
    px0 = std::max(sheetX, int(std::floor(originX + x0 * pixelsPerCellX)));
    py0 = std::max(sheetY, int(std::floor(originY + y0 * pixelsPerCellY)));
    px1 = std::min(sheetX + sheetWidth, int(std::ceil(originX + x1 * pixelsPerCellX)));
    py1 = std::min(sheetY + sheetHeight, int(std::ceil(originY + y1 * pixelsPerCellY)));
    return px1 > px0 && py1 > py0;
}

} // namespace cartuchera
