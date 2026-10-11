#pragma once

#include "lienzo/LightTable.h"
#include "lienzo/Media.h"
#include "lienzo/PenMenu.h"
#include "lienzo/SheetMapping.h"
#include "lienzo/SheetRole.h"
#include "lienzo/Tone.h"

#include <appkit/Shortcuts.h>

#include <windows.h>

#include <QRect>
#include <QString>
#include <QWidget>

#include <functional>
#include <memory>
#include <vector>

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
    std::function<void(UINT)> onKeyUp; // al soltar (HU-90); 0 = perdió el foco

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    bool focusNextPrevChild(bool) override { return false; } // Tab es un atajo (HU-114), no pasa el foco
};

// El monitor guardado ("monitor", sección común de config.json) si está conectado; si no,
// el principal.
QScreen* savedScreen(const appkit::Config& config);

struct LienzoOptions {
    QString name = QStringLiteral("lienzo"); // para los archivos de diagnóstico (<name>-muestras.csv)
    bool undo = true;                        // Z / Ctrl+Y (Ejercicios no deshace)
    bool sheets = false; // pila de hojas (Mesa de animación): Ctrl+N limpia la hoja, con deshacer; F3 dice qué hoja es
    // Perfil de medio (HU-112): Animation es sin bruñido, punta seca (E), daño de fibra ni
    // desgaste (A afila); esas teclas no están.
    MediumProfile profile = MediumProfile::Illustration;
    // El papel (HU-97): su color (BGRA, como los de Tone.h), la profundidad del grano en la
    // simulación (PaperSpec::grain) y si tiene textura visual (Ctrl+[ ], común a la familia).
    // Un solo grafito (HU-98, Mesa de animación): la dureza fija (índice de kGradeNames), de
    // medios.json pero sin tocarlo: sin F5, sin las teclas de calibración ni Ctrl+S, y no se
    // guarda en config. -1: se elige con F5 (Cartuchera, Ejercicios).
    int fixedGrade = -1;
    bool tiltByDefault = true; // el costado (I) la primera vez; después, lo guardado
    uint32_t paperColor = kPaperColor;
    int paperGrain = 2047;
    bool paperTexture = true;
    // Lápices de color (HU-99, Mesa de animación): 1 es el grafito y 2 a 5 los colores (azul,
    // rojo, verde, violeta); los números dejan de girar la vista (gira la rueda y Shift +
    // arrastrar). Queda guardado en config.json ("pencil").
    bool colorPencils = false;
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
    // Menú del botón del lápiz (HU-116, lienzo/PenMenu.h): con él, el botón lateral abre un anillo
    // en la punta con los lápices (si hay lápices de color) y estas acciones, desde arriba en el
    // sentido del reloj. Cada opción corre su atajo del registro (por nombre) y queda en el log.
    // Reemplaza a setOnStylusButton. Llamar después de registrar los atajos de la app.
    void setPenMenu(std::vector<PenMenuAction> actions);
    // La hoja cambió de lugar o de tamaño en pantalla (después de calibrar con F9). Lo
    // dibujado se conserva; las guías ya pasadas se vuelven a ubicar solas.
    void setOnSheetChanged(std::function<void()> callback);
    // Muestra una ventana propia de la app (menú, HU-11) centrada en la hoja, que es lo que
    // alcanza el lápiz, con el foco del teclado. focusCanvas() devuelve el foco al lienzo
    // (al cerrarla).
    void showOverlay(QWidget* overlay);
    void focusCanvas();

    // Pila de hojas (HU-82, Mesa de animación): Cartuchera y Ejercicios tienen una sola. El
    // trazo cae en la activa; agregar no la cambia, quitar la activa activa la que queda en
    // su lugar, mover conserva cuál es. Se atiende en la simulación, entre trazos.
    int sheetCount() const;
    int activeSheet() const;
    // Rol de la hoja (HU-104, mesa "Rol y número"): K pasa la activa de sin rol a key, a
    // breakdown y otra vez a sin rol (sin deshacer). El número de la hoja y su marca se ven en la
    // esquina superior derecha y en el panel; se guarda en toma.json.
    SheetRole cycleActiveRole();
    SheetRole activeRole() const;
    // La activa no tiene ningún trazo (para no pedir confirmación al borrarla). false si
    // todavía no se sabe (hay pedidos en camino).
    bool activeSheetBlank() const;
    void addSheet(int at); // hoja en blanco en `at` (0..sheetCount())
    // HU-87, con deshacer (Z): hoja en blanco justo después de la activa, o una copia de la
    // activa, que pasa a ser la activa.
    void insertSheet();
    void duplicateSheet();
    void removeSheet(int index);
    void moveSheet(int from, int to); // HU-88: con deshacer
    // HU-88: la activa un lugar hacia el principio (step = -1) o el final (+1). false en los
    // extremos (no hace nada).
    bool moveActiveSheet(int step);
    void activateSheet(int index);

    // Mesa de luz (HU-91): las hojas que se ven debajo de la activa, contadas desde ella (ver
    // LightTable.h); se recalcula sola cada vez que cambia la pila. Vacía = apagada. Cada hoja
    // se ve como quedó la última vez que estuvo activa.
    void setLightTable(const LightTable& table);
    // La hoja que se muestra en lugar de la activa (HU-83; -1 = la activa), por posición.
    void showSheet(int index);
    // Flip (HU-90): la hoja anterior (step = -1) o la siguiente (+1) se muestra sin activarla
    // (desde su textura en la GPU: no se repinta nada) y, al soltar la tecla del atajo, pasa a
    // ser la activa. Mientras tanto el lápiz no dibuja y la mesa de luz no se ve. En los
    // extremos de la pila se queda. Va en el atajo de la tecla (la app elige cuál).
    void flip(int step);
    bool flipping() const;
    // La toma (HU-85; formato en lienzo/Take.h): guarda la pila en una carpeta (toma.json y un
    // archivo por hoja) o la abre en lugar de la que hay, con la vista. Detienen la
    // simulación mientras tanto (useTake, más abajo, guarda solo lo cambiado). Las hojas
    // dañadas o faltantes quedan en blanco, con aviso en el log y en F3. false si no se pudo
    // guardar, o si la carpeta no tiene una toma que abrir.
    bool saveTake(const QString& dir);
    bool openTake(const QString& dir);
    // Guardado automático (HU-86): abre la toma de la carpeta (o, si no hay, empieza una nueva
    // ahí) y desde ese momento la guarda sola, lo que cambió, en segundo plano: la hoja después
    // de dejar de dibujar un momento, y toma.json al cambiar la pila o la vista. stop() espera
    // a que esté todo escrito. Si la carpeta tiene una toma que no se pudo abrir, no se pisa:
    // se usa la de al lado (<carpeta>-2, -3...) que abra, o una nueva (ver takeFolder en
    // lienzo/Take.h). Devuelve la carpeta que usa. Llamar antes de start().
    QString useTake(const QString& dir);
    // Panel de hojas (HU-114, lienzo/SheetPanel.h): angosto, a la derecha de la pantalla, con
    // la toma, la hoja activa y sus vecinas en miniatura, el lápiz, la mesa de luz y el giro.
    // Solo con la pila de hojas (options.sheets); la app elige la tecla y lo guarda.
    void setSheetPanel(bool visible);
    bool sheetPanel() const;
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
