#pragma once

#include <QColor>
#include <QImage>
#include <QRectF>
#include <QString>

#include <cstdint>
#include <map>

class QPainter;
class QFont;

namespace lienzo {

// El rol de una hoja (HU-104, Mesa de animación; mesa "Rol y número" del canvas): key o
// breakdown, como se marca en papel. Va por id de hoja, así sigue a su hoja al reordenar y
// vuelve con ella al deshacer un borrado; una copia (Ctrl+D) tiene id nuevo: sin rol.
enum class SheetRole { None, Key, Breakdown };
using SheetRoles = std::map<uint64_t, SheetRole>;

// K: sin rol → key → breakdown → sin rol.
SheetRole nextRole(SheetRole role);
// "key", "breakdown" o vacío (toma.json y el log).
QString roleName(SheetRole role);
SheetRole parseRole(const QString& name);
// El de la hoja `id` (sin rol si no está).
SheetRole roleOf(const SheetRoles& roles, uint64_t id);

// El número con su marca, centrado en `box`: solo, con círculo (key) o con una raya debajo
// (breakdown). Lo usan la marca de la hoja y el panel de hojas. Número 0: la hoja fija (HU-89), "F".
void drawRoleMark(QPainter& p, const QRectF& box, int number, SheetRole role, const QFont& font, const QColor& color,
                  double line);

// La marca de la esquina de la hoja: una imagen cuadrada de `size` px, transparente, con el
// número y su marca en blanco (el render usa solo la cobertura y la tiñe del gris tinta).
QImage paintSheetMark(int number, SheetRole role, int size);

} // namespace lienzo
