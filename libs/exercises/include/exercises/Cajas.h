#pragma once

#include "exercises/Exercise.h"

namespace ejercicios {

// HU-28 · Cajas en perspectiva: un horizonte (siempre horizontal sobre la hoja, la
// convención de la perspectiva) con 1 o 2 puntos de fuga en rombos ámbar y la esquina de
// partida de la caja. Con 2 PF la esquina cae dentro del cono de visión (a no más de un tercio
// de la separación desde el punto medio entre los PF), lejos de donde la caja se deforma. Un
// PF fuera de la hoja se marca con un triángulo en el borde, sobre el horizonte, y dos rectas
// de ejemplo que fugan hacia él (una por encima y otra por debajo del horizonte).
// HU-29: con 3 PF se suma el de las verticales, sobre la vertical del foco y del lado de la
// caja (a thirdDistance altos de hoja del horizonte); la arista inicial opcional es la
// vertical más cercana de la caja, desde la esquina alejándose del horizonte: vertical exacta
// con 1 o 2 PF, hacia el tercer PF con 3.
// Parámetros: pf1, pf2 y pf3 (modos habilitados; sin ninguno, todos), separation (2 y 3 PF,
// en anchos de hoja; más de 1 sale de la hoja), thirdDistance, edge y edgeLength.
// Ideal: el horizonte, la esquina de partida, los PF (puntos sueltos; el tercero es el que no
// está sobre el horizonte), las rectas de ejemplo de los PF que quedan afuera y, al final, la
// arista inicial si está activa.
class Cajas : public Exercise {
public:
    QString id() const override;
    QString title() const override;
    QString group() const override;
    QList<appkit::Param> params() const override;
    Generated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const override;
};

} // namespace ejercicios
