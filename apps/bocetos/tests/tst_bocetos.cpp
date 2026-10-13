#include "BocetosCore.h"

#include <notebooks/Library.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class TestBocetos : public QObject {
    Q_OBJECT

private slots:
    void laBiblioteca()
    {
        QTemporaryDir dir;
        QCOMPARE(QDir(bocetos::libraryRoot({"bocetos.exe", "--biblioteca", dir.path()})), QDir(dir.path()));
        QCOMPARE(bocetos::libraryRoot({"bocetos.exe"}), notebooks::defaultLibraryRoot());
        QCOMPARE(bocetos::libraryRoot({"bocetos.exe", "--biblioteca"}), notebooks::defaultLibraryRoot());
    }

    void encuentraCartuchera()
    {
        QTemporaryDir dir;
        QVERIFY(bocetos::findCartuchera(dir.path()).isEmpty());
        QFile exe(QDir(dir.path()).filePath(QStringLiteral("cartuchera.exe")));
        QVERIFY(exe.open(QIODevice::WriteOnly));
        exe.close();
        QCOMPARE(QFileInfo(bocetos::findCartuchera(dir.path())), QFileInfo(exe.fileName()));
        // Una carpeta con ese nombre no es Cartuchera.
        QTemporaryDir other;
        QVERIFY(QDir(other.path()).mkdir(QStringLiteral("cartuchera.exe")));
        QVERIFY(bocetos::findCartuchera(other.path()).isEmpty());
    }
};

QTEST_MAIN(TestBocetos)
#include "tst_bocetos.moc"
