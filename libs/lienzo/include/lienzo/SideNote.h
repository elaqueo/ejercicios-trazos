#pragma once

#include <QColor>
#include <QImage>
#include <QRect>
#include <QString>

#include <utility>
#include <vector>

namespace lienzo {

// El aviso del costado (HU-133, Cartuchera en modo cuaderno; mesa "Cartuchera en modo
// cuaderno" del canvas): dónde se está dibujando y las teclas que importan ahí. Como la franja
// del audio, lo pinta Qt cuando cambia y el render lo pone encima (Renderer::setNote).
struct SideNote {
    QString label;   // "CUADERNO"
    QString title;   // "Bocetos diarios"
    QColor swatch;   // el color de la tapa (inválido: sin muestra)
    QString detail;  // "hoja 13 de 40", en mono y en el color de lo elegido
    std::vector<std::pair<QString, QString>> keys; // la tecla y qué hace; la primera, resaltada
    QString footer;  // "papel · kraft"
    QString compact; // la línea que queda si no hay costado libre
    bool operator==(const SideNote&) const = default;
};

// Dónde va: en el costado libre a la derecha de la hoja, arriba, si entra; si no (la hoja ocupa
// todo el ancho), una línea en la esquina superior izquierda de la hoja (*compact = true).
// scale: la de Windows.
QRect sideNoteRect(const SideNote& note, int clientWidth, int clientHeight, const QRect& sheet, double scale,
                   bool* compact);

// La imagen del aviso, del tamaño de sideNoteRect; `outside` es el fondo de fuera de la hoja.
QImage paintSideNote(const SideNote& note, const QSize& size, double scale, bool compact, const QColor& outside);

} // namespace lienzo
