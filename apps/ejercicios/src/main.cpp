// Ejercicios de trazos sobre el lienzo de baja latencia (HU-65): la HB de medios.json y la
// goma para practicar, sobre la hoja = tableta, con la latencia de Cartuchera. Sin
// deshacer: un intento por ejercicio (alcance de v1). Los números quedan para la vista,
// como en el Ejercicios anterior (5 volvía a 0°; 4 y 6, HU-40).
//   → o el botón lateral del lápiz: siguiente ejercicio · Alt+F4 sale · Ctrl+N borra la hoja
//   · el resto de las teclas, las del lienzo (lienzo/Lienzo.h): F3, [ ] , . - = calibración,
//   Ctrl+S, F12

#include "ExerciseSession.h"
#include "exercises/Recta.h"

#include <appkit/Config.h>
#include <appkit/Log.h>
#include <lienzo/Lienzo.h>

#include <QApplication>
#include <QScreen>

namespace {

// El lienzo para la sesión de ejercicios.
class SheetCanvas : public ejercicios::ExerciseCanvas {
public:
    explicit SheetCanvas(lienzo::Lienzo& canvas)
        : m_canvas(canvas)
    {
    }
    void clear() override { m_canvas.clear(); }
    QSize sheetSize() const override { return m_canvas.sheetRect().size(); }
    void setGuides(const QPicture& guides) override { m_canvas.setGuides(guides); }

private:
    lienzo::Lienzo& m_canvas;
};

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Ejercicios de trazos"));
    appkit::installFileLog(QStringLiteral("ejercicios"));

    appkit::Config config(QStringLiteral("ejercicios"));
    QScreen* screen = lienzo::savedScreen(config);

    lienzo::Shell shell;
    shell.setWindowFlag(Qt::FramelessWindowHint);
    shell.setWindowTitle(QApplication::applicationName());
    shell.setScreen(screen); // antes de crear la ventana nativa, o Windows elige el monitor
    shell.setGeometry(screen->geometry());
    shell.show();

    lienzo::Lienzo canvas(shell, screen, config, {.name = QStringLiteral("ejercicios"), .undo = false, .gradeKeys = false});
    SheetCanvas sheet(canvas);
    const ejercicios::Recta recta;
    ejercicios::ExerciseSession session(&sheet, &recta);
    session.regenerate();
    canvas.setAppKeys([&session](UINT vk, bool) {
        if (vk == VK_RIGHT)
            session.next();
    });
    // El botón lateral del lápiz hace lo mismo que →, sin soltar el lápiz.
    canvas.setOnStylusButton([&session] { session.next(); });

    canvas.start();
    const int result = QApplication::exec();
    canvas.stop();
    return result;
}
