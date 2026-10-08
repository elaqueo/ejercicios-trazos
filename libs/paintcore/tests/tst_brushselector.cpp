#include <paintcore/BrushLibrary.h>
#include <paintcore/BrushSelector.h>

#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QListWidget>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {

void writeBrush(const QString& path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"version": 3, "settings": {"radius_logarithmic": {"base_value": 2.0, "inputs": {}}}})");
}

} // namespace

class TestBrushSelector : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        QVERIFY(m_dir.isValid());
        writeBrush(m_dir.filePath("classic/pencil.myb"));
        writeBrush(m_dir.filePath("classic/charcoal.myb"));
        writeBrush(m_dir.filePath("tanda/pencil-2b.myb"));
        m_library.load({m_dir.path()});
    }

    void listaElPorDefectoYLosCargados()
    {
        paintcore::BrushSelector selector;
        selector.setLibrary(&m_library);
        QCOMPARE(selector.visibleCount(), 4); // "Por defecto" + 3
    }

    void elBuscadorFiltra()
    {
        paintcore::BrushSelector selector;
        selector.setLibrary(&m_library);
        selector.findChild<QLineEdit*>()->setText("pencil");
        QCOMPARE(selector.visibleCount(), 2);
    }

    void elegirEmiteElNombre()
    {
        paintcore::BrushSelector selector;
        selector.setLibrary(&m_library);
        selector.show();
        QVERIFY(QTest::qWaitForWindowExposed(&selector));
        QSignalSpy spy(&selector, &paintcore::BrushSelector::brushSelected);

        auto* list = selector.findChild<QListWidget*>();
        const QListWidgetItem* item = list->findItems("charcoal", Qt::MatchExactly).value(0);
        QVERIFY(item);
        QTest::mouseClick(list->viewport(), Qt::LeftButton, {}, list->visualItemRect(item).center());

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("classic/charcoal"));
    }

private:
    QTemporaryDir m_dir;
    paintcore::BrushLibrary m_library;
};

QTEST_MAIN(TestBrushSelector)
#include "tst_brushselector.moc"
