// Ejercicios de trazos sobre el lienzo de baja latencia (HU-65): la HB de medios.json y la
// goma para practicar, sobre la hoja = tableta, con la latencia de Cartuchera. Sin
// deshacer: un intento por ejercicio (alcance de v1). Los números quedan para la vista,
// como en el Ejercicios anterior (5 volvía a 0°; 4 y 6, HU-40).
//   → o el botón lateral del lápiz: siguiente ejercicio · R repite el mismo (HU-17) · Alt+F4 sale · Ctrl+N borra la hoja
//   · el resto de las teclas, las del lienzo (lienzo/Lienzo.h): F5 lápices, F9 área útil, F10
//   monitor, F3, [ ] tamaño, , . blandura, - = techo, Ctrl+S, F12. Todas en el registro
//   único de atajos (HU-14).

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
    canvas.shortcuts().add(QStringLiteral("Ejercicio siguiente"), {{VK_RIGHT}}, [&session] { session.next(); });
    canvas.shortcuts().add(QStringLiteral("Repetir el ejercicio"), {{'R'}}, [&session] { session.repeat(); }); // HU-17
    // Calibrar con F9 mueve la hoja: el mismo ejercicio, adaptado a la hoja nueva.
    canvas.setOnSheetChanged([&session] { session.regenerate(); });
    // El botón lateral del lápiz hace lo mismo que →, sin soltar el lápiz.
    canvas.setOnStylusButton([&session] { session.next(); });

    canvas.start();
    const int result = QApplication::exec();
    canvas.stop();
    return result;
}
