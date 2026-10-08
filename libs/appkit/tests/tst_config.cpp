#include <appkit/Config.h>

#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

class TestConfig : public QObject {
    Q_OBJECT

private slots:
    void loQueSeGuardaSeVuelveALeer()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("config.json");
        {
            appkit::Config config(QStringLiteral("ejercicios"), path);
            config.setValue("brush", "classic/pencil");
            config.setValue("monitor", 2, appkit::Config::Scope::Common);
        }
        appkit::Config config(QStringLiteral("ejercicios"), path);
        QCOMPARE(config.value("brush").toString(), QStringLiteral("classic/pencil"));
        QCOMPARE(config.value("monitor", {}, appkit::Config::Scope::Common).toInt(), 2);

        // Cada app tiene su sección: otra app no ve el pincel, pero sí lo común.
        appkit::Config otra(QStringLiteral("otra-app"), path);
        QVERIFY(!otra.value("brush").isValid());
        QCOMPARE(otra.value("monitor", {}, appkit::Config::Scope::Common).toInt(), 2);
    }

    void archivoAusenteUsaElValorPorDefecto()
    {
        QTemporaryDir dir;
        appkit::Config config(QStringLiteral("ejercicios"), dir.filePath("no-existe.json"));
        QCOMPARE(config.value("brush", "Por defecto").toString(), QStringLiteral("Por defecto"));
    }

    void archivoCorruptoUsaElValorPorDefecto()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("config.json");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{ roto");
        file.close();

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Configuración inválida"));
        appkit::Config config(QStringLiteral("ejercicios"), path);
        QCOMPARE(config.value("brush", "Por defecto").toString(), QStringLiteral("Por defecto"));
    }
};

QTEST_GUILESS_MAIN(TestConfig)
#include "tst_config.moc"
