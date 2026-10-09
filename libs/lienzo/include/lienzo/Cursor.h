#pragma once

#include <windows.h>

#include <array>
#include <cstdint>

namespace lienzo {

// Puntero propio del lienzo (HU-38): la cruz abierta del diseño (HU-37, opción A), cuatro
// trazos finos de tinta y un punto de 1 px en el centro (pedido del usuario), con el resto
// del centro libre para ver el contacto; todo con un borde claro de 1 px para que se vea
// también sobre grafito oscuro. Es un cursor de Windows: lo
// mueve el sistema (por hardware), sin latencia del render, y sigue la punta con la vista
// rotada.
constexpr int kCursorSize = 32;
constexpr int kCursorHotspot = 15; // el centro de la cruz

// Píxeles del cursor en BGRA premultiplicado, fila por fila (0 = transparente).
std::array<uint32_t, kCursorSize * kCursorSize> crossCursorPixels();

// Crea el cursor (el llamador lo destruye con DestroyCursor).
HCURSOR createCrossCursor();

} // namespace lienzo
