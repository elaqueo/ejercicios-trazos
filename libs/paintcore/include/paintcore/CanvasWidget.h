#pragma once

#include <QWidget>

#include "paintcore/BrushLibrary.h"

#include <memory>

namespace paintcore {

// Lienzo de dibujo: pinta con libmypaint a partir de la tableta (presión,
// inclinación y tiempo de cada muestra) o del mouse (presión fija).
class CanvasWidget : public QWidget {
    Q_OBJECT

public:
    explicit CanvasWidget(QWidget* parent = nullptr);
    ~CanvasWidget() override;

    // Cambia el pincel. Si hay un trazo en curso, se aplica desde el siguiente.
    // Un preset inválido deja el pincel por defecto (con un warning en el log).
    void setBrush(const BrushPreset& preset);

    // Presión que se usa al pintar con mouse (0..1).
    static constexpr float kMousePressure = 0.5f;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void tabletEvent(QTabletEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    // pimpl: el estado de libmypaint queda fuera del header público (RNF-05).
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace paintcore
