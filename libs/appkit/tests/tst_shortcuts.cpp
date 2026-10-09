#include <appkit/Shortcuts.h>

#include <windows.h>

#include <QTest>

using appkit::KeyChord;
using appkit::Shortcuts;

class TestShortcuts : public QObject {
    Q_OBJECT

private slots:
    // Cada tecla corre su acción; las que no están registradas no son del registro.
    void cadaTeclaCorreSuAccion()
    {
        Shortcuts shortcuts;
        int repetir = 0, rotar = 0;
        QVERIFY(shortcuts.add(QStringLiteral("Repetir"), {{'R'}}, [&] { ++repetir; }));
        QVERIFY(shortcuts.add(QStringLiteral("Rotar"), {{'4'}, {VK_NUMPAD4}}, [&] { ++rotar; }));

        QVERIFY(shortcuts.trigger('R', false));
        QVERIFY(shortcuts.trigger('4', false));
        QVERIFY(shortcuts.trigger(VK_NUMPAD4, false));
        QCOMPARE(repetir, 1);
        QCOMPARE(rotar, 2);
        QVERIFY(!shortcuts.trigger('Q', false));
        QVERIFY(shortcuts.conflicts().isEmpty());
    }

    // Ctrl+R y R son atajos distintos.
    void ctrlNoSeConfunde()
    {
        Shortcuts shortcuts;
        int repetir = 0, otra = 0;
        shortcuts.add(QStringLiteral("Repetir"), {{'R'}}, [&] { ++repetir; });
        QVERIFY(!shortcuts.trigger('R', true));
        QVERIFY(shortcuts.add(QStringLiteral("Otra"), {{'R', true}}, [&] { ++otra; }));
        QVERIFY(shortcuts.trigger('R', true));
        QCOMPARE(repetir, 0);
        QCOMPARE(otra, 1);
    }

    // Dos acciones con la misma tecla: el choque queda anotado y gana la primera.
    void teclaRepetidaEsConflicto()
    {
        Shortcuts shortcuts;
        int primera = 0, segunda = 0;
        shortcuts.add(QStringLiteral("Hoja nueva"), {{'N', true}}, [&] { ++primera; });
        QVERIFY(!shortcuts.add(QStringLiteral("Otra"), {{'N', true}, {'M'}}, [&] { ++segunda; }));

        QCOMPARE(shortcuts.conflicts().size(), 1);
        QVERIFY(shortcuts.conflicts().first().contains(QStringLiteral("Ctrl+N")));
        QVERIFY(shortcuts.conflicts().first().contains(QStringLiteral("Hoja nueva")));
        shortcuts.trigger('N', true);
        QCOMPARE(primera, 1);
        QCOMPARE(segunda, 0);
        // La tecla libre de la segunda acción sí queda.
        QVERIFY(shortcuts.trigger('M', false));
        QCOMPARE(segunda, 1);
    }

    void nombresDeTeclas()
    {
        QCOMPARE(Shortcuts::describe({VK_F10}), QStringLiteral("F10"));
        QCOMPARE(Shortcuts::describe({'S', true}), QStringLiteral("Ctrl+S"));
        QCOMPARE(Shortcuts::describe({VK_OEM_4}), QStringLiteral("["));
        QCOMPARE(Shortcuts::describe({VK_RIGHT}), QStringLiteral("→"));
    }
};

QTEST_MAIN(TestShortcuts)
#include "tst_shortcuts.moc"
