#include <paintcore/BrushLibrary.h>

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

namespace {

const QByteArray kValidBrush = R"({"version": 3, "settings": {"radius_logarithmic": {"base_value": 2.0, "inputs": {}}}})";

void writeFile(const QString& path, const QByteArray& content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

} // namespace

class TestBrushLibrary : public QObject {
    Q_OBJECT

private slots:
    void cargaSoloLosValidos()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        writeFile(dir.filePath("lapiz.myb"), kValidBrush);
        writeFile(dir.filePath("lapiz_prev.png"), "png");
        writeFile(dir.filePath("tinta/pluma.myb"), kValidBrush);
        writeFile(dir.filePath("roto.myb"), "{ esto no es json");
        writeFile(dir.filePath("version2.myb"), R"({"version": 2, "settings": {}})");
        writeFile(dir.filePath("notas.txt"), kValidBrush);

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Pincel inválido.*roto[.]myb"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Pincel inválido.*version2[.]myb"));

        paintcore::BrushLibrary library;
        QCOMPARE(library.load({dir.path()}), 2);

        QCOMPARE(library.brushes().size(), 2);
        QCOMPARE(library.brushes()[0].name, QStringLiteral("lapiz"));
        QCOMPARE(library.brushes()[1].name, QStringLiteral("tinta/pluma"));
        QCOMPARE(library.brushes()[0].json, kValidBrush);
        QVERIFY(library.brushes()[0].previewPath.endsWith("lapiz_prev.png"));
        QVERIFY(library.brushes()[1].previewPath.isEmpty());
        QVERIFY(library.find("tinta/pluma"));
        QVERIFY(!library.find("roto"));
    }

    void laUltimaCarpetaReemplazaPorNombre()
    {
        QTemporaryDir fabrica;
        QTemporaryDir usuario;
        writeFile(fabrica.filePath("lapiz.myb"), kValidBrush);
        writeFile(fabrica.filePath("carbon.myb"), kValidBrush);
        writeFile(usuario.filePath("lapiz.myb"), kValidBrush);

        paintcore::BrushLibrary library;
        QCOMPARE(library.load({fabrica.path(), usuario.path()}), 2);
        QCOMPARE(QFileInfo(library.find("lapiz")->filePath).absolutePath(), QDir(usuario.path()).absolutePath());
    }

    // Regresión: los .myb de libmypaint 2.0 traen ajustes o entradas que 1.6.1 no
    // conoce (p. ej. surfacemap_x en los pinceles Dieterle). Con MSVC eso corrompía
    // el heap. Ahora, como en Linux, libmypaint saltea el ajuste afectado: el pincel
    // es válido si al menos un ajuste se cargó bien.
    void ajustesDesconocidosSeSaltean()
    {
        QTemporaryDir dir;
        const QByteArray smudgeNuevo =
            R"("smudge": {"base_value": 0.5, "inputs": {"surfacemap_x": [[0.0, 0.0], [1.0, 1.0]]}})";
        const QByteArray ajusteNuevo = R"("ajuste_que_no_existe": {"base_value": 1.0, "inputs": {}})";
        const QByteArray radio = R"("radius_logarithmic": {"base_value": 2.0, "inputs": {}})";
        writeFile(dir.filePath("solo_desconocidos.myb"),
                  "{\"version\": 3, \"settings\": {" + smudgeNuevo + ", " + ajusteNuevo + "}}");
        writeFile(dir.filePath("mixto.myb"),
                  "{\"version\": 3, \"settings\": {" + smudgeNuevo + ", " + ajusteNuevo + ", " + radio + "}}");
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Pincel inválido.*solo_desconocidos[.]myb"));

        paintcore::BrushLibrary library;
        QCOMPARE(library.load({dir.path()}), 1);
        QVERIFY(library.find("mixto"));
    }

    void carpetaInexistenteSeOmite()
    {
        paintcore::BrushLibrary library;
        QCOMPARE(library.load({QStringLiteral("C:/no/existe/brushes")}), 0);
    }
};

QTEST_GUILESS_MAIN(TestBrushLibrary)
#include "tst_brushlibrary.moc"
