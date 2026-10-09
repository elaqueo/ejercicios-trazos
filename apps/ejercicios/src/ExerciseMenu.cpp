#include "ExerciseMenu.h"

#include <algorithm>

namespace ejercicios {

QList<appkit::MenuGroup> exerciseMenu(const QList<const Exercise*>& exercises)
{
    QList<appkit::MenuGroup> groups;
    groups.append({QString(),
                   {{kMixedModeId, QStringLiteral("Modo mixto"), false, true, QStringLiteral("llega en HU-20")}}});
    for (const Exercise* exercise : exercises) {
        auto it = std::find_if(groups.begin() + 1, groups.end(),
                               [&](const appkit::MenuGroup& g) { return g.title == exercise->group(); });
        if (it == groups.end())
            it = groups.insert(groups.end(), {exercise->group(), {}});
        it->items.append({exercise->id(), exercise->title()});
    }
    return groups;
}

const Exercise* findExercise(const QList<const Exercise*>& exercises, const QString& id)
{
    for (const Exercise* exercise : exercises)
        if (exercise->id() == id)
            return exercise;
    return nullptr;
}

} // namespace ejercicios
