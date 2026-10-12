#include <exercises/ExerciseMenu.h>
#include <exercises/Recta.h>
#include <exercises/StackSession.h>
#include <exercises/StrokeStacks.h>

#include <QPainter>
#include <QTest>

using namespace ejercicios;

namespace {

// Los bytes de una guía, para compararlas (RNF-03).
QByteArray bytes(const QPicture& picture)
{
    return QByteArray(picture.data(), int(picture.size()));
}

// Lienzo de mentira: la pila que se armó, la guía de cada hoja y si se ven.
class FakeStack : public StackCanvas {
public:
    void resetSheets(int count) override
    {
        sheets = count;
        guides = QList<QPicture>(count);
        ++resets;
    }
    QSize sheetSize() const override { return {800, 450}; }
    void setSheetGuide(int index, const QPicture& guide) override { guides[index] = guide; }
    void setGuidesVisible(bool v) override { visible = v; }

    int sheets = 0;
    int resets = 0;
    QList<QPicture> guides;
    bool visible = false;
};

// Un ejercicio de pila cuya guía cambia de hoja en hoja (como el prisma que gira).
class Cambia : public StackExercise {
public:
    QString id() const override { return QStringLiteral("cambia"); }
    QString title() const override { return QStringLiteral("Cambia"); }
    QString group() const override { return QStringLiteral("Prueba"); }
    QList<appkit::Param> params() const override { return {sheetsParam(4)}; }
    StackGenerated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const override
    {
        StackGenerated out;
        for (int i = 0; i < sheetCount(params); ++i) {
            Generated g;
            QPainter p(&g.guides);
            p.drawLine(zone.center, zone.center + QPointF(10.0 * i, double(seed % 100)));
            p.end();
            out.sheets.append(g);
        }
        return out;
    }
};

} // namespace

class TestStackSession : public QObject {
    Q_OBJECT

private slots:
    // Enter arma una pila de N hojas, cada una con su guía, y con otra semilla otras guías.
    void siguienteArmaLaPila()
    {
        FakeStack canvas;
        const Recta recta;
        const SameOnEverySheet stack(recta, 6);
        StackSession session(&canvas, &stack);
        session.next();
        QCOMPARE(canvas.resets, 1);
        QCOMPARE(canvas.sheets, 6);
        QVERIFY(canvas.visible);
        for (const QPicture& g : canvas.guides)
            QVERIFY(!g.isNull());
        const quint32 before = session.seed();
        const QByteArray first = bytes(canvas.guides[0]);
        session.next();
        QCOMPARE(canvas.resets, 2);
        QVERIFY(session.seed() != before);
        QVERIFY(bytes(canvas.guides[0]) != first);
    }

    // R arma la misma pila, en blanco, con las mismas guías (RNF-03); cada hoja con la suya.
    void repetirDaLasMismasGuias()
    {
        FakeStack canvas;
        const Cambia cambia;
        StackSession session(&canvas, &cambia);
        session.next();
        QCOMPARE(canvas.sheets, 4);
        QList<QByteArray> before;
        for (const QPicture& g : canvas.guides)
            before.append(bytes(g));
        QVERIFY(before[0] != before[1]); // cada hoja con la suya
        session.toggleGuides();
        session.repeat();
        QCOMPARE(canvas.resets, 2);
        QVERIFY(canvas.visible); // repetir las vuelve a mostrar
        for (int i = 0; i < 4; ++i)
            QCOMPARE(bytes(canvas.guides[i]), before[i]);
    }

    // G oculta y muestra las guías de todas las hojas, sin armar otra pila.
    void guiasSeOcultanYMuestran()
    {
        FakeStack canvas;
        const Cambia cambia;
        StackSession session(&canvas, &cambia);
        session.next();
        session.toggleGuides();
        QVERIFY(!canvas.visible);
        QVERIFY(!session.guidesVisible());
        session.toggleGuides();
        QVERIFY(canvas.visible);
        QCOMPARE(canvas.resets, 1);
    }

    // Los parámetros (las hojas, entre ellos) rigen desde el ejercicio siguiente, y se recortan
    // a 2–24 hojas.
    void parametrosDesdeElSiguiente()
    {
        FakeStack canvas;
        const Cambia cambia;
        StackSession session(&canvas, &cambia);
        session.next();
        session.setParams({{kSheetsParam, 12}});
        session.repeat();
        QCOMPARE(canvas.sheets, 4); // R repite el actual igual
        session.next();
        QCOMPARE(canvas.sheets, 12);
        session.setParams({{kSheetsParam, 99}});
        session.next();
        QCOMPARE(canvas.sheets, kMaxSheets);
        session.setParams({{kSheetsParam, 0}});
        session.next();
        QCOMPARE(canvas.sheets, kMinSheets);
        QCOMPARE(session.allParams().value(QStringLiteral("cambia")).toMap().value(kSheetsParam).toInt(), kMinSheets);
    }

    // Lo guardado se carga antes del primer ejercicio.
    void cargaLosParametros()
    {
        FakeStack canvas;
        const Cambia cambia;
        StackSession session(&canvas, &cambia);
        session.loadParams({{QStringLiteral("cambia"), QVariantMap{{kSheetsParam, 7}}}});
        session.next();
        QCOMPARE(canvas.sheets, 7);
    }

    // Modo mixto: cada Enter sortea entre los habilitados; elegir uno del menú lo apaga.
    void modoMixto()
    {
        FakeStack canvas;
        const Recta recta;
        const SameOnEverySheet stack(recta, 6);
        const Cambia cambia;
        const QList<const StackExercise*> all{&stack, &cambia};
        StackSession session(&canvas, &stack);
        session.setMixedPool(all);
        int changes = 0;
        session.onExerciseChanged = [&changes] { ++changes; };
        session.startMixed();
        QVERIFY(session.mixed());
        QSet<QString> seen;
        for (int i = 0; i < 40; ++i) {
            session.next();
            seen.insert(session.exercise()->id());
        }
        QCOMPARE(seen.size(), 2);
        QCOMPARE(changes, 41);
        session.setExercise(&cambia);
        QVERIFY(!session.mixed());
        QCOMPARE(session.exercise(), &cambia);
        QCOMPARE(findExercise(all, QStringLiteral("cambia")), &cambia);
        QCOMPARE(exerciseMenu(all).size(), 3); // el modo mixto y dos grupos
    }

    // HU-122: los trazos de Ejercicios en pila son los de líneas, curvas, elipses y perspectiva
    // (sin escritura, ghosting ni presión), con 6 hojas por defecto; cada uno pone la misma guía
    // (y la misma geometría ideal) en todas las hojas, repetir da las mismas y otra semilla otras.
    void trazosEnPila()
    {
        const StrokeStacks strokes;
        QStringList ids;
        QSet<QString> groups;
        for (const StackExercise* exercise : strokes.exercises()) {
            ids.append(exercise->id());
            groups.insert(exercise->group());
        }
        QCOMPARE(ids, (QStringList{"recta", "hatching", "radiales", "direccion", "curva", "curvaDireccion", "elipse",
                                   "concentricas", "elipsePerspectiva", "cajas", "cajasRotadas"}));
        QCOMPARE(groups, (QSet<QString>{"Líneas", "Curvas", "Elipses", "Perspectiva"}));
        for (const StackExercise* exercise : strokes.exercises()) {
            const QByteArray name = exercise->id().toUtf8();
            QCOMPARE(sheetCount(exercise->defaults()), kStrokeSheets);
            FakeStack canvas;
            StackSession session(&canvas, exercise);
            session.next();
            QVERIFY2(canvas.sheets == kStrokeSheets, name.constData());
            const QByteArray first = bytes(canvas.guides[0]);
            QVERIFY2(!first.isEmpty(), name.constData());
            const QList<QPainterPath> ideal = session.current().sheets[0].ideal;
            for (int i = 1; i < canvas.sheets; ++i) {
                QVERIFY2(bytes(canvas.guides[i]) == first, name.constData());
                QVERIFY2(session.current().sheets[i].ideal == ideal, name.constData());
            }
            session.repeat();
            for (int i = 0; i < canvas.sheets; ++i)
                QVERIFY2(bytes(canvas.guides[i]) == first, name.constData());
            session.next();
            QVERIFY2(bytes(canvas.guides[0]) != first, name.constData());
        }
    }
};

QTEST_MAIN(TestStackSession)
#include "tst_stack_session.moc"
