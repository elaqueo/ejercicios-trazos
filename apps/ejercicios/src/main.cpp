#include <appkit/AppWindow.h>

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Ejercicios de trazos"));

    appkit::AppWindow window;
    window.setWindowTitle(QApplication::applicationName());
    window.resize(1280, 800);
    window.show();

    return QApplication::exec();
}
