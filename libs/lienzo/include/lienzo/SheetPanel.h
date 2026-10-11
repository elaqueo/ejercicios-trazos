#pragma once

#include "lienzo/SheetStack.h"

#include <QColor>
#include <QImage>
#include <QRect>
#include <QString>

#include <cstdint>
#include <vector>

namespace lienzo {

// El panel de hojas (HU-114, Mesa de animación; mesa "Panel de hojas" del canvas): angosto, a
// la derecha de la pantalla, con la toma, la hoja activa y el total, las miniaturas de la
// activa y sus vecinas, el lápiz, la mesa de luz y el giro de la vista. El panel es una
// imagen que se pinta acá (en el hilo de la interfaz, solo cuando cambia algo); las
// miniaturas las pone el render encima, desde las texturas de las hojas (Renderer::setPanel).

// Un lugar de la tira de miniaturas: la hoja (id 0: fuera de la pila) y su número.
struct PanelSlot {
    uint64_t id = 0;
    int number = 0;      // desde 1
    bool center = false; // la activa (o la que se ve al flipear)
    bool operator==(const PanelSlot&) const = default;
};
// La hoja `center` y `around` antes y después, en orden; los lugares fuera de la pila quedan
// con id 0.
std::vector<PanelSlot> panelSlots(const SheetOrder& order, int center, int around = 3);

struct PanelInfo {
    QString take;       // el nombre de la toma (vacío: sin toma)
    bool saved = true;  // todo lo cambiado está escrito
    int number = 1;     // la hoja del centro, desde 1
    int total = 1;
    std::vector<PanelSlot> strip;
    QString lead;               // "HB · 0,87 mm"
    std::vector<QColor> light;  // los colores de la mesa de luz (vacío: apagada)
    int degrees = 0;            // giro de la vista
    QString key = QStringLiteral("Tab");
    bool operator==(const PanelInfo&) const = default;
};

// Dónde va cada cosa, en píxeles del cliente. Las medidas del diseño están en px a escala 1;
// `scale` es la de Windows. Si la pantalla es baja, las miniaturas se achican para que entre.
struct PanelLayout {
    QRect panel;               // en el cliente
    std::vector<QRect> thumbs; // la imagen de cada miniatura, dentro del panel (desde su esquina)
    double scale = 1;
    int rowHeight = 0;         // de cada lugar de la tira, en px del cliente
};
// sheetAspect: ancho / alto de la hoja, para que la miniatura tenga su forma.
PanelLayout panelLayout(int clientWidth, int clientHeight, double scale, double sheetAspect, int count = 7);

// El panel pintado: fondo, textos, números y marcos de las miniaturas (el render pone la
// imagen de cada una adentro). outside: el color de atrás, para las esquinas redondeadas.
QImage paintPanel(const PanelInfo& info, const PanelLayout& layout, const QColor& paper, const QColor& outside);

} // namespace lienzo
