#pragma once

#include "paintcore/BrushLibrary.h"

#include <QPicture>
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

    // Área útil: rectángulo del widget (en sus coordenadas) que ocupa el lienzo. Fuera
    // se ve un gris neutro, y la rotación gira alrededor de su centro. Vacío (por
    // defecto): todo el widget. Cambiarlo reinicia el lienzo.
    void setCanvasRect(const QRect& rect);
    QRect canvasRect() const;

    // Color de la hoja (debajo del trazo; la superficie de libmypaint es transparente y
    // premultiplicada, así que el trazo se compone sobre ella sin halos) y de lo que
    // queda fuera del área útil. Por defecto, blanco y gris oscuro.
    void setPaperColor(const QColor& color);
    void setOutsideColor(const QColor& color);

    // Capa de guías: un dibujo en coordenadas del lienzo que se pinta entre la hoja y
    // la tinta, con la rotación de la vista, sin pasar por libmypaint. Ocultarla o
    // cambiarla no toca la superficie del trazo.
    void setGuides(const QPicture& guides);
    void clearGuides();
    void setGuidesVisible(bool visible);
    bool guidesVisible() const;

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
    // Se apretó un botón lateral del lápiz (Qt::RightButton o Qt::MiddleButton, según
    // el driver). No pinta: la app decide qué hace.
    void stylusButtonClicked(Qt::MouseButton button);

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
