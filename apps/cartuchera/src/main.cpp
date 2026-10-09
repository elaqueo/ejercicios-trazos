// Cartuchera: dibujo con medios secos (docs/medios-secos/arquitectura.md). Grafito de
// 2H a 6B y goma sobre una hoja del tamaño de la tableta, sin interfaz, en el lienzo de libs/lienzo (teclas
// comunes en lienzo/Lienzo.h).
//   Alt+F4 sale · Ctrl+N hoja nueva · Z deshace y Ctrl+Y rehace (hasta 100 trazos) ·
//   1 a 0 eligen la dureza (2H … 6B) · el extremo goma borra · F3 latencia y herramienta ·
//   [ ] tamaño, , . blandura, - = techo calibran la mina (o la goma, con el lápiz dado vuelta) · Ctrl+S guarda en
//   medios.json · F5 lápices · F9 área útil · F10 monitor · F12 guarda la imagen de pantalla
//   Diagnóstico: --grabar (muestras en <datos>/cartuchera-muestras.csv) · --bench [undo] [costado]

#include <appkit/Config.h>
#include <appkit/Log.h>
#include <lienzo/Lienzo.h>

#include <QApplication>
#include <QScreen>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Cartuchera"));
    appkit::installFileLog(QStringLiteral("cartuchera"));

    // Monitor y área útil: los de la familia de apps (sección común de config.json), los
    // que se eligen y calibran con F10 y F9 (en cualquiera de las apps).
    appkit::Config config(QStringLiteral("cartuchera"));
    QScreen* screen = lienzo::savedScreen(config);

    lienzo::Shell shell;
    shell.setWindowFlag(Qt::FramelessWindowHint);
    shell.setWindowTitle(QApplication::applicationName());
    shell.setScreen(screen); // antes de crear la ventana nativa, o Windows elige el monitor
    shell.setGeometry(screen->geometry());
    shell.show();

    lienzo::Lienzo canvas(shell, screen, config, {.name = QStringLiteral("cartuchera"), .undo = true});
    canvas.start();
    const int result = QApplication::exec();
    canvas.stop();
    return result;
}
