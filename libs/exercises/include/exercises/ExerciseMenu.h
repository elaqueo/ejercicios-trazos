#pragma once

#include "exercises/Exercise.h"

#include <appkit/MenuOverlay.h>

#include <QList>
#include <QStringList>

namespace ejercicios {

// Id del modo mixto en el menú (HU-20).
inline const QString kMixedModeId = QStringLiteral("mixto");

// Contenido del menú (HU-11, mesa "Menú overlay" del diseño): el modo mixto destacado arriba
// (con cuántos ejercicios participan; -1 = todos) y los ejercicios agrupados por group(), en
// el orden en que llegan.
QList<appkit::MenuGroup> exerciseMenu(const QList<const Exercise*>& exercises, int mixedCount = -1);

// Los ejercicios del modo mixto: los de `enabledIds`, en el orden del catálogo; sin
// ninguno (o ninguno conocido), todos.
QList<const Exercise*> mixedPool(const QList<const Exercise*>& exercises, const QStringList& enabledIds);

// El ejercicio con ese id, o nullptr.
const Exercise* findExercise(const QList<const Exercise*>& exercises, const QString& id);

} // namespace ejercicios
