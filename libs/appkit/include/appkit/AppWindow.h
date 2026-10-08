#pragma once

#include <QWidget>

namespace paintcore {
class BrushLibrary;
class BrushSelector;
class CanvasWidget;
}

namespace appkit {

class Config;

// Ventana principal de una app de la familia, con el lienzo como contenido.
// Pantalla completa en el monitor elegido y área útil llegan en HU-09 y HU-10.
class AppWindow : public QWidget {
    Q_OBJECT

public:
    explicit AppWindow(QWidget* parent = nullptr);

    paintcore::CanvasWidget* canvas() const { return m_canvas; }

    // Activa el selector de pinceles (F5 lo abre y cierra, Esc lo cierra) y aplica
    // el pincel guardado en la configuración ("brush" en la sección de la app).
    // library y config deben vivir más que la ventana.
    void setupBrushes(const paintcore::BrushLibrary* library, Config* config);

    // Tecla que abre y cierra el selector de pinceles.
    static constexpr Qt::Key kBrushSelectorKey = Qt::Key_F5;

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void applyBrush(const QString& name);
    void toggleBrushSelector();
    void placeBrushSelector();

    paintcore::CanvasWidget* m_canvas = nullptr;
    paintcore::BrushSelector* m_brushSelector = nullptr;
    const paintcore::BrushLibrary* m_library = nullptr;
    Config* m_config = nullptr;
};

} // namespace appkit
