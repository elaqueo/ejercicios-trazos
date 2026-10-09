#include "SheetMapping.h"

#include <algorithm>
#include <cmath>

namespace cartuchera {

SheetMapping SheetMapping::fit(int clientWidth, int clientHeight, int cellsWidth, int cellsHeight)
{
    SheetMapping m;
    m.clientWidth = clientWidth;
    m.clientHeight = clientHeight;
    m.cellsWidth = cellsWidth;
    m.cellsHeight = cellsHeight;
    m.pixelsPerCell = std::min(double(clientWidth) / cellsWidth, double(clientHeight) / cellsHeight);
    m.sheetWidth = int(std::floor(cellsWidth * m.pixelsPerCell));
    m.sheetHeight = int(std::floor(cellsHeight * m.pixelsPerCell));
    m.sheetX = (clientWidth - m.sheetWidth) / 2;
    m.sheetY = (clientHeight - m.sheetHeight) / 2;
    return m;
}

void SheetMapping::cellsOfPixel(int p, int cells, int& first, int& last) const
{
    first = std::clamp(int(std::floor(p / pixelsPerCell)), 0, cells - 1);
    last = std::clamp(int(std::floor((p + 1) / pixelsPerCell)), first + 1, cells);
}

bool SheetMapping::pixelsOfCells(int x0, int y0, int x1, int y1, int& px0, int& py0, int& px1, int& py1) const
{
    px0 = std::max(sheetX, sheetX + int(std::floor(x0 * pixelsPerCell)));
    py0 = std::max(sheetY, sheetY + int(std::floor(y0 * pixelsPerCell)));
    px1 = std::min(sheetX + sheetWidth, sheetX + int(std::ceil(x1 * pixelsPerCell)));
    py1 = std::min(sheetY + sheetHeight, sheetY + int(std::ceil(y1 * pixelsPerCell)));
    return px1 > px0 && py1 > py0;
}

} // namespace cartuchera
