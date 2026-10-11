#pragma once

#include "lienzo/Media.h"
#include "lienzo/Tone.h"

#include <drymedia/Paper.h>

#include <QImage>
#include <QWidget>

#include <array>
#include <functional>
#include <optional>

namespace lienzo {

// Muestra de una mina (HU-67): un trazo en S con la presión subiendo y bajando, simulado
// con drymedia sobre un papel chico y pintado a `size` píxeles con la escala de la hoja en
// pantalla (`pixelsPerCell`), así el tono y el grano se ven como al dibujar.
// cellsPerMm y profile: los del papel de la app (HU-112).
QImage leadSample(const Lead& lead, QSize size, double pixelsPerCell, double cellsPerMm = drymedia::kCellsPerMm,
                  MediumProfile profile = MediumProfile::Illustration);
// Lo mismo con cualquier medio (HU-100: los lápices de color), ya con su perfil, sobre una
// hoja de `paperColor`. bold: más presión y tres pasadas (a 300 dpi el trazo de un solo
// repaso liviano casi no se ve en una muestra chica y no deja ver el color).
QImage mediumSample(const drymedia::Medium& medium, QSize size, double pixelsPerCell,
                    double cellsPerMm = drymedia::kCellsPerMm, uint32_t paperColor = kPaperColor, bool bold = false);

// Selector de lápices (F5, HU-67): las diez minas con su muestra; la activa resaltada. Es
// una ventana propia sin borde encima del lienzo nativo (un widget hijo quedaría tapado por
// la ventana del swapchain, decisión 5 del 9 de octubre). Se elige con clic o el lápiz, o
// con flechas y Enter; F5 o Esc cierran. Arriba lleva la imagen de la colección de grafito.
class LeadPicker : public QWidget {
public:
    explicit LeadPicker(QWidget* owner);

    // Minas y activa; rehace solo las muestras de las minas que cambiaron.
    void setLeads(const Grades& grades, int active, double pixelsPerCell);
    // El papel de la app (HU-112): las muestras se dibujan en su resolución y con su perfil.
    void setPaper(double cellsPerMm, MediumProfile profile)
    {
        m_cellsPerMm = cellsPerMm;
        m_profile = profile;
        m_sampleOf.fill(std::nullopt);
    }
    int active() const { return m_active; }

    std::function<void(int)> onPick; // eligió la mina (índice 0 = 2H … 9 = 6B)
    std::function<void()> onClose;   // se cerró sin elegir (F5 o Esc)

    static constexpr int kWidth = 440;
    static constexpr int kRowHeight = 72;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    int headerHeight() const; // la imagen de la colección, al ancho del selector
    int rowAt(QPoint point) const;
    QRect rowRect(int row) const;
    void pick(int row);

    QImage m_header; // encabezado: la colección de grafito (resources/grafito.jpg)
    Grades m_grades{};
    std::array<QImage, kGradeCount> m_samples;
    std::array<std::optional<Lead>, kGradeCount> m_sampleOf; // con qué valores se hizo cada muestra
    double m_pixelsPerCell = 0;
    double m_cellsPerMm = drymedia::kCellsPerMm;
    MediumProfile m_profile = MediumProfile::Illustration;
    int m_active = kHbIndex;
    int m_cursor = kHbIndex; // fila marcada con el teclado
    int m_hover = -1;
};

// Selector de lápices de Mesa de animación (F5, HU-100; mesa "Selector de lápices" del canvas):
// el grafito y los cuatro lápices de color, cada uno con su tecla, su muestra simulada y para
// qué es; el activo en ámbar. Va centrado sobre la hoja, al alcance del lápiz (la hoja es el
// área útil de la tableta). Se elige tocando una fila con el lápiz o con clic, con 1 a 5, o
// con flechas y Enter, y se cierra; F5 o Esc lo cierran sin cambiar. Ventana propia sin borde,
// como LeadPicker.
class PencilPicker : public QWidget {
public:
    explicit PencilPicker(QWidget* owner);
    // graphite: la mina del grafito (la HB). active: -1 el grafito, 0..3 un color. Las muestras
    // se rehacen solo si cambió la escala o la mina.
    void setPencils(const Lead& graphite, int active, double pixelsPerCell, double cellsPerMm, MediumProfile profile,
                    uint32_t paperColor);
    int active() const { return m_active - 1; }
    std::function<void(int)> onPick; // eligió: -1 el grafito, 0..3 un color
    std::function<void()> onClose;   // se cerró sin elegir (F5 o Esc)
    static constexpr int kWidth = 400;
    static constexpr int kRowHeight = 72;
    static constexpr int kRows = 1 + drymedia::kColorCount;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QRect rowRect(int row) const;
    int rowAt(QPoint point) const;
    void pick(int row);
    std::array<QImage, kRows> m_samples; // fila 0 el grafito, 1..4 los colores
    std::optional<Lead> m_graphiteOf;
    double m_pixelsPerCell = 0;
    uint32_t m_paperColor = kPaperColor;
    int m_active = 0; // fila
    int m_cursor = 0;
    int m_hover = -1;
};

} // namespace lienzo
