#pragma once

#include <QWidget>

namespace paintcore {
class CanvasWidget;
}

namespace appkit {

// Ventana principal de una app de la familia, con el lienzo como contenido.
// Pantalla completa en el monitor elegido y área útil llegan en HU-09 y HU-10.
class AppWindow : public QWidget {
    Q_OBJECT

public:
    explicit AppWindow(QWidget* parent = nullptr);

    paintcore::CanvasWidget* canvas() const { return m_canvas; }

private:
    paintcore::CanvasWidget* m_canvas = nullptr;
};

} // namespace appkit
