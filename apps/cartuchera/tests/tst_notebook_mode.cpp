#include "NotebookMode.h"

#include <notebooks/Library.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace cartuchera;

namespace {

QString makeNotebook(const QTemporaryDir& root, int sheets = 3)
{
    notebooks::Notebook nb;
    nb.name = QStringLiteral("Bocetos diarios");
    nb.paper = notebooks::paperPreset(QStringLiteral("tonadoGris"));
    nb.maxSheets = sheets;
    nb.started = nb.updated = QDate(2026, 10, 1);
    return notebooks::createNotebook(root.path(), nb);
}

void finish(const QString& dir, int index)
{
    QVERIFY(QDir().mkpath(notebooks::sheetFolder(dir, index)));
    QFile png(QDir(notebooks::sheetFolder(dir, index)).filePath(QString::fromLatin1(notebooks::kFinishedImage)));
    QVERIFY(png.open(QIODevice::WriteOnly));
}

} // namespace

class TestNotebookMode : public QObject {
    Q_OBJECT

private slots:
    void elArgumento()
    {
        QCOMPARE(sheetArgument({"cartuchera.exe", "--hoja", "C:/x/hoja-0001"}), QStringLiteral("C:/x/hoja-0001"));
        QVERIFY(sheetArgument({"cartuchera.exe"}).isEmpty());
        QVERIFY(sheetArgument({"cartuchera.exe", "--hoja"}).isEmpty());
    }

    // La siguiente hoja abre, con el cuaderno y su papel; la carpeta todavía no existe.
    void laSiguienteAbre()
    {
        QTemporaryDir root;
        const QString dir = makeNotebook(root);
        QString error;
        const std::optional<NotebookSheet> sheet = notebookSheet(notebooks::sheetFolder(dir, 1), &error);
        QVERIFY2(sheet, qPrintable(error));
        QCOMPARE(sheet->index, 1);
        QCOMPARE(QDir(sheet->notebookDir), QDir(dir));
        QCOMPARE(sheet->notebook.paper.preset, QStringLiteral("tonadoGris"));
        QCOMPARE(paperName(sheet->notebook.paper), QStringLiteral("tonado gris"));
    }

    // Una terminada no se edita; tampoco se salta el orden, ni fuera del máximo, ni fuera de un
    // cuaderno.
    void loQueNoAbre()
    {
        QTemporaryDir root;
        const QString dir = makeNotebook(root, 2);
        finish(dir, 1);
        QString error;
        QVERIFY(!notebookSheet(notebooks::sheetFolder(dir, 1), &error));
        QVERIFY(error.contains(QStringLiteral("terminada")));
        QVERIFY(notebookSheet(notebooks::sheetFolder(dir, 2), &error));
        QVERIFY(!notebookSheet(notebooks::sheetFolder(dir, 3), &error));
        QVERIFY(error.contains(QStringLiteral("no existe")));
        QVERIFY(!notebookSheet(QDir(dir).filePath(QStringLiteral("otra")), &error));
        QTemporaryDir empty;
        QVERIFY(!notebookSheet(QDir(empty.path()).filePath(QStringLiteral("hoja-0001")), &error));
        QVERIFY(error.contains(QStringLiteral("no es un cuaderno")));

        const QString other = makeNotebook(root, 3);
        QVERIFY(!notebookSheet(notebooks::sheetFolder(other, 2), &error));
        QVERIFY(error.contains(QStringLiteral("siguiente")));
    }

    void terminarActualizaLaFecha()
    {
        QTemporaryDir root;
        const QString dir = makeNotebook(root);
        const std::optional<NotebookSheet> sheet = notebookSheet(notebooks::sheetFolder(dir, 1), nullptr);
        QVERIFY(markUpdated(*sheet, QDate(2026, 10, 13)));
        const std::optional<notebooks::Notebook> nb = notebooks::loadNotebook(dir);
        QCOMPARE(nb->updated, QDate(2026, 10, 13));
        QCOMPARE(nb->started, QDate(2026, 10, 1));
    }
};

QTEST_MAIN(TestNotebookMode)
#include "tst_notebook_mode.moc"
