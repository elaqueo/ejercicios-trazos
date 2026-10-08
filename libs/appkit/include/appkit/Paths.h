#pragma once

#include <QString>
#include <QStringList>

namespace appkit {

// Carpeta de datos compartida por todas las apps de la familia:
// %LOCALAPPDATA%\trazos en Windows.
QString familyDataDirectory();

// Carpetas de pinceles, en orden de carga: los de fábrica (junto al ejecutable)
// y la carpeta compartida del usuario (<familyDataDirectory>/brushes), que puede
// reemplazarlos por nombre. Requiere un QCoreApplication creado.
QStringList brushDirectories();

} // namespace appkit
