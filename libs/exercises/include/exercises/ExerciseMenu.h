#pragma once

#include "exercises/Exercise.h"

#include <appkit/MenuOverlay.h>

#include <QList>
#include <QRandomGenerator>
#include <QStringList>

#include <algorithm>

namespace ejercicios {

// Id del modo mixto en el menú (HU-20).
inline const QString kMixedModeId = QStringLiteral("mixto");

// Las funciones de abajo sirven para los ejercicios de Ejercicios (Exercise) y para los de pila
// (StackExercise, HU-121): cualquier tipo con id(), title() y group().

// Contenido del menú (HU-11, mesa "Menú overlay" del diseño): el modo mixto destacado arriba
// (con cuántos ejercicios participan; -1 = todos) y los ejercicios agrupados por group(), en
// el orden en que llegan.
template <class E>
QList<appkit::MenuGroup> exerciseMenu(const QList<const E*>& exercises, int mixedCount = -1)
{
    QList<appkit::MenuGroup> groups;
    appkit::MenuItem mixed{kMixedModeId, QStringLiteral("Modo mixto"), true, true};
    mixed.description = QStringLiteral("Alterna al azar entre los ejercicios habilitados en el panel.");
    mixed.note = QStringLiteral("%1 habilitados").arg(mixedCount < 0 ? exercises.size() : mixedCount);
    groups.append({QString(), {mixed}});
    for (const E* exercise : exercises) {
        auto it = std::find_if(groups.begin() + 1, groups.end(),
                               [&](const appkit::MenuGroup& g) { return g.title == exercise->group(); });
        if (it == groups.end())
            it = groups.insert(groups.end(), {exercise->group(), {}});
        it->items.append({exercise->id(), exercise->title()});
    }
    return groups;
}

// Los ejercicios del modo mixto: los de `enabledIds`, en el orden del catálogo; sin
// ninguno (o ninguno conocido), todos.
template <class E>
QList<const E*> mixedPool(const QList<const E*>& exercises, const QStringList& enabledIds)
{
    QList<const E*> pool;
    for (const E* exercise : exercises)
        if (enabledIds.contains(exercise->id()))
            pool.append(exercise);
    return pool.isEmpty() ? exercises : pool;
}

// El ejercicio con ese id, o nullptr.
template <class E>
const E* findExercise(const QList<const E*>& exercises, const QString& id)
{
    for (const E* exercise : exercises)
        if (exercise->id() == id)
            return exercise;
    return nullptr;
}

// Las de Ejercicios, también con una lista entre llaves ({&recta, &curva}).
inline QList<appkit::MenuGroup> exerciseMenu(const QList<const Exercise*>& exercises, int mixedCount = -1)
{
    return exerciseMenu<Exercise>(exercises, mixedCount);
}
inline QList<const Exercise*> mixedPool(const QList<const Exercise*>& exercises, const QStringList& enabledIds)
{
    return mixedPool<Exercise>(exercises, enabledIds);
}
inline const Exercise* findExercise(const QList<const Exercise*>& exercises, const QString& id)
{
    return findExercise<Exercise>(exercises, id);
}

// El sorteo del modo mixto (HU-20): uno de `pool`, salvo el que salió las dos últimas veces
// (`recent`, del más viejo al más nuevo) si hay otro. Con un solo ejercicio, se repite.
template <class E>
const E* pickMixed(const QList<const E*>& pool, const QStringList& recent, QRandomGenerator& rng)
{
    if (pool.isEmpty())
        return nullptr;
    QList<const E*> candidates = pool;
    if (recent.size() >= 2 && recent[recent.size() - 1] == recent[recent.size() - 2] && pool.size() > 1)
        candidates.removeIf([&](const E* e) { return e->id() == recent.last(); });
    return candidates[int(rng.bounded(quint32(candidates.size())))];
}

} // namespace ejercicios
