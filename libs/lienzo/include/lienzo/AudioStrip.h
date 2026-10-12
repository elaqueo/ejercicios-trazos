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
    bool loading = false;      // HU-107: se está leyendo (en un hilo aparte)
    std::vector<double> marks; // HU-108: las marcas, en segundos, en orden
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

// Marcas (HU-108): segundos, siempre en orden (el número de cada una es su lugar + 1).
// addMark la inserta y devuelve su lugar; moveMark la cambia de tiempo y devuelve su lugar
// nuevo; nearestMark es la más cercana a `seconds` (-1 si no hay).
int addMark(std::vector<double>& marks, double seconds);
int moveMark(std::vector<double>& marks, int index, double seconds);
int nearestMark(const std::vector<double>& marks, double seconds);
// La marca a menos de `tolerance` px de `x` en el área de la onda `wave` (-1 si ninguna).
int markAt(const std::vector<double>& marks, double x, const QRect& wave, double duration, double tolerance);
// El tiempo de la columna `x` del área de la onda (dentro de la duración).
double timeAt(double x, const QRect& wave, double duration);

QImage paintAudioStrip(const AudioStripInfo& info, const QSize& size, double scale, const QColor& outside);

} // namespace lienzo
