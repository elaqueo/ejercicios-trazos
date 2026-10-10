// Mesa de animación: rough animation con hojas apiladas, mesa de luz y flip a mano
// (docs/mesa-de-animacion/idea.md y requerimientos.md). Corre sobre el lienzo de libs/lienzo,
// como Cartuchera; las teclas comunes están en lienzo/Lienzo.h.
// HU-84: el armazón. La pila (HU-82), la mesa de luz en el render (HU-83) y el resto de la
// Fase 1 (flip, Insert, papel de animación, grafito único, toma guardada) entran en las
// historias que siguen.

#include <appkit/Config.h>
#include <appkit/Log.h>
#include <lienzo/Lienzo.h>

#include <QApplication>
#include <QScreen>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Mesa de animación"));
    appkit::installFileLog(QStringLiteral("mesadeanimacion"));

    // Monitor y área útil: los de la familia de apps (sección común de config.json, se eligen
    // y calibran con F10 y F9 en cualquiera de las apps); lo propio (la toma, la vista) en la
    // sección "mesadeanimacion".
    appkit::Config config(QStringLiteral("mesadeanimacion"));
    QScreen* screen = lienzo::savedScreen(config);

    lienzo::Shell shell;
    shell.setWindowFlag(Qt::FramelessWindowHint);
    shell.setWindowTitle(QApplication::applicationName());
    shell.setScreen(screen); // antes de crear la ventana nativa, o Windows elige el monitor
    shell.setGeometry(screen->geometry());
    shell.show();

    lienzo::Lienzo canvas(shell, screen, config, {.name = QStringLiteral("mesadeanimacion"), .undo = true});
    canvas.start();
    const int result = QApplication::exec();
    canvas.stop();
    return result;
}
