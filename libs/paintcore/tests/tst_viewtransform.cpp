#include <paintcore/ViewTransform.h>

#include <QTest>

using paintcore::ViewTransform;

namespace {

bool near(QPointF a, QPointF b)
{
    return QLineF(a, b).length() < 1e-9;
}

} // namespace

class TestViewTransform : public QObject {
    Q_OBJECT

private slots:
    void sinRotacionEsIdentidad()
    {
        ViewTransform view;
        view.setCenter({100, 50});
        QVERIFY(near(view.toView({10, 20}), {10, 20}));
        QVERIFY(near(view.toCanvas({10, 20}), {10, 20}));
    }

    void rota90HorarioAlrededorDelCentro()
    {
        ViewTransform view;
        view.setCenter({100, 100});
        view.setAngle(90);
        // Un punto a la derecha del centro queda abajo (horario, y hacia abajo).
        QVERIFY(near(view.toView({150, 100}), {100, 150}));
        QVERIFY(near(view.toView({100, 100}), {100, 100}));
    }

    void idaYVueltaConCualquierAngulo_data()
    {
        QTest::addColumn<double>("angle");
        for (double a : {0.0, 15.0, 37.5, 90.0, 180.0, 271.0, 359.0})
            QTest::addRow("%g", a) << a;
    }

    void idaYVueltaConCualquierAngulo()
    {
        QFETCH(double, angle);
        ViewTransform view;
        view.setCenter({1720, 720});
        view.setAngle(angle);
        for (QPointF p : {QPointF(0, 0), QPointF(3440, 1440), QPointF(123.25, 987.5)})
            QVERIFY(near(view.toCanvas(view.toView(p)), p));
    }

    void normalizaElAngulo()
    {
        QCOMPARE(ViewTransform::normalized(-15), 345.0);
        QCOMPARE(ViewTransform::normalized(360), 0.0);
        QCOMPARE(ViewTransform::normalized(725), 5.0);
    }

    void snapDe15Grados()
    {
        QCOMPARE(ViewTransform::snapped(7.4, 15), 0.0);
        QCOMPARE(ViewTransform::snapped(7.6, 15), 15.0);
        QCOMPARE(ViewTransform::snapped(52, 15), 45.0);
        QCOMPARE(ViewTransform::snapped(-8, 15), 345.0);
        QCOMPARE(ViewTransform::snapped(353, 15), 0.0);
    }
};

QTEST_GUILESS_MAIN(TestViewTransform)
#include "tst_viewtransform.moc"
