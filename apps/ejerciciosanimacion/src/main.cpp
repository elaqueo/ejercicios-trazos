// Ejercicios de animación: ejercicios de trazo en pila, con la mesa de luz y el flip a mano de
// la Mesa (docs/ejercicios-animacion/idea.md y requerimientos.md). Corre sobre el lienzo de
// libs/lienzo con la pila de hojas; las teclas comunes están en lienzo/Lienzo.h.
// HU-120: el armazón. Sin toma: la pila vive en memoria y no se guarda.
// HU-121: la sesión de pila (libs/exercises/StackSession): cada ejercicio arma su pila con su
// guía en cada hoja; Enter, R, G, F4 y F2, y lo elegido queda en config.
// HU-123: el prisma que gira, primero en el menú (grupo "Volumen"). HU-130: el cilindro, a su lado.
// HU-131: el cono y la cabeza.

#include <appkit/Config.h>
#include <appkit/Log.h>
#include <appkit/MenuOverlay.h>
#include <appkit/ParamForm.h>
#include <appkit/SidePanel.h>
#include <appkit/Theme.h>
#include <drymedia/Paper.h>
#include <exercises/CabezaQueGira.h>
#include <exercises/CilindroQueGira.h>
#include <exercises/ConoQueGira.h>
#include <exercises/ExerciseMenu.h>
#include <exercises/PrismaQueGira.h>
#include <exercises/StackSession.h>
#include <exercises/StrokeStacks.h>
#include <lienzo/Lienzo.h>
#include <lienzo/Tone.h>

#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QLabel>
#include <QScreen>
#include <QVBoxLayout>

namespace {

// Grano del papel de animación (HU-97), el mismo de la Mesa.
constexpr int kAnimationGrain = 300;

// El lienzo para la sesión de pila.
class StackSheets : public ejercicios::StackCanvas {
public:
    explicit StackSheets(lienzo::Lienzo& canvas)
        : m_canvas(canvas)
    {
    }
    void resetSheets(int count) override { m_canvas.resetSheets(count); }
    QSize sheetSize() const override { return m_canvas.sheetRect().size(); }
    double pixelsPerMm() const override { return m_canvas.mapping().pixelsPerCellX * drymedia::kCellsPerMm; }
    void setSheetGuide(int index, const QPicture& guide) override { m_canvas.setSheetGuide(index, guide); }
    void setGuidesVisible(bool visible) override { m_canvas.setSheetGuidesVisible(visible); }

private:
    lienzo::Lienzo& m_canvas;
};

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Ejercicios de animación"));
    appkit::installFileLog(QStringLiteral("ejerciciosanimacion"));

    // Monitor y área útil: los de la familia de apps (sección común de config.json); lo propio
    // (el ejercicio, sus parámetros, la mesa de luz, el panel) en la sección "ejerciciosanimacion".
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

    // Los ejercicios de pila: las formas que giran (HU-123, HU-130, HU-131) y los trazos de Ejercicios en
    // pila (HU-122).
    const ejercicios::PrismaQueGira prism;
    const ejercicios::CilindroQueGira cylinder;
    const ejercicios::ConoQueGira cone;
    const ejercicios::CabezaQueGira head;
    const ejercicios::StrokeStacks strokes;
    QList<const ejercicios::StackExercise*> exercises{&prism, &cylinder, &cone, &head};
    exercises.append(strokes.exercises());

    // Lo guardado (RNF-06): el último ejercicio, los parámetros de cada uno y el modo mixto.
    StackSheets sheets(canvas);
    const ejercicios::StackExercise* first =
        ejercicios::findExercise(exercises, config.value(QStringLiteral("exercise")).toString());
    ejercicios::StackSession session(&sheets, first ? first : exercises.first());
    session.loadParams(config.value(QStringLiteral("params")).toMap());
    const auto enabledMixed = [&config] { return config.value(QStringLiteral("mixedExercises")).toStringList(); };
    session.setMixedPool(ejercicios::mixedPool(exercises, enabledMixed()));
    // El encabezado del panel de hojas (HU-121, mesa "Panel de hojas"): el ejercicio y, si está
    // prendido, el modo mixto.
    const auto showHeading = [&] {
        canvas.setPanelHeading(QStringLiteral("EJERCICIO"), session.exercise()->title(),
                               session.mixed() ? QStringLiteral("modo mixto") : QString());
    };
    if (config.value(QStringLiteral("mixed"), false).toBool())
        session.startMixed();
    else
        session.next();
    showHeading();

    using lienzo::Lienzo;
    const QString groupExercises = QStringLiteral("Ejercicios");
    const QString groupSheets = QStringLiteral("Pila de hojas");
    canvas.shortcuts().setOrder({Lienzo::kGroupLead, Lienzo::kGroupSheet, Lienzo::kGroupView, Lienzo::kGroupGestures,
                                 groupExercises, groupSheets, Lienzo::kGroupScreen});

    // Menú de ejercicios (HU-11): F4 lo abre y lo cierra; elegir arma ese ejercicio.
    appkit::MenuOverlay menu(&shell, QStringLiteral("Ejercicios"), Qt::Key_F4);
    menu.setGroups(ejercicios::exerciseMenu(exercises));
    // Panel de configuración (HU-12): F2. Pestaña Ejercicio con los parámetros del actual (rigen
    // desde el siguiente) y las del lienzo (Lápiz, Pantalla).
    appkit::SidePanel panel(&shell, QStringLiteral("Configuración"), Qt::Key_F2);
    auto* exercisePage = new QWidget;
    auto* exerciseLayout = new QVBoxLayout(exercisePage);
    exerciseLayout->setContentsMargins(0, 0, 0, 0);
    exerciseLayout->setSpacing(12);
    auto* exerciseTitle = new QLabel(exercisePage);
    QFont titleFont = exerciseTitle->font();
    titleFont.setBold(true);
    exerciseTitle->setFont(titleFont);
    auto* exerciseHint = new QLabel(QStringLiteral("Los cambios rigen desde el ejercicio siguiente (Enter)."), exercisePage);
    exerciseHint->setObjectName(QStringLiteral("secundario"));
    exerciseHint->setWordWrap(true);
    auto* exerciseForm = new appkit::ParamForm(exercisePage);
    // Modo mixto: una casilla por ejercicio (solo con el modo prendido).
    auto* mixedSection = new QWidget(exercisePage);
    auto* mixedLayout = new QVBoxLayout(mixedSection);
    mixedLayout->setContentsMargins(0, 0, 0, 16);
    mixedLayout->setSpacing(8);
    auto* mixedTitle = new QLabel(QStringLiteral("Modo mixto"), mixedSection);
    mixedTitle->setFont(titleFont);
    auto* mixedHint = new QLabel(QStringLiteral("Participan los marcados (sin ninguno, todos)."), mixedSection);
    mixedHint->setObjectName(QStringLiteral("secundario"));
    mixedHint->setWordWrap(true);
    auto* mixedForm = new appkit::ParamForm(mixedSection);
    QList<appkit::Param> mixedParams;
    QVariantMap mixedValues;
    const QStringList savedMixed = enabledMixed();
    for (const ejercicios::StackExercise* exercise : exercises) {
        mixedParams.append({.key = exercise->id(), .label = exercise->title(), .type = appkit::Param::Type::Toggle,
                            .defaultValue = true});
        mixedValues.insert(exercise->id(), savedMixed.isEmpty() || savedMixed.contains(exercise->id()));
    }
    mixedForm->setParams(mixedParams, mixedValues);
    mixedForm->onChanged = [&session, &config, &exercises](const QVariantMap& values) {
        QStringList ids;
        for (auto it = values.cbegin(); it != values.cend(); ++it)
            if (it.value().toBool())
                ids.append(it.key());
        config.setValue(QStringLiteral("mixedExercises"), ids);
        session.setMixedPool(ejercicios::mixedPool(exercises, ids));
    };
    mixedLayout->addWidget(mixedTitle);
    mixedLayout->addWidget(mixedHint);
    mixedLayout->addWidget(mixedForm);
    exerciseLayout->addWidget(mixedSection);
    exerciseLayout->addWidget(exerciseTitle);
    exerciseLayout->addWidget(exerciseHint);
    exerciseLayout->addWidget(exerciseForm);
    exerciseForm->onChanged = [&session, &config](const QVariantMap& values) {
        session.setParams(values);
        config.setValue(QStringLiteral("params"), session.allParams());
    };
    const auto showExerciseParams = [&session, exerciseTitle, exerciseForm, mixedSection] {
        const ejercicios::StackExercise* exercise = session.exercise();
        mixedSection->setVisible(session.mixed());
        exerciseTitle->setText(exercise->title());
        exerciseForm->setParams(exercise->params(), session.params(exercise));
    };
    showExerciseParams();
    session.onExerciseChanged = [&session, showExerciseParams, showHeading] {
        qInfo() << "Ejercicio:" << session.exercise()->id() << (session.mixed() ? "(modo mixto)" : "");
        showExerciseParams();
        showHeading();
    };
    panel.addTab(exercisePage, QStringLiteral("Ejercicio"));
    canvas.addPanelTabs(panel);
    panel.onClose = [&canvas] {
        qInfo() << "Panel: cerrado";
        canvas.focusCanvas();
    };

    // Enter, R y G (idea.md: las flechas son del flip, así que "siguiente" va a Enter). Las
    // acciones quedan en el log: las verifica la prueba de la app real (scripts/smoke.ps1).
    // RNF-04: de la tecla a la pila armada con sus guías, menos de 300 ms (lo mide el smoke).
    const auto timed = [&canvas](const auto& build) {
        QElapsedTimer time;
        time.start();
        build();
        qInfo().noquote() << QStringLiteral("Pila lista: %1 hojas con sus guías en %2 ms")
                                 .arg(canvas.sheetCount())
                                 .arg(time.elapsed());
    };
    canvas.shortcuts().setGroup(groupExercises);
    canvas.shortcuts().add(QStringLiteral("Ejercicio siguiente"), {{VK_RETURN}}, [&] { timed([&] { session.next(); }); });
    canvas.shortcuts().add(QStringLiteral("Repetir el ejercicio"), {{'R'}}, [&] {
        timed([&] { session.repeat(); });
        qInfo() << "Repetir:" << session.exercise()->id();
    });
    canvas.shortcuts().add(QStringLiteral("Mostrar u ocultar las guías"), {{'G'}}, [&session] { session.toggleGuides(); });
    menu.onPick = [&](const QString& id) {
        qInfo() << "Menú:" << id;
        if (id == ejercicios::kMixedModeId) {
            session.startMixed();
            config.setValue(QStringLiteral("mixed"), true);
        } else if (const ejercicios::StackExercise* exercise = ejercicios::findExercise(exercises, id)) {
            session.setExercise(exercise);
            config.setValue(QStringLiteral("mixed"), false);
            config.setValue(QStringLiteral("exercise"), id);
        }
        canvas.focusCanvas();
    };
    menu.onClose = [&canvas] { canvas.focusCanvas(); };
    canvas.shortcuts().add(QStringLiteral("Menú de ejercicios"), {{VK_F4}}, [&] {
        if (menu.isVisible()) {
            menu.hide();
            canvas.focusCanvas();
            return;
        }
        menu.setGroups(ejercicios::exerciseMenu(exercises, int(ejercicios::mixedPool(exercises, enabledMixed()).size())));
        menu.setCurrent(session.mixed() ? ejercicios::kMixedModeId : session.exercise()->id());
        panel.hide();
        canvas.showOverlay(&menu);
    });
    canvas.shortcuts().add(QStringLiteral("Panel de configuración"), {{VK_F2}}, [&] {
        if (panel.isVisible()) {
            panel.hide();
            qInfo() << "Panel: cerrado";
            canvas.focusCanvas();
            return;
        }
        menu.hide();
        canvas.showSidePanel(panel);
        qInfo() << "Panel: abierto";
    });
    // Calibrar con F9 mueve la hoja: las mismas guías, para la hoja nueva.
    canvas.setOnSheetChanged([&session] { session.regenerate(); });

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
    // Panel de hojas (HU-114): Tab lo muestra y lo oculta; queda guardado.
    bool panelOn = config.value(QStringLiteral("sheetPanel"), true).toBool();
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
    const int result = QApplication::exec();
    canvas.stop();
    return result;
}
