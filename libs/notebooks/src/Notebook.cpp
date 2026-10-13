#include "notebooks/Notebook.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>

namespace notebooks {

QList<Paper> paperPresets()
{
    // Crema y blanco son los de Cartuchera y la Mesa (theme::kHoja, kHojaAnimacion); el kraft
    // tiene más textura (la fibra) y los tonados, como el Strathmore Toned, un grano más suave.
    return {
        {QStringLiteral("crema"), QColor(0xF5, 0xF0, 0xE6), 2047, 20},
        {QStringLiteral("blanco"), QColor(0xFA, 0xF8, 0xF2), 2047, 20},
        {QStringLiteral("kraft"), QColor(0xB5, 0x98, 0x78), 2047, 35},
        {QStringLiteral("tonadoTostado"), QColor(0xD6, 0xC4, 0xA1), 1600, 20},
        {QStringLiteral("tonadoGris"), QColor(0xB8, 0xB7, 0xB0), 1600, 20},
    };
}

Paper paperPreset(const QString& preset)
{
    const QList<Paper> all = paperPresets();
    const auto it = std::find_if(all.begin(), all.end(), [&](const Paper& p) { return p.preset == preset; });
    return it != all.end() ? *it : all.front();
}

namespace {

QJsonObject paperJson(const Paper& paper)
{
    return {{QStringLiteral("preset"), paper.preset},
            {QStringLiteral("color"), paper.color.name()},
            {QStringLiteral("grain"), paper.grain},
            {QStringLiteral("texture"), paper.texture}};
}

Paper paperFrom(const QJsonObject& o)
{
    Paper paper = paperPreset(o.value(QStringLiteral("preset")).toString());
    paper.preset = o.value(QStringLiteral("preset")).toString(paper.preset);
    const QColor color(o.value(QStringLiteral("color")).toString());
    if (color.isValid())
        paper.color = color;
    paper.grain = std::clamp(o.value(QStringLiteral("grain")).toInt(paper.grain), 0, 2047);
    paper.texture = std::clamp(o.value(QStringLiteral("texture")).toInt(paper.texture), 0, 100);
    return paper;
}

QString filePath(const QString& dir)
{
    return QDir(dir).filePath(QString::fromLatin1(kNotebookFile));
}

} // namespace

std::optional<Notebook> loadNotebook(const QString& dir, LoadError* error)
{
    const auto fail = [&](LoadError e) {
        if (error)
            *error = e;
        return std::nullopt;
    };
    QFile file(filePath(dir));
    if (!file.open(QIODevice::ReadOnly))
        return fail(LoadError::Missing);
    QJsonParseError parse;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject())
        return fail(LoadError::Corrupt);
    const QJsonObject o = doc.object();
    const int version = o.value(QStringLiteral("version")).toInt(0);
    if (version < 1)
        return fail(LoadError::Corrupt);
    if (version > kNotebookVersion)
        return fail(LoadError::NewerVersion);

    Notebook nb;
    nb.name = o.value(QStringLiteral("name")).toString();
    nb.collection = o.value(QStringLiteral("collection")).toString();
    nb.cover = QColor(o.value(QStringLiteral("cover")).toString());
    nb.paper = paperFrom(o.value(QStringLiteral("paper")).toObject());
    nb.maxSheets = std::max(1, o.value(QStringLiteral("maxSheets")).toInt(nb.maxSheets));
    nb.started = QDate::fromString(o.value(QStringLiteral("started")).toString(), Qt::ISODate);
    nb.updated = QDate::fromString(o.value(QStringLiteral("updated")).toString(), Qt::ISODate);
    nb.app = o.value(QStringLiteral("app")).toString() == QLatin1String("krita") ? App::Krita : App::Cartuchera;
    if (nb.name.isEmpty())
        return fail(LoadError::Corrupt);
    if (error)
        *error = LoadError::None;
    return nb;
}

bool saveNotebook(const QString& dir, const Notebook& notebook)
{
    const bool krita = notebook.app == App::Krita;
    QJsonObject o{{QStringLiteral("version"), krita ? kNotebookVersion : 1},
                        {QStringLiteral("name"), notebook.name},
                        {QStringLiteral("collection"), notebook.collection},
                        {QStringLiteral("cover"), notebook.cover.name()},
                        {QStringLiteral("paper"), paperJson(notebook.paper)},
                        {QStringLiteral("maxSheets"), notebook.maxSheets},
                        {QStringLiteral("started"), notebook.started.toString(Qt::ISODate)},
                        {QStringLiteral("updated"), notebook.updated.toString(Qt::ISODate)}};
    if (krita)
        o.insert(QStringLiteral("app"), QStringLiteral("krita"));
    QSaveFile file(filePath(dir));
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(o).toJson());
    return file.commit();
}

QString sheetFolder(const QString& dir, int index)
{
    return QDir(dir).filePath(QStringLiteral("hoja-%1").arg(index, 4, 10, QLatin1Char('0')));
}

SheetState sheetState(const QString& dir, int index)
{
    const QString folder = sheetFolder(dir, index);
    if (!QFileInfo(folder).isDir())
        return SheetState::Empty;
    return QFileInfo::exists(QDir(folder).filePath(QString::fromLatin1(kFinishedImage))) ? SheetState::Finished
                                                                                          : SheetState::Draft;
}

int finishedSheets(const QString& dir, const Notebook& notebook)
{
    int count = 0;
    for (int i = 1; i <= notebook.maxSheets; ++i)
        count += sheetState(dir, i) == SheetState::Finished;
    return count;
}

std::optional<int> nextSheet(const QString& dir, const Notebook& notebook)
{
    for (int i = 1; i <= notebook.maxSheets; ++i)
        if (sheetState(dir, i) != SheetState::Finished)
            return i;
    return std::nullopt;
}

} // namespace notebooks
