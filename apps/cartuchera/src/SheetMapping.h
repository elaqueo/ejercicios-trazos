#pragma once

namespace cartuchera {

// Dónde se dibuja la hoja dentro de la ventana y cómo se pasa de píxeles a celdas de
// simulación. HU-51: la hoja se ajusta a la ventana manteniendo la proporción; HU-52 la
// reemplaza por la escala real de la tableta (área útil calibrada).
struct SheetMapping {
    int clientWidth = 0, clientHeight = 0;
    int cellsWidth = 0, cellsHeight = 0;
    double pixelsPerCell = 1;
    int sheetX = 0, sheetY = 0, sheetWidth = 0, sheetHeight = 0; // rectángulo de la hoja, en píxeles

    static SheetMapping fit(int clientWidth, int clientHeight, int cellsWidth, int cellsHeight);

    // Píxel del cliente (con decimales) → celda.
    double cellX(double px) const { return (px - sheetX) / pixelsPerCell; }
    double cellY(double py) const { return (py - sheetY) / pixelsPerCell; }

    // Rango de celdas [first, last) que cubre la columna/fila de píxeles p (relativa a la
    // hoja, 0..sheetWidth/Height). Siempre al menos una celda.
    void cellsOfPixel(int p, int cells, int& first, int& last) const;

    // Rectángulo de celdas [x0, x1) × [y0, y1) → rectángulo de píxeles del cliente que lo
    // cubre (recortado a la hoja). Devuelve false si queda vacío.
    bool pixelsOfCells(int x0, int y0, int x1, int y1, int& px0, int& py0, int& px1, int& py1) const;
};

} // namespace cartuchera
