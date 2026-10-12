#pragma once

#include "exercises/StackExercise.h"

#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QRandomGenerator>
#include <QRectF>

#include <span>
#include <vector>

// Lo común de las formas que giran (HU-123 prisma, HU-130 cilindro, Ejercicios de animación): la
// cámara (el giro sobre un eje, la inclinación y la perspectiva), los parámetros y el ghost
// (mesas "Prisma que gira" y "Cilindro que gira": gris tinta al 50 %, lo oculto punteado y más
// fino, el ángulo abajo a la derecha).
namespace ejercicios {

namespace giro {

enum class Axis { Vertical, Horizontal, Depth };

// La cámara mira un poco desde arriba, para que se vea la tapa.
constexpr double kElevationDeg = 22;

struct Vec3 {
    double x, y, z;
};
double dot(Vec3 a, Vec3 b);

// El punto (o la dirección) girado `degrees` sobre `axis` y después inclinado por la cámara.
Vec3 place(Vec3 p, Axis axis, double degrees);
// Un punto ya ubicado, visto con el ojo a `eye` del centro: en el plano del centro, en su tamaño
// real (x a la derecha, y hacia arriba).
QPointF project(Vec3 placed, double eye);
// Si la superficie de normal `normal` en `point` (ya ubicados) mira a la cámara, en (0, 0, eye).
bool facesCamera(Vec3 normal, Vec3 point, double eye);

// El eje que pide el parámetro (0 vertical, 1 horizontal, 2 profundidad, 3 al azar); al azar,
// de rng.
Axis axisFor(int param, QRandomGenerator& rng);

// Dónde va la forma en la hoja: el ojo (mm) y la escala (px por mm del plano del centro), según
// la perspectiva y el tamaño (la diagonal, en mm) de los parámetros. Achica si la esfera que
// envuelve la forma no entra en la hoja con cualquier giro.
struct Frame {
    double eye = 1;
    double radius = 1; // media diagonal, en mm
    double scale = 1;
    QPointF center;
    QRectF sheet;
    QPointF toSheet(QPointF mm) const { return center + QPointF(mm.x(), -mm.y()) * scale; }
};
Frame frame(const QVariantMap& values, const SafeZone& zone);

// Pinta el ghost de una hoja y guarda la geometría ideal: cada trazo (en mm del plano del
// centro) va como ruta a `ideal`; los visibles, llenos, y los ocultos, punteados si `hidden`.
class Ghost {
public:
    Ghost(Generated& out, const Frame& frame, const SafeZone& zone, bool hidden);
    ~Ghost();
    Ghost(const Ghost&) = delete;
    Ghost& operator=(const Ghost&) = delete;

    void line(QPointF a, QPointF b, bool visible);
    // Una curva (en mm) con la visibilidad de cada tramo (visible[i]: de points[i] a points[i+1]);
    // una sola ruta ideal, y en el dibujo los tramos seguidos iguales van juntos.
    void curve(std::span<const QPointF> points, const std::vector<bool>& visible);
    // El giro de la hoja, abajo a la derecha (arriba a la derecha va el número de la hoja).
    void angle(int degrees);

private:
    Generated& m_out;
    const Frame& m_frame;
    QPainter m_painter;
    QPen m_solid, m_behind;
    bool m_hidden;
    double m_margin;
    double m_fontPx;
};

} // namespace giro

// Base de las formas que giran: el grupo, las claves de los parámetros y los parámetros comunes.
class TurningExercise : public StackExercise {
public:
    QString group() const override;

    static const QString kAxis, kStep, kPerspective, kHidden, kSize;
    // El valor de kAxis que sortea el eje.
    static constexpr int kRandomAxis = 3;

protected:
    // Hojas, ghost solo en las keys, eje (con su valor por defecto), paso, perspectiva, aristas
    // ocultas y tamaño.
    static QList<appkit::Param> turnParams(int defaultAxis, const QString& hiddenLabel);
};

} // namespace ejercicios
