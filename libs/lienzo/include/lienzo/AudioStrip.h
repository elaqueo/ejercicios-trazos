#pragma once

#include <QImage>
#include <QRect>
#include <QString>

#include <vector>

namespace lienzo {

// La franja del audio (HU-106, Mesa de animación; mesa "Audio" del canvas): el nombre del
// archivo, la posición y la duración, la onda y el cursor. Como el panel de hojas, la pinta Qt
// cuando cambia algo y el render la pone encima (Renderer::setStrip): no toca el trazo.
struct AudioStripInfo {
    QString name;              // vacío: sin audio
    QString problem;           // por qué no se pudo leer (vacío: se leyó)
    double seconds = 0;        // duración
    double position = 0;       // el cursor, en segundos
    std::vector<float> peaks;  // la onda, una columna por píxel del área de la onda
    bool operator==(const AudioStripInfo&) const = default;
};

// Dónde va: en el hueco entre la hoja y `rightLimit` (el borde izquierdo del panel de hojas, o
// el del cliente), abajo, si entra; si no (una pantalla 16:9), sobre el pie de la hoja.
// scale: la de Windows.
QRect audioStripRect(int clientWidth, int clientHeight, const QRect& sheet, int rightLimit, double scale);

// El área de la onda dentro de la franja (desde su esquina): su ancho es cuántas columnas pedir.
QRect audioWaveRect(const QSize& strip, double scale);

// "0:03,2"
QString audioTime(double seconds);

QImage paintAudioStrip(const AudioStripInfo& info, const QSize& size, double scale, const QColor& outside);

} // namespace lienzo
