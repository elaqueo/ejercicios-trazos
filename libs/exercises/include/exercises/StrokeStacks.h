#pragma once

#include "exercises/StackExercise.h"

#include <QList>

#include <memory>
#include <vector>

// Los trazos de Ejercicios en pila (HU-122, Ejercicios de animación): los ejercicios de líneas,
// curvas, elipses y perspectiva con la misma consigna en todas las hojas (SameOnEverySheet).
// Escritura, ghosting y presión no: no tienen sentido en pila.
namespace ejercicios {

// Hojas por defecto de estos ejercicios.
constexpr int kStrokeSheets = 6;

class StrokeStacks {
public:
    StrokeStacks();
    ~StrokeStacks();
    StrokeStacks(const StrokeStacks&) = delete;
    StrokeStacks& operator=(const StrokeStacks&) = delete;

    // En el orden del menú de Ejercicios, agrupados por group().
    const QList<const StackExercise*>& exercises() const { return m_list; }

private:
    std::vector<std::unique_ptr<Exercise>> m_exercises;
    std::vector<std::unique_ptr<SameOnEverySheet>> m_stacks;
    QList<const StackExercise*> m_list;
};

} // namespace ejercicios
