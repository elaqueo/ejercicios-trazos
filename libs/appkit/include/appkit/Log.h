#pragma once

#include <QString>

namespace appkit {

// Redirige los mensajes de Qt (qDebug/qInfo/qWarning/..., con su categoría) a
// <familyDataDirectory>/<appName>.log, que se reescribe en cada arranque. Una app
// de ventana en Windows no tiene consola donde verlos. Los mensajes siguen
// llegando también al manejador anterior (salida del depurador).
// Devuelve la ruta del archivo de log.
QString installFileLog(const QString& appName);

} // namespace appkit
