#pragma once

#include "BocetosCore.h"
#include "SheetCache.h"

#include <QElapsedTimer>
#include <QHash>
#include <QPointF>
#include <QWidget>

#include <functional>

namespace appkit {
class Shortcuts;
}

// El cuaderno abierto (HU-137, mesa "Bocetos · cuaderno abierto"): la hoja ocupando la ventana,
// encima de la biblioteca, con el pie. Las teclas llegan por el registro (la ventana tiene el
// foco); el visor solo recibe el mouse y el lápiz para arrastrar la hoja acercada.
class NotebookView : public QWidget {
public:
    NotebookView(appkit::Shortcuts& shortcuts, QWidget* parent);

    // Abre el cuaderno en la última hoja terminada (o la siguiente, si no hay).
    void open(const bocetos::Book& book);
    // Al volver a la biblioteca: suelta las imágenes (la entera ocupa unos 150 MB).
    void release();

    // PgDn/PgUp (±1), Inicio y Fin.
    // HU-138: la hoja `sheet` (desde 1), al volver de Cartuchera.
    void showSheet(int sheet);
    void step(int delta);
    void first();
    void last();
    void zoomIn();
    void zoomOut();

    // HU-145: la hoja que se mira (para el TV) y la línea del TV en la barra.
    const bocetos::Book& book() const { return m_book; }
    const bocetos::Page* currentPage() const { return m_page >= 0 ? &m_pages[m_page] : nullptr; }
    std::function<void(const bocetos::Page&)> onPageShown;
    struct TvLine {
        QString name, status; // vacíos: sin TV
    };
    std::function<TvLine()> tvLine;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void showPage(int page);
    void request(); // pide la actual (y la entera si hace falta) y precarga las vecinas
    void sheetReady(const QString& path, bool full, double ms);
    QRectF area() const;     // donde va la hoja
    QSizeF sheetSize() const; // en px de la hoja
    bool needFull() const;    // acercada más de lo que da la achicada
    void setZoom(double zoom);
    void logShown(const QString& how); // RNF-02: cuánto tardó en pintarse

    appkit::Shortcuts& m_shortcuts;
    SheetCache m_cache;
    bocetos::Book m_book;
    QList<bocetos::Page> m_pages;
    int m_page = -1;
    QSize m_nominal{1600, 1000}; // el tamaño de las hojas en blanco: el de la última terminada
    QHash<QString, QSize> m_sizes; // el de cada terminada (la cabecera del PNG)
    double m_zoom = 1.0;
    QPointF m_pan;
    bool m_dragging = false;
    QPointF m_dragStart, m_panStart;
    QElapsedTimer m_since; // desde que se pidió la hoja (RNF-02)
    bool m_waiting = false; // la hoja actual todavía no se pintó
    QString m_how;          // de dónde salió: "precargada" o "leída en X ms"
};
