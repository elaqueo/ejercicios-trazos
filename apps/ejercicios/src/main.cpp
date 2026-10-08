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
    window.setWindowTitle(QApplication::applicationName());
    window.resize(1280, 800);
    window.show();

    return QApplication::exec();
}
