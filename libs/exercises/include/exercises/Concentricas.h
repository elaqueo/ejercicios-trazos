#pragma once

#include "exercises/Exercise.h"

namespace ejercicios {

// HU-27 · Elipses concéntricas sobre un eje: varias elipses del mismo ancho con un eje menor
// común (las secciones de un cilindro). Guías: el eje firme, cada centro, los extremos del eje
// mayor de cada una como puntos de paso y su grado en ámbar a la derecha. Los grados van en
// progresión del mínimo al máximo (redondeados a 5°, sentido al azar) o en desorden; las
// elipses no se tocan. Parámetros: count, degMin, degMax, widthMin, widthMax (ancho como
// fracción del diámetro de la zona) y shuffle.
// Ideal: el eje y cada elipse, en orden a lo largo del eje.
class Concentricas : public Exercise {
public:
    QString id() const override;
    QString title() const override;
    QString group() const override;
    QList<appkit::Param> params() const override;
    Generated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const override;
};

} // namespace ejercicios
