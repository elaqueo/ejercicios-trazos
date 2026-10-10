#pragma once

#include "lienzo/SheetMapping.h"

#include <appkit/Shortcuts.h>

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
class SidePanel;
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

// El lienzo de baja latencia (HU-63, nacido en Cartuchera): una hoja de grafito del tamaño de
// la superficie activa de la tableta (HU-69) a escala
// real centrada sobre la tableta (hoja = tableta, HU-52), en una ventana nativa con
// swapchain, con la simulación y el render en hilos propios; minas 2H … 6B y goma de
// medios.json.
//
// Teclas comunes: Ctrl+N hoja nueva · Z deshace / Ctrl+Y rehace (si undo) · F5 selector de
// lápices (elige la dureza) · 4 y 6 rotan la vista y 5 la vuelve a 0° (las dos apps igual,
// pedido del usuario el 10 de octubre de 2026; antes en Cartuchera 1 a 0 eran las durezas) ·
// la rueda de la tableta gira la vista libremente (HU-74) ·
// Shift + arrastrar con el lápiz rota la vista libremente (HU-79) · I activa o desactiva el
// costado (apagado, la punta es siempre la vertical) · E cambia entre la mina y la punta
// seca (hunde el papel sin grafito: líneas blancas) · A afila la mina activa (cada dureza
// se gasta por su cuenta) · F3
// latencia y herramienta · calibración de la mina activa ([ ] diámetro, , . blandura,
// - = techo) o de la goma con el lápiz dado vuelta ([ ] diámetro, , . fuerza) · Ctrl+S
// guarda en medios.json · F9 calibra el área útil · F10 pasa al monitor siguiente (guarda la
// elección y reinicia la app ahí: el lienzo nativo no cambia de tamaño en caliente) · F12
// guarda la imagen de pantalla · Ctrl+, la lista de atajos (HU-78). Opciones de línea de comandos:
// --grabar (muestras crudas en CSV) y --bench [undo] [costado] (30 s de trazos sintéticos).
class Lienzo {
public:
    // Crea la ventana nativa dentro de `shell` (ya ubicado en `screen` y visible).
    // config tiene que vivir más que el lienzo (F9 guarda ahí el área útil y F10 el monitor).
    Lienzo(Shell& shell, QScreen* screen, appkit::Config& config, LienzoOptions options = {});
    ~Lienzo();
    Lienzo(const Lienzo&) = delete;
    Lienzo& operator=(const Lienzo&) = delete;

    // Registro único de atajos (HU-14): ya tiene los del lienzo; la app agrega los suyos
    // antes de start(), que muestra los choques si los hay. Ctrl+, muestra la lista (HU-78),
    // por grupos: los del lienzo son estos; la app suma los suyos con setGroup y los ubica con
    // setColumns.
    appkit::Shortcuts& shortcuts();
    static inline const QString kGroupLead = QStringLiteral("Lápiz");
    static inline const QString kGroupSheet = QStringLiteral("Hoja y papel");
    static inline const QString kGroupView = QStringLiteral("Vista");
    static inline const QString kGroupScreen = QStringLiteral("Pantalla");
    // Botón lateral del lápiz (en el hilo de la interfaz).
    void setOnStylusButton(std::function<void()> callback);
    // La hoja cambió de lugar o de tamaño en pantalla (después de calibrar con F9). Lo
    // dibujado se conserva; las guías ya pasadas se vuelven a ubicar solas.
    void setOnSheetChanged(std::function<void()> callback);
    // Muestra una ventana propia de la app (menú, HU-11) centrada en la hoja, que es lo que
    // alcanza el lápiz, con el foco del teclado. focusCanvas() devuelve el foco al lienzo
    // (al cerrarla).
    void showOverlay(QWidget* overlay);
    void focusCanvas();
    // Panel de configuración (HU-12): agrega las pestañas del lienzo ("Lápiz": mina activa y
    // goma; "Pantalla": área útil y monitor) y lo muestra pegado al borde derecho del área
    // útil.
    void addPanelTabs(appkit::SidePanel& panel);
    void showSidePanel(appkit::SidePanel& panel);

    void clear(); // hoja nueva (y, si hay deshacer, sin historial); las guías quedan

    // Guías de los ejercicios (HU-64), en coordenadas de la hoja (sheetRect(): el origen es
    // su esquina). Van bajo el grafito. Un QPicture vacío las saca.
    void setGuides(const QPicture& guides);

    // Arranca los hilos (y --grabar / --bench). stop() los detiene y escribe en el log el
    // resumen de latencia y tiempos.
    void start();
    void stop();

    QRect sheetRect() const; // la hoja en píxeles físicos del cliente (los de Qt pueden ser más grandes: escala de Windows)
    const SheetMapping& mapping() const;
    bool calibrated() const; // hay área útil calibrada en este monitor

private:
    void registerShortcuts();

    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace lienzo
