#include "notebooks/Library.h"

#include <QDir>
#include <QStandardPaths>
#include <QtDebug>

namespace notebooks {

QString defaultLibraryRoot()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(QStringLiteral("Trazos/Bocetos"));
}

QString folderName(const QString& name)
{
    // Sin acentos: se descompone (NFD) y se sacan las marcas.
    QString out;
    bool dash = false;
    for (const QChar c : name.normalized(QString::NormalizationForm_D)) {
        if (c.category() == QChar::Mark_NonSpacing)
            continue;
        if (c.isLetterOrNumber() && c.unicode() < 0x80) {
            if (dash && !out.isEmpty())
                out += QLatin1Char('-');
            out += c.toLower();
            dash = false;
        } else {
            dash = true;
        }
    }
    return out.isEmpty() ? QStringLiteral("cuaderno") : out;
}

QList<Entry> listNotebooks(const QString& root)
{
    QList<Entry> out;
    const QDir dir(root);
    for (const QString& sub : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        const QString path = dir.filePath(sub);
        LoadError error = LoadError::None;
        if (const std::optional<Notebook> nb = loadNotebook(path, &error))
            out.append({path, *nb});
        else if (error != LoadError::Missing)
            qWarning().noquote() << "Bocetos: no se puede leer el cuaderno" << path
                                 << (error == LoadError::NewerVersion ? "(versión más nueva)" : "(dañado)");
    }
    return out;
}

QString createNotebook(const QString& root, const Notebook& notebook)
{
    if (!QDir().mkpath(root))
        return {};
    const QDir dir(root);
    const QString base = folderName(notebook.name);
    QString name = base;
    for (int n = 2; dir.exists(name); ++n)
        name = QStringLiteral("%1-%2").arg(base).arg(n);
    if (!dir.mkdir(name))
        return {};
    const QString path = dir.filePath(name);
    return saveNotebook(path, notebook) ? path : QString();
}

} // namespace notebooks
