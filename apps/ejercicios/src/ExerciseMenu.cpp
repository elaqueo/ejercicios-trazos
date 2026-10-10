#include "ExerciseMenu.h"

#include <algorithm>

namespace ejercicios {

QList<appkit::MenuGroup> exerciseMenu(const QList<const Exercise*>& exercises, int mixedCount)
{
    QList<appkit::MenuGroup> groups;
    appkit::MenuItem mixed{kMixedModeId, QStringLiteral("Modo mixto"), true, true};
    mixed.description = QStringLiteral("Alterna al azar entre los ejercicios habilitados en el panel.");
    mixed.note = QStringLiteral("%1 habilitados").arg(mixedCount < 0 ? exercises.size() : mixedCount);
    groups.append({QString(), {mixed}});
    for (const Exercise* exercise : exercises) {
        auto it = std::find_if(groups.begin() + 1, groups.end(),
                               [&](const appkit::MenuGroup& g) { return g.title == exercise->group(); });
        if (it == groups.end())
            it = groups.insert(groups.end(), {exercise->group(), {}});
        it->items.append({exercise->id(), exercise->title()});
    }
    return groups;
}

QList<const Exercise*> mixedPool(const QList<const Exercise*>& exercises, const QStringList& enabledIds)
{
    QList<const Exercise*> pool;
    for (const Exercise* exercise : exercises)
        if (enabledIds.contains(exercise->id()))
            pool.append(exercise);
    return pool.isEmpty() ? exercises : pool;
}

const Exercise* findExercise(const QList<const Exercise*>& exercises, const QString& id)
{
    for (const Exercise* exercise : exercises)
        if (exercise->id() == id)
            return exercise;
    return nullptr;
}

} // namespace ejercicios
