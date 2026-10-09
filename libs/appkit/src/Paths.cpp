#include "appkit/Paths.h"

#include <QStandardPaths>

namespace appkit {

QString familyDataDirectory()
{
    // GenericDataLocation (y no AppDataLocation) para que la ruta no dependa del
    // nombre de cada app: es la misma para toda la familia.
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/trazos");
}

} // namespace appkit
