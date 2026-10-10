// Mesa de animación: rough animation con hojas apiladas, mesa de luz y flip a mano
// (docs/mesa-de-animacion/idea.md y requerimientos.md). Corre sobre el lienzo de libs/lienzo,
// como Cartuchera; las teclas comunes están en lienzo/Lienzo.h.
// HU-84: el armazón. HU-87: Insert, Ctrl+D, Ctrl+N y Supr manejan las hojas, con deshacer.
// El resto de la Fase 1 (flip, mesa de luz, papel de animación, grafito único, toma
// guardada) entra en las historias que siguen.

#include <appkit/Config.h>
#include <appkit/ConfirmOverlay.h>
#include <appkit/Log.h>
#include <lienzo/Lienzo.h>

#include <QApplication>
#include <QDebug>
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

    lienzo::Lienzo canvas(shell, screen, config,
                          {.name = QStringLiteral("mesadeanimacion"), .undo = true, .sheets = true});

    // Hojas (HU-87). Se ve cuál es la activa en F3; las acciones quedan en el log (las verifica
    // la prueba de la app real, scripts/smoke.ps1).
    using lienzo::Lienzo;
    const QString groupSheets = QStringLiteral("Hojas");
    canvas.shortcuts().setColumns({{Lienzo::kGroupLead}, {Lienzo::kGroupSheet, Lienzo::kGroupView},
                                   {groupSheets, Lienzo::kGroupScreen}});
    canvas.shortcuts().setGroup(groupSheets);
    const auto logSheet = [&canvas](const char* what) {
        qInfo().noquote() << QStringLiteral("Hojas: %1 · hoja %2 de %3")
                                 .arg(QString::fromUtf8(what))
                                 .arg(canvas.activeSheet() + 1)
                                 .arg(canvas.sheetCount());
    };
    canvas.shortcuts().add(QStringLiteral("Hoja nueva después"), {{VK_INSERT}}, [&] {
        canvas.insertSheet();
        logSheet("insertada");
    });
    canvas.shortcuts().add(QStringLiteral("Duplicar la hoja"), {{'D', true}}, [&] {
        canvas.duplicateSheet();
        logSheet("duplicada");
    });
    appkit::ConfirmOverlay confirm(&shell);
    const auto removeActive = [&] {
        canvas.removeSheet(canvas.activeSheet());
        logSheet("borrada");
        canvas.focusCanvas();
    };
    confirm.onConfirm = removeActive;
    confirm.onCancel = [&] {
        qInfo() << "Hojas: borrar cancelado";
        canvas.focusCanvas();
    };
    canvas.shortcuts().add(QStringLiteral("Borrar la hoja"), {{VK_DELETE}}, [&] {
        if (canvas.sheetCount() <= 1) {
            qInfo() << "Hojas: es la única, no se borra";
            return;
        }
        if (canvas.activeSheetBlank()) { // en blanco: sin preguntar
            removeActive();
            return;
        }
        confirm.ask(QStringLiteral("¿Borrar la hoja %1?").arg(canvas.activeSheet() + 1),
                    QStringLiteral("Tiene trazos. Se puede deshacer con Z."), Qt::Key_Delete, QStringLiteral("Supr"),
                    QStringLiteral("la borra"));
        canvas.showOverlay(&confirm);
        qInfo() << "Hojas: confirmar borrado";
    });

    canvas.start();
    const int result = QApplication::exec();
    canvas.stop();
    return result;
}
