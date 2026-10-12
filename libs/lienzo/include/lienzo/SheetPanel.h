#pragma once

#include "lienzo/FixedSheet.h"
#include "lienzo/SheetRole.h"
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
    SheetRole role = SheetRole::None; // HU-104: el número va con su marca
    bool operator==(const PanelSlot&) const = default;
};
// La hoja `center` y `around` antes y después, en orden; los lugares fuera de la pila quedan
// con id 0. La hoja fija (HU-89) no está en la tira y no se numera; `center` no puede ser ella.
// circle (HU-94): la tira da la vuelta (sin repetir hojas).
std::vector<PanelSlot> panelSlots(const SheetOrder& order, int center, int around = 3, const SheetRoles& roles = {},
                                  uint64_t fixed = 0, bool circle = false);

struct PanelInfo {
    QString take;       // el nombre de la toma (vacío: sin toma)
    bool saved = true;  // todo lo cambiado está escrito
    // HU-120: el rótulo del encabezado y si muestra el estado del guardado (sin toma, la app
    // pone lo suyo: Ejercicios de animación, "PILA" y "no se guarda", sin estado).
    QString heading = QStringLiteral("TOMA");
    bool showState = true;
    QString note; // sin estado, lo que va a la derecha del rótulo (HU-121: "modo mixto")
    // HU-109: en lugar del estado del guardado, "exportando 3/12" o, al terminar, "exportado"
    // (vacío: el del guardado); exportBusy pone el punto en acento.
    QString exportState;
    bool exportBusy = false;
    int number = 1;     // la hoja del centro, desde 1; 0: la hoja fija (HU-89), "F"
    SheetRole role = SheetRole::None;
    int total = 1;
    std::vector<PanelSlot> strip;
    QString lead;               // "HB · 0,87 mm", o el nombre del lápiz de color
    QColor leadColor;           // el del lápiz de color (HU-99); inválido: el grafito
    std::vector<QColor> light;  // los colores de la mesa de luz (vacío: apagada)
    bool keysOnly = false;      // HU-93: la mesa de luz muestra solo las keys
    int degrees = 0;            // giro de la vista
    int zoom = 100;             // HU-101, en %
    bool hasFixed = false;      // HU-89: hay hoja fija
    bool onFixed = false;       // HU-89: se está dibujando en ella
    bool circle = false;        // HU-94: la pila en círculo
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
