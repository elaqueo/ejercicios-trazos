#pragma once

#include "exercises/Exercise.h"

namespace ejercicios {

// HU-24 · Radiales: un centro (ámbar, el destino) y puntos de partida a su alrededor, todos a
// la misma distancia, para trazar rectas que converjan en él. Los ángulos van parejos con
// ±20 % de variación; con `sector`, los puntos cubren solo 180° o 270°.
// Parámetros: count, dist (fracción del diámetro de la zona), sector.
// Ideal: una recta por punto de partida, del punto al centro.
class Radiales : public Exercise {
public:
    QString id() const override;
    QString title() const override;
    QString group() const override;
    QList<appkit::Param> params() const override;
    Generated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const override;
};

} // namespace ejercicios
