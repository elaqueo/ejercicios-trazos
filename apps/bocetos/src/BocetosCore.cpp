#include "BocetosCore.h"

#include <notebooks/Library.h>

#include <QDir>
#include <QFileInfo>

namespace bocetos {

QString libraryRoot(const QStringList& args)
{
    const qsizetype at = args.indexOf(QStringLiteral("--biblioteca"));
    if (at >= 0 && at + 1 < args.size())
        return QFileInfo(args[at + 1]).absoluteFilePath();
    return notebooks::defaultLibraryRoot();
}

QString findCartuchera(const QString& appDir)
{
    const QString path = QDir(appDir).filePath(QStringLiteral("cartuchera.exe"));
    return QFileInfo(path).isFile() ? path : QString();
}

} // namespace bocetos
