#pragma once

#include "exercises/Exercise.h"

namespace ejercicios {

// HU-21 · Dos puntos → recta: dos puntos al azar para unirlos con un trazo recto.
// Parámetros: distMin y distMax, fracción del diámetro de la zona segura.
class Recta : public Exercise {
public:
    QString id() const override;
    QString title() const override;
    QVariantMap defaults() const override;
    Generated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const override;
};

} // namespace ejercicios
