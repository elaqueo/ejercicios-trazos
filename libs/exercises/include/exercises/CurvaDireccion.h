#pragma once

#include "exercises/Exercise.h"

namespace ejercicios {

// HU-80 · Curvas, dirección forzada: como la dirección forzada de las líneas (HU-25), pero el
// trazo es una curva, y la curva objetivo se ve tenue (línea de construcción) todo el tiempo;
// G la oculta como al resto de las guías. Dos extremos y, del lado de afuera de la curva (el
// opuesto a la panza), la flecha ámbar con el sentido. La dirección es la de la cuerda, sobre
// la hoja (no con la orientación de la zona), con las mismas 8 casillas y la variación ±15°
// de Direccion; largo y desvío como "Tres puntos → curva".
// Ideal: la cuadrática de la partida a la llegada.
class CurvaDireccion : public Exercise {
public:
    QString id() const override;
    QString title() const override;
    QString group() const override;
    QList<appkit::Param> params() const override;
    Generated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const override;
};

} // namespace ejercicios
