#include <ExerciseMenu.h>
#include <ExerciseSession.h>
#include <exercises/Curva.h>
#include <exercises/Recta.h>

#include <appkit/Config.h>
#include <appkit/MenuOverlay.h>

#include <QTemporaryDir>
#include <QTest>

using namespace ejercicios;

namespace {

// Lienzo de mentira: cuenta las veces que se borró y guarda las últimas guías.
class FakeCanvas : public ExerciseCanvas {
public:
    void clear() override { ++clears; }
    QSize sheetSize() const override { return size; }
    void setGuides(const QPicture& g) override
    {
        guides = g;
        ++guideChanges;
    }

    QSize size{400, 300};
    int clears = 0;
    int guideChanges = 0;
    QPicture guides;
};

} // namespace

class TestSession : public QObject {
    Q_OBJECT

private slots:
    // → borra la hoja y genera otro ejercicio con otra semilla (y otras guías).
    void siguienteLimpiaYCambiaSemilla()
    {
        FakeCanvas canvas;
        const Recta recta;
        ExerciseSession session(&canvas, &recta);
        session.regenerate();
        QCOMPARE(canvas.guideChanges, 1);
        QVERIFY(!canvas.guides.isNull());
        const quint32 antes = session.seed();
        const auto idealAntes = session.current().ideal;

        session.next();
        QCOMPARE(canvas.clears, 1);
        QCOMPARE(canvas.guideChanges, 2);
        QVERIFY(session.seed() != antes);
        QVERIFY(session.current().ideal != idealAntes);
    }

    // R borra la hoja y vuelve a dibujar el mismo ejercicio (HU-17).
    void repetirLimpiaYConservaElEjercicio()
    {
        FakeCanvas canvas;
        const Recta recta;
        ExerciseSession session(&canvas, &recta);
        session.regenerate();
        const quint32 seed = session.seed();
        const auto ideal = session.current().ideal;

        session.repeat();
        QCOMPARE(canvas.clears, 1);
        QCOMPARE(canvas.guideChanges, 2);
        QCOMPARE(session.seed(), seed);
        QCOMPARE(session.current().ideal, ideal);
    }

    // Elegir otro ejercicio en el menú (HU-11): hoja limpia, semilla nueva, ese ejercicio.
    void cambiarDeEjercicio()
    {
        FakeCanvas canvas;
        const Recta recta, otra;
        ExerciseSession session(&canvas, &recta);
        session.regenerate();
        const quint32 antes = session.seed();
        session.setExercise(&otra);
        QCOMPARE(session.exercise(), &otra);
        QCOMPARE(canvas.clears, 1);
        QCOMPARE(canvas.guideChanges, 2);
        QVERIFY(session.seed() != antes);
    }

    // El menú: modo mixto arriba, destacado y deshabilitado; los ejercicios por grupo.
    void contenidoDelMenu()
    {
        const Recta recta;
        const QList<const Exercise*> exercises{&recta};
        const auto groups = exerciseMenu(exercises);
        QCOMPARE(groups.size(), 2);
        QCOMPARE(groups[0].items.size(), 1);
        QCOMPARE(groups[0].items[0].id, kMixedModeId);
        QVERIFY(groups[0].items[0].featured);
        QVERIFY(groups[0].items[0].enabled); // HU-20
        QCOMPARE(groups[1].title, QStringLiteral("Líneas"));
        QCOMPARE(groups[1].items[0].id, QStringLiteral("recta"));
        QVERIFY(groups[1].items[0].enabled);
        QCOMPARE(findExercise(exercises, QStringLiteral("recta")), &recta);
        QCOMPARE(findExercise(exercises, kMixedModeId), nullptr);
    }

    // Los parámetros del panel rigen desde el ejercicio siguiente; R repite con los de antes.
    void parametrosDesdeElSiguiente()
    {
        FakeCanvas canvas;
        const Recta recta;
        ExerciseSession session(&canvas, &recta);
        session.regenerate();
        const auto ideal = session.current().ideal;
        const QVariantMap antes = session.currentParams();
        QCOMPARE(antes, recta.defaults());

        QVariantMap corto = antes;
        corto.insert(QStringLiteral("distMin"), 0.1);
        corto.insert(QStringLiteral("distMax"), 0.1);
        corto.insert(QStringLiteral("otra"), 3); // no es de la recta: se descarta
        session.setParams(corto);
        QCOMPARE(session.currentParams(), antes); // el actual no cambia
        QCOMPARE(session.current().ideal, ideal);
        session.repeat();
        QCOMPARE(session.currentParams(), antes);
        QCOMPARE(session.current().ideal, ideal);

        session.next();
        QCOMPARE(session.currentParams().value(QStringLiteral("distMax")).toDouble(), 0.1);
        QVERIFY(!session.currentParams().contains(QStringLiteral("otra")));
        // Con distMin = distMax = 0,1, el segmento mide 0,1 del diámetro de la zona.
        const SafeZone zone = SafeZone::withRandomOrientation(QRect(QPoint(0, 0), canvas.size), session.seed());
        QCOMPARE(qRound(session.current().ideal.first().length()), qRound(0.1 * 2 * zone.radius));
        QCOMPARE(session.params(&recta), session.currentParams());
    }

    // HU-13: los parámetros sobreviven a cerrar y abrir la app (config.json).
    void parametrosPersisten()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("config.json"));
        const Recta recta;
        QVariantMap corto = recta.defaults();
        corto.insert(QStringLiteral("distMax"), 0.3);
        {
            FakeCanvas canvas;
            ExerciseSession session(&canvas, &recta);
            session.setParams(corto);
            appkit::Config config(QStringLiteral("ejercicios"), path);
            config.setValue(QStringLiteral("params"), session.allParams());
        }
        FakeCanvas canvas;
        ExerciseSession session(&canvas, &recta);
        appkit::Config config(QStringLiteral("ejercicios"), path);
        session.loadParams(config.value(QStringLiteral("params")).toMap());
        QCOMPARE(session.params(&recta).value(QStringLiteral("distMax")).toDouble(), 0.3);
        QCOMPARE(session.currentParams().value(QStringLiteral("distMax")).toDouble(), 0.3); // ya en el primero
    }

    // HU-20: el sorteo solo da ejercicios del grupo, nunca tres veces seguidas el mismo, y
    // salen todos.
    void sorteoDelModoMixto()
    {
        const Recta a, b, c;
        const QList<const Exercise*> pool{&a, &b};
        QRandomGenerator rng(5);
        QStringList recent;
        QList<const Exercise*> salidos;
        for (int i = 0; i < 1000; ++i) {
            const Exercise* e = ExerciseSession::pickMixed(pool, recent, rng);
            QVERIFY(e == &a || e == &b);
            salidos.append(e);
            recent.append(QString::number(quintptr(e))); // ids distintos por instancia
            if (recent.size() > 2)
                recent.removeFirst();
        }
        // pickMixed compara por id(): con dos Recta iguales no puede distinguirlas, así que el
        // control de "tres seguidas" se prueba con la sesión y ejercicios distintos (abajo).
        QVERIFY(salidos.contains(&a) && salidos.contains(&b));
        QCOMPARE(ExerciseSession::pickMixed({&c}, {QStringLiteral("recta"), QStringLiteral("recta")}, rng), &c);
        QCOMPARE(ExerciseSession::pickMixed({}, {}, rng), nullptr);
    }

    // En la sesión: con dos ejercicios distintos, nunca tres seguidos iguales; cada uno con
    // sus parámetros; R repite el que salió; elegir uno en el menú apaga el modo.
    void modoMixtoEnLaSesion()
    {
        FakeCanvas canvas;
        const Recta recta;
        const Curva curva;
        ExerciseSession session(&canvas, &recta);
        session.setMixedPool(mixedPool({&recta, &curva}, {}));
        QVariantMap corto = recta.defaults();
        corto.insert(QStringLiteral("distMax"), 0.1);
        session.setParams(corto); // de la recta
        int avisos = 0;
        session.onExerciseChanged = [&] { ++avisos; };
        session.startMixed();
        QVERIFY(session.mixed());
        QStringList ids;
        int rectas = 0, curvas = 0;
        for (int i = 0; i < 300; ++i) {
            ids.append(session.exercise()->id());
            if (ids.size() >= 3)
                QVERIFY2(!(ids[ids.size() - 1] == ids[ids.size() - 2] && ids[ids.size() - 2] == ids[ids.size() - 3]),
                         qPrintable(ids.mid(ids.size() - 3).join(',')));
            if (session.exercise() == &recta) {
                ++rectas;
                QCOMPARE(session.currentParams().value(QStringLiteral("distMax")).toDouble(), 0.1);
            } else {
                ++curvas;
                QCOMPARE(session.currentParams(), curva.defaults());
            }
            const Exercise* antes = session.exercise();
            session.repeat();
            QCOMPARE(session.exercise(), antes);
            session.next();
        }
        QVERIFY(rectas > 50 && curvas > 50);
        QCOMPARE(avisos, 301);
        session.setExercise(&recta);
        QVERIFY(!session.mixed());
        for (int i = 0; i < 5; ++i) {
            session.next();
            QCOMPARE(session.exercise(), &recta);
        }
    }

    // El menú real deja elegir el modo mixto, con el lápiz y con el teclado.
    void elegirModoMixtoEnElMenu()
    {
        const Recta recta;
        const Curva curva;
        appkit::MenuOverlay menu(nullptr, QStringLiteral("Ejercicios"), Qt::Key_F4);
        menu.setGroups(exerciseMenu({&recta, &curva}));
        menu.setCurrent(QStringLiteral("curva"));
        QString elegido;
        menu.onPick = [&](const QString& id) { elegido = id; };
        menu.show();
        QTest::mouseClick(&menu, Qt::LeftButton, {}, QPoint(appkit::MenuOverlay::kWidth / 2, 44 + 24));
        QCOMPARE(elegido, kMixedModeId);
        elegido.clear();
        menu.show();
        for (int i = 0; i < 10; ++i) // de más: se frena en el primero, el modo mixto
            QTest::keyClick(&menu, Qt::Key_Up);
        QTest::keyClick(&menu, Qt::Key_Return);
        QCOMPARE(elegido, kMixedModeId);
    }

    // El grupo del modo mixto: los habilitados en orden del catálogo; sin ninguno, todos.
    void grupoDelModoMixto()
    {
        const Recta recta;
        const Curva curva;
        const QList<const Exercise*> all{&recta, &curva};
        QCOMPARE(mixedPool(all, {QStringLiteral("curva")}), (QList<const Exercise*>{&curva}));
        QCOMPARE(mixedPool(all, {}), all);
        QCOMPARE(mixedPool(all, {QStringLiteral("noExiste")}), all);
    }

    // Regenerar (cambio de área) conserva la semilla y, con la misma área, el ejercicio; no
    // borra la hoja.
    void regenerarConservaElEjercicio()
    {
        FakeCanvas canvas;
        const Recta recta;
        ExerciseSession session(&canvas, &recta);
        session.regenerate();
        const quint32 seed = session.seed();
        const auto ideal = session.current().ideal;

        session.regenerate();
        QCOMPARE(session.seed(), seed);
        QCOMPARE(session.current().ideal, ideal);
        QCOMPARE(canvas.clears, 0);

        // Con otra hoja, mismo ejercicio adaptado: la recta queda en la zona nueva.
        canvas.size = QSize(200, 200);
        session.regenerate();
        QCOMPARE(session.seed(), seed);
        const SafeZone zone = SafeZone::withRandomOrientation(QRect(0, 0, 200, 200), seed);
        QVERIFY(zone.contains(session.current().ideal.first()));
    }
};

QTEST_MAIN(TestSession)
#include "tst_session.moc"
