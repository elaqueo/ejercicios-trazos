#pragma once

#include <QJsonObject>
#include <QString>
#include <QVariant>

namespace appkit {

// Configuración persistente en JSON, compartida por la familia de apps:
//   { "common": { ... }, "<app>": { ... } }
// Cada app lee y escribe su sección; "common" es para lo compartido.
// Un archivo ausente deja los valores por defecto; uno corrupto (HU-13) también, y antes se
// copia a <nombre>.corrupto.json para no perder lo que tenía (el próximo guardado lo pisa).
class Config {
public:
    enum class Scope { App, Common };

    // filePath vacío: <familyDataDirectory>/config.json.
    explicit Config(const QString& appSection, const QString& filePath = {});

    QString filePath() const { return m_path; }

    QVariant value(const QString& key, const QVariant& fallback = {}, Scope scope = Scope::App) const;

    // Guarda el archivo en el acto (escritura atómica).
    void setValue(const QString& key, const QVariant& value, Scope scope = Scope::App);
    // Borra la clave (y guarda) si existe.
    void remove(const QString& key, Scope scope = Scope::App);

private:
    QString sectionName(Scope scope) const;
    void load();
    void save() const;

    QString m_path;
    QString m_appSection;
    QJsonObject m_root;
};

} // namespace appkit
