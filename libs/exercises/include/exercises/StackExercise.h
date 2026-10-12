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
// Ghost solo en las keys (HU-125): la guía va solo en la primera hoja, la del medio y la última;
// las intermedias se dibujan de memoria. Lo tienen los ejercicios de pila de la app; apagado por
// defecto.
inline const QString kKeysParam = QStringLiteral("soloKeys");
appkit::Param keysParam();
bool keysOnly(const QVariantMap& params);
// Las keys de una pila de `count` hojas: la primera, la del medio (count / 2, desde 0) y la
// última, sin repetir.
QList<int> keySheets(int count);

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
