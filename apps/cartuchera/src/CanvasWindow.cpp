#include "CanvasWindow.h"

#include "SampleQueue.h"

namespace cartuchera {

namespace {

const wchar_t* kClassName = L"CartucheraCanvas";

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
        for (tabletinput::PenSample& s : m_samples) { // pantalla → cliente
            s.x -= m_origin.x;
            s.y -= m_origin.y;
        }
        m_queue.push(m_samples);
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

} // namespace cartuchera
