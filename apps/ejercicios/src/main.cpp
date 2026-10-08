#include <appkit/AppWindow.h>
#include <appkit/Config.h>
#include <appkit/Log.h>
#include <appkit/Paths.h>
#include <paintcore/BrushLibrary.h>

#include <QApplication>

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

    return QApplication::exec();
}
