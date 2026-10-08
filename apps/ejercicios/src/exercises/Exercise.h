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

// Zona segura: círculo inscripto en el área útil. Todo lo que se genera adentro
// sigue visible con cualquier rotación de vista. Coordenadas del lienzo.
struct SafeZone {
    QPointF center;
    qreal radius = 0;

    static SafeZone fromRect(const QRect& area);
    // Punto a dist del centro en la dirección angle (radianes, horario en pantalla).
    QPointF pointAt(qreal angle, qreal dist) const;
    bool contains(QPointF p) const;
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
    // Parámetros por defecto; el panel edita y Config guarda este mismo mapa.
    virtual QVariantMap defaults() const = 0;
    // Determinista: misma semilla y mismos parámetros, misma geometría (RNF-08).
    // Usar QRandomGenerator(seed) y nada más como fuente de azar.
    virtual Generated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const = 0;
};

} // namespace ejercicios
