#pragma once

#include <QFont>
#include <QString>

class QPainter;

// Lo que pintan igual la biblioteca y el cuaderno abierto (el tema de la suite).
namespace bocetos {

QFont themeFont(bool mono, double px, int weight = QFont::Normal);
// Una tecla como las de la página de atajos, en (x, y); devuelve su ancho.
double drawKey(QPainter& p, double x, double y, const QString& key);

} // namespace bocetos
