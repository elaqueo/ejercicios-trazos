#include <appkit/MenuOverlay.h>

#include <QTest>

using appkit::MenuGroup;
using appkit::MenuOverlay;

namespace {

QList<MenuGroup> groups()
{
    return {
        {QString(), {{QStringLiteral("mixto"), QStringLiteral("Modo mixto"), false, true, QStringLiteral("pronto")}}},
        {QStringLiteral("Rectas"), {{QStringLiteral("recta"), QStringLiteral("Recta")},
                                    {QStringLiteral("paralelas"), QStringLiteral("Paralelas")}}},
    };
}

} // namespace

class TestMenuOverlay : public QObject {
    Q_OBJECT

private slots:
    // Flechas y Enter eligen; el cursor arranca en el actual y saltea lo deshabilitado.
    void eligeConTeclado()
    {
        MenuOverlay menu(nullptr, QStringLiteral("Ejercicios"), Qt::Key_F4);
        menu.setGroups(groups());
        menu.setCurrent(QStringLiteral("recta"));
        QString elegido;
        menu.onPick = [&](const QString& id) { elegido = id; };
        menu.show();
        QTest::keyClick(&menu, Qt::Key_Down);
        QTest::keyClick(&menu, Qt::Key_Return);
        QCOMPARE(elegido, QStringLiteral("paralelas"));
        QCOMPARE(menu.current(), QStringLiteral("paralelas"));
        QVERIFY(!menu.isVisible()); // elegir cierra

        // El cursor se frena en los extremos: desde la primera habilitada, subir no hace nada
        // (el modo mixto está deshabilitado) y bajar de más se queda en la última.
        menu.setCurrent(QStringLiteral("recta"));
        menu.show();
        QTest::keyClick(&menu, Qt::Key_Up);
        QTest::keyClick(&menu, Qt::Key_Return);
        QCOMPARE(elegido, QStringLiteral("recta"));
        menu.show();
        for (int i = 0; i < 5; ++i)
            QTest::keyClick(&menu, Qt::Key_Down);
        QTest::keyClick(&menu, Qt::Key_Return);
        QCOMPARE(elegido, QStringLiteral("paralelas"));
    }

    // Un ítem deshabilitado no se elige con el lápiz.
    void deshabilitadoNoSeElige()
    {
        MenuOverlay menu(nullptr, QStringLiteral("Ejercicios"), Qt::Key_F4);
        menu.setGroups(groups());
        int elegidos = 0;
        menu.onPick = [&](const QString&) { ++elegidos; };
        menu.show();
        QTest::mouseClick(&menu, Qt::LeftButton, {}, menu.itemRect(QStringLiteral("mixto")).center());
        QCOMPARE(elegidos, 0);
        QVERIFY(menu.isVisible());
    }

    // HU-77: los grupos en una grilla de tres columnas; las flechas se mueven en las dos
    // direcciones y se frenan en los bordes. Con el lápiz, cada ítem se elige donde está.
    void grillaDeTresColumnas()
    {
        const auto item = [](const char* id) { return appkit::MenuItem{QString::fromLatin1(id), QString::fromLatin1(id)}; };
        QList<MenuGroup> grid{
            {QString(), {{QStringLiteral("mixto"), QStringLiteral("Modo mixto"), true, true}}},
            {QStringLiteral("A"), {item("a1"), item("a2"), item("a3")}},
            {QStringLiteral("B"), {item("b1")}},
            {QStringLiteral("C"), {item("c1"), item("c2")}},
            {QStringLiteral("D"), {item("d1"), item("d2")}},
        };
        MenuOverlay menu(nullptr, QStringLiteral("Ejercicios"), Qt::Key_F4);
        menu.setGroups(grid);
        // A, B y C en la primera fila; D abajo de A.
        QCOMPARE(menu.itemRect("a1").top(), menu.itemRect("b1").top());
        QVERIFY(menu.itemRect("b1").left() > menu.itemRect("a1").right());
        QVERIFY(menu.itemRect("c1").left() > menu.itemRect("b1").right());
        QCOMPARE(menu.itemRect("d1").left(), menu.itemRect("a1").left());
        QVERIFY(menu.itemRect("d1").top() > menu.itemRect("a3").bottom());
        QVERIFY(menu.itemRect("mixto").width() > 3 * menu.itemRect("a1").width()); // a todo el ancho
        QVERIFY(menu.height() < 700);

        QString elegido;
        menu.onPick = [&](const QString& id) { elegido = id; };
        const auto walk = [&](const QString& from, QList<Qt::Key> keys) {
            menu.setCurrent(from);
            menu.show();
            for (const Qt::Key k : keys)
                QTest::keyClick(&menu, k);
            QTest::keyClick(&menu, Qt::Key_Return);
            return elegido;
        };
        QCOMPARE(walk("mixto", {Qt::Key_Down}), QStringLiteral("a1"));          // del destacado, a la primera columna
        QCOMPARE(walk("a1", {Qt::Key_Right}), QStringLiteral("b1"));
        QCOMPARE(walk("a1", {Qt::Key_Right, Qt::Key_Right, Qt::Key_Right}), QStringLiteral("c1")); // se frena
        QCOMPARE(walk("c2", {Qt::Key_Left}), QStringLiteral("b1"));
        QCOMPARE(walk("a3", {Qt::Key_Down}), QStringLiteral("d1"));             // a la fila de abajo, misma columna
        QCOMPARE(walk("d1", {Qt::Key_Up}), QStringLiteral("a3"));
        QCOMPARE(walk("b1", {Qt::Key_Up}), QStringLiteral("mixto"));
        QCOMPARE(walk("d2", {Qt::Key_Up, Qt::Key_Up, Qt::Key_Up, Qt::Key_Up, Qt::Key_Up, Qt::Key_Up}), QStringLiteral("mixto"));

        menu.show();
        QTest::mouseClick(&menu, Qt::LeftButton, {}, menu.itemRect("c2").center());
        QCOMPARE(elegido, QStringLiteral("c2"));
    }

    // Esc y la tecla del menú cierran sin elegir.
    void cierraSinElegir()
    {
        MenuOverlay menu(nullptr, QStringLiteral("Ejercicios"), Qt::Key_F4);
        menu.setGroups(groups());
        int cerrado = 0, elegidos = 0;
        menu.onClose = [&] { ++cerrado; };
        menu.onPick = [&](const QString&) { ++elegidos; };
        menu.show();
        QTest::keyClick(&menu, Qt::Key_F4);
        QVERIFY(!menu.isVisible());
        menu.show();
        QTest::keyClick(&menu, Qt::Key_Escape);
        QCOMPARE(cerrado, 2);
        QCOMPARE(elegidos, 0);
    }
};

QTEST_MAIN(TestMenuOverlay)
#include "tst_menuoverlay.moc"
