#pragma once

// tabletinput: entrada del lápiz por Windows Ink (WM_POINTER), sin Qt.
//
// Lo que se usa de cada muestra salió del spike HU-43
// (docs/medios-secos/spikes/entrada.md): posición de ptHimetricLocation (con
// decimales; ptPixelLocation y ptHimetricLocationRaw están cuantizados al píxel),
// tiempo de PerformanceCount (exacto; el de Qt salta de a 16 ms) y el historial
// completo de cada mensaje, en orden.

#include <windows.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace tabletinput {

struct PenSample {
    double x = 0, y = 0;            // píxeles de pantalla (escritorio virtual), con decimales
    float pressure = 0;             // 0..1
    float tiltX = 0, tiltY = 0;     // grados, como los entrega Windows
    float azimuth = 0;              // grados [0, 360): 0 = hacia +x, 90 = hacia +y (abajo)
    float altitude = 90;            // grados: 90 = lápiz vertical
    std::optional<float> rotation;  // grados; solo si el lápiz la informa (Art Pen)
    int64_t timeUs = 0;             // µs, reloj de QueryPerformanceCounter
    bool inContact = false;         // false = proximidad (en el aire)
    bool eraser = false;            // extremo goma
    bool barrel = false;            // botón lateral apretado
};

// Rectángulos de GetPointerDeviceRects: la tableta (himétrico) y la pantalla a la que
// el driver dice que mapea. Ojo: puede no ser el mapeo real (HU-43); para milímetros
// hace falta el área útil calibrada.
struct DeviceRects {
    RECT pointer{};
    RECT display{};
};

// --- Normalización (funciones puras, probadas sin tableta) ---------------------------

// Inclinación X/Y (grados) → azimut y altitud (grados).
void tiltToAzimuthAltitude(float tiltX, float tiltY, float& azimuth, float& altitude);

// PerformanceCount → µs, sin desbordar con contadores grandes.
int64_t qpcToMicroseconds(int64_t count, int64_t frequency);

// Una muestra de Windows → PenSample.
PenSample normalize(const POINTER_PEN_INFO& pen, const DeviceRects& rects, int64_t qpcFrequency);

// Historial de GetPointerPenInfoHistory (de la más nueva a la más vieja) → muestras en
// orden cronológico, agregadas al final de out.
void appendHistory(const POINTER_PEN_INFO* history, uint32_t count, const DeviceRects& rects, int64_t qpcFrequency,
                   std::vector<PenSample>& out);

// --- Lectura desde una ventana -------------------------------------------------------

class PenReader {
public:
    PenReader();

    // Para llamar desde el WndProc de la ventana que recibe el lápiz. Si el mensaje es
    // WM_POINTERDOWN/UPDATE/UP de un lápiz, agrega a out todas sus muestras en orden y
    // devuelve true: el WndProc tiene que devolver 0 sin pasar por DefWindowProc, así
    // Windows no sintetiza además mensajes de mouse. Si no, devuelve false.
    bool handleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, std::vector<PenSample>& out);

private:
    int64_t m_qpcFrequency = 0;
    HANDLE m_device = nullptr;
    DeviceRects m_rects;
    std::vector<POINTER_PEN_INFO> m_history;
};

} // namespace tabletinput
