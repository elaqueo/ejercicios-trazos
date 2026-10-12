#pragma once

#include "exercises/Exercise.h"

namespace ejercicios {

// HU-31 · Elipses en perspectiva: un cuadrado en perspectiva para inscribirle la elipse. Es un
// cuadrado verdadero en 3D proyectado con el modelo de cámara de las cajas rotadas (ojo a
// distancia f, horizonte a la altura de los ojos), así que es correcto por construcción: sus
// aristas fugan a los PF de su orientación. Planos: piso o techo (horizontal, nunca cruza el
// horizonte) y pared (vertical). Con 1 PF va de frente (el piso) o de costado (la pared) y
// fuga al centro; con 2 PF va girado un ángulo al azar. Guías: horizonte, cuadrado firme,
// diagonales punteadas (el centro), PF en la hoja como rombos y, hacia los de afuera, las
// aristas prolongadas. Parámetros: floor, wall, pf1, pf2, size (lado aparente, × alto de la
// hoja), eye (f, × ancho de la hoja).
// Ideal: el horizonte, el cuadrado (4 esquinas, cerrado), la elipse inscrita exacta (el
// círculo proyectado, cerrado) y los PF del cuadrado (puntos sueltos).
class ElipsePerspectiva : public Exercise {
public:
    QString id() const override;
    QString title() const override;
    QString group() const override;
    QList<appkit::Param> params() const override;
    Generated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const override;
};

} // namespace ejercicios
