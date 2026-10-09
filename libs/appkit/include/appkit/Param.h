#pragma once

#include <QList>
#include <QString>
#include <QVariant>
#include <QVariantMap>

namespace appkit {

// Un parámetro configurable (HU-12): cada app o ejercicio declara los suyos y ParamForm arma
// los controles. Los valores viajan en un QVariantMap por clave, que es lo que reciben los
// ejercicios y lo que guarda Config.
struct Param {
    enum class Type { Real, Integer, Toggle };

    QString key;
    QString label;
    Type type = Type::Real;
    double minimum = 0;
    double maximum = 1;
    double step = 0.01; // Real: paso del deslizador; Integer: 1 si no se dice otra cosa
    QVariant defaultValue;
    int decimals = 2;   // Real: decimales que se muestran
    QString suffix;     // unidad que se muestra después del valor (" mm", " %")

    // El valor llevado al tipo y al rango del parámetro, redondeado al paso; sin valor,
    // el de defecto.
    QVariant clamp(const QVariant& value) const;
};

QVariantMap defaultValues(const QList<Param>& params);
// Un valor por parámetro, recortado al rango; lo que falta toma el valor por defecto y las
// claves que no son de la lista se descartan.
QVariantMap clampValues(const QList<Param>& params, const QVariantMap& values);

} // namespace appkit
