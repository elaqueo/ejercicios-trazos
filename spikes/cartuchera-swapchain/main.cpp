// Spike HU-44 (Cartuchera, Fase 0): swapchain Direct3D 11 flip model en un HWND hijo de
// una ventana Qt, y latencia desde la muestra del lápiz hasta el vsync en que se ve.
//
// Código descartable. Un QWidget sin bordes cubre el monitor principal; adentro, un HWND
// hijo Win32 puro recibe WM_POINTER (como en el spike de HU-43) y encola las muestras.
// Un hilo de render dibuja los segmentos nuevos sobre una textura persistente, la copia
// al backbuffer y presenta. Cuatro modos de presentación para comparar (teclas 0..3):
//   0 ingenuo:        latencia máxima 3 frames, sin waitable (como una app típica)
//   1 flip+waitable:  latencia 1, renderiza apenas el waitable avisa
//   2 justo a tiempo: como 1, pero duerme hasta N ms antes del vsync (+/- ajustan N)
//   3 tearing:        ALLOW_TEARING, presenta apenas llegan muestras, sin esperar vsync
// Por frame se registra el PerformanceCount de la muestra más nueva y, con
// GetFrameStatistics, el vsync en que se mostró. CSV en %LOCALAPPDATA%\trazos\spikes\.
// Teclas: 0..3 modo, +/- adelanto del modo 2, C limpia, Esc sale.

#include <QApplication>
#include <QKeyEvent>
#include <QScreen>
#include <QWidget>

#include <windows.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dwmapi.h>
#include <dxgi1_5.h>
#include <shlobj.h>
#include <share.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using Microsoft::WRL::ComPtr;

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace {

// --- Entrada (hilo de la GUI) -------------------------------------------------------

struct Sample {
    float x = 0, y = 0;   // píxeles del cliente, con decimales (de ptHimetricLocation)
    float pressure = 0;   // 0..1
    bool inContact = false;
    UINT64 qpc = 0;       // PerformanceCount de la muestra
};

std::mutex g_queueMutex;
std::vector<Sample> g_queue;
HANDLE g_samplesEvent = nullptr; // se señala al encolar (lo usa el modo tearing)

std::atomic<int> g_mode{1};
std::atomic<int> g_leadTenthsMs{40}; // adelanto del modo 2, en décimas de ms
std::atomic<bool> g_clear{false};
std::atomic<bool> g_quit{false};

HWND g_child = nullptr;
POINT g_clientOrigin{}; // origen del cliente en coordenadas de pantalla

struct DeviceRects {
    HANDLE device = nullptr;
    RECT pointer{}, display{};
} g_rects;

LARGE_INTEGER g_qpcFrequency{};

double qpcToMs(INT64 ticks)
{
    return double(ticks) * 1000.0 / double(g_qpcFrequency.QuadPart);
}

INT64 qpcNow()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return now.QuadPart;
}

void readPen(UINT32 pointerId)
{
    POINTER_INFO info{};
    if (!GetPointerInfo(pointerId, &info) || info.pointerType != PT_PEN)
        return;
    if (info.sourceDevice != g_rects.device) {
        g_rects.device = info.sourceDevice;
        GetPointerDeviceRects(info.sourceDevice, &g_rects.pointer, &g_rects.display);
    }
    UINT32 count = std::max<UINT32>(info.historyCount, 1);
    std::vector<POINTER_PEN_INFO> history(count);
    if (!GetPointerPenInfoHistory(pointerId, &count, history.data()))
        return;

    const double pw = double(g_rects.pointer.right - g_rects.pointer.left);
    const double ph = double(g_rects.pointer.bottom - g_rects.pointer.top);
    const double dw = double(g_rects.display.right - g_rects.display.left);
    const double dh = double(g_rects.display.bottom - g_rects.display.top);

    std::lock_guard lock(g_queueMutex);
    for (UINT32 i = count; i-- > 0;) { // del historial más viejo al más nuevo
        const POINTER_PEN_INFO& pen = history[i];
        const POINTER_INFO& p = pen.pointerInfo;
        Sample s;
        // Himétrico → pantalla con decimales (HU-43: ptHimetricLocation no está cuantizado).
        s.x = float(g_rects.display.left + (p.ptHimetricLocation.x - g_rects.pointer.left) * dw / pw - g_clientOrigin.x);
        s.y = float(g_rects.display.top + (p.ptHimetricLocation.y - g_rects.pointer.top) * dh / ph - g_clientOrigin.y);
        s.pressure = float(pen.pressure) / 1024.0f;
        s.inContact = (p.pointerFlags & POINTER_FLAG_INCONTACT) != 0;
        s.qpc = p.PerformanceCount;
        g_queue.push_back(s);
    }
    SetEvent(g_samplesEvent);
}

LRESULT CALLBACK childProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_POINTERDOWN:
    case WM_POINTERUPDATE:
    case WM_POINTERUP: {
        const UINT32 id = GET_POINTERID_WPARAM(wParam);
        POINTER_INPUT_TYPE type{};
        if (GetPointerType(id, &type) && type == PT_PEN) {
            if (msg == WM_POINTERDOWN)
                SetFocus(hwnd);
            readPen(id);
            return 0; // sin mouse sintetizado
        }
        break;
    }
    case WM_KEYDOWN:
        if (wParam >= '0' && wParam <= '3')
            g_mode = int(wParam - '0');
        else if (wParam == VK_OEM_PLUS || wParam == VK_ADD)
            g_leadTenthsMs = std::min(160, g_leadTenthsMs + 5);
        else if (wParam == VK_OEM_MINUS || wParam == VK_SUBTRACT)
            g_leadTenthsMs = std::max(5, g_leadTenthsMs - 5);
        else if (wParam == 'C')
            g_clear = true;
        else if (wParam == VK_ESCAPE)
            QMetaObject::invokeMethod(qApp, &QCoreApplication::quit, Qt::QueuedConnection);
        return 0;
    case WM_ERASEBKGND:
        return 1; // lo pinta D3D
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            SetCursor(LoadCursorW(nullptr, IDC_CROSS));
            return TRUE;
        }
        break;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// --- Render (hilo propio) -----------------------------------------------------------

const char* kShaders = R"(
cbuffer Screen : register(b0) { float2 invHalf; float2 pad; };
float4 vs(float2 pos : POSITION) : SV_Position {
    return float4(pos.x * invHalf.x - 1.0, 1.0 - pos.y * invHalf.y, 0.0, 1.0);
}
float4 ps() : SV_Target { return float4(0.067, 0.067, 0.067, 1.0); }
)";

constexpr int kStatsW = 1100, kStatsH = 150;
constexpr UINT kMaxVertices = 6 * 8192;
const char* kModeNames[] = {"0 ingenuo (latencia 3, sin waitable)", "1 flip + waitable (latencia 1)",
                            "2 justo a tiempo", "3 tearing"};

struct FrameRecord {
    int mode = 0;
    int leadTenths = 0;
    UINT presentCount = 0;
    int samples = 0;
    INT64 newestSample = 0; // PerformanceCount de la muestra más nueva dibujada (0 = ninguna)
    INT64 drain = 0;        // cuándo se tomaron las muestras
    INT64 present = 0;      // cuándo se llamó a Present
};

struct LatencyRow {
    int mode;
    double toVsyncMs;   // muestra → vsync en que se mostró
    double toPresentMs; // muestra → Present
    double ageMs;       // muestra → momento en que se tomó (late latch)
    INT64 when;
};

class Renderer {
public:
    explicit Renderer(HWND hwnd)
        : m_hwnd(hwnd)
    {
    }

    void run();

private:
    bool init();
    void createSwapChain(int mode);
    void drawSegments(const std::vector<Sample>& samples);
    void updateStats(INT64 now);
    void resolveStatistics(double periodMs);
    void openCsv();
    void sleepUntil(INT64 target);

    HWND m_hwnd;
    UINT m_width = 0, m_height = 0;
    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGIFactory2> m_factory;
    ComPtr<IDXGISwapChain2> m_swapChain;
    HANDLE m_waitable = nullptr;
    bool m_tearingSupported = false;
    int m_swapMode = -1;

    ComPtr<ID3D11Texture2D> m_canvas;
    ComPtr<ID3D11RenderTargetView> m_canvasRtv;
    ComPtr<ID3D11Texture2D> m_statsTexture;
    ComPtr<ID3D11Buffer> m_vertices;
    ComPtr<ID3D11Buffer> m_constants;
    ComPtr<ID3D11VertexShader> m_vs;
    ComPtr<ID3D11PixelShader> m_ps;
    ComPtr<ID3D11InputLayout> m_layout;
    ComPtr<ID3D11RasterizerState> m_rasterizer;

    HDC m_statsDc = nullptr;
    HBITMAP m_statsBitmap = nullptr;
    void* m_statsBits = nullptr;
    INT64 m_lastStats = 0;

    HANDLE m_timer = nullptr;
    bool m_hasLast = false;
    Sample m_last{};
    std::deque<FrameRecord> m_pending;
    std::deque<LatencyRow> m_recent;
    FILE* m_csv = nullptr;
    std::wstring m_csvPath;
    bool m_statsAvailable = true;
};

bool Renderer::init()
{
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    m_width = UINT(rc.right - rc.left);
    m_height = UINT(rc.bottom - rc.top);

    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 1,
                                 D3D11_SDK_VERSION, &m_device, nullptr, &m_context)))
        return false;

    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<IDXGIAdapter> adapter;
    m_device.As(&dxgiDevice);
    dxgiDevice->GetAdapter(&adapter);
    adapter->GetParent(IID_PPV_ARGS(&m_factory));
    ComPtr<IDXGIFactory5> factory5;
    if (SUCCEEDED(m_factory.As(&factory5))) {
        BOOL allow = FALSE;
        if (SUCCEEDED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow, sizeof(allow))))
            m_tearingSupported = allow == TRUE;
    }
    m_factory->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_ALT_ENTER);

    // Lienzo persistente: la tinta se acumula acá y cada frame se copia al backbuffer.
    D3D11_TEXTURE2D_DESC td{};
    td.Width = m_width;
    td.Height = m_height;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET;
    m_device->CreateTexture2D(&td, nullptr, &m_canvas);
    m_device->CreateRenderTargetView(m_canvas.Get(), nullptr, &m_canvasRtv);
    const float paper[4] = {0.902f, 0.941f, 0.961f, 1.0f}; // #F5F0E6 en BGRA
    m_context->ClearRenderTargetView(m_canvasRtv.Get(), paper);

    td.Width = kStatsW;
    td.Height = kStatsH;
    td.BindFlags = 0;
    m_device->CreateTexture2D(&td, nullptr, &m_statsTexture);

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = kMaxVertices * sizeof(float) * 2;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    m_device->CreateBuffer(&bd, nullptr, &m_vertices);
    const float constants[4] = {2.0f / float(m_width), 2.0f / float(m_height), 0, 0};
    D3D11_BUFFER_DESC cd{};
    cd.ByteWidth = sizeof(constants);
    cd.Usage = D3D11_USAGE_IMMUTABLE;
    cd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    D3D11_SUBRESOURCE_DATA cdata{constants, 0, 0};
    m_device->CreateBuffer(&cd, &cdata, &m_constants);

    ComPtr<ID3DBlob> vsBlob, psBlob, errors;
    if (FAILED(D3DCompile(kShaders, strlen(kShaders), nullptr, nullptr, nullptr, "vs", "vs_5_0", 0, 0, &vsBlob, &errors)) ||
        FAILED(D3DCompile(kShaders, strlen(kShaders), nullptr, nullptr, nullptr, "ps", "ps_5_0", 0, 0, &psBlob, &errors)))
        return false;
    m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vs);
    m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_ps);
    const D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}};
    m_device->CreateInputLayout(layout, 1, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_layout);
    // Sin descarte de caras: el orden de los vértices de cada segmento depende de su
    // dirección, y con el descarte por defecto (caras traseras) no se veía ninguno.
    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    m_device->CreateRasterizerState(&rd, &m_rasterizer);

    // Texto de estadísticas: GDI sobre un DIB, que se sube a una textura.
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = kStatsW;
    bi.bmiHeader.biHeight = -kStatsH; // de arriba hacia abajo
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    m_statsDc = CreateCompatibleDC(nullptr);
    m_statsBitmap = CreateDIBSection(m_statsDc, &bi, DIB_RGB_COLORS, &m_statsBits, nullptr, 0);
    SelectObject(m_statsDc, m_statsBitmap);

    m_timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!m_timer)
        m_timer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
    return true;
}

// Cada modo usa un swapchain nuevo: el semáforo del waitable arrastraría cuentas del modo
// anterior (el modo 0 no lo espera) y falsearía las primeras mediciones.
void Renderer::createSwapChain(int mode)
{
    m_context->ClearState();
    m_context->Flush();
    if (m_waitable) {
        CloseHandle(m_waitable);
        m_waitable = nullptr;
    }
    m_swapChain.Reset();

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = m_width;
    desc.Height = m_height;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 3;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    if (mode == 3 && m_tearingSupported)
        desc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    ComPtr<IDXGISwapChain1> swapChain1;
    if (FAILED(m_factory->CreateSwapChainForHwnd(m_device.Get(), m_hwnd, &desc, nullptr, nullptr, &swapChain1)))
        return;
    swapChain1.As(&m_swapChain);
    m_swapChain->SetMaximumFrameLatency(mode == 0 ? 3 : 1);
    m_waitable = m_swapChain->GetFrameLatencyWaitableObject();
    m_swapMode = mode;
    m_pending.clear();
    m_hasLast = false;
}

void Renderer::drawSegments(const std::vector<Sample>& samples)
{
    std::vector<float> v;
    v.reserve(samples.size() * 12);
    for (const Sample& s : samples) {
        if (!s.inContact) {
            m_hasLast = false;
            continue;
        }
        if (m_hasLast) {
            // Segmento como rectángulo de ancho según la presión (1,5..4 px).
            const float w = 0.75f + 1.25f * s.pressure;
            float dx = s.x - m_last.x, dy = s.y - m_last.y;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len > 0.001f) {
                const float nx = -dy / len * w, ny = dx / len * w;
                const float ex = dx / len * w * 0.5f, ey = dy / len * w * 0.5f; // extiende un poco: sin huecos en las uniones
                const float ax = m_last.x - ex, ay = m_last.y - ey, bx = s.x + ex, by = s.y + ey;
                const float quad[12] = {ax + nx, ay + ny, bx + nx, by + ny, bx - nx, by - ny,
                                        ax + nx, ay + ny, bx - nx, by - ny, ax - nx, ay - ny};
                v.insert(v.end(), quad, quad + 12);
            }
        }
        m_last = s;
        m_hasLast = true;
    }
    if (v.empty())
        return;
    const UINT vertexCount = std::min<UINT>(UINT(v.size() / 2), kMaxVertices);
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(m_context->Map(m_vertices.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return;
    memcpy(mapped.pData, v.data(), vertexCount * sizeof(float) * 2);
    m_context->Unmap(m_vertices.Get(), 0);

    const UINT stride = sizeof(float) * 2, offset = 0;
    D3D11_VIEWPORT vp{0, 0, float(m_width), float(m_height), 0, 1};
    m_context->RSSetViewports(1, &vp);
    m_context->RSSetState(m_rasterizer.Get());
    m_context->OMSetRenderTargets(1, m_canvasRtv.GetAddressOf(), nullptr);
    m_context->IASetInputLayout(m_layout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->IASetVertexBuffers(0, 1, m_vertices.GetAddressOf(), &stride, &offset);
    m_context->VSSetShader(m_vs.Get(), nullptr, 0);
    m_context->VSSetConstantBuffers(0, 1, m_constants.GetAddressOf());
    m_context->PSSetShader(m_ps.Get(), nullptr, 0);
    m_context->Draw(vertexCount, 0);
}

double median(std::vector<double> v)
{
    if (v.empty())
        return 0;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

double percentile95(std::vector<double> v)
{
    if (v.empty())
        return 0;
    std::sort(v.begin(), v.end());
    return v[std::min(v.size() - 1, size_t(double(v.size()) * 0.95))];
}

void Renderer::updateStats(INT64 now)
{
    if (qpcToMs(now - m_lastStats) < 250)
        return;
    m_lastStats = now;
    if (m_csv)
        fflush(m_csv);
    const int mode = g_mode;
    // Últimos 3 s del modo actual.
    while (!m_recent.empty() && qpcToMs(now - m_recent.front().when) > 3000)
        m_recent.pop_front();
    std::vector<double> vsync, present, age;
    for (const LatencyRow& r : m_recent) {
        if (r.mode != mode)
            continue;
        if (r.toVsyncMs > 0)
            vsync.push_back(r.toVsyncMs);
        present.push_back(r.toPresentMs);
        age.push_back(r.ageMs);
    }
    wchar_t text[1024];
    const int len = swprintf(
        text, 1024,
        L"Spike HU-44 · swapchain D3D11   (0-3 modo, +/- adelanto, C limpia, Esc sale)\n"
        L"Modo %hs%ls   tearing soportado: %ls\n"
        L"muestra → vsync en que se ve: mediana %.1f ms   p95 %.1f ms   (%zu frames)%ls\n"
        L"muestra → Present: mediana %.1f ms   ·   edad de la muestra al tomarla: mediana %.1f ms\n"
        L"No incluye scanout ni respuesta del panel (10-20 ms según el plan).\n"
        L"CSV: %ls",
        kModeNames[mode], mode == 2 ? (L"  (adelanto " + std::to_wstring(g_leadTenthsMs / 10.0).substr(0, 4) + L" ms)").c_str() : L"",
        m_tearingSupported ? L"sí" : L"no", median(vsync), percentile95(vsync), vsync.size(),
        m_statsAvailable ? L"" : L"   [sin GetFrameStatistics]", median(present), median(age), m_csvPath.c_str());

    RECT box{0, 0, kStatsW, kStatsH};
    HBRUSH brush = CreateSolidBrush(RGB(34, 38, 43));
    FillRect(m_statsDc, &box, brush);
    DeleteObject(brush);
    SetBkMode(m_statsDc, TRANSPARENT);
    SetTextColor(m_statsDc, RGB(236, 238, 240));
    RECT inner{12, 8, kStatsW - 12, kStatsH - 8};
    DrawTextW(m_statsDc, text, len, &inner, DT_LEFT | DT_TOP | DT_NOPREFIX);
    GdiFlush();
    m_context->UpdateSubresource(m_statsTexture.Get(), 0, nullptr, m_statsBits, kStatsW * 4, 0);
}

// Une cada Present con el vsync en que se mostró. GetFrameStatistics informa el último
// frame que llegó a pantalla; los anteriores se estiman restando períodos.
void Renderer::resolveStatistics(double periodMs)
{
    DXGI_FRAME_STATISTICS stats{};
    const bool ok = SUCCEEDED(m_swapChain->GetFrameStatistics(&stats)) && stats.PresentCount > 0;
    m_statsAvailable = ok;
    while (!m_pending.empty()) {
        FrameRecord& f = m_pending.front();
        INT64 sync = 0;
        bool estimated = false;
        if (f.mode == 3 || !ok) {
            sync = 0; // tearing: no hay vsync de referencia
        } else if (f.presentCount > stats.PresentCount) {
            break; // todavía no se mostró
        } else {
            const UINT behind = stats.PresentCount - f.presentCount;
            sync = stats.SyncQPCTime.QuadPart -
                   INT64(double(behind) * periodMs * double(g_qpcFrequency.QuadPart) / 1000.0);
            estimated = behind > 0;
        }
        if (f.newestSample) {
            LatencyRow row{f.mode, sync ? qpcToMs(sync - f.newestSample) : 0.0, qpcToMs(f.present - f.newestSample),
                           qpcToMs(f.drain - f.newestSample), f.present};
            m_recent.push_back(row);
        }
        if (m_csv)
            fprintf(m_csv, "%d,%.1f,%u,%d,%lld,%lld,%lld,%lld,%d\n", f.mode, f.leadTenths / 10.0, f.presentCount,
                    f.samples, f.newestSample, f.drain, f.present, sync, estimated ? 1 : 0);
        m_pending.pop_front();
    }
}

void Renderer::openCsv()
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
    swprintf(name, 64, L"\\swapchain-%04u%02u%02u-%02u%02u%02u.csv", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
             t.wSecond);
    m_csvPath = dir + name;
    m_csv = _wfsopen(m_csvPath.c_str(), L"w", _SH_DENYNO); // legible mientras corre
    if (m_csv) {
        fprintf(m_csv, "# qpc_hz=%lld tearing=%d size=%ux%u\n", g_qpcFrequency.QuadPart, m_tearingSupported ? 1 : 0,
                m_width, m_height);
        fprintf(m_csv, "mode,lead_ms,present_count,samples,newest_sample_qpc,drain_qpc,present_qpc,sync_qpc,sync_estimated\n");
    }
}

void Renderer::sleepUntil(INT64 target)
{
    // Timer de alta resolución hasta ~0,5 ms antes; el resto, espera activa.
    const INT64 margin = g_qpcFrequency.QuadPart / 2000;
    const INT64 now = qpcNow();
    if (target - margin > now && m_timer) {
        LARGE_INTEGER due;
        due.QuadPart = -INT64(double(target - margin - now) * 1e7 / double(g_qpcFrequency.QuadPart));
        SetWaitableTimer(m_timer, &due, 0, nullptr, nullptr, FALSE);
        WaitForSingleObject(m_timer, 100);
    }
    while (qpcNow() < target)
        YieldProcessor();
}

void Renderer::run()
{
    if (!init())
        return;
    openCsv();
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    std::vector<Sample> samples;
    while (!g_quit) {
        const int mode = g_mode;
        if (mode != m_swapMode)
            createSwapChain(mode);
        if (!m_swapChain)
            break;

        DWM_TIMING_INFO timing{};
        timing.cbSize = sizeof(timing);
        double periodMs = 1000.0 / 60.0;
        if (SUCCEEDED(DwmGetCompositionTimingInfo(nullptr, &timing)) && timing.qpcRefreshPeriod)
            periodMs = qpcToMs(INT64(timing.qpcRefreshPeriod));

        // 1. Esperar el momento de tomar las muestras, según el modo.
        if (mode == 1 || mode == 2)
            WaitForSingleObjectEx(m_waitable, 1000, TRUE);
        if (mode == 2 && timing.qpcVBlank) {
            const INT64 period = INT64(timing.qpcRefreshPeriod);
            const INT64 now = qpcNow();
            INT64 next = INT64(timing.qpcVBlank);
            while (next <= now)
                next += period;
            const INT64 lead = INT64(double(g_leadTenthsMs) / 10.0 * double(g_qpcFrequency.QuadPart) / 1000.0);
            if (next - lead > now)
                sleepUntil(next - lead);
        }
        if (mode == 3)
            WaitForSingleObject(g_samplesEvent, 16);

        // 2. Tomar las muestras nuevas (late latch) y dibujarlas.
        samples.clear();
        {
            std::lock_guard lock(g_queueMutex);
            samples.swap(g_queue);
            ResetEvent(g_samplesEvent);
        }
        FrameRecord frame;
        frame.mode = mode;
        frame.leadTenths = g_leadTenthsMs;
        frame.drain = qpcNow();
        for (const Sample& s : samples) {
            if (s.inContact) {
                ++frame.samples;
                frame.newestSample = std::max<INT64>(frame.newestSample, INT64(s.qpc));
            }
        }
        if (g_clear.exchange(false)) {
            const float paper[4] = {0.902f, 0.941f, 0.961f, 1.0f};
            m_context->ClearRenderTargetView(m_canvasRtv.Get(), paper);
            m_hasLast = false;
        }
        drawSegments(samples);

        // 3. Componer: lienzo + recuadro de estadísticas, y presentar.
        updateStats(frame.drain);
        ComPtr<ID3D11Texture2D> back;
        m_swapChain->GetBuffer(0, IID_PPV_ARGS(&back));
        m_context->CopyResource(back.Get(), m_canvas.Get());
        m_context->CopySubresourceRegion(back.Get(), 0, 16, 16, 0, m_statsTexture.Get(), 0, nullptr);
        frame.present = qpcNow();
        if (mode == 3)
            m_swapChain->Present(0, m_tearingSupported ? DXGI_PRESENT_ALLOW_TEARING : 0);
        else
            m_swapChain->Present(1, 0);
        m_swapChain->GetLastPresentCount(&frame.presentCount);
        m_pending.push_back(frame);
        resolveStatistics(periodMs);
    }

    if (m_csv)
        fclose(m_csv);
    m_context->ClearState();
    m_swapChain.Reset();
    if (m_waitable)
        CloseHandle(m_waitable);
    if (m_timer)
        CloseHandle(m_timer);
    DeleteDC(m_statsDc);
    DeleteObject(m_statsBitmap);
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QueryPerformanceFrequency(&g_qpcFrequency);
    g_samplesEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    // Cascarón Qt: sin bordes, cubre el monitor principal. Qt se queda con el foco al
    // activarse la ventana, así que las teclas se atienden también acá.
    class Shell : public QWidget {
    protected:
        void keyPressEvent(QKeyEvent* event) override
        {
            const int key = event->key();
            if (key >= Qt::Key_0 && key <= Qt::Key_3)
                g_mode = key - Qt::Key_0;
            else if (key == Qt::Key_Plus || key == Qt::Key_Equal)
                g_leadTenthsMs = std::min(160, g_leadTenthsMs + 5);
            else if (key == Qt::Key_Minus)
                g_leadTenthsMs = std::max(5, g_leadTenthsMs - 5);
            else if (key == Qt::Key_C)
                g_clear = true;
            else if (key == Qt::Key_Escape)
                QCoreApplication::quit();
        }
    } shell;
    shell.setWindowFlag(Qt::FramelessWindowHint);
    shell.setWindowTitle(QStringLiteral("Spike HU-44 · swapchain"));
    shell.setGeometry(QGuiApplication::primaryScreen()->geometry());
    shell.show();
    const HWND parent = reinterpret_cast<HWND>(shell.winId());

    // Lienzo: HWND hijo Win32 puro, sin Qt. D3D11 presenta acá.
    WNDCLASSW wc{};
    wc.lpfnWndProc = childProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
    wc.lpszClassName = L"CartucheraSpikeSwapchain";
    RegisterClassW(&wc);
    RECT client;
    GetClientRect(parent, &client);
    g_child = CreateWindowExW(0, wc.lpszClassName, L"", WS_CHILD | WS_VISIBLE, 0, 0, client.right, client.bottom, parent,
                              nullptr, wc.hInstance, nullptr);
    if (!g_child)
        return 1;
    ClientToScreen(g_child, &g_clientOrigin);
    SetFocus(g_child);

    Renderer renderer(g_child);
    std::thread renderThread([&] { renderer.run(); });

    const int result = QApplication::exec();
    g_quit = true;
    SetEvent(g_samplesEvent);
    // Esperar al hilo de render SIN dejar de atender mensajes: Present puede necesitar que
    // la ventana responda, y un join() a secas se trababa al salir.
    const HANDLE renderHandle = renderThread.native_handle();
    while (MsgWaitForMultipleObjects(1, &renderHandle, FALSE, INFINITE, QS_ALLINPUT) == WAIT_OBJECT_0 + 1) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            DispatchMessageW(&msg);
    }
    renderThread.join();
    DestroyWindow(g_child);
    CloseHandle(g_samplesEvent);
    return result;
}
