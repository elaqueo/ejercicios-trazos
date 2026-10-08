#include "exercises/Recta.h"

#include <appkit/AppWindow.h>
#include <appkit/Config.h>
#include <appkit/Log.h>
#include <appkit/Paths.h>
#include <paintcore/BrushLibrary.h>
#include <paintcore/CanvasWidget.h>

#include <QApplication>
#include <QRandomGenerator>
#include <QTimer>

namespace {

// Genera el ejercicio para la zona segura del lienzo actual y muestra sus guías.
void showExercise(paintcore::CanvasWidget* canvas, const ejercicios::Exercise& exercise, quint32 seed)
{
    // Coordenadas del lienzo: el origen es la esquina del área útil.
    const QRect area(QPoint(0, 0), canvas->canvasRect().size());
    const auto zone = ejercicios::SafeZone::withRandomOrientation(area, seed);
    canvas->setGuides(exercise.generate(exercise.defaults(), seed, zone).guides);
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Ejercicios de trazos"));
    appkit::installFileLog(QStringLiteral("ejercicios"));

    appkit::Config config(QStringLiteral("ejercicios"));
    paintcore::BrushLibrary brushes;
    brushes.load(appkit::brushDirectories());

    appkit::AppWindow window;
    window.setupBrushes(&brushes, &config);
    window.setupCanvasColors(&config);
    window.setupUsableArea(&config);
    window.setWindowTitle(QApplication::applicationName());
    // --ventana: abre en una ventana común, para desarrollar y depurar sin tapar todo.
    if (QApplication::arguments().contains(QStringLiteral("--ventana"))) {
        window.resize(1280, 800);
        window.show();
    } else {
        window.showOnSavedScreen(&config);
    }

    // Después del primer resize, cuando el área útil ya está aplicada.
    const ejercicios::Recta recta;
    QTimer::singleShot(0, &window, [&] { showExercise(window.canvas(), recta, QRandomGenerator::global()->generate()); });

    return QApplication::exec();
}
