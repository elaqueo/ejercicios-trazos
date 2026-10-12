#pragma once

#include <QImage>
#include <QRectF>

namespace lienzo {

// Guía de campo (HU-103, Mesa de animación; mesa "Guía de campo" del canvas): 12 campos en
// 16:9 a tamaño real (el campo 12 mide 12 pulgadas de ancho), centrada en la hoja, con los
// rectángulos de 1 a 12 campos, la cruz, las diagonales y el número de los campos pares. No es
// grafito: el render la tiñe de gris tinta debajo del grafito y gira con la hoja; no queda en
// las imágenes de las hojas.
constexpr int kFieldCount = 12;
constexpr double kFieldInchMm = 25.4;

// El rectángulo del campo `field` (1..12) en una hoja de width × height px, con pxPerMm.
QRectF fieldRect(int field, int width, int height, double pxPerMm);

// La guía pintada en una imagen del tamaño de la hoja, transparente, en blanco (el render usa
// solo la cobertura). scale: la de Windows, para el grosor de las líneas.
QImage paintFieldGuide(int width, int height, double pxPerMm, double scale);

} // namespace lienzo
