#pragma once

#include <QPointF>
#include <QTransform>

namespace paintcore {

// Rotación de la vista alrededor de un centro. El trazo vive siempre en
// coordenadas del lienzo; solo la vista rota (sin zoom ni pan, por alcance).
// Ángulos en grados, positivos en sentido horario en pantalla (eje y hacia abajo).
class ViewTransform {
public:
    QPointF center() const { return m_center; }
    void setCenter(QPointF center);

    // Normalizado a [0, 360).
    double angle() const { return m_angle; }
    void setAngle(double degrees);

    // Lienzo → pantalla, y su inversa.
    QPointF toView(QPointF canvasPoint) const;
    QPointF toCanvas(QPointF viewPoint) const;
    const QTransform& matrix() const { return m_matrix; }

    // Normaliza a [0, 360).
    static double normalized(double degrees);
    // Redondea al múltiplo de step más cercano y normaliza.
    static double snapped(double degrees, double step);

private:
    void update();

    QPointF m_center;
    double m_angle = 0.0;
    QTransform m_matrix;
    QTransform m_inverse;
};

} // namespace paintcore
