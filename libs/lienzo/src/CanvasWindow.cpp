#include "lienzo/CanvasWindow.h"

#include "lienzo/SampleQueue.h"

namespace lienzo {

namespace {

const wchar_t* kClassName = L"LienzoCanvas";

} // namespace

CanvasWindow::CanvasWindow(HWND parent, SampleQueue& queue, std::function<void(UINT, bool)> onKey)
    : m_queue(queue)
    , m_onKey(std::move(onKey))
{
    WNDCLASSW wc{};
    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
    wc.lpszClassName = kClassName;
    RegisterClassW(&wc); // si ya estaba registrada, falla sin consecuencias

    RECT client;
    GetClientRect(parent, &client);
    m_width = client.right - client.left;
    m_height = client.bottom - client.top;
    m_hwnd = CreateWindowExW(0, kClassName, L"", WS_CHILD | WS_VISIBLE, 0, 0, m_width, m_height, parent, nullptr,
                             wc.hInstance, this);
    ClientToScreen(m_hwnd, &m_origin);
    SetFocus(m_hwnd);
}

CanvasWindow::~CanvasWindow()
{
    if (m_hwnd)
        DestroyWindow(m_hwnd);
}

LRESULT CALLBACK CanvasWindow::proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE) {
        auto* self = static_cast<CanvasWindow*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    }
    auto* self = reinterpret_cast<CanvasWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->handle(msg, wParam, lParam) : DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CanvasWindow::handle(UINT msg, WPARAM wParam, LPARAM lParam)
{
    m_samples.clear();
    if (m_reader.handleMessage(m_hwnd, msg, wParam, lParam, m_samples)) {
        if (msg == WM_POINTERDOWN)
            SetFocus(m_hwnd);
        // Con el lápiz el cursor lo pone el procesamiento por defecto de WM_POINTER, que
        // acá no corre (se consume para que Windows no sintetice mouse): sin esto queda el
        // cursor de "programa iniciando" del arranque.
        SetCursor(LoadCursorW(nullptr, IDC_CROSS));
        std::vector<tabletinput::PenSample> toDraw;
        toDraw.reserve(m_samples.size());
        for (tabletinput::PenSample& s : m_samples) { // pantalla → cliente
            s.x -= m_origin.x;
            s.y -= m_origin.y;
            if (s.barrel && !m_barrel && m_onStylusButton)
                m_onStylusButton();
            m_barrel = s.barrel;
            // Gesto de rotación: apoyar con Shift empieza; mientras siga apoyado, gira.
            const bool touching = s.inContact && !s.eraser;
            if (touching && !m_contact && m_onRotateGesture && (GetKeyState(VK_SHIFT) & 0x8000)) {
                m_rotating = true;
                m_onRotateGesture(0, s.x, s.y);
            } else if (m_rotating && touching) {
                m_onRotateGesture(1, s.x, s.y);
            } else if (m_rotating) {
                m_rotating = false;
                m_onRotateGesture(2, s.x, s.y);
            }
            m_contact = touching;
            if (!m_rotating)
                toDraw.push_back(s);
        }
        m_queue.push(toDraw);
        return 0; // sin mouse sintetizado
    }
    switch (msg) {
    case WM_KEYDOWN:
        if (m_onKey)
            m_onKey(UINT(wParam), (GetKeyState(VK_CONTROL) & 0x8000) != 0);
        return 0;
    case WM_SYSKEYDOWN:
        if (wParam == VK_F4) { // Alt+F4 desde el lienzo: cerrar la ventana de arriba
            PostMessageW(GetAncestor(m_hwnd, GA_ROOT), WM_CLOSE, 0, 0);
            return 0;
        }
        if (wParam == VK_F10) { // F10 es tecla de sistema (la del menú): llega por acá, no por WM_KEYDOWN
            if (m_onKey)
                m_onKey(VK_F10, (GetKeyState(VK_CONTROL) & 0x8000) != 0);
            return 0;
        }
        break;
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            SetCursor(LoadCursorW(nullptr, IDC_CROSS)); // el puntero propio llega más adelante
            return TRUE;
        }
        break;
    case WM_ERASEBKGND:
        return 1; // lo pinta D3D
    default:
        break;
    }
    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

} // namespace lienzo
