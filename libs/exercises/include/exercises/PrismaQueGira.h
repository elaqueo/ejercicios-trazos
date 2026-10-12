#pragma once

#include "exercises/Turntable.h"

#include <QPointF>

#include <array>

// Prisma que gira (HU-123, Ejercicios de animación): un prisma de largo, ancho y alto al azar
// gira sobre uno de sus ejes, un paso por hoja; en cada hoja va su wireframe con el giro de esa
// hoja, tenue (el ghost), para dibujar encima y ver al flipear si el volumen se mantiene.
// Diseño en la mesa "Prisma que gira" del canvas; la cámara y el ghost, en Turntable.h.
namespace ejercicios {

namespace prisma {

using Axis = giro::Axis;
constexpr double kElevationDeg = giro::kElevationDeg;

// Un prisma centrado en el origen, en mm: x a lo ancho, y hacia arriba, z hacia la cámara.
struct Box {
    double width = 1, height = 1, depth = 1;
};

// Las 8 esquinas: el bit 2 del índice es x, el 1 es y y el 0 es z (0: negativo, 1: positivo).
// Las 12 aristas unen esquinas que difieren en un bit.
constexpr int kCorners = 8;
constexpr int kEdges = 12;
struct Edge {
    int a, b;
};
std::array<Edge, kEdges> edges();

// El prisma girado `degrees` sobre `axis`, visto con la cámara inclinada kElevationDeg y a
// `eye` mm del centro: las esquinas en mm sobre el plano del centro (x a la derecha, y hacia
// arriba, en su tamaño real a la distancia del centro) y si cada arista se ve. Una arista
// está oculta si las dos caras que la tocan dan la espalda a la cámara.
struct View {
    std::array<QPointF, kCorners> corners;
    std::array<bool, kEdges> visible;
};
View project(const Box& box, Axis axis, double degrees, double eye);

// Lo que sale de la semilla: las medidas (proporciones de 1:1 a 1:3, diagonal 1), el eje si
// es al azar y el giro de la primera hoja (múltiplo de 5°).
struct Draw {
    Box box;
    Axis axis = Axis::Vertical;
    int startDeg = 0;
};
Draw draw(quint32 seed, int axisParam);

} // namespace prisma

class PrismaQueGira : public TurningExercise {
public:
    QString id() const override;
    QString title() const override;
    QList<appkit::Param> params() const override;
    StackGenerated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const override;
};

} // namespace ejercicios
