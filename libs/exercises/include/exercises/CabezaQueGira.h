#pragma once

#include "exercises/Turntable.h"

#include <QPointF>

#include <array>
#include <vector>

// Cabeza que gira (HU-131, Ejercicios de animación): la cabeza de Loomis simplificada gira sobre
// un eje, un paso por hoja, con su ghost: la esfera del cráneo, la línea media y la de las cejas,
// los cortes laterales (la oreja) y la cara (la línea media que baja al mentón y la mandíbula). La
// línea media hace de costura. Solo la estructura, sin nariz ni boca (mesa "Cono y cabeza que
// giran"); la cámara y el ghost, en Turntable.h.
namespace ejercicios {

namespace cabeza {

using Axis = giro::Axis;

// La cabeza con el cráneo de radio `skull` centrado en el origen (el pivote), la cara hacia +z:
// los cortes laterales de radio `cut` en x = ±√(skull² − cut²), la línea de las cejas en y = 0, la
// base de la nariz en y = −cut y el mentón en (0, −chinDrop, chinForward).
struct Shape {
    double skull = 1, cut = 0.62, chinDrop = 1.3, chinForward = 0.72;
};

// Los círculos, en kSteps tramos (cada curva cierra: kSteps + 1 puntos).
constexpr int kSteps = 96;

struct Segment {
    QPointF a, b;
    bool visible = true;
};
struct Curve {
    std::vector<QPointF> points;
    std::vector<bool> visible; // de cada tramo
};

// La cabeza girada `degrees` sobre `axis`, con la cámara inclinada y el ojo a `eye` mm, en mm
// sobre el plano del centro:
// - outline: el contorno de la esfera (el círculo de tangencia de los rayos del ojo, proyectado);
// - middle y brow: la línea media y la de las cejas, lo de atrás oculto;
// - cuts[0] el corte de la izquierda (x < 0) y cuts[1] el de la derecha: se ven si miran a la cámara;
// - face: la línea media de la cara, de la nariz al mentón; jaw: las dos líneas de la mandíbula.
struct View {
    Curve outline, middle, brow;
    std::array<Curve, 2> cuts;
    Segment face;
    std::array<Segment, 2> jaw;
};
View project(const Shape& shape, Axis axis, double degrees, double eye);

// Lo que sale de la semilla: la cabeza (el largo de la cara y el corte varían poco; todo dentro de
// la esfera de radio 1), el eje si es al azar y el giro de la primera hoja.
struct Draw {
    Shape shape;
    Axis axis = Axis::Vertical;
    int startDeg = 0;
};
Draw draw(quint32 seed, int axisParam);

} // namespace cabeza

class CabezaQueGira : public TurningExercise {
public:
    QString id() const override;
    QString title() const override;
    QList<appkit::Param> params() const override;
    StackGenerated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const override;
};

} // namespace ejercicios
