#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

namespace paintcore {

// Un pincel .myb cargado y validado. El JSON se guarda tal cual: solo paintcore
// lo interpreta, con libmypaint (RNF-05).
struct BrushPreset {
    QString name;        // ruta relativa a su carpeta, sin extensión: "classic/pencil"
    QString filePath;    // ruta absoluta del .myb
    QString previewPath; // "<nombre>_prev.png" junto al .myb; vacío si no existe
    QByteArray json;     // vacío: el pincel por defecto de paintcore

    bool isDefault() const { return json.isEmpty(); }
};

// El pincel por defecto de paintcore (una birome con presión), que no viene de un .myb.
BrushPreset defaultBrushPreset();

// Colección de pinceles .myb (formato JSON de MyPaint 2.x, compatible con los
// packs de Krita), cargados de una o más carpetas.
class BrushLibrary {
public:
    // Recorre cada carpeta con sus subcarpetas y carga los .myb válidos. Los
    // inválidos se ignoran y se registran como warning en "paintcore.brushes".
    // Si dos carpetas tienen un pincel con el mismo nombre, gana la última, así
    // una carpeta del usuario puede reemplazar a los pinceles de fábrica.
    // Reemplaza lo cargado antes. Devuelve la cantidad de pinceles cargados.
    qsizetype load(const QStringList& directories);

    // Ordenados por nombre.
    const QList<BrushPreset>& brushes() const { return m_brushes; }

    // nullptr si no existe.
    const BrushPreset* find(const QString& name) const;

private:
    QList<BrushPreset> m_brushes;
};

} // namespace paintcore
