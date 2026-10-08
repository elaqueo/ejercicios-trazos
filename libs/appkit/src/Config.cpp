#include "appkit/Config.h"

#include "appkit/Paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QSaveFile>

Q_LOGGING_CATEGORY(lcConfig, "appkit.config")

namespace appkit {

Config::Config(const QString& appSection, const QString& filePath)
    : m_path(filePath.isEmpty() ? familyDataDirectory() + QStringLiteral("/config.json") : filePath)
    , m_appSection(appSection)
{
    load();
}

QString Config::sectionName(Scope scope) const
{
    return scope == Scope::Common ? QStringLiteral("common") : m_appSection;
}

QVariant Config::value(const QString& key, const QVariant& fallback, Scope scope) const
{
    const QJsonObject section = m_root.value(sectionName(scope)).toObject();
    return section.contains(key) ? section.value(key).toVariant() : fallback;
}

void Config::setValue(const QString& key, const QVariant& value, Scope scope)
{
    const QString name = sectionName(scope);
    QJsonObject section = m_root.value(name).toObject();
    section.insert(key, QJsonValue::fromVariant(value));
    m_root.insert(name, section);
    save();
}

void Config::load()
{
    QFile file(m_path);
    if (!file.exists())
        return;
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcConfig) << "No se pudo leer la configuración" << QDir::toNativeSeparators(m_path) << file.errorString();
        return;
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (!document.isObject()) {
        qCWarning(lcConfig) << "Configuración inválida, se usan los valores por defecto:"
                            << QDir::toNativeSeparators(m_path) << error.errorString();
        return;
    }
    m_root = document.object();
}

void Config::save() const
{
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(lcConfig) << "No se pudo guardar la configuración" << QDir::toNativeSeparators(m_path) << file.errorString();
        return;
    }
    file.write(QJsonDocument(m_root).toJson(QJsonDocument::Indented));
    if (!file.commit())
        qCWarning(lcConfig) << "No se pudo guardar la configuración" << QDir::toNativeSeparators(m_path) << file.errorString();
}

} // namespace appkit
