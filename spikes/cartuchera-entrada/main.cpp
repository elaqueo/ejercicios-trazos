// Spike HU-43 (Cartuchera, Fase 0): ¿Windows Ink por WM_POINTER entrega las 200
// muestras/s del lápiz, con presión, inclinación y timestamp de alta resolución?
//
// Código descartable: Win32 puro, sin Qt. Abre una ventana sin bordes a pantalla
// completa, lee cada WM_POINTER* del lápiz con GetPointerPenInfoHistory (todas las
// muestras del mensaje, en orden), las dibuja con GDI y las registra en un CSV en
// %LOCALAPPDATA%\trazos\spikes\. Arriba a la izquierda muestra estadísticas en vivo.
//
// Uso: cartuchera-entrada.exe [índice de monitor]   (0 = principal)
// Teclas: C limpia, Esc sale (y cierra el CSV).

#include <windows.h>

#include <shlobj.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cwchar>
#include <deque>
#include <string>
#include <vector>

namespace {

LARGE_INTEGER g_qpcFrequency{};
HWND g_window = nullptr;
FILE* g_csv = nullptr;
std::wstring g_csvPath;

// Lienzo en memoria: las líneas se dibujan acá y WM_PAINT lo copia.
HDC g_canvasDc = nullptr;
HBITMAP g_canvasBitmap = nullptr;
SIZE g_canvasSize{};

// Estado del trazo y estadísticas.
bool g_hasLast = false;
POINT g_last{};
UINT64 g_lastPerfCount = 0;
DWORD g_lastTime = 0;
UINT64 g_messageCount = 0;
UINT64 g_sampleCount = 0;
std::deque<UINT64> g_recentSamples;                // PerformanceCount de las muestras del último segundo
std::array<UINT64, 8> g_perMessage{};              // muestras por mensaje: 1..7, 8+
UINT64 g_minPerfDelta = UINT64_MAX;                // menor salto de PerformanceCount > 0 entre muestras
UINT64 g_repeatedTime = 0;                         // muestras con el mismo dwTime que la anterior
UINT64 g_strokeSamples = 0;
std::wstring g_deviceInfo;

double qpcToMs(UINT64 ticks)
{
    return double(ticks) * 1000.0 / double(g_qpcFrequency.QuadPart);
}

UINT64 qpcNow()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return UINT64(now.QuadPart);
}

void openCsv()
{
    PWSTR localAppData = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData)))
        return;
    std::wstring dir = std::wstring(localAppData) + L"\\trazos\\spikes";
    CoTaskMemFree(localAppData);
    SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);

    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t name[64];
    swprintf(name, 64, L"\\entrada-%04u%02u%02u-%02u%02u%02u.csv", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
             t.wSecond);
    g_csvPath = dir + name;
    if (_wfopen_s(&g_csv, g_csvPath.c_str(), L"w") != 0)
        g_csv = nullptr;
    if (!g_csv)
        return;

    wchar_t computer[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    GetComputerNameW(computer, &size);
    fwprintf(g_csv, L"# equipo=%ls qpc_hz=%lld\n", computer, g_qpcFrequency.QuadPart);
    fwprintf(g_csv, L"recv_qpc,msg,msg_samples,hist_index,pointer_id,frame_id,flags,in_contact,time_ms,perf_count,"
                    L"px,py,him_x,him_y,raw_x,raw_y,pressure,tilt_x,tilt_y,rotation,pen_flags,pen_mask\n");
}

// Rectángulos de la tableta (himétricos, 0,01 mm) y de la pantalla a la que mapea:
// con eso se pasa de píxeles a milímetros sin calibrar.
void logDevice(HANDLE device)
{
    static HANDLE logged = nullptr;
    if (device == logged)
        return;
    logged = device;
    RECT pointerRect{}, displayRect{};
    if (!GetPointerDeviceRects(device, &pointerRect, &displayRect))
        return;
    wchar_t text[256];
    swprintf(text, 256, L"tableta %ldx%ld (0,01 mm) -> pantalla %ld,%ld %ldx%ld px",
             pointerRect.right - pointerRect.left, pointerRect.bottom - pointerRect.top, displayRect.left,
             displayRect.top, displayRect.right - displayRect.left, displayRect.bottom - displayRect.top);
    g_deviceInfo = text;
    if (g_csv)
        fwprintf(g_csv, L"# dispositivo: tableta=%ld,%ld,%ld,%ld pantalla=%ld,%ld,%ld,%ld\n", pointerRect.left,
                 pointerRect.top, pointerRect.right, pointerRect.bottom, displayRect.left, displayRect.top,
                 displayRect.right, displayRect.bottom);
}

void drawSample(POINT client, UINT32 pressure, bool inContact)
{
    if (!inContact) {
        g_hasLast = false;
        return;
    }
    const int width = 1 + int(pressure * 6 / 1024);
    if (g_hasLast) {
        HPEN pen = CreatePen(PS_SOLID, width, RGB(17, 17, 17));
        HGDIOBJ old = SelectObject(g_canvasDc, pen);
        MoveToEx(g_canvasDc, g_last.x, g_last.y, nullptr);
        LineTo(g_canvasDc, client.x, client.y);
        SelectObject(g_canvasDc, old);
        DeleteObject(pen);
        RECT dirty{std::min(g_last.x, client.x) - width, std::min(g_last.y, client.y) - width,
                   std::max(g_last.x, client.x) + width + 1, std::max(g_last.y, client.y) + width + 1};
        InvalidateRect(g_window, &dirty, FALSE);
    }
    g_last = client;
    g_hasLast = true;
}

void handlePen(UINT32 pointerId, UINT64 receivedQpc)
{
    POINTER_INFO info{};
    if (!GetPointerInfo(pointerId, &info) || info.pointerType != PT_PEN)
        return;
    logDevice(info.sourceDevice);

    UINT32 count = std::max<UINT32>(info.historyCount, 1);
    std::vector<POINTER_PEN_INFO> history(count);
    if (!GetPointerPenInfoHistory(pointerId, &count, history.data()))
        return;
    history.resize(count);

    ++g_messageCount;
    ++g_perMessage[std::min<size_t>(count, 8) - 1];

    // El historial viene de la más nueva a la más vieja: se recorre al revés.
    for (UINT32 i = count; i-- > 0;) {
        const POINTER_PEN_INFO& pen = history[i];
        const POINTER_INFO& p = pen.pointerInfo;
        const bool inContact = (p.pointerFlags & POINTER_FLAG_INCONTACT) != 0;
        POINT client = p.ptPixelLocation;
        ScreenToClient(g_window, &client);

        if (inContact) {
            ++g_sampleCount;
            ++g_strokeSamples;
            if (g_lastPerfCount != 0 && p.PerformanceCount > g_lastPerfCount)
                g_minPerfDelta = std::min<UINT64>(g_minPerfDelta, p.PerformanceCount - g_lastPerfCount);
            if (g_lastPerfCount != 0 && p.dwTime == g_lastTime)
                ++g_repeatedTime;
            g_lastPerfCount = p.PerformanceCount;
            g_lastTime = p.dwTime;
            g_recentSamples.push_back(p.PerformanceCount);
        } else {
            g_lastPerfCount = 0;
        }

        if (g_csv)
            fwprintf(g_csv, L"%llu,%llu,%u,%u,%u,%u,0x%x,%d,%lu,%llu,%ld,%ld,%ld,%ld,%ld,%ld,%u,%d,%d,%u,0x%x,0x%x\n",
                     receivedQpc, g_messageCount, count, i, p.pointerId, p.frameId, p.pointerFlags, inContact ? 1 : 0,
                     p.dwTime, p.PerformanceCount, client.x, client.y, p.ptHimetricLocation.x, p.ptHimetricLocation.y,
                     p.ptHimetricLocationRaw.x,
                     p.ptHimetricLocationRaw.y, pen.pressure, pen.tiltX, pen.tiltY, pen.rotation, pen.penFlags,
                     pen.penMask);

        drawSample(client, pen.pressure, inContact);
    }
}

void drawStats(HDC dc)
{
    // Muestras en el último segundo de PerformanceCount.
    if (!g_recentSamples.empty()) {
        const UINT64 newest = g_recentSamples.back();
        while (!g_recentSamples.empty() && newest - g_recentSamples.front() > UINT64(g_qpcFrequency.QuadPart))
            g_recentSamples.pop_front();
    }
    wchar_t text[1024];
    int len = swprintf(text, 1024,
                       L"Spike HU-43 · WM_POINTER   (C limpia, Esc sale)\n"
                       L"%ls\n"
                       L"muestras en contacto: %llu   mensajes: %llu   último segundo: %zu\n"
                       L"muestras por mensaje: 1=%llu 2=%llu 3=%llu 4=%llu 5=%llu 6=%llu 7=%llu 8+=%llu\n"
                       L"menor salto de PerformanceCount: %.3f ms   dwTime repetido: %llu\n"
                       L"CSV: %ls",
                       g_deviceInfo.c_str(), g_sampleCount, g_messageCount, g_recentSamples.size(), g_perMessage[0],
                       g_perMessage[1], g_perMessage[2], g_perMessage[3], g_perMessage[4], g_perMessage[5],
                       g_perMessage[6], g_perMessage[7],
                       g_minPerfDelta == UINT64_MAX ? 0.0 : qpcToMs(g_minPerfDelta), g_repeatedTime,
                       g_csvPath.c_str());
    RECT box{16, 16, 1200, 160};
    SetBkColor(dc, RGB(34, 38, 43));
    SetTextColor(dc, RGB(236, 238, 240));
    HBRUSH brush = CreateSolidBrush(RGB(34, 38, 43));
    FillRect(dc, &box, brush);
    DeleteObject(brush);
    RECT inner{24, 22, 1192, 156};
    DrawTextW(dc, text, len, &inner, DT_LEFT | DT_TOP | DT_NOPREFIX);
}

void clearCanvas()
{
    RECT r{0, 0, g_canvasSize.cx, g_canvasSize.cy};
    HBRUSH paper = CreateSolidBrush(RGB(245, 240, 230));
    FillRect(g_canvasDc, &r, paper);
    DeleteObject(paper);
    InvalidateRect(g_window, nullptr, FALSE);
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_POINTERDOWN:
    case WM_POINTERUPDATE:
    case WM_POINTERUP: {
        const UINT64 received = qpcNow();
        const UINT32 id = GET_POINTERID_WPARAM(wParam);
        POINTER_INPUT_TYPE type{};
        if (GetPointerType(id, &type) && type == PT_PEN) {
            handlePen(id, received);
            if (msg == WM_POINTERUP) {
                g_hasLast = false;
                g_strokeSamples = 0;
                if (g_csv)
                    fflush(g_csv);
            }
            return 0; // sin esto Windows sintetiza además mensajes de mouse
        }
        break;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
            DestroyWindow(hwnd);
        else if (wParam == 'C')
            clearCanvas();
        return 0;
    case WM_TIMER:
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        BitBlt(dc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left,
               ps.rcPaint.bottom - ps.rcPaint.top, g_canvasDc, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
        drawStats(dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        if (g_csv)
            fclose(g_csv);
        g_csv = nullptr;
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

struct MonitorList {
    std::vector<RECT> rects;
};

BOOL CALLBACK collectMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM data)
{
    MONITORINFO info{sizeof(info)};
    if (GetMonitorInfoW(monitor, &info)) {
        auto* list = reinterpret_cast<MonitorList*>(data);
        // El principal primero.
        if (info.dwFlags & MONITORINFOF_PRIMARY)
            list->rects.insert(list->rects.begin(), info.rcMonitor);
        else
            list->rects.push_back(info.rcMonitor);
    }
    return TRUE;
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    QueryPerformanceFrequency(&g_qpcFrequency);

    MonitorList monitors;
    EnumDisplayMonitors(nullptr, nullptr, collectMonitor, reinterpret_cast<LPARAM>(&monitors));
    const size_t index = std::min<size_t>(size_t(_wtoi(commandLine)), monitors.rects.size() - 1);
    const RECT area = monitors.rects[index];
    g_canvasSize = {area.right - area.left, area.bottom - area.top};

    WNDCLASSW wc{};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
    wc.lpszClassName = L"CartucheraSpikeEntrada";
    RegisterClassW(&wc);

    g_window = CreateWindowExW(0, wc.lpszClassName, L"Spike HU-43 · entrada", WS_POPUP | WS_VISIBLE, area.left,
                               area.top, g_canvasSize.cx, g_canvasSize.cy, nullptr, nullptr, instance, nullptr);
    if (!g_window)
        return 1;

    HDC screen = GetDC(g_window);
    g_canvasDc = CreateCompatibleDC(screen);
    g_canvasBitmap = CreateCompatibleBitmap(screen, g_canvasSize.cx, g_canvasSize.cy);
    SelectObject(g_canvasDc, g_canvasBitmap);
    ReleaseDC(g_window, screen);
    clearCanvas();

    openCsv();
    SetTimer(g_window, 1, 250, nullptr);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    DeleteDC(g_canvasDc);
    DeleteObject(g_canvasBitmap);
    return 0;
}
