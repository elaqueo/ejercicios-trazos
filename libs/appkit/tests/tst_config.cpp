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

        // HU-13: lo que tenía queda en una copia, y el primer guardado deja un archivo válido.
        QFile backup(dir.filePath("config.corrupto.json"));
        QVERIFY(backup.open(QIODevice::ReadOnly));
        QCOMPARE(backup.readAll(), QByteArray("{ roto"));
        config.setValue("lead", "HB");
        appkit::Config otra(QStringLiteral("ejercicios"), path);
        QCOMPARE(otra.value("lead").toString(), QStringLiteral("HB"));
    }

    // Una sección con otro tipo (no objeto) se ignora y el primer guardado la reemplaza.
    void seccionInvalidaSeReemplaza()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("config.json");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({ "common": 5, "ejercicios": [1, 2] })");
        file.close();

        appkit::Config config(QStringLiteral("ejercicios"), path);
        QCOMPARE(config.value("params", 7).toInt(), 7);
        QCOMPARE(config.value("monitor", 3, appkit::Config::Scope::Common).toInt(), 3);
        config.setValue("params", QVariantMap{{"recta", QVariantMap{{"distMin", 0.1}}}});
        appkit::Config otra(QStringLiteral("ejercicios"), path);
        QCOMPARE(otra.value("params").toMap().value("recta").toMap().value("distMin").toDouble(), 0.1);
    }

    // remove borra la clave y no toca el resto.
    void borrarUnaClave()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("config.json");
        appkit::Config config(QStringLiteral("ejercicios"), path);
        config.setValue("brush", "classic/pencil");
        config.setValue("lead", "HB");
        config.remove("brush");
        config.remove("noExiste");
        appkit::Config otra(QStringLiteral("ejercicios"), path);
        QVERIFY(!otra.value("brush").isValid());
        QCOMPARE(otra.value("lead").toString(), QStringLiteral("HB"));
    }
};

QTEST_GUILESS_MAIN(TestConfig)
#include "tst_config.moc"
