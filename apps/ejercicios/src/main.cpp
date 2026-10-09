// Ejercicios de trazos sobre el lienzo de baja latencia (HU-65): la HB de medios.json y la
// goma para practicar, sobre la hoja = tableta, con la latencia de Cartuchera. Sin
// deshacer: un intento por ejercicio (alcance de v1). Los números quedan para la vista,
// como en el Ejercicios anterior (5 volvía a 0°; 4 y 6, HU-40).
//   → o el botón lateral del lápiz: siguiente ejercicio · R repite el mismo (HU-17) · F4 menú
//   de ejercicios (HU-11) · F2 panel de configuración (HU-12) · Alt+F4 sale · Ctrl+N borra la hoja
//   · el resto de las teclas, las del lienzo (lienzo/Lienzo.h): F5 lápices, F9 área útil, F10
//   monitor, F3, [ ] tamaño, , . blandura, - = techo, Ctrl+S, F12. Todas en el registro
//   único de atajos (HU-14).

#include "ExerciseMenu.h"
#include "ExerciseSession.h"
#include "exercises/Concentricas.h"
#include "exercises/Curva.h"
#include "exercises/Direccion.h"
#include "exercises/Elipse.h"
#include "exercises/Hatching.h"
#include "exercises/Radiales.h"
#include "exercises/Recta.h"
#include "exercises/Renglon.h"

#include <appkit/Config.h>
#include <appkit/Log.h>
#include <appkit/MenuOverlay.h>
#include <appkit/ParamForm.h>
#include <appkit/SidePanel.h>
#include <drymedia/Paper.h>
#include <lienzo/Lienzo.h>

#include <QApplication>
#include <QLabel>
#include <QVBoxLayout>
#include <QScreen>

namespace {

// El lienzo para la sesión de ejercicios.
class SheetCanvas : public ejercicios::ExerciseCanvas {
public:
    explicit SheetCanvas(lienzo::Lienzo& canvas)
        : m_canvas(canvas)
    {
    }
    void clear() override { m_canvas.clear(); }
    QSize sheetSize() const override { return m_canvas.sheetRect().size(); }
    void setGuides(const QPicture& guides) override { m_canvas.setGuides(guides); }
    double pixelsPerMm() const override { return m_canvas.mapping().pixelsPerCellX * drymedia::kCellsPerMm; }

private:
    lienzo::Lienzo& m_canvas;
};

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Ejercicios de trazos"));
    appkit::installFileLog(QStringLiteral("ejercicios"));

    appkit::Config config(QStringLiteral("ejercicios"));
    QScreen* screen = lienzo::savedScreen(config);

    lienzo::Shell shell;
    shell.setWindowFlag(Qt::FramelessWindowHint);
    shell.setWindowTitle(QApplication::applicationName());
    shell.setScreen(screen); // antes de crear la ventana nativa, o Windows elige el monitor
    shell.setGeometry(screen->geometry());
    shell.show();

    lienzo::Lienzo canvas(shell, screen, config, {.name = QStringLiteral("ejercicios"), .undo = false, .gradeKeys = false});
    SheetCanvas sheet(canvas);
    const ejercicios::Recta recta;
    const ejercicios::Curva curva;
    const ejercicios::Hatching hatching;
    const ejercicios::Radiales radiales;
    const ejercicios::Direccion direccion;
    const ejercicios::Elipse elipse;
    const ejercicios::Concentricas concentricas;
    const ejercicios::Renglon renglon;
    const QList<const ejercicios::Exercise*> exercises{&recta,  &hatching, &radiales,     &direccion,
                                                       &curva,  &elipse,   &concentricas, &renglon};
    // Lo guardado (HU-13): el último ejercicio elegido y los parámetros de cada uno.
    config.remove(QStringLiteral("brush")); // del selector de pinceles de libmypaint (HU-68)
    const ejercicios::Exercise* first =
        ejercicios::findExercise(exercises, config.value(QStringLiteral("exercise")).toString());
    ejercicios::ExerciseSession session(&sheet, first ? first : exercises.first());
    session.loadParams(config.value(QStringLiteral("params")).toMap());
    // Modo mixto (HU-20): los ejercicios que participan y si estaba prendido.
    const auto enabledMixed = [&config] { return config.value(QStringLiteral("mixedExercises")).toStringList(); };
    session.setMixedPool(ejercicios::mixedPool(exercises, enabledMixed()));
    if (config.value(QStringLiteral("mixed"), false).toBool())
        session.startMixed();
    else
        session.regenerate();

    // Menú de ejercicios (HU-11): F4 lo abre y lo cierra; elegir genera ese ejercicio.
    appkit::MenuOverlay menu(&shell, QStringLiteral("Ejercicios"), Qt::Key_F4);
    menu.setGroups(ejercicios::exerciseMenu(exercises));
    // Panel de configuración (HU-12): F2. Pestaña Ejercicio con los parámetros del actual
    // (rigen desde el siguiente) y las del lienzo (Lápiz, Pantalla).
    appkit::SidePanel panel(&shell, QStringLiteral("Configuración"), Qt::Key_F2);
    auto* exercisePage = new QWidget;
    auto* exerciseLayout = new QVBoxLayout(exercisePage);
    exerciseLayout->setContentsMargins(0, 0, 0, 0);
    exerciseLayout->setSpacing(12);
    auto* exerciseTitle = new QLabel(exercisePage);
    QFont titleFont = exerciseTitle->font();
    titleFont.setBold(true);
    exerciseTitle->setFont(titleFont);
    auto* exerciseHint = new QLabel(QStringLiteral("Los cambios rigen desde el ejercicio siguiente (→)."), exercisePage);
    exerciseHint->setObjectName(QStringLiteral("secundario"));
    exerciseHint->setWordWrap(true);
    auto* exerciseForm = new appkit::ParamForm(exercisePage);
    // Sección del modo mixto: una casilla por ejercicio (solo con el modo prendido).
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
    for (const ejercicios::Exercise* exercise : exercises) {
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
        const ejercicios::Exercise* exercise = session.exercise();
        mixedSection->setVisible(session.mixed());
        exerciseTitle->setText(exercise->title());
        exerciseForm->setParams(exercise->params(), session.params(exercise));
    };
    showExerciseParams();
    session.onExerciseChanged = [&session, showExerciseParams] { // en modo mixto, cada → cambia de ejercicio
        qInfo() << "Ejercicio:" << session.exercise()->id() << (session.mixed() ? "(modo mixto)" : "");
        showExerciseParams();
    };
    panel.addTab(exercisePage, QStringLiteral("Ejercicio"));
    canvas.addPanelTabs(panel);
    panel.onClose = [&canvas] { canvas.focusCanvas(); };
    canvas.shortcuts().add(QStringLiteral("Panel de configuración"), {{VK_F2}}, [&] {
        if (panel.isVisible()) {
            panel.hide();
            canvas.focusCanvas();
            return;
        }
        menu.hide();
        canvas.showSidePanel(panel);
    });

    menu.onPick = [&](const QString& id) {
        qInfo() << "Menú:" << id;
        if (id == ejercicios::kMixedModeId) {
            session.startMixed();
            config.setValue(QStringLiteral("mixed"), true);
        } else if (const ejercicios::Exercise* exercise = ejercicios::findExercise(exercises, id)) {
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
        menu.setCurrent(session.mixed() ? ejercicios::kMixedModeId : session.exercise()->id());
        panel.hide();
        canvas.showOverlay(&menu);
    });
    canvas.shortcuts().add(QStringLiteral("Ejercicio siguiente"), {{VK_RIGHT}}, [&session] { session.next(); });
    canvas.shortcuts().add(QStringLiteral("Repetir el ejercicio"), {{'R'}}, [&session] { session.repeat(); }); // HU-17
    // Calibrar con F9 mueve la hoja: el mismo ejercicio, adaptado a la hoja nueva.
    canvas.setOnSheetChanged([&session] { session.regenerate(); });
    // El botón lateral del lápiz hace lo mismo que →, sin soltar el lápiz.
    canvas.setOnStylusButton([&session] { session.next(); });

    canvas.start();
    const int result = QApplication::exec();
    canvas.stop();
    return result;
}
