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
namespace detail {
class CalibrationOverlay;
}

// Ventana principal de una app de la familia, con el lienzo como contenido.
class AppWindow : public QWidget {
    Q_OBJECT

public:
    explicit AppWindow(QWidget* parent = nullptr);

    paintcore::CanvasWidget* canvas() const { return m_canvas; }

    // Activa el selector de pinceles (F5 lo abre y cierra, Esc lo cierra) y aplica
    // el pincel guardado en la configuración ("brush" en la sección de la app).
    // library y config deben vivir más que la ventana.
    void setupBrushes(const paintcore::BrushLibrary* library, Config* config);

    // Colores del lienzo: la hoja ("paperColor" en la sección común, por defecto
    // theme::kHoja, para que el panel la pueda cambiar) y la zona fuera del área útil
    // (theme::kFuera). config debe vivir más que la ventana.
    void setupCanvasColors(Config* config);

    // Activa el área útil: el lienzo ocupa el rectángulo calibrado para el monitor
    // actual (fuera se ve gris neutro) y F9 lo calibra tocando dos esquinas de la
    // tableta con el lápiz. Sin calibrar, el lienzo ocupa toda la ventana.
    // config debe vivir más que la ventana.
    void setupUsableArea(Config* config);

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
    // Tecla que inicia la calibración del área útil.
    static constexpr Qt::Key kCalibrateKey = Qt::Key_F9;

signals:
    // El área útil del lienzo cambió (primer resize, recalibración, otro monitor). El
    // lienzo ya se reinició; la app regenera lo que dependa del área, como las guías.
    void usableAreaChanged();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void moveEvent(QMoveEvent* event) override;

private:
    void applyBrush(const QString& name);
    void toggleBrushSelector();
    void placeBrushSelector();
    void moveToScreen(QScreen* screen);
    void moveToNextScreen();
    void closeOverlay();
    void startCalibration();
    void finishCalibration(const QRect& rectInWindow);
    void applyUsableArea();

    paintcore::CanvasWidget* m_canvas = nullptr;
    paintcore::BrushSelector* m_brushSelector = nullptr;
    detail::CalibrationOverlay* m_calibration = nullptr;
    const paintcore::BrushLibrary* m_library = nullptr;
    Config* m_config = nullptr;
};

} // namespace appkit
