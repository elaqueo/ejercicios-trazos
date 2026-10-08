#pragma once

#include <QWidget>

namespace paintcore {

// Lienzo de dibujo. Por ahora solo pinta el fondo; el trazo con libmypaint llega en HU-04.
class CanvasWidget : public QWidget {
    Q_OBJECT

public:
    explicit CanvasWidget(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
};

} // namespace paintcore
