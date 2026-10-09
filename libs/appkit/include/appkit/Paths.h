#pragma once

#include <QString>

namespace appkit {

// Carpeta de datos compartida por todas las apps de la familia:
// %LOCALAPPDATA%\trazos en Windows.
QString familyDataDirectory();

} // namespace appkit
