#include "ExerciseSession.h"
#include "exercises/Recta.h"

#include <appkit/AppWindow.h>
#include <appkit/Config.h>
#include <appkit/Log.h>
#include <appkit/Paths.h>
#include <paintcore/BrushLibrary.h>
#include <paintcore/CanvasWidget.h>

#include <QApplication>
#include <QShortcut>

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

    // El ejercicio se genera cuando el área útil queda aplicada (primer resize) y se
    // regenera, con la misma semilla, si cambia. → pasa al siguiente.
    const ejercicios::Recta recta;
    ejercicios::ExerciseSession session(window.canvas(), &recta);
    QObject::connect(&window, &appkit::AppWindow::usableAreaChanged, &session, &ejercicios::ExerciseSession::regenerate);
    auto* nextShortcut = new QShortcut(Qt::Key_Right, &window);
    QObject::connect(nextShortcut, &QShortcut::activated, &session, &ejercicios::ExerciseSession::next);
    // El botón lateral del lápiz hace lo mismo que →, sin soltar el lápiz.
    QObject::connect(window.canvas(), &paintcore::CanvasWidget::stylusButtonClicked, &session,
                     &ejercicios::ExerciseSession::next);

    // --ventana: abre en una ventana común, para desarrollar y depurar sin tapar todo.
    if (QApplication::arguments().contains(QStringLiteral("--ventana"))) {
        window.resize(1280, 800);
        window.show();
    } else {
        window.showOnSavedScreen(&config);
    }

    return QApplication::exec();
}
