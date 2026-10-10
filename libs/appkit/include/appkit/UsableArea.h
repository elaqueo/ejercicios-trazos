#pragma once

#include "appkit/ScreenChoice.h"

#include <QPointF>
#include <QRect>

#include <optional>

namespace appkit {

class Config;

// Área útil: el rectángulo de pantalla mapeado a la tableta. Se guarda por monitor
// ("usableAreas" en la sección común), en coordenadas relativas a la esquina del
// monitor, porque cada monitor puede tener su propio mapeo. Va en píxeles físicos (los
// del lienzo nativo y la tableta), no en los de Qt: con la escala de Windows en 125 % los
// de Qt son más grandes y la hoja quedaba chica (reportado en la laptop, 9 de octubre de
// 2026). Así tampoco cambia si después se cambia la escala.

// Rectángulo que tiene a a y b como esquinas opuestas, en cualquier orden.
QRect rectFromCorners(QPointF a, QPointF b);

// El rectángulo con sus bordes multiplicados por factor (de píxeles de Qt a físicos con
// devicePixelRatio, o al revés con 1 / devicePixelRatio), redondeando cada borde.
QRect scaledRect(const QRect& rect, double factor);

// Rectángulo de un panel lateral (selector de pinceles, panel de configuración)
// pegado al borde derecho DEL ÁREA ÚTIL, no de la ventana: fuera del área el lápiz no
// llega. Ancho preferido width, separado margin del borde; si el área es angosta, se
// achica para entrar.
QRect sidePanelRect(const QRect& area, int width, int margin);

std::optional<QRect> loadUsableArea(const Config& config, const ScreenId& monitor);
void saveUsableArea(Config& config, const ScreenId& monitor, const QRect& rect);

} // namespace appkit
