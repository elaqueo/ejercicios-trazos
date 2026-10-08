#pragma once

#include "appkit/ScreenChoice.h"

#include <QPointF>
#include <QRect>

#include <optional>

namespace appkit {

class Config;

// Área útil: el rectángulo de pantalla mapeado a la tableta. Se guarda por monitor
// ("usableAreas" en la sección común), en coordenadas relativas a la esquina del
// monitor, porque cada monitor puede tener su propio mapeo.

// Rectángulo que tiene a a y b como esquinas opuestas, en cualquier orden.
QRect rectFromCorners(QPointF a, QPointF b);

std::optional<QRect> loadUsableArea(const Config& config, const ScreenId& monitor);
void saveUsableArea(Config& config, const ScreenId& monitor, const QRect& rect);

} // namespace appkit
