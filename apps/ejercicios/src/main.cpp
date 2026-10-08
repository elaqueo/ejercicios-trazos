#include <appkit/AppWindow.h>
#include <appkit/Log.h>
#include <appkit/Paths.h>
#include <paintcore/BrushLibrary.h>

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Ejercicios de trazos"));
    appkit::installFileLog(QStringLiteral("ejercicios"));

    // El selector que los usa llega en HU-06; por ahora solo se cargan y se registran.
    paintcore::BrushLibrary brushes;
    brushes.load(appkit::brushDirectories());

    appkit::AppWindow window;
    window.setWindowTitle(QApplication::applicationName());
    window.resize(1280, 800);
    window.show();

    return QApplication::exec();
}
