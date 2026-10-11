#pragma once

#include <QColor>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QSize>
#include <QString>
#include <QWidget>

#include <functional>
#include <vector>

namespace lienzo {

// Una acción del anillo de afuera del menú del lápiz (HU-116): el atajo que corre, por su nombre
// en el registro, cómo se llama en el anillo y su ícono. key es la tecla que se muestra (la pone
// Lienzo desde el registro).
struct PenMenuAction {
    QString shortcut;
    QString label;
    QString icon;
    QString key;
};

// Menú radial del botón del lápiz (HU-116, mesa "Menú del lápiz" del canvas), como la paleta
// emergente de Krita: una ventana transparente del tamaño de la hoja (así un toque fuera del anillo
// no dibuja) con el anillo centrado en la punta, corrido lo justo para entrar entero. Adentro los
// lápices (elegir uno cierra el menú); afuera las acciones (el menú queda abierto, para deshacer
// dos veces o ir y volver de hoja). Se elige tocando con la punta, o manteniendo el botón y
// soltándolo sobre la opción; tocar fuera del anillo, el botón otra vez o Esc lo cierran.
class PenMenu : public QWidget {
public:
    explicit PenMenu(QWidget* owner);

    static constexpr int kRadius = 176;      // borde de afuera
    static constexpr int kActionInner = 108; // las acciones, entre 108 y 172
    static constexpr int kActionOuter = 172;
    static constexpr int kPencilRing = 104;  // el disco de los lápices
    static constexpr int kPencilOrbit = 68;  // centro de cada lápiz
    static constexpr int kPencilDot = 20;
    static constexpr int kCenter = 30;       // la punta: no es una opción
    static constexpr int kMargin = 8;        // del anillo al borde de la hoja

    // Qué hay en un punto, relativo al centro del anillo (y hacia abajo, como la pantalla).
    struct Hit {
        enum Kind { Center, Pencil, Action, Outside };
        Kind kind = Outside;
        int index = -1;
        bool operator==(const Hit&) const = default;
    };
    static Hit hitTest(QPointF offset, int pencils, int actions);
    // Ángulo de la opción i de n (grados, antihorario desde la derecha): la primera arriba y las
    // demás en el sentido del reloj.
    static double angleOf(int i, int n);
    // El centro del anillo con la punta en `tip`, en un área de `size`: la punta, corrida lo justo
    // para que el anillo entre entero (si el área es más chica que el anillo, al medio).
    static QPoint centerFor(QPoint tip, QSize size);

    // colors[0] el grafito, después los colores. active: índice en colors.
    void setPencils(std::vector<QColor> colors, int active);
    void setActions(std::vector<PenMenuAction> actions);
    const std::vector<PenMenuAction>& actions() const { return m_actions; }
    // Lo abre sobre `area` (pantalla, píxeles de Qt) con la punta en `tip` (relativa al área). El
    // botón está apretado: soltarlo sobre otra opción la elige.
    void open(const QRect& area, QPoint tip);
    QPoint ringCenter() const { return m_center; }

    std::function<void(int)> onPencil;               // índice en colors; ya se cerró
    std::function<void(int)> onAction;               // índice en actions; sigue abierto
    std::function<void(const char* how)> onDismiss;  // se cerró sin elegir: "fuera", "botón", "Esc"

    // Lo que hace la punta (también desde el mouse): toque, botón apretado o soltado, en el aire.
    void tipDown(QPointF position);
    void barrel(bool down, QPointF position);
    void hover(QPointF position);

protected:
    void paintEvent(QPaintEvent* event) override;
    // El lápiz, de WM_POINTER: Qt no siempre dice el botón lateral en el aire.
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    Hit hitAt(QPointF position) const;
    void choose(const Hit& hit);
    void dismiss(const char* how);

    std::vector<QColor> m_colors;
    std::vector<PenMenuAction> m_actions;
    int m_active = 0;
    QPoint m_center;
    Hit m_hover;
    Hit m_openedOn;            // lo que había bajo la punta al abrir: soltar ahí no elige
    bool m_barrelDown = false;
};

} // namespace lienzo
