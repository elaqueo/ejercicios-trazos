#pragma once

#include <QList>
#include <QPointF>
#include <QWidget>

namespace appkit {

// Overlay que cubre la ventana para calibrar el área útil: pide tocar con el lápiz
// la esquina superior izquierda y después la inferior derecha de la superficie
// activa de la tableta. Como el lápiz solo llega a la zona mapeada, esos dos toques
// dan el rectángulo exacto. Esc cancela. Lo usa el lienzo de baja latencia, dentro
// de una ventana propia que cubre el monitor (HU-66). Colores de appkit::theme; el layout del
// diseño de HU-37 (indicador de pasos, dibujo de la tableta) queda para más adelante.
class CalibrationOverlay : public QWidget {
    Q_OBJECT

public:
    explicit CalibrationOverlay(QWidget* parent);

    // Muestra el overlay sobre todo el padre y empieza de cero.
    void start();
    // Lo oculta sin calibrar (Esc).
    void cancel();

signals:
    // Rectángulo en coordenadas del padre.
    void finished(const QRect& rect);
    void cancelled();

protected:
    void paintEvent(QPaintEvent* event) override;
    void tabletEvent(QTabletEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void addCorner(QPointF pos);

    QList<QPointF> m_corners;
};

} // namespace appkit
