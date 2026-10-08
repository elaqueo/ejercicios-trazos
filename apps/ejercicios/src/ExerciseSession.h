#pragma once

#include "exercises/Exercise.h"

#include <QObject>

namespace paintcore {
class CanvasWidget;
}

namespace ejercicios {

// Ciclo de ejercicios sobre un lienzo (HU-16): muestra el ejercicio actual y pasa al
// siguiente. Un intento por ejercicio: no hay deshacer.
class ExerciseSession : public QObject {
    Q_OBJECT

public:
    // canvas y exercise deben vivir más que la sesión.
    ExerciseSession(paintcore::CanvasWidget* canvas, const Exercise* exercise, QObject* parent = nullptr);

    quint32 seed() const { return m_seed; }
    const Generated& current() const { return m_current; }

    // Siguiente ejercicio: limpia el lienzo y genera con otra semilla.
    void next();
    // Vuelve a generar con la misma semilla para el área útil actual (cuando cambia
    // el área). No toca la tinta.
    void regenerate();

private:
    paintcore::CanvasWidget* m_canvas;
    const Exercise* m_exercise;
    quint32 m_seed;
    Generated m_current;
};

} // namespace ejercicios
