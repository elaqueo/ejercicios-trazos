#pragma once

#include "paintcore/BrushLibrary.h"

#include <QWidget>

#include <memory>

namespace paintcore {

// Lienzo de dibujo: pinta con libmypaint a partir de la tableta (presión,
// inclinación y tiempo de cada muestra) o del mouse (presión fija). La vista se
// puede rotar alrededor del centro: el trazo se guarda en coordenadas del lienzo
// y al input se le aplica la rotación inversa, así cae bajo la punta del lápiz.
class CanvasWidget : public QWidget {
    Q_OBJECT

public:
    explicit CanvasWidget(QWidget* parent = nullptr);
    ~CanvasWidget() override;

    // Cambia el pincel. Si hay un trazo en curso, se aplica desde el siguiente.
    // Un preset inválido deja el pincel por defecto (con un warning en el log).
    void setBrush(const BrushPreset& preset);

    // Borra todo lo pintado (el lienzo queda en blanco). Si hay un trazo en curso,
    // las muestras siguientes pintan sobre el lienzo limpio.
    void clear();

    // Rotación de la vista en grados, [0, 360), horaria.
    double viewRotation() const;
    void setViewRotation(double degrees);
    void resetViewRotation() { setViewRotation(0.0); }

    // Con snap, rotar con el gesto salta de a kRotationSnapStep grados.
    bool rotationSnap() const;
    void setRotationSnap(bool enabled);

    // Presión que se usa al pintar con mouse (0..1).
    static constexpr float kMousePressure = 0.5f;
    // Modificador + arrastre (lápiz o mouse) rota la vista en vez de pintar.
    static constexpr Qt::KeyboardModifier kRotateModifier = Qt::ShiftModifier;
    // Tecla que vuelve la rotación a 0°.
    static constexpr Qt::Key kResetRotationKey = Qt::Key_5;
    static constexpr double kRotationSnapStep = 15.0;

signals:
    void viewRotationChanged(double degrees);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void tabletEvent(QTabletEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    enum class Phase { Press, Move, Release };
    struct Sample;
    void handlePointer(Phase phase, const Sample& sample);
    void updateCanvasRect(const QRect& canvasRect);

    // pimpl: el estado de libmypaint queda fuera del header público (RNF-05).
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace paintcore
