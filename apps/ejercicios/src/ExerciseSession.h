#pragma once

#include "ExerciseCanvas.h"
#include "exercises/Exercise.h"

#include <QHash>
#include <QObject>
#include <QRandomGenerator>
#include <QTimer>
#include <QStringList>

#include <functional>

namespace ejercicios {

// Ciclo de ejercicios sobre un lienzo (HU-16): muestra el ejercicio actual y pasa al
// siguiente. Un intento por ejercicio: no hay deshacer.
class ExerciseSession : public QObject {
    Q_OBJECT

public:
    // canvas y exercise deben vivir más que la sesión.
    ExerciseSession(ExerciseCanvas* canvas, const Exercise* exercise, QObject* parent = nullptr);

    quint32 seed() const { return m_seed; }
    const Exercise* exercise() const { return m_exercise; }
    // Los parámetros con que se generó el ejercicio actual.
    const QVariantMap& currentParams() const { return m_currentParams; }
    // Los parámetros configurados para un ejercicio (los de defecto si no se tocaron).
    QVariantMap params(const Exercise* exercise) const;
    // Configura los parámetros del ejercicio actual (panel, HU-12). Rigen desde el ejercicio
    // siguiente: el actual queda como está y R lo repite igual.
    void setParams(const QVariantMap& values);
    // Los parámetros configurados de todos los ejercicios, por id (lo que se guarda en
    // config.json, HU-13), y su carga al arrancar: antes de generar el primer ejercicio.
    QVariantMap allParams() const;
    void loadParams(const QVariantMap& all);
    const Generated& current() const { return m_current; }

    // Siguiente ejercicio: limpia el lienzo y genera con otra semilla.
    void next();
    // Repetir (HU-17): limpia el lienzo y vuelve a generar con la misma semilla, para
    // reintentar el mismo caso.
    void repeat();
    // Cambia de ejercicio (menú, HU-11): hoja limpia y semilla nueva, como next(). Apaga el
    // modo mixto.
    void setExercise(const Exercise* exercise);

    // Modo mixto (HU-20): cada next() sortea un ejercicio de `pool` (con sus propios
    // parámetros), sin que el mismo salga más de dos veces seguidas. startMixed() lo prende
    // y pasa al primero.
    void setMixedPool(const QList<const Exercise*>& pool) { m_pool = pool; }
    void startMixed();
    bool mixed() const { return m_mixed; }
    // El sorteo: uno de `pool`, salvo el que salió las dos últimas veces (`recent`, del más
    // viejo al más nuevo) si hay otro. Con un solo ejercicio, se repite.
    static const Exercise* pickMixed(const QList<const Exercise*>& pool, const QStringList& recent,
                                     QRandomGenerator& rng);

    // Cambió el ejercicio en pantalla (el panel muestra sus parámetros).
    std::function<void()> onExerciseChanged;
    // Vuelve a generar con la misma semilla para el área útil actual (cuando cambia
    // el área). No toca la tinta. Muestra las guías; si el ejercicio lo pide
    // (Generated::visibleMs), se ocultan solas pasado ese tiempo (ghosting, HU-35).
    void regenerate();

    // G (HU-35): muestra u oculta las guías del ejercicio actual (para comparar el trazo con
    // la forma). Mostradas así, quedan hasta la próxima G.
    void toggleGuides();
    bool guidesVisible() const { return m_guidesVisible; }

private:
    ExerciseCanvas* m_canvas;
    const Exercise* m_exercise;
    quint32 m_seed;
    QHash<QString, QVariantMap> m_configured; // por id de ejercicio
    QVariantMap m_currentParams;
    Generated m_current;
    bool m_mixed = false;
    QList<const Exercise*> m_pool;
    QStringList m_recent; // ids de los últimos dos ejercicios, del más viejo al más nuevo
    QTimer m_hideTimer;   // ghosting: oculta las guías
    bool m_guidesVisible = false;
    void hideGuides();
};

} // namespace ejercicios
