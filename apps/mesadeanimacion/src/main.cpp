// Mesa de animación: rough animation con hojas apiladas, mesa de luz y flip a mano
// (docs/mesa-de-animacion/idea.md y requerimientos.md). Corre sobre el lienzo de libs/lienzo,
// como Cartuchera; las teclas comunes están en lienzo/Lienzo.h.
// HU-84: el armazón. HU-87: Insert, Ctrl+D, Ctrl+N y Supr manejan las hojas, con deshacer.
// HU-88: ↑ ↓ mueven la hoja activa en la pila. HU-115: Inicio y Fin saltan a la primera y la
// última hoja. HU-114: Tab muestra y oculta el panel de hojas.
// HU-90: ← → flipean. HU-91: L prende y apaga la mesa de luz. HU-85 y HU-86: la toma se abre
// al arrancar y se guarda sola mientras se trabaja.

#include <appkit/Config.h>
#include <appkit/ConfirmOverlay.h>
#include <appkit/Log.h>
#include <appkit/Paths.h>
#include <appkit/Theme.h>
#include <lienzo/Lienzo.h>
#include <lienzo/Tone.h>

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QScreen>

namespace {

// Grano del papel de animación (HU-97), de 2047 en Cartuchera: 300, el diente bajo, con el
// papel a 300 dpi (HU-113, decisión del usuario del 11 de octubre de 2026; antes 900 a 600 dpi).
constexpr int kAnimationGrain = 300;

} // namespace

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

    // Papel de animación (HU-97): casi blanco, liso a la vista (sin textura ni su atajo) y con
    // el diente más bajo en la simulación (kAnimationGrain).
    const QColor sheetColor = appkit::theme::kHojaAnimacion;
    lienzo::Lienzo canvas(shell, screen, config,
                          {.name = QStringLiteral("mesadeanimacion"),
                           .undo = true,
                           .sheets = true,
                           .profile = lienzo::MediumProfile::Animation, // medio recortado (HU-112)
                           .fixedGrade = lienzo::kHbIndex, // un solo grafito, HB (HU-98: elegida dibujando)
                           .tiltByDefault = false,
                           .paperColor = lienzo::bgra(sheetColor.red(), sheetColor.green(), sheetColor.blue()),
                           .paperGrain = kAnimationGrain,
                           .paperTexture = false});

    // Hojas (HU-87). Se ve cuál es la activa en F3; las acciones quedan en el log (las verifica
    // la prueba de la app real, scripts/smoke.ps1).
    using lienzo::Lienzo;
    const QString groupSheets = QStringLiteral("Hojas");
    canvas.shortcuts().setColumns({{Lienzo::kGroupLead}, {Lienzo::kGroupSheet, Lienzo::kGroupView},
                                   {groupSheets, Lienzo::kGroupScreen}});
    canvas.shortcuts().setGroup(groupSheets);
    // Flip (HU-90): mantener la flecha recorre la pila; al soltarla, la hoja que se ve queda activa.
    canvas.shortcuts().add(QStringLiteral("Hoja anterior"), {{VK_LEFT}}, [&canvas] { canvas.flip(-1); });
    canvas.shortcuts().add(QStringLiteral("Hoja siguiente"), {{VK_RIGHT}}, [&canvas] { canvas.flip(1); });
    canvas.shortcuts().joinWithPrevious(QStringLiteral("Flip"));

    // Mesa de luz (HU-91): 2 hojas hacia atrás en rojo y 1 hacia adelante en verde, la más
    // lejana más tenue (tokens de la mesa "Mesa de luz"). L la prende y apaga; queda guardada.
    namespace theme = appkit::theme;
    const auto tint = [](const QColor& c, float opacity) {
        return lienzo::Tint{lienzo::bgra(c.red(), c.green(), c.blue()), opacity};
    };
    const lienzo::LightTable lightTable{
        {tint(theme::kMesaLuzAnterior, theme::kMesaLuzOpacidad[0]), tint(theme::kMesaLuzAnterior, theme::kMesaLuzOpacidad[1])},
        {tint(theme::kMesaLuzSiguiente, theme::kMesaLuzOpacidad[0])}};
    bool lightOn = config.value(QStringLiteral("lightTable"), true).toBool();
    const auto applyLight = [&] {
        canvas.setLightTable(lightOn ? lightTable : lienzo::LightTable{});
        qInfo() << "Mesa de luz:" << (lightOn ? "prendida" : "apagada");
    };
    applyLight();
    canvas.shortcuts().add(QStringLiteral("Mesa de luz"), {{'L'}}, [&] {
        lightOn = !lightOn;
        config.setValue(QStringLiteral("lightTable"), lightOn);
        applyLight();
    });
    // Panel de hojas (HU-114): Tab lo muestra y lo oculta; queda guardado.
    bool panelOn = config.value(QStringLiteral("sheetPanel"), true).toBool();
    canvas.setSheetPanel(panelOn);
    canvas.shortcuts().add(QStringLiteral("Panel de hojas"), {{VK_TAB}}, [&] {
        panelOn = !panelOn;
        config.setValue(QStringLiteral("sheetPanel"), panelOn);
        canvas.setSheetPanel(panelOn);
        qInfo() << "Panel de hojas:" << (panelOn ? "visible" : "oculto");
    });
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
    // Reordenar (HU-88): la activa un lugar hacia el principio o el final, con deshacer.
    const auto moveActive = [&](int step) {
        if (canvas.moveActiveSheet(step))
            logSheet(step < 0 ? "movida hacia el principio" : "movida hacia el final");
        else
            qInfo() << "Hojas: ya está en el extremo, no se mueve";
    };
    canvas.shortcuts().add(QStringLiteral("Mover la hoja hacia el principio"), {{VK_UP}}, [&] { moveActive(-1); });
    canvas.shortcuts().add(QStringLiteral("Mover la hoja hacia el final"), {{VK_DOWN}}, [&] { moveActive(1); });
    canvas.shortcuts().joinWithPrevious(QStringLiteral("Mover la hoja"));
    // Saltar a un extremo (HU-115): como soltar un flip ahí, sin deshacer.
    const auto jumpTo = [&](bool last) {
        const int target = last ? canvas.sheetCount() - 1 : 0;
        if (target == canvas.activeSheet()) {
            qInfo() << "Hojas: ya está en" << (last ? "la última" : "la primera");
            return;
        }
        canvas.activateSheet(target);
        logSheet(last ? "la última" : "la primera");
    };
    canvas.shortcuts().add(QStringLiteral("Primera hoja"), {{VK_HOME}}, [&] { jumpTo(false); });
    canvas.shortcuts().add(QStringLiteral("Última hoja"), {{VK_END}}, [&] { jumpTo(true); });
    canvas.shortcuts().joinWithPrevious(QStringLiteral("Primera o última hoja"));
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

    // La toma (HU-85): la última que se usó ("take" en config.json, en <datos>/tomas; la
    // primera vez toma-1), o la de --toma <carpeta>, que no cambia config.json (la usa la
    // prueba de la app real, para no tocar la del usuario). Se guarda sola (HU-86).
    const QStringList args = QApplication::arguments();
    const qsizetype takeArg = args.indexOf(QStringLiteral("--toma"));
    if (takeArg >= 0 && takeArg + 1 < args.size()) {
        canvas.useTake(args[takeArg + 1]);
    } else {
        QString name = QFileInfo(config.value(QStringLiteral("take")).toString()).fileName(); // solo el nombre
        if (name.isEmpty())
            name = QStringLiteral("toma-1");
        // Si la toma no se pudo abrir y se usa otra (la de al lado), esa queda como la última.
        const QString used = canvas.useTake(QDir(appkit::familyDataDirectory()).filePath(QStringLiteral("tomas/") + name));
        config.setValue(QStringLiteral("take"), QFileInfo(used).fileName());
    }

    canvas.start();
    const int result = QApplication::exec();
    canvas.stop(); // espera a que la toma esté escrita
    return result;
}
