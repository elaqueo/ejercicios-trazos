#pragma once

#include <QWidget>

class QScreen;

namespace paintcore {
class BrushLibrary;
class BrushSelector;
class CanvasWidget;
}

namespace appkit {

class Config;

// Ventana principal de una app de la familia, con el lienzo como contenido.
// El área útil llega en HU-10.
class AppWindow : public QWidget {
    Q_OBJECT

public:
    explicit AppWindow(QWidget* parent = nullptr);

    paintcore::CanvasWidget* canvas() const { return m_canvas; }

    // Activa el selector de pinceles (F5 lo abre y cierra, Esc lo cierra) y aplica
    // el pincel guardado en la configuración ("brush" en la sección de la app).
    // library y config deben vivir más que la ventana.
    void setupBrushes(const paintcore::BrushLibrary* library, Config* config);

    // Muestra la ventana a pantalla completa en el monitor guardado ("monitor" en la
    // sección común: la tableta se mapea a un monitor y vale para toda la familia),
    // o en el principal si no hay guardado o no está conectado. F10 pasa al monitor
    // siguiente y lo guarda; si el monitor se desconecta, pasa al principal.
    // config debe vivir más que la ventana.
    void showOnSavedScreen(Config* config);

    // Tecla que abre y cierra el selector de pinceles.
    static constexpr Qt::Key kBrushSelectorKey = Qt::Key_F5;
    // Tecla que pasa la ventana al monitor siguiente.
    static constexpr Qt::Key kNextScreenKey = Qt::Key_F10;

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void applyBrush(const QString& name);
    void toggleBrushSelector();
    void placeBrushSelector();
    void moveToScreen(QScreen* screen);
    void moveToNextScreen();

    paintcore::CanvasWidget* m_canvas = nullptr;
    paintcore::BrushSelector* m_brushSelector = nullptr;
    const paintcore::BrushLibrary* m_library = nullptr;
    Config* m_config = nullptr;
};

} // namespace appkit
