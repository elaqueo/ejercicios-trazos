#pragma once

#include <tabletinput/PenReader.h>

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
    tabletinput::PenReader m_reader;
    std::vector<tabletinput::PenSample> m_samples;
};

} // namespace lienzo
