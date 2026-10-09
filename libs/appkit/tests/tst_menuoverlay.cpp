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
        QTest::mouseClick(&menu, Qt::LeftButton, {}, QPoint(MenuOverlay::kWidth / 2, 44 + MenuOverlay::kRowHeight / 2));
        QCOMPARE(elegidos, 0);
        QVERIFY(menu.isVisible());
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
