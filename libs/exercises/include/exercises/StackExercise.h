#pragma once

#include "exercises/Exercise.h"

#include <QList>

// Ejercicios de pila (HU-121, Ejercicios de animación): como Exercise, pero generan una guía
// por hoja de una pila. La sesión de pila (StackSession) arma la pila y pone cada guía en su hoja.
namespace ejercicios {

// Lo que produce un ejercicio de pila para una semilla: la guía y la geometría ideal de cada
// hoja, en orden (tantas como hojas).
struct StackGenerated {
    QList<Generated> sheets;
};

// La cantidad de hojas es un parámetro de todo ejercicio de pila (de 2 a 24).
inline const QString kSheetsParam = QStringLiteral("hojas");
constexpr int kMinSheets = 2;
constexpr int kMaxSheets = 24;
appkit::Param sheetsParam(int defaultSheets);
// Las hojas que piden estos parámetros, recortadas al rango.
int sheetCount(const QVariantMap& params);

class StackExercise {
public:
    virtual ~StackExercise() = default;

    virtual QString id() const = 0;
    virtual QString title() const = 0;
    virtual QString group() const = 0;
    // Incluye sheetsParam.
    virtual QList<appkit::Param> params() const = 0;
    QVariantMap defaults() const { return appkit::defaultValues(params()); }
    // Determinista (RNF-03): misma semilla y mismos parámetros, las mismas guías. Tantas hojas
    // como sheetCount(params).
    virtual StackGenerated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const = 0;
};

// Un ejercicio de Ejercicios en pila (HU-122): la misma consigna (misma semilla, misma guía) en
// todas las hojas, para ver al flipear si el trazo se mantiene. Mismo id, título y grupo, y sus
// parámetros más la cantidad de hojas. `exercise` tiene que vivir más que el adaptador.
class SameOnEverySheet : public StackExercise {
public:
    SameOnEverySheet(const Exercise& exercise, int defaultSheets);
    QString id() const override { return m_exercise.id(); }
    QString title() const override { return m_exercise.title(); }
    QString group() const override { return m_exercise.group(); }
    QList<appkit::Param> params() const override;
    StackGenerated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const override;

private:
    const Exercise& m_exercise;
    int m_defaultSheets;
};

} // namespace ejercicios
