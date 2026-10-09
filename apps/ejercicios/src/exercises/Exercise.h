#pragma once

#include <QList>
#include <QPainterPath>
#include <QPicture>
#include <QPointF>
#include <QRect>
#include <QString>
#include <QVariantMap>

// Contrato de ejercicio (HU-15). El núcleo (ciclo siguiente/repetir, modo mixto,
// panel) solo conoce esta interfaz; cada ejercicio concreto implementa generate().
// Vive en la app, no en appkit: las bibliotecas no saben de ejercicios (RNF-05/06).
namespace ejercicios {

// Zona segura (HU-18): círculo inscripto en el área útil, con una orientación. Todo
// lo que se genera adentro sigue visible con cualquier rotación de vista. Los
// ejercicios generan en su propio marco (ángulos relativos a orientation) y usan
// pointAt/toCanvas para pasar a coordenadas del lienzo: así la orientación aleatoria
// la pone el núcleo y ningún ejercicio la implementa.
struct SafeZone {
    QPointF center;
    qreal radius = 0;
    qreal orientation = 0; // radianes

    static SafeZone fromRect(const QRect& area, qreal orientation = 0);
    // Orientación al azar en 360°, derivada de la semilla: repetir el ejercicio
    // (misma semilla) repite la orientación.
    static SafeZone withRandomOrientation(const QRect& area, quint32 seed);

    // Punto a dist del centro en la dirección angle (radianes, horario en pantalla),
    // relativo a orientation.
    QPointF pointAt(qreal angle, qreal dist) const;
    // Punto en el marco del ejercicio (origen en el centro, sin rotar) → lienzo.
    QPointF toCanvas(QPointF local) const;

    bool contains(QPointF p) const;
    // Todos los puntos de control de la ruta están adentro. Para una curva de Bézier
    // eso garantiza que la curva entera está adentro (está en la envolvente convexa
    // de sus puntos de control).
    bool contains(const QPainterPath& path) const;
};

// Lo que produce un ejercicio para una semilla dada.
struct Generated {
    // Geometría ideal, en coordenadas del lienzo: una ruta por trazo esperado. La v1
    // no la evalúa, pero se conserva para comparar el trazo en versiones futuras.
    QList<QPainterPath> ideal;
    // Guías a mostrar, compuestas con appkit::guides (CanvasWidget::setGuides).
    QPicture guides;
};

class Exercise {
public:
    virtual ~Exercise() = default;

    // Identificador estable para configuración y menú ("recta").
    virtual QString id() const = 0;
    // Nombre para la interfaz ("Dos puntos → recta").
    virtual QString title() const = 0;
    // Grupo del menú ("Rectas"), como en la tabla de ejercicios de docs/alcance-v1.md.
    virtual QString group() const = 0;
    // Parámetros por defecto; el panel edita y Config guarda este mismo mapa.
    virtual QVariantMap defaults() const = 0;
    // Determinista: misma semilla y mismos parámetros, misma geometría (RNF-08).
    // Usar QRandomGenerator(seed) y nada más como fuente de azar. Toda la geometría
    // ideal debe quedar dentro de la zona (zone.contains); los puntos de fuga pueden
    // quedar afuera, pero sus líneas guía no.
    virtual Generated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const = 0;
};

} // namespace ejercicios
