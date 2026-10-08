#pragma once

// Tokens del sistema visual "Taller nocturno" (HU-37). Es la fuente única para el
// código; el diseño completo está en docs/diseno.md. Cada pantalla los adopta en su
// propia historia (hoja: HU-41, guías: HU-19, menú y panel: HU-11/12).

#include <QColor>

namespace appkit::theme {

// Interfaz (overlays sobre el lienzo).
inline const QColor kFondo{0x17, 0x19, 0x1C};      // fondo de pantallas a página completa
inline const QColor kPanel{0x22, 0x26, 0x2B};      // paneles y overlays
inline const QColor kPanelAlto{0x2C, 0x31, 0x38};  // controles, teclas, ítems elevados
inline const QColor kBorde{0x3A, 0x40, 0x48};      // separa paneles (sin sombras)
inline const QColor kTexto{0xEC, 0xEE, 0xF0};
inline const QColor kTextoSecundario{0xA7, 0xAE, 0xB6};
inline const QColor kSeleccion{0x3A, 0x2A, 0x12};      // fondo del ítem elegido
inline const QColor kSeleccionTexto{0xF0, 0xA5, 0x41}; // texto del ítem elegido

// Lienzo y guías.
inline const QColor kFuera{0x30, 0x33, 0x38};      // fuera del área útil y esquinas al rotar
inline const QColor kHoja{0xF5, 0xF0, 0xE6};       // off-white cálido (HU-41)
inline const QColor kTinta{0x11, 0x11, 0x11};
inline const QColor kGuia{0x25, 0x63, 0xEB};       // lo que se une o se sigue
inline const QColor kGuiaSuave{0x9D, 0xB8, 0xF2};  // construcción (horizonte, líneas a PF)
inline const QColor kEnfasis{0xE0, 0x8A, 0x1E};    // dirección, PF, objetivos

// Medidas (px).
inline constexpr int kEspacio[] = {4, 8, 12, 16, 24, 32};
inline constexpr int kRadioPanel = 12;
inline constexpr int kRadioControl = 8;
inline constexpr int kRadioTecla = 6;
inline constexpr int kObjetivoTactil = 44;
inline constexpr double kGrosorGuia = 2.0;
inline constexpr double kGrosorConstruccion = 1.5; // punteada 4/5
inline constexpr int kRadioPuntoUnir = 12;         // anillo; centro de 4,5
inline constexpr int kRadioPuntoPaso = 7;          // anillo hueco

// Tipografía: IBM Plex Sans (interfaz) e IBM Plex Mono (teclas y valores), licencia
// OFL. Hasta empaquetarlas, Qt usa la fuente del sistema como respaldo.
inline constexpr const char* kFuente = "IBM Plex Sans";
inline constexpr const char* kFuenteMono = "IBM Plex Mono";

} // namespace appkit::theme
