#pragma once

#include <QString>
#include <QStringList>

// Bocetos (HU-134): lo que no es pantalla, para probarlo.
namespace bocetos {

// La biblioteca de `--biblioteca <carpeta>` (la prueba de la app real, para no tocar los
// cuadernos del usuario); si no está, la de siempre (notebooks::defaultLibraryRoot()).
QString libraryRoot(const QStringList& args);

// cartuchera.exe junto a Bocetos (en `appDir`), o vacío si no está.
QString findCartuchera(const QString& appDir);

} // namespace bocetos
