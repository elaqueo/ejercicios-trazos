#include "paintcore/BrushLibrary.h"

#include "Brush.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>

#include <algorithm>
#include <map>

Q_LOGGING_CATEGORY(lcBrushes, "paintcore.brushes")

namespace paintcore {

BrushPreset defaultBrushPreset()
{
    BrushPreset preset;
    preset.name = QStringLiteral("Por defecto");
    return preset;
}

qsizetype BrushLibrary::load(const QStringList& directories)
{
    std::map<QString, BrushPreset> byName; // ordenado por nombre
    for (const QString& directory : directories) {
        const QDir root(directory);
        if (!root.exists()) {
            qCInfo(lcBrushes) << "Carpeta de pinceles inexistente, se omite:" << QDir::toNativeSeparators(directory);
            continue;
        }

        int loaded = 0;
        QDirIterator it(root.absolutePath(), {QStringLiteral("*.myb")}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QFileInfo info = it.nextFileInfo();
            const QString nativePath = QDir::toNativeSeparators(info.filePath());

            QFile file(info.filePath());
            if (!file.open(QIODevice::ReadOnly)) {
                qCWarning(lcBrushes) << "No se pudo leer el pincel, se ignora:" << nativePath << file.errorString();
                continue;
            }
            QByteArray json = file.readAll();
            if (!detail::Brush::isValidJson(json)) {
                qCWarning(lcBrushes) << "Pincel inválido, se ignora:" << nativePath;
                continue;
            }

            BrushPreset preset;
            const QString relative = root.relativeFilePath(info.filePath());
            preset.name = relative.chopped(info.suffix().size() + 1); // sin ".myb"
            preset.filePath = info.absoluteFilePath();
            const QString preview = info.absolutePath() + u'/' + info.completeBaseName() + QStringLiteral("_prev.png");
            if (QFileInfo::exists(preview))
                preset.previewPath = preview;
            preset.json = std::move(json);
            byName.insert_or_assign(preset.name, std::move(preset));
            ++loaded;
        }
        qCInfo(lcBrushes) << loaded << "pinceles en" << QDir::toNativeSeparators(directory);
    }

    m_brushes.clear();
    m_brushes.reserve(qsizetype(byName.size()));
    for (auto& [name, preset] : byName)
        m_brushes.append(std::move(preset));
    qCInfo(lcBrushes) << m_brushes.size() << "pinceles cargados en total";
    return m_brushes.size();
}

const BrushPreset* BrushLibrary::find(const QString& name) const
{
    const auto it = std::find_if(m_brushes.cbegin(), m_brushes.cend(),
                                 [&](const BrushPreset& preset) { return preset.name == name; });
    return it == m_brushes.cend() ? nullptr : &*it;
}

} // namespace paintcore
