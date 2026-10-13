// Cartuchera: dibujo con medios secos (docs/medios-secos/arquitectura.md). Grafito de
// 2H a 6B y goma sobre una hoja del tamaño de la tableta, sin interfaz, en el lienzo de libs/lienzo (teclas
// comunes en lienzo/Lienzo.h).
//   Alt+F4 sale · Ctrl+N hoja nueva · Z deshace y Ctrl+Y rehace (hasta 100 trazos) ·
//   F5 elige la dureza (2H … 6B) · 4 / 6 giran la vista y 5 la vuelve a 0° (como Ejercicios) · la rueda de la
//   tableta gira libre · el extremo goma borra · I costado sí o no · E punta seca · A afila · F3 latencia y herramienta ·
//   [ ] tamaño, , . blandura, - = techo calibran la mina (o la goma, con el lápiz dado vuelta) · Ctrl+S guarda en
//   medios.json · F5 lápices · F9 área útil · F10 monitor · F12 guarda la imagen de pantalla
//   Diagnóstico: --grabar (muestras en <datos>/cartuchera-muestras.csv) · --bench [undo] [costado]
//   Modo cuaderno (HU-133, Bocetos): --hoja <carpeta> abre esa hoja de un cuaderno con su papel y
//   la guarda sola; Ctrl+Enter la termina (terminada.png) y cierra; cerrar deja el borrador.

#include "NotebookMode.h"

#include <appkit/ConfirmOverlay.h>
#include <appkit/Config.h>
#include <appkit/Log.h>
#include <lienzo/Lienzo.h>

#include <QApplication>
#include <QDir>
#include <QScreen>

#include <optional>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Cartuchera"));
    appkit::installFileLog(QStringLiteral("cartuchera"));

    // Monitor y área útil: los de la familia de apps (sección común de config.json), los
    // que se eligen y calibran con F10 y F9 (en cualquiera de las apps).
    // Modo cuaderno (HU-133): si la hoja no se puede dibujar (terminada, fuera de orden, no es de
    // un cuaderno), sale sin abrir, con el motivo en el log.
    std::optional<cartuchera::NotebookSheet> sheet;
    if (const QString arg = cartuchera::sheetArgument(QApplication::arguments()); !arg.isEmpty()) {
        QString error;
        sheet = cartuchera::notebookSheet(arg, &error);
        if (!sheet) {
            qWarning().noquote() << "Cuaderno: no se abre:" << error;
            return 2;
        }
        qInfo().noquote() << QStringLiteral("Cuaderno: %1 · hoja %2 de %3 · papel %4")
                                 .arg(sheet->notebook.name)
                                 .arg(sheet->index)
                                 .arg(sheet->notebook.maxSheets)
                                 .arg(cartuchera::paperName(sheet->notebook.paper));
    }

    appkit::Config config(QStringLiteral("cartuchera"));
    QScreen* screen = lienzo::savedScreen(config);

    lienzo::Shell shell;
    shell.setWindowFlag(Qt::FramelessWindowHint);
    shell.setWindowTitle(sheet ? QStringLiteral("Cartuchera · %1 · hoja %2").arg(sheet->notebook.name).arg(sheet->index)
                               : QApplication::applicationName());
    shell.setScreen(screen); // antes de crear la ventana nativa, o Windows elige el monitor
    shell.setGeometry(screen->geometry());
    shell.show();

    lienzo::LienzoOptions options{.name = QStringLiteral("cartuchera"), .undo = true};
    if (sheet) { // el papel del cuaderno; sin Ctrl+N, que borraría el borrador sin deshacer
        const QColor color = sheet->notebook.paper.color;
        options.paperColor = lienzo::bgra(uint8_t(color.red()), uint8_t(color.green()), uint8_t(color.blue()));
        options.paperGrain = sheet->notebook.paper.grain;
        options.textureLevel = sheet->notebook.paper.texture;
        options.clearKey = false;
    }
    lienzo::Lienzo canvas(shell, screen, config, options);

    appkit::ConfirmOverlay confirm(&shell);
    if (sheet) {
        canvas.useTake(sheet->sheetDir);
        const QString name = sheet->notebook.name;
        canvas.setSideNote(lienzo::SideNote{
            .label = QStringLiteral("CUADERNO"),
            .title = name,
            .swatch = sheet->notebook.cover,
            .detail = QStringLiteral("hoja %1 de %2").arg(sheet->index).arg(sheet->notebook.maxSheets),
            .keys = {{QStringLiteral("Ctrl+Enter"), QStringLiteral("terminar la hoja · ya no se edita")},
                     {QStringLiteral("Ctrl+Q"), QStringLiteral("cerrar: queda como borrador")},
                     {QStringLiteral("Z"), QStringLiteral("deshacer, como siempre")}},
            .footer = QStringLiteral("papel · %1").arg(cartuchera::paperName(sheet->notebook.paper)),
            .compact = QStringLiteral("%1 · hoja %2 · Ctrl+Enter termina").arg(name).arg(sheet->index)});

        // Terminar (HU-133): la imagen primero (aparece entera o no aparece), después la fecha del
        // cuaderno; al cerrar, stop() espera a que la toma esté escrita.
        const auto finish = [&] {
            if (!canvas.exportActiveSheet(QDir(sheet->sheetDir).filePath(QString::fromLatin1(notebooks::kFinishedImage)))) {
                qWarning() << "Cuaderno: la hoja no se pudo terminar";
                canvas.focusCanvas();
                return;
            }
            if (!cartuchera::markUpdated(*sheet, QDate::currentDate()))
                qWarning() << "Cuaderno: no se pudo actualizar la fecha del cuaderno";
            qInfo().noquote() << QStringLiteral("Cuaderno: hoja %1 terminada").arg(sheet->index);
            QApplication::quit();
        };
        confirm.onConfirm = finish;
        confirm.onCancel = [&] {
            qInfo() << "Cuaderno: terminar cancelado";
            canvas.focusCanvas();
        };
        canvas.shortcuts().setGroup(lienzo::Lienzo::kGroupSheet);
        canvas.shortcuts().add(QStringLiteral("Terminar la hoja"), {{VK_RETURN, true}}, [&] {
            if (canvas.activeSheetBlank()) { // en blanco: se pregunta
                confirm.ask(QStringLiteral("¿Terminar la hoja %1 en blanco?").arg(sheet->index),
                            QStringLiteral("Queda vacía en el cuaderno y no se puede editar."), Qt::Key_Return,
                            QStringLiteral("Ctrl+Enter"), QStringLiteral("la termina"));
                canvas.showOverlay(&confirm);
                qInfo() << "Cuaderno: confirmar terminar en blanco";
                return;
            }
            finish();
        });
    }
    canvas.start();
    const int result = QApplication::exec();
    canvas.stop();
    return result;
}
