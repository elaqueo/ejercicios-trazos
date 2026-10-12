#pragma once

#include "exercises/Exercise.h"

namespace ejercicios {

// HU-30 · Cajas rotadas: la misma caja girando sobre su eje vertical, en una fila de N cajas a
// un mismo lado del horizonte, cada una Δ grados más girada. Con el ojo a distancia f de la
// hoja, una caja girada α tiene sus PF sobre el horizonte a f·tan α y a −f/tan α del centro de
// visión: al girar, uno se acerca al centro y el otro se aleja (el producto queda en −f²).
// Guías por caja: la esquina numerada con su ángulo, dos rectas cortas desde la esquina hacia
// sus PF (las aristas de la base) y los PF numerados (rombo, o triángulo en el borde si quedan
// afuera). Parámetros: count, step (Δ, grados), eye (f en anchos de hoja).
// Ideal: el horizonte y, por caja, la esquina y sus dos PF (puntos sueltos).
class CajasRotadas : public Exercise {
public:
    QString id() const override;
    QString title() const override;
    QString group() const override;
    QList<appkit::Param> params() const override;
    Generated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const override;
};

} // namespace ejercicios
