#include "NotebookMode.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace cartuchera {

QString sheetArgument(const QStringList& args)
{
    const qsizetype at = args.indexOf(QStringLiteral("--hoja"));
    return at >= 0 && at + 1 < args.size() ? args[at + 1] : QString();
}

std::optional<NotebookSheet> notebookSheet(const QString& sheetDir, QString* error)
{
    const auto fail = [&](const QString& why) {
        if (error)
            *error = why;
        return std::nullopt;
    };
    const QFileInfo info(sheetDir);
    const QRegularExpressionMatch m = QRegularExpression(QStringLiteral("^hoja-(\\d{4})$")).match(info.fileName());
    if (!m.hasMatch())
        return fail(QStringLiteral("%1 no es la carpeta de una hoja (hoja-NNNN)").arg(sheetDir));
    NotebookSheet sheet;
    sheet.sheetDir = info.absoluteFilePath();
    sheet.notebookDir = info.absolutePath();
    sheet.index = m.captured(1).toInt();
    notebooks::LoadError load = notebooks::LoadError::None;
    const std::optional<notebooks::Notebook> nb = notebooks::loadNotebook(sheet.notebookDir, &load);
    if (!nb)
        return fail(load == notebooks::LoadError::NewerVersion
                        ? QStringLiteral("el cuaderno de %1 es de una versión más nueva").arg(sheet.notebookDir)
                        : QStringLiteral("%1 no es un cuaderno").arg(sheet.notebookDir));
    sheet.notebook = *nb;
    if (sheet.index < 1 || sheet.index > nb->maxSheets)
        return fail(QStringLiteral("la hoja %1 no existe (el cuaderno tiene %2)").arg(sheet.index).arg(nb->maxSheets));
    if (notebooks::sheetState(sheet.notebookDir, sheet.index) == notebooks::SheetState::Finished)
        return fail(QStringLiteral("la hoja %1 ya está terminada: no se edita").arg(sheet.index));
    if (notebooks::nextSheet(sheet.notebookDir, *nb) != sheet.index)
        return fail(QStringLiteral("la hoja %1 no es la siguiente (se llena en orden)").arg(sheet.index));
    return sheet;
}

QString paperName(const notebooks::Paper& paper)
{
    if (paper.preset == QLatin1String("tonadoTostado"))
        return QStringLiteral("tonado tostado");
    if (paper.preset == QLatin1String("tonadoGris"))
        return QStringLiteral("tonado gris");
    return paper.preset;
}

bool markUpdated(const NotebookSheet& sheet, const QDate& today)
{
    notebooks::Notebook nb = sheet.notebook;
    if (const std::optional<notebooks::Notebook> fresh = notebooks::loadNotebook(sheet.notebookDir))
        nb = *fresh; // lo que haya ahora, por si cambió mientras se dibujaba
    nb.updated = today;
    return notebooks::saveNotebook(sheet.notebookDir, nb);
}

} // namespace cartuchera
