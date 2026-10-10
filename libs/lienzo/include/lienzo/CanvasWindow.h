#pragma once

#include <tabletinput/PenReader.h>
#include <tabletinput/TouchRing.h>

#include <windows.h>

#include <functional>
#include <vector>

namespace lienzo {

class SampleQueue;

// El lienzo: un HWND hijo Win32 puro dentro del cascarón Qt (spike HU-44). Recibe el
// lápiz por WM_POINTER (tabletinput), pasa las muestras a coordenadas del cliente y las
// encola para la simulación; D3D11 presenta en este HWND.
class CanvasWindow {
public:
    // onKey(vk, ctrl): teclas que llegan al lienzo (cuando tiene el foco).
    CanvasWindow(HWND parent, SampleQueue& queue, std::function<void(UINT, bool)> onKey);
    ~CanvasWindow();
    CanvasWindow(const CanvasWindow&) = delete;
    CanvasWindow& operator=(const CanvasWindow&) = delete;

    HWND hwnd() const { return m_hwnd; }
    // Gesto de rotación (HU-40): apoyar el lápiz con Shift apretado y arrastrar. Mientras
    // dura, las muestras no van a la simulación (no dibuja). phase: 0 empieza, 1 sigue,
    // 2 termina; (x, y) en píxeles del cliente.
    void setOnRotateGesture(std::function<void(int phase, double x, double y)> callback)
    {
        m_onRotateGesture = std::move(callback);
    }
    // Rueda táctil de la tableta (HU-74): grados que giró el dedo (positivos = sentido de las
    // agujas del reloj) y ended al levantarlo. En el hilo de la interfaz.
    void setOnRing(std::function<void(double degrees, bool ended)> callback) { m_onRing = std::move(callback); }
    // Al apretar el botón lateral del lápiz (flanco de subida; en el hilo de la interfaz).
    void setOnStylusButton(std::function<void()> callback) { m_onStylusButton = std::move(callback); }
    int width() const { return m_width; }
    int height() const { return m_height; }

private:
    static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handle(UINT msg, WPARAM wParam, LPARAM lParam);

    HWND m_hwnd = nullptr;
    int m_width = 0, m_height = 0;
    POINT m_origin{}; // esquina del cliente en coordenadas de pantalla
    SampleQueue& m_queue;
    std::function<void(UINT, bool)> m_onKey;
    std::function<void()> m_onStylusButton;
    std::function<void(int, double, double)> m_onRotateGesture;
    bool m_barrel = false;
    bool m_contact = false;  // el lápiz estaba apoyado en la muestra anterior
    bool m_rotating = false; // gesto de rotación en curso
    HCURSOR m_cursor = nullptr; // la cruz abierta (HU-38)
    tabletinput::PenReader m_reader;
    tabletinput::TouchRing m_ring; // rueda táctil (HU-74)
    std::vector<tabletinput::RingEvent> m_ringEvents;
    tabletinput::RingTracker m_ringTracker;
    std::function<void(double, bool)> m_onRing;
    std::vector<tabletinput::PenSample> m_samples;
};

} // namespace lienzo
