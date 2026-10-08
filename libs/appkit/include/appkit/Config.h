#pragma once

#include <QJsonObject>
#include <QString>
#include <QVariant>

namespace appkit {

// Configuración persistente en JSON, compartida por la familia de apps:
//   { "common": { ... }, "<app>": { ... } }
// Cada app lee y escribe su sección; "common" es para lo compartido.
// Versión mínima de HU-06: HU-13 suma el manejo de archivos corruptos y el resto
// de las claves.
class Config {
public:
    enum class Scope { App, Common };

    // filePath vacío: <familyDataDirectory>/config.json.
    explicit Config(const QString& appSection, const QString& filePath = {});

    QString filePath() const { return m_path; }

    QVariant value(const QString& key, const QVariant& fallback = {}, Scope scope = Scope::App) const;

    // Guarda el archivo en el acto (escritura atómica).
    void setValue(const QString& key, const QVariant& value, Scope scope = Scope::App);

private:
    QString sectionName(Scope scope) const;
    void load();
    void save() const;

    QString m_path;
    QString m_appSection;
    QJsonObject m_root;
};

} // namespace appkit
