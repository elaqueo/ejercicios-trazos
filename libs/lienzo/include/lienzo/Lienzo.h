#pragma once

#include "lienzo/SheetMapping.h"

#include <windows.h>

#include <QRect>
#include <QString>
#include <QWidget>

#include <functional>
#include <memory>

class QKeyEvent;
class QPicture;
class QScreen;

namespace appkit {
class Config;
}

namespace lienzo {

// Cascarón Qt de las apps del lienzo: sin bordes, cubre el monitor. Las teclas que llegan
// acá (cuando Qt tiene el foco) van al mismo manejador que las del lienzo nativo.
class Shell : public QWidget {
public:
    std::function<void(UINT, bool)> onKey;

protected:
    void keyPressEvent(QKeyEvent* event) override;
};

// El monitor guardado ("monitor", sección común de config.json) si está conectado; si no,
// el principal.
QScreen* savedScreen(const appkit::Config& config);

struct LienzoOptions {
    QString name = QStringLiteral("lienzo"); // para los archivos de diagnóstico (<name>-muestras.csv)
    bool undo = true;                        // Z / Ctrl+Y (Ejercicios no deshace)
};

// El lienzo de baja latencia (HU-63, nacido en Cartuchera): una hoja de grafito A4 a escala
// real centrada sobre la tableta (hoja = tableta, HU-52), en una ventana nativa con
// swapchain, con la simulación y el render en hilos propios; minas 2H … 6B y goma de
// medios.json.
//
// Teclas comunes: Ctrl+N hoja nueva · Z deshace / Ctrl+Y rehace (si undo) · 1 a 0 dureza · F3
// latencia y herramienta · calibración de la mina activa ([ ] blandura, , . diámetro,
// - = techo) o de la goma con el lápiz dado vuelta ([ ] fuerza, , . diámetro) · Ctrl+S
// guarda en medios.json · F12 guarda la imagen de pantalla. Opciones de línea de comandos:
// --grabar (muestras crudas en CSV) y --bench [undo] (30 s de trazos sintéticos).
class Lienzo {
public:
    // Crea la ventana nativa dentro de `shell` (ya ubicado en `screen` y visible).
    Lienzo(Shell& shell, QScreen* screen, const appkit::Config& config, LienzoOptions options = {});
    ~Lienzo();
    Lienzo(const Lienzo&) = delete;
    Lienzo& operator=(const Lienzo&) = delete;

    // Las teclas que el lienzo no usa van a la app.
    void setAppKeys(std::function<void(UINT, bool)> keys);
    // Devuelve true si la tecla es del lienzo.
    bool handleKey(UINT vk, bool ctrl);

    void clear(); // hoja nueva (y, si hay deshacer, sin historial); las guías quedan

    // Guías de los ejercicios (HU-64), en coordenadas de la hoja (sheetRect(): el origen es
    // su esquina). Van bajo el grafito. Un QPicture vacío las saca.
    void setGuides(const QPicture& guides);

    // Arranca los hilos (y --grabar / --bench). stop() los detiene y escribe en el log el
    // resumen de latencia y tiempos.
    void start();
    void stop();

    QRect sheetRect() const; // la hoja en píxeles del cliente
    const SheetMapping& mapping() const;
    bool calibrated() const; // hay área útil calibrada en este monitor

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace lienzo
