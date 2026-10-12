#pragma once

#include "exercises/Turntable.h"

#include <QPointF>

#include <vector>

// Cono que gira (HU-131, Ejercicios de animación): como el cilindro, un cono de radio y altura al
// azar gira sobre un eje, un paso por hoja, con su ghost y su costura. Diseño en la mesa "Cono y
// cabeza que giran" del canvas; la cámara y el ghost, en Turntable.h.
namespace ejercicios {

namespace cono {

using Axis = giro::Axis;

// Un cono con su eje en y: la base de radio `radius` en y = −height/2 y el vértice en y = +height/2.
struct Shape {
    double radius = 1, height = 1;
};

// La base: un círculo en kRimSteps tramos (la curva cierra: kRimSteps + 1 puntos).
constexpr int kRimSteps = 96;
// La costura va en la generatriz que a 0° mira a la cámara; la marca, de ahí hacia el centro de la
// base, hasta kMarkRatio del radio.
constexpr double kMarkRatio = 0.45;

struct Segment {
    QPointF a, b;
    bool visible = true;
};

// El cono girado `degrees` sobre `axis`, con la cámara inclinada y el ojo a `eye` mm, en mm sobre
// el plano del centro:
// - rim, la base, con la visibilidad de cada tramo (se ve si lo ve la base o el costado ahí);
// - silhouette: las generatrices de canto a la cámara, del borde de la base al vértice (ninguna si
//   el vértice cae adentro de la base, visto de punta);
// - seam y mark: la costura y la marca en la base.
struct View {
    std::vector<QPointF> rim;
    std::vector<bool> rimVisible;
    QPointF apex;
    std::vector<Segment> silhouette;
    Segment seam, mark;
};
View project(const Shape& shape, Axis axis, double degrees, double eye);

// Lo que sale de la semilla: las medidas (altura/diámetro de 1:3 a 3:1, la esfera de radio 1
// alrededor del centro del eje lo envuelve), el eje si es al azar y el giro de la primera hoja.
struct Draw {
    Shape shape;
    Axis axis = Axis::Horizontal;
    int startDeg = 0;
};
Draw draw(quint32 seed, int axisParam);

} // namespace cono

class ConoQueGira : public TurningExercise {
public:
    QString id() const override;
    QString title() const override;
    QList<appkit::Param> params() const override;
    StackGenerated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const override;

    // Con o sin costura (además de las comunes de TurningExercise).
    static const QString kSeam;
};

} // namespace ejercicios
