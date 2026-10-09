#include <ExerciseMenu.h>
#include <ExerciseSession.h>
#include <exercises/Recta.h>

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
        QVERIFY(!groups[0].items[0].enabled);
        QCOMPARE(groups[1].title, QStringLiteral("Rectas"));
        QCOMPARE(groups[1].items[0].id, QStringLiteral("recta"));
        QVERIFY(groups[1].items[0].enabled);
        QCOMPARE(findExercise(exercises, QStringLiteral("recta")), &recta);
        QCOMPARE(findExercise(exercises, kMixedModeId), nullptr);
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
