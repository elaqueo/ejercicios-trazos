#pragma once

#include "ExerciseCanvas.h"
#include "exercises/Exercise.h"

#include <QObject>

namespace ejercicios {

// Ciclo de ejercicios sobre un lienzo (HU-16): muestra el ejercicio actual y pasa al
// siguiente. Un intento por ejercicio: no hay deshacer.
class ExerciseSession : public QObject {
    Q_OBJECT

public:
    // canvas y exercise deben vivir más que la sesión.
    ExerciseSession(ExerciseCanvas* canvas, const Exercise* exercise, QObject* parent = nullptr);

    quint32 seed() const { return m_seed; }
    const Generated& current() const { return m_current; }

    // Siguiente ejercicio: limpia el lienzo y genera con otra semilla.
    void next();
    // Vuelve a generar con la misma semilla para el área útil actual (cuando cambia
    // el área). No toca la tinta.
    void regenerate();

private:
    ExerciseCanvas* m_canvas;
    const Exercise* m_exercise;
    quint32 m_seed;
    Generated m_current;
};

} // namespace ejercicios
