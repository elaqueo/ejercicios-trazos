#include <ExerciseSession.h>
#include <exercises/Recta.h>

#include <paintcore/CanvasWidget.h>

#include <QImage>
#include <QTest>

using namespace ejercicios;

namespace {

// Píxeles con tinta (las guías se ocultan antes de mirar: el azul también es oscuro).
int inkPixels(paintcore::CanvasWidget& canvas)
{
    canvas.setGuidesVisible(false);
    const QImage image = canvas.grab().toImage();
    canvas.setGuidesVisible(true);
    int count = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            count += qGray(image.pixel(x, y)) < 200 ? 1 : 0;
    return count;
}

} // namespace

class TestSession : public QObject {
    Q_OBJECT

private slots:
    // → limpia el lienzo y genera otro ejercicio con otra semilla.
    void siguienteLimpiaYCambiaSemilla()
    {
        paintcore::CanvasWidget canvas;
        canvas.resize(400, 300);
        const Recta recta;
        ExerciseSession session(&canvas, &recta);
        session.regenerate();
        const quint32 antes = session.seed();
        const auto idealAntes = session.current().ideal;

        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 150));
        for (int x = 50; x <= 380; x += 30)
            QTest::mouseMove(&canvas, QPoint(x, 150), 5);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(380, 150), 5);
        QVERIFY(inkPixels(canvas) > 100);

        session.next();
        QVERIFY(session.seed() != antes);
        QVERIFY(session.current().ideal != idealAntes);
        QCOMPARE(inkPixels(canvas), 0);
    }

    // Regenerar (cambio de área) conserva la semilla y, con la misma área, el ejercicio.
    void regenerarConservaElEjercicio()
    {
        paintcore::CanvasWidget canvas;
        canvas.resize(400, 300);
        const Recta recta;
        ExerciseSession session(&canvas, &recta);
        session.regenerate();
        const quint32 seed = session.seed();
        const auto ideal = session.current().ideal;

        session.regenerate();
        QCOMPARE(session.seed(), seed);
        QCOMPARE(session.current().ideal, ideal);

        // Con otra área, mismo ejercicio adaptado: la recta queda en la zona nueva.
        canvas.setCanvasRect(QRect(0, 0, 200, 200));
        session.regenerate();
        QCOMPARE(session.seed(), seed);
        const SafeZone zone = SafeZone::withRandomOrientation(QRect(0, 0, 200, 200), seed);
        QVERIFY(zone.contains(session.current().ideal.first()));
    }
};

QTEST_MAIN(TestSession)
#include "tst_session.moc"
