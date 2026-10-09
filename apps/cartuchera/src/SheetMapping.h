#pragma once

namespace cartuchera {

// Dónde se dibuja la hoja dentro de la ventana y cómo se pasa de píxeles a celdas de
// simulación. Escala propia en cada eje: el área calibrada no tiene por qué tener
// exactamente la proporción de la tableta.
struct SheetMapping {
    int clientWidth = 0, clientHeight = 0;
    int cellsWidth = 0, cellsHeight = 0;
    double originX = 0, originY = 0;               // píxel (con decimales) de la esquina de la celda (0, 0)
    double pixelsPerCellX = 1, pixelsPerCellY = 1;
    int sheetX = 0, sheetY = 0, sheetWidth = 0, sheetHeight = 0; // la hoja en píxeles enteros

    // HU-51 / sin calibrar: la hoja ajustada a la ventana, centrada, misma escala en los dos ejes.
    static SheetMapping fit(int clientWidth, int clientHeight, int cellsWidth, int cellsHeight);

    // HU-52: hoja = tableta. area es el rectángulo del cliente al que está mapeada toda la
    // superficie activa de la tableta (tabletWidthMm × tabletHeightMm); la hoja
    // (sheetWidthMm × sheetHeightMm) va centrada sobre esa superficie, a escala real.
    static SheetMapping onTablet(int clientWidth, int clientHeight, int areaX, int areaY, int areaWidth, int areaHeight,
                                 double tabletWidthMm, double tabletHeightMm, double sheetWidthMm, double sheetHeightMm,
                                 int cellsWidth, int cellsHeight);

    // Píxel del cliente (con decimales) → celda.
    double cellX(double px) const { return (px - originX) / pixelsPerCellX; }
    double cellY(double py) const { return (py - originY) / pixelsPerCellY; }

    // Columnas [first, last) de celdas que cubre la columna de píxeles px del cliente (y lo
    // mismo para filas). Siempre al menos una celda, dentro de la hoja.
    void cellsOfColumn(int px, int& first, int& last) const;
    void cellsOfRow(int py, int& first, int& last) const;

    // Rectángulo de celdas [x0, x1) × [y0, y1) → rectángulo de píxeles del cliente que lo
    // cubre (recortado a la hoja). Devuelve false si queda vacío.
    bool pixelsOfCells(int x0, int y0, int x1, int y1, int& px0, int& py0, int& px1, int& py1) const;

    bool contains(int px, int py) const
    {
        return px >= sheetX && px < sheetX + sheetWidth && py >= sheetY && py < sheetY + sheetHeight;
    }
};

} // namespace cartuchera
