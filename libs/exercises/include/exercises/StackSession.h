#pragma once

#include "exercises/StackExercise.h"

#include <QHash>
#include <QPicture>
#include <QSize>
#include <QStringList>

#include <functional>

namespace ejercicios {

// Lo que la sesión de pila necesita del lienzo (HU-121): armar una pila nueva de hojas en
// blanco, el tamaño de la hoja, la guía de cada hoja y mostrarlas u ocultarlas todas. En la app
// lo implementa lienzo::Lienzo; en los tests, uno de mentira.
class StackCanvas {
public:
    virtual ~StackCanvas() = default;
    // `count` hojas en blanco, activa la primera, sin deshacer ni guías.
    virtual void resetSheets(int count) = 0;
    virtual QSize sheetSize() const = 0;
    virtual double pixelsPerMm() const { return 96.0 / 25.4; }
    virtual void setSheetGuide(int index, const QPicture& guide) = 0;
    virtual void setGuidesVisible(bool visible) = 0;
};

// Ciclo de ejercicios de pila (HU-121): cada ejercicio arma su pila con su guía en cada hoja.
// Como ExerciseSession: siguiente, repetir, parámetros por ejercicio y modo mixto.
class StackSession {
public:
    // canvas y exercise deben vivir más que la sesión. No arma nada hasta next() o repeat().
    StackSession(StackCanvas* canvas, const StackExercise* exercise);

    quint32 seed() const { return m_seed; }
    const StackExercise* exercise() const { return m_exercise; }
    const QVariantMap& currentParams() const { return m_currentParams; }
    const StackGenerated& current() const { return m_current; }
    // Los parámetros configurados de un ejercicio (los de defecto si no se tocaron).
    QVariantMap params(const StackExercise* exercise) const;
    // Los del ejercicio actual (panel F2): rigen desde el siguiente; R repite el actual igual.
    void setParams(const QVariantMap& values);
    QVariantMap allParams() const;
    void loadParams(const QVariantMap& all);

    // Enter: otra semilla (en modo mixto, otro ejercicio), y la pila nueva.
    void next();
    // R: la misma pila, en blanco, en la primera hoja.
    void repeat();
    // Menú (F4): ese ejercicio, con semilla nueva; apaga el modo mixto.
    void setExercise(const StackExercise* exercise);
    void setMixedPool(const QList<const StackExercise*>& pool) { m_pool = pool; }
    void startMixed();
    bool mixed() const { return m_mixed; }

    // Las guías del ejercicio actual otra vez, para la hoja de ahora (al calibrar con F9), sin
    // tocar lo dibujado. La pila ya tiene que estar armada.
    void regenerate();
    // G: oculta y muestra las guías de todas las hojas.
    void toggleGuides();
    bool guidesVisible() const { return m_guidesVisible; }

    // Cambió el ejercicio (el panel muestra sus parámetros).
    std::function<void()> onExerciseChanged;

private:
    StackCanvas* m_canvas;
    const StackExercise* m_exercise;
    quint32 m_seed;
    QHash<QString, QVariantMap> m_configured; // por id de ejercicio
    QVariantMap m_currentParams;
    StackGenerated m_current;
    bool m_mixed = false;
    QList<const StackExercise*> m_pool;
    QStringList m_recent; // ids de los últimos dos ejercicios, del más viejo al más nuevo
    bool m_guidesVisible = true;
    void build();
};

} // namespace ejercicios
