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
    // HU-78: la lista sale del registro, por grupo y en las columnas pedidas; los grupos que
    // no figuran van al final de la última columna.
    void listaPorGrupos()
    {
        Shortcuts keys;
        keys.setColumns({{QStringLiteral("Lápiz")}, {QStringLiteral("Ejercicios"), QStringLiteral("Pantalla")}});
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
        QCOMPARE(sheet[0].column, 0);
        QCOMPARE(sheet[1].column, 1);
        QCOMPARE(sheet[2].column, 1);
        QCOMPARE(sheet[3].column, 1);
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

    // Las columnas del overlay son las de la lista (como la mesa: Lápiz | Hoja y Vista |
    // Ejercicios y Pantalla), y el alto alcanza para la más larga.
    void columnasDeLaMesa()
    {
        Shortcuts keys;
        keys.setColumns({{QStringLiteral("Lápiz")}, {QStringLiteral("Hoja"), QStringLiteral("Vista")},
                         {QStringLiteral("Ejercicios"), QStringLiteral("Pantalla")}});
        keys.setGroup(QStringLiteral("Lápiz"));
        for (int i = 0; i < 8; ++i)
            keys.add(QStringLiteral("L%1").arg(i), {{unsigned('A' + i)}}, [] {});
        for (const char* group : {"Pantalla", "Vista", "Ejercicios", "Hoja"}) {
            keys.setGroup(QString::fromLatin1(group));
            keys.add(QString::fromLatin1(group), {{unsigned(VK_F1 + group[0] % 12)}}, [] {});
        }
        ShortcutsOverlay overlay(nullptr);
        overlay.setSheet(QStringLiteral("Ejercicios"), keys.sheet());
        QCOMPARE(overlay.columns(), (QList<QList<int>>{{0}, {1, 2}, {3, 4}}));
        QVERIFY(overlay.height() > 24 + 8 * ShortcutsOverlay::kRowHeight);
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
