#pragma once

#include "exercises/Turntable.h"

#include <QPointF>

#include <vector>

// Cilindro que gira (HU-130, Ejercicios de animación): como el prisma, un cilindro de radio y
// largo al azar gira sobre un eje, un paso por hoja, con su ghost en cada hoja. Una costura (una
// generatriz y una marca en la tapa) deja ver el giro, aun sobre su propio eje. Diseño en la mesa
// "Cilindro que gira" del canvas; la cámara y el ghost, en Turntable.h.
namespace ejercicios {

namespace cilindro {

using Axis = giro::Axis;

// Un cilindro centrado en el origen, en mm, con su eje en y: las tapas en y = ±length/2.
struct Shape {
    double radius = 1, length = 1;
};

// Puntos de las tapas: un círculo en kRimSteps tramos (la curva cierra: kRimSteps + 1 puntos).
constexpr int kRimSteps = 96;
// La costura va en la generatriz que a 0° mira a la cámara (ángulo 90° alrededor del eje); la
// marca, de ahí hacia el centro de la tapa de arriba, hasta kMarkRatio del radio.
constexpr double kMarkRatio = 0.45;

struct Segment {
    QPointF a, b;
    bool visible = true;
};

// El cilindro girado `degrees` sobre `axis`, con la cámara inclinada y el ojo a `eye` mm, en
// mm sobre el plano del centro:
// - rims[0] la tapa de abajo y rims[1] la de arriba, con la visibilidad de cada tramo (se ve si lo
//   ve su tapa o el costado en ese punto);
// - silhouette: las generatrices de canto a la cámara, el contorno del costado (ninguna si se lo
//   ve justo de punta);
// - seam y mark: la costura y la marca en la tapa.
struct View {
    std::vector<QPointF> rims[2];
    std::vector<bool> rimVisible[2];
    std::vector<Segment> silhouette;
    Segment seam, mark;
};
View project(const Shape& shape, Axis axis, double degrees, double eye);

// Lo que sale de la semilla: las medidas (largo/diámetro de 1:3 a 3:1, la esfera que lo envuelve
// de radio 1), el eje si es al azar y el giro de la primera hoja (múltiplo de 5°).
struct Draw {
    Shape shape;
    Axis axis = Axis::Horizontal;
    int startDeg = 0;
};
Draw draw(quint32 seed, int axisParam);

} // namespace cilindro

class CilindroQueGira : public TurningExercise {
public:
    QString id() const override;
    QString title() const override;
    QList<appkit::Param> params() const override;
    StackGenerated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const override;

    // Con o sin costura (además de las comunes de TurningExercise).
    static const QString kSeam;
};

} // namespace ejercicios
