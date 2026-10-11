#include <lienzo/PenMenu.h>

#include <QTest>

#include <cmath>
#include <string>

using namespace lienzo;
using Hit = PenMenu::Hit;

namespace {

QPointF polar(double radius, double degrees)
{
    const double t = degrees * 3.14159265358979 / 180.0;
    return {radius * std::cos(t), -radius * std::sin(t)};
}

std::vector<PenMenuAction> sixActions()
{
    std::vector<PenMenuAction> a;
    for (const char* name : {"Mesa de luz", "Hoja siguiente", "Rehacer", "Vista inicial", "Deshacer", "Hoja anterior"})
        a.push_back({QString::fromUtf8(name), QString::fromUtf8(name), QString(), QString()});
    return a;
}

// Un menú abierto en un área de 600 × 600 con la punta al medio, que anota lo que pasa.
struct Opened {
    PenMenu menu{nullptr};
    int pencil = -2, action = -2;
    std::string dismissed;
    QPointF center{300, 300};
    Opened()
    {
        menu.setPencils({Qt::black, Qt::blue, Qt::red, Qt::green, Qt::magenta}, 0);
        menu.setActions(sixActions());
        menu.onPencil = [this](int i) { pencil = i; };
        menu.onAction = [this](int i) { action = i; };
        menu.onDismiss = [this](const char* how) { dismissed = how; };
        menu.open(QRect(0, 0, 600, 600), QPoint(300, 300));
    }
    QPointF at(double radius, double degrees) const { return center + polar(radius, degrees); }
};

} // namespace

class TestPenMenu : public QObject {
    Q_OBJECT

private slots:
    // La primera arriba, las demás en el sentido del reloj (mesa "Menú del lápiz").
    void angulos()
    {
        QCOMPARE(PenMenu::angleOf(0, 6), 90.0);
        QCOMPARE(PenMenu::angleOf(1, 6), 30.0);
        QCOMPARE(PenMenu::angleOf(3, 6), -90.0);
        QCOMPARE(PenMenu::angleOf(1, 5), 18.0);
    }

    void queHayEnCadaLugar()
    {
        QCOMPARE(PenMenu::hitTest({0, 0}, 5, 6), (Hit{Hit::Center, -1}));
        QCOMPARE(PenMenu::hitTest(polar(68, 90), 5, 6), (Hit{Hit::Pencil, 0}));
        QCOMPARE(PenMenu::hitTest(polar(68, 18), 5, 6), (Hit{Hit::Pencil, 1}));
        QCOMPARE(PenMenu::hitTest(polar(90, 162), 5, 6), (Hit{Hit::Pencil, 4})); // todo el gajo, no solo el punto
        QCOMPARE(PenMenu::hitTest(polar(140, 90), 5, 6), (Hit{Hit::Action, 0}));
        QCOMPARE(PenMenu::hitTest(polar(140, 30), 5, 6), (Hit{Hit::Action, 1}));
        QCOMPARE(PenMenu::hitTest(polar(140, -90), 5, 6), (Hit{Hit::Action, 3}));
        QCOMPARE(PenMenu::hitTest(polar(140, 150), 5, 6), (Hit{Hit::Action, 5}));
        QCOMPARE(PenMenu::hitTest(polar(200, 0), 5, 6), (Hit{Hit::Outside, -1}));
        QCOMPARE(PenMenu::hitTest(polar(68, 90), 0, 6), (Hit{Hit::Center, -1})); // sin lápices de color
    }

    // El anillo entra entero en la hoja: la punta, corrida lo justo.
    void dentroDeLaHoja()
    {
        const int reach = PenMenu::kRadius + PenMenu::kMargin;
        QCOMPARE(PenMenu::centerFor({500, 400}, {1000, 800}), QPoint(500, 400));
        QCOMPARE(PenMenu::centerFor({10, 10}, {1000, 800}), QPoint(reach, reach));
        QCOMPARE(PenMenu::centerFor({990, 790}, {1000, 800}), QPoint(1000 - reach, 800 - reach));
        QCOMPARE(PenMenu::centerFor({10, 10}, {300, 300}), QPoint(150, 150)); // más chica que el anillo
    }

    void unLapizCierra()
    {
        Opened o;
        o.menu.tipDown(o.at(68, -54)); // el 3: rojo
        QCOMPARE(o.pencil, 2);
        QVERIFY(!o.menu.isVisible());
    }

    void unaAccionDejaAbierto()
    {
        Opened o;
        o.menu.tipDown(o.at(140, -150)); // deshacer
        QCOMPARE(o.action, 4);
        QVERIFY(o.menu.isVisible());
        QVERIFY(o.dismissed.empty());
    }

    void cerrarSinElegir()
    {
        {
            Opened o;
            o.menu.tipDown(o.at(220, 0));
            QCOMPARE(o.dismissed, std::string("fuera"));
            QVERIFY(!o.menu.isVisible());
        }
        {
            Opened o;
            o.menu.barrel(false, o.center); // soltó donde abrió: queda abierto
            QVERIFY(o.menu.isVisible());
            o.menu.barrel(true, o.center); // el botón otra vez
            QCOMPARE(o.dismissed, std::string("botón"));
            QCOMPARE(o.pencil, -2);
        }
        {
            Opened o;
            QTest::keyClick(&o.menu, Qt::Key_Escape);
            QCOMPARE(o.dismissed, std::string("Esc"));
        }
    }

    // Mantener el botón, llevar la punta en el aire y soltar sobre la opción la elige.
    void soltarSobreLaOpcion()
    {
        Opened o;
        o.menu.hover(o.at(140, 30));
        o.menu.barrel(false, o.at(140, 30));
        QCOMPARE(o.action, 1);
        Opened p;
        p.menu.barrel(false, p.at(68, 162));
        QCOMPARE(p.pencil, 4);
    }

    // Cerca del borde el anillo se corre: soltar el botón sobre lo que ya estaba bajo la punta
    // al abrir no elige (fue un clic del botón, no un gesto).
    void clicCercaDelBorde()
    {
        PenMenu menu(nullptr);
        int action = -1;
        menu.setActions(sixActions());
        menu.onAction = [&](int i) { action = i; };
        menu.open(QRect(0, 0, 1000, 800), QPoint(960, 330)); // la punta cae sobre "Hoja siguiente"
        QCOMPARE(menu.ringCenter(), QPoint(1000 - PenMenu::kRadius - PenMenu::kMargin, 330));
        menu.barrel(false, QPointF(960, 330));
        QCOMPARE(action, -1);
        QVERIFY(menu.isVisible());
    }
};

QTEST_MAIN(TestPenMenu)
#include "tst_penmenu.moc"
