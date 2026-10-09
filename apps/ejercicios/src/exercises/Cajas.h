#pragma once

#include "exercises/Exercise.h"

namespace ejercicios {

// HU-28 · Cajas en perspectiva: un horizonte (siempre horizontal sobre la hoja, la
// convención de la perspectiva) con 1 o 2 puntos de fuga en rombos ámbar y la esquina de
// partida de la caja. Con 2 PF la esquina cae dentro del cono de visión (a no más de un tercio
// de la separación desde el punto medio entre los PF), lejos de donde la caja se deforma. Un
// PF fuera de la hoja se marca con un triángulo en el borde, sobre el horizonte, y dos rectas
// de ejemplo que fugan hacia él (una por encima y otra por debajo del horizonte).
// Parámetros: pf1 y pf2 (modos habilitados; sin ninguno, los dos) y separation (2 PF, en
// anchos de hoja; más de 1 sale de la hoja).
// Ideal: el horizonte, la esquina de partida, los PF (puntos sueltos) y las rectas de ejemplo
// de los PF que quedan afuera, en ese orden.
class Cajas : public Exercise {
public:
    QString id() const override;
    QString title() const override;
    QString group() const override;
    QList<appkit::Param> params() const override;
    Generated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const override;
};

} // namespace ejercicios
