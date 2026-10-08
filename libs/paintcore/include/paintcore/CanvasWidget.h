#pragma once

#include <QWidget>

#include <memory>

namespace paintcore {

// Lienzo de dibujo. Por ahora solo pinta el fondo; el trazo con libmypaint llega en HU-04.
class CanvasWidget : public QWidget {
    Q_OBJECT

public:
    explicit CanvasWidget(QWidget* parent = nullptr);
    ~CanvasWidget() override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    // pimpl: el estado de libmypaint queda fuera del header público (RNF-05).
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace paintcore
