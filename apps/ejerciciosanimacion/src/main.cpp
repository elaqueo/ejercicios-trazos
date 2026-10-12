// Ejercicios de animación: ejercicios de trazo en pila, con la mesa de luz y el flip a mano de
// la Mesa (docs/ejercicios-animacion/idea.md y requerimientos.md). Corre sobre el lienzo de
// libs/lienzo con la pila de hojas; las teclas comunes están en lienzo/Lienzo.h.
// HU-120: el armazón. Sin toma: la pila vive en memoria y no se guarda.

#include <appkit/Config.h>
#include <appkit/Log.h>
#include <appkit/Theme.h>
#include <lienzo/Lienzo.h>
#include <lienzo/Tone.h>

#include <QApplication>
#include <QDebug>
#include <QScreen>

namespace {

// Grano del papel de animación (HU-97), el mismo de la Mesa.
constexpr int kAnimationGrain = 300;
// Hojas de la pila mientras no haya ejercicios (HU-121 la arma por ejercicio): la propuesta
// de idea.md.
constexpr int kSheets = 12;

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Ejercicios de animación"));
    appkit::installFileLog(QStringLiteral("ejerciciosanimacion"));

    // Monitor y área útil: los de la familia de apps (sección común de config.json); lo propio
    // (la mesa de luz, el panel) en la sección "ejerciciosanimacion".
    appkit::Config config(QStringLiteral("ejerciciosanimacion"));
    QScreen* screen = lienzo::savedScreen(config);

    lienzo::Shell shell;
    shell.setWindowFlag(Qt::FramelessWindowHint);
    shell.setWindowTitle(QApplication::applicationName());
    shell.setScreen(screen); // antes de crear la ventana nativa, o Windows elige el monitor
    shell.setGeometry(screen->geometry());
    shell.show();

    // El medio de la Mesa: papel de animación, perfil recortado, HB y los lápices de color.
    const QColor sheetColor = appkit::theme::kHojaAnimacion;
    lienzo::Lienzo canvas(shell, screen, config,
                          {.name = QStringLiteral("ejerciciosanimacion"),
                           .undo = true,
                           .sheets = true,
                           .profile = lienzo::MediumProfile::Animation,
                           .fixedGrade = lienzo::kHbIndex,
                           .tiltByDefault = false,
                           .paperColor = lienzo::bgra(sheetColor.red(), sheetColor.green(), sheetColor.blue()),
                           .paperGrain = kAnimationGrain,
                           .paperTexture = false,
                           .colorPencils = true,
                           .zoom = true});
    for (int i = 1; i < kSheets; ++i)
        canvas.addSheet(i);

    using lienzo::Lienzo;
    const QString groupSheets = QStringLiteral("Pila de hojas");
    canvas.shortcuts().setOrder({Lienzo::kGroupLead, Lienzo::kGroupSheet, Lienzo::kGroupView, Lienzo::kGroupGestures,
                                 groupSheets, Lienzo::kGroupScreen});
    canvas.shortcuts().setGroup(groupSheets);
    // Flip (HU-90): mantener la flecha recorre la pila; al soltarla, la hoja que se ve queda activa.
    canvas.shortcuts().add(QStringLiteral("Hoja anterior"), {{VK_LEFT}}, [&canvas] { canvas.flip(-1); });
    canvas.shortcuts().add(QStringLiteral("Hoja siguiente"), {{VK_RIGHT}}, [&canvas] { canvas.flip(1); });
    canvas.shortcuts().joinWithPrevious(QStringLiteral("Flip"));
    const auto jumpTo = [&canvas](bool last) {
        if (canvas.jumpToEdge(last))
            qInfo().noquote() << QStringLiteral("Hojas: %1 · %2")
                                     .arg(last ? QStringLiteral("la última") : QStringLiteral("la primera"),
                                          canvas.activeSheetLabel());
    };
    canvas.shortcuts().add(QStringLiteral("Primera hoja"), {{VK_HOME}}, [&] { jumpTo(false); });
    canvas.shortcuts().add(QStringLiteral("Última hoja"), {{VK_END}}, [&] { jumpTo(true); });
    canvas.shortcuts().joinWithPrevious(QStringLiteral("Primera o última hoja"));

    // Mesa de luz (HU-91): la de la Mesa, 2 hojas atrás en naranja y 1 adelante en turquesa. L
    // la prende y apaga; queda guardado.
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
    // Panel de hojas (HU-114): Tab lo muestra y lo oculta; queda guardado. Sin toma, su
    // encabezado lo dice (HU-120).
    bool panelOn = config.value(QStringLiteral("sheetPanel"), true).toBool();
    canvas.setPanelHeading(QStringLiteral("PILA"), QStringLiteral("no se guarda"));
    canvas.setSheetPanel(panelOn);
    canvas.shortcuts().add(QStringLiteral("Panel de hojas"), {{VK_TAB}}, [&] {
        panelOn = !panelOn;
        config.setValue(QStringLiteral("sheetPanel"), panelOn);
        canvas.setSheetPanel(panelOn);
        qInfo() << "Panel de hojas:" << (panelOn ? "visible" : "oculto");
    });

    // Menú del botón del lápiz (HU-116): el de la Mesa.
    canvas.setPenMenu({{QStringLiteral("Mesa de luz"), QStringLiteral("Mesa de luz"), QStringLiteral("◫")},
                       {QStringLiteral("Hoja siguiente"), QStringLiteral("Siguiente"), QStringLiteral("→")},
                       {QStringLiteral("Rehacer"), QStringLiteral("Rehacer"), QStringLiteral("↷")},
                       {QStringLiteral("Vista inicial"), QStringLiteral("Vista inicial"), QStringLiteral("⌂")},
                       {QStringLiteral("Deshacer"), QStringLiteral("Deshacer"), QStringLiteral("↶")},
                       {QStringLiteral("Hoja anterior"), QStringLiteral("Anterior"), QStringLiteral("←")}});

    canvas.start();
    qInfo().noquote() << QStringLiteral("Pila: %1 hojas en memoria").arg(canvas.sheetCount());
    const int result = QApplication::exec();
    canvas.stop();
    return result;
}
