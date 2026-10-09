#pragma once

#include "exercises/Exercise.h"

#include <appkit/MenuOverlay.h>

#include <QList>

namespace ejercicios {

// Id del modo mixto en el menú (HU-20; hasta entonces, deshabilitado).
inline const QString kMixedModeId = QStringLiteral("mixto");

// Contenido del menú (HU-11, mesa "Menú" del diseño): el modo mixto destacado arriba y los
// ejercicios agrupados por group(), en el orden en que llegan.
QList<appkit::MenuGroup> exerciseMenu(const QList<const Exercise*>& exercises);

// El ejercicio con ese id, o nullptr.
const Exercise* findExercise(const QList<const Exercise*>& exercises, const QString& id);

} // namespace ejercicios
