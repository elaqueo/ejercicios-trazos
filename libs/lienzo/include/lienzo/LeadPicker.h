#pragma once

#include "lienzo/Media.h"

#include <QImage>
#include <QWidget>

#include <array>
#include <functional>
#include <optional>

namespace lienzo {

// Muestra de una mina (HU-67): un trazo en S con la presión subiendo y bajando, simulado
// con drymedia sobre un papel chico y pintado a `size` píxeles con la escala de la hoja en
// pantalla (`pixelsPerCell`), así el tono y el grano se ven como al dibujar.
QImage leadSample(const Lead& lead, QSize size, double pixelsPerCell);

// Selector de lápices (F5, HU-67): las diez minas con su muestra; la activa resaltada. Es
// una ventana propia sin borde encima del lienzo nativo (un widget hijo quedaría tapado por
// la ventana del swapchain, decisión 5 del 9 de octubre). Se elige con clic o el lápiz, o
// con flechas y Enter; F5 o Esc cierran.
class LeadPicker : public QWidget {
public:
    explicit LeadPicker(QWidget* owner);

    // Minas y activa; rehace solo las muestras de las minas que cambiaron.
    void setLeads(const Grades& grades, int active, double pixelsPerCell);
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
    int rowAt(QPoint point) const;
    QRect rowRect(int row) const;
    void pick(int row);

    Grades m_grades{};
    std::array<QImage, kGradeCount> m_samples;
    std::array<std::optional<Lead>, kGradeCount> m_sampleOf; // con qué valores se hizo cada muestra
    double m_pixelsPerCell = 0;
    int m_active = kHbIndex;
    int m_cursor = kHbIndex; // fila marcada con el teclado
    int m_hover = -1;
};

} // namespace lienzo
