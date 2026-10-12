#include <appkit/Shortcuts.h>
#include <appkit/ShortcutsOverlay.h>

#include <windows.h>

#include <QTest>

using appkit::ShortcutGroup;
using appkit::Shortcuts;
using appkit::ShortcutsOverlay;

namespace {

int rowCount(const QList<ShortcutGroup>& groups)
{
    int n = 0;
    for (const ShortcutGroup& g : groups)
        n += int(g.rows.size());
    return n;
}

} // namespace

class TestShortcutsOverlay : public QObject {
    Q_OBJECT

private slots:
    // HU-78: la lista sale del registro, por grupo y en el orden pedido (HU-117); los grupos
    // que no figuran van al final.
    void listaPorGrupos()
    {
        Shortcuts keys;
        keys.setOrder({QStringLiteral("Lápiz"), QStringLiteral("Ejercicios"), QStringLiteral("Pantalla")});
        keys.setGroup(QStringLiteral("Pantalla"));
        keys.add(QStringLiteral("Latencia"), {{VK_F3}}, [] {});
        keys.setGroup(QStringLiteral("Lápiz"));
        keys.add(QStringLiteral("Afilar"), {{'A'}}, [] {});
        keys.setGroup(QStringLiteral("Otro"));
        keys.add(QStringLiteral("Suelto"), {{'Q'}}, [] {});
        keys.setGroup(QStringLiteral("Ejercicios"));
        keys.add(QStringLiteral("Repetir"), {{'R'}}, [] {});

        const QList<ShortcutGroup> sheet = keys.sheet();
        QCOMPARE(sheet.size(), 4);
        QCOMPARE(sheet[0].title, QStringLiteral("Lápiz"));
        QCOMPARE(sheet[1].title, QStringLiteral("Ejercicios"));
        QCOMPARE(sheet[2].title, QStringLiteral("Pantalla"));
        QCOMPARE(sheet[3].title, QStringLiteral("Otro"));
        QCOMPARE(sheet[0].rows.first().keys, QStringList{QStringLiteral("A")});
        QCOMPARE(sheet[0].rows.first().name, QStringLiteral("Afilar"));
    }

    // Cada atajo registrado aparece; el de dos teclas ('4' y Num 4) una sola vez, con la
    // primera; un par en una fila con su nombre; los gestos marcados como tales.
    void cadaAtajoUnaVez()
    {
        Shortcuts keys;
        keys.setGroup(QStringLiteral("Vista"));
        keys.add(QStringLiteral("Izquierda"), {{'4'}, {VK_NUMPAD4}}, [] {});
        keys.add(QStringLiteral("Derecha"), {{'6'}, {VK_NUMPAD6}}, [] {});
        keys.joinWithPrevious(QStringLiteral("Girar de a 15°"));
        keys.add(QStringLiteral("Vista a 0°"), {{'5'}, {VK_NUMPAD5}}, [] {});
        keys.note(QStringLiteral("Shift+arrastrar"), QStringLiteral("Girar libre"));
        keys.setGroup(QStringLiteral("Pantalla"));
        keys.add(QStringLiteral("Atajos"), {{VK_OEM_COMMA, true}}, [] {});

        const QList<ShortcutGroup> sheet = keys.sheet();
        QCOMPARE(rowCount(sheet), 4);
        const QList<appkit::ShortcutRow>& vista = sheet[0].rows;
        QCOMPARE(vista[0].keys, (QStringList{QStringLiteral("4"), QStringLiteral("6")}));
        QCOMPARE(vista[0].name, QStringLiteral("Girar de a 15°"));
        QCOMPARE(vista[1].keys, QStringList{QStringLiteral("5")});
        QVERIFY(!vista[1].gesture);
        QCOMPARE(vista[2].keys, QStringList{QStringLiteral("Shift+arrastrar")});
        QVERIFY(vista[2].gesture);
        QCOMPARE(sheet[1].rows[0].keys, QStringList{QStringLiteral("Ctrl+,")});
        // Los gestos no son teclas: no se disparan.
        QVERIFY(!keys.trigger('S', false));
    }

    // Un atajo cuyas teclas eran todas de otro no tiene fila (no hace nada).
    void sinTeclasNoSeMuestra()
    {
        Shortcuts keys;
        keys.add(QStringLiteral("Primera"), {{'R'}}, [] {});
        keys.add(QStringLiteral("Repetida"), {{'R'}}, [] {});
        QCOMPARE(rowCount(keys.sheet()), 1);
    }

    // HU-117: las columnas salen parejas, con los grupos en orden: el corte que deja la columna
    // más alta lo más baja posible; si sobran columnas, las últimas quedan vacías.
    void columnasParejas()
    {
        using Columns = QList<QList<int>>;
        QCOMPARE(ShortcutsOverlay::balance({100, 100, 100, 100}, 4, 10), (Columns{{0}, {1}, {2}, {3}}));
        // 300 sola; después 100 + 100 + 100 (+ 2 huecos) = 320 en una, contra 300 y 210.
        QCOMPARE(ShortcutsOverlay::balance({300, 100, 100, 100}, 2, 10), (Columns{{0}, {1, 2, 3}}));
        QCOMPARE(ShortcutsOverlay::balance({50, 50, 50, 50, 200}, 2, 0), (Columns{{0, 1, 2, 3}, {4}}));
        QCOMPARE(ShortcutsOverlay::balance({80, 60}, 4, 10), (Columns{{0}, {1}, {}, {}}));
        QCOMPARE(ShortcutsOverlay::balance({}, 3, 10), (Columns{{}, {}, {}}));
    }

    // HU-117: la página toma las cuatro columnas y entra en el lugar que hay: sin límite, filas de
    // 30 px; con poco alto, las filas se achican (no menos de 22 px) y el ancho no pasa del lugar.
    void entraEnLaPantalla()
    {
        Shortcuts keys;
        for (int g = 0; g < 8; ++g) {
            keys.setGroup(QStringLiteral("G%1").arg(g));
            for (int i = 0; i < 6; ++i)
                keys.add(QStringLiteral("A%1-%2").arg(g).arg(i), {{unsigned('A' + (g * 6 + i) % 26), g * 6 + i >= 26}}, [] {});
            keys.note(QStringLiteral("gesto %1").arg(g), QStringLiteral("algo"));
        }
        ShortcutsOverlay overlay(nullptr);
        overlay.setSheet(QStringLiteral("App"), keys.sheet());
        QCOMPARE(overlay.rowHeight(), ShortcutsOverlay::kRowHeight);
        QCOMPARE(overlay.width(), ShortcutsOverlay::kWidth);
        QCOMPARE(overlay.columns().size(), ShortcutsOverlay::kColumns);
        for (const QList<int>& column : overlay.columns())
            QCOMPARE(column.size(), 2);
        const int tall = overlay.height();

        overlay.setSheet(QStringLiteral("App"), keys.sheet(), QSize(1280, tall)); // un poco menos de alto
        QVERIFY(overlay.rowHeight() < ShortcutsOverlay::kRowHeight);
        QVERIFY(overlay.height() <= tall - 48);
        QCOMPARE(overlay.width(), 1280 - 48);

        overlay.setSheet(QStringLiteral("App"), keys.sheet(), QSize(1280, 300)); // no alcanza: el mínimo
        QCOMPARE(overlay.rowHeight(), ShortcutsOverlay::kMinRowHeight);
    }

    // Esc y Ctrl+, cierran (y avisan); otra tecla no.
    void escYCtrlComaCierran()
    {
        ShortcutsOverlay overlay(nullptr);
        int closed = 0;
        overlay.onClose = [&closed] { ++closed; };
        for (const auto& [key, mods] : {std::pair{Qt::Key_Escape, Qt::NoModifier}, std::pair{Qt::Key_Comma, Qt::ControlModifier}}) {
            overlay.show();
            QTest::keyClick(&overlay, Qt::Key_A);
            QVERIFY(overlay.isVisible());
            QTest::keyClick(&overlay, key, mods);
            QVERIFY(!overlay.isVisible());
        }
        QCOMPARE(closed, 2);
    }
};

QTEST_MAIN(TestShortcutsOverlay)
#include "tst_shortcutsoverlay.moc"
