#include "lienzo/Renderer.h"

#include "lienzo/DisplayImage.h"
#include "lienzo/LeadController.h"
#include "lienzo/Timing.h"
#include "lienzo/Tone.h"

#include <tabletinput/PenReader.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dwmapi.h>
#include <dxgi1_5.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <vector>

using Microsoft::WRL::ComPtr;

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace lienzo {

namespace {

constexpr int kStatsW = 760, kStatsH = 110;
// Píxeles que se suben a la GPU como máximo por frame (~1 MB). Un trazo normal sube unos
// cientos; deshacer un trazo largo puede cambiar casi toda la hoja.
constexpr LONG kUploadBudget = 256 * 1024;

int64_t qpcNow()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return now.QuadPart;
}

double percentile(std::vector<double> v, double p)
{
    if (v.empty())
        return 0;
    std::sort(v.begin(), v.end());
    return v[std::min(v.size() - 1, size_t(p * double(v.size())))];
}

// Vista rotada (HU-40): un rectángulo del tamaño del cliente girado alrededor de (cx, cy),
// con la imagen de pantalla como textura.
const char kRotateShader[] = R"(
cbuffer View : register(b0) { float2 size; float2 center; float cosA; float sinA; float2 pad; };
Texture2D image : register(t0);
SamplerState linearClamp : register(s0);
struct V { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
V vs(uint id : SV_VertexID)
{
    float2 uv = float2(id & 1, id >> 1);
    float2 d = uv * size - center;
    float2 p = center + float2(cosA * d.x - sinA * d.y, sinA * d.x + cosA * d.y);
    V o;
    o.pos = float4(p.x / size.x * 2 - 1, 1 - p.y / size.y * 2, 0, 1);
    o.uv = uv;
    return o;
}
float4 ps(V v) : SV_Target { return image.Sample(linearClamp, v.uv); }
)";

struct ViewConstants {
    float size[2];
    float center[2];
    float cosA, sinA;
    float pad[2];
};

struct FrameRecord {
    UINT presentCount = 0;
    int64_t newestSampleUs = 0; // 0 = el frame no trajo muestras nuevas
};

} // namespace

struct Renderer::Impl {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain2> swapChain;
    ComPtr<ID3D11Texture2D> display, stats;
    // Vista rotada (HU-40).
    ComPtr<ID3D11ShaderResourceView> displayView;
    ComPtr<ID3D11VertexShader> rotateVs;
    ComPtr<ID3D11PixelShader> rotatePs;
    ComPtr<ID3D11Buffer> viewConstants;
    ComPtr<ID3D11SamplerState> sampler;
    HANDLE waitable = nullptr, timer = nullptr;
    HDC statsDc = nullptr;
    HBITMAP statsBitmap = nullptr;
    void* statsBits = nullptr;
    int64_t qpcFrequency = 0;
    UINT width = 0, height = 0;

    LeadController lead;
    std::deque<FrameRecord> pending;
    std::deque<double> latencies; // ms, últimos ~3 s con muestras
    std::vector<double> allLatencies;
    int64_t lastSyncQpc = 0;
    UINT lastSyncPresent = 0;
    int64_t lastStats = 0;

    ~Impl()
    {
        if (waitable)
            CloseHandle(waitable);
        if (timer)
            CloseHandle(timer);
        if (statsDc)
            DeleteDC(statsDc);
        if (statsBitmap)
            DeleteObject(statsBitmap);
    }

    bool init(HWND hwnd)
    {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        qpcFrequency = f.QuadPart;
        RECT rc;
        GetClientRect(hwnd, &rc);
        width = UINT(rc.right - rc.left);
        height = UINT(rc.bottom - rc.top);

        const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};
        if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                     levels, 1, D3D11_SDK_VERSION, &device, nullptr, &context)))
            return false;
        ComPtr<IDXGIDevice> dxgiDevice;
        ComPtr<IDXGIAdapter> adapter;
        ComPtr<IDXGIFactory2> factory;
        device.As(&dxgiDevice);
        dxgiDevice->GetAdapter(&adapter);
        adapter->GetParent(IID_PPV_ARGS(&factory));

        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = width;
        desc.Height = height;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 3;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        ComPtr<IDXGISwapChain1> swapChain1;
        if (FAILED(factory->CreateSwapChainForHwnd(device.Get(), hwnd, &desc, nullptr, nullptr, &swapChain1)))
            return false;
        factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
        swapChain1.As(&swapChain);
        swapChain->SetMaximumFrameLatency(1);
        waitable = swapChain->GetFrameLatencyWaitableObject();

        D3D11_TEXTURE2D_DESC td{};
        td.Width = width;
        td.Height = height;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE; // la vista rotada la usa como textura
        device->CreateTexture2D(&td, nullptr, &display);
        device->CreateShaderResourceView(display.Get(), nullptr, &displayView);
        td.BindFlags = 0;
        td.Width = kStatsW;
        td.Height = kStatsH;
        device->CreateTexture2D(&td, nullptr, &stats);
        initRotation();

        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
        bi.bmiHeader.biWidth = kStatsW;
        bi.bmiHeader.biHeight = -kStatsH;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        statsDc = CreateCompatibleDC(nullptr);
        statsBitmap = CreateDIBSection(statsDc, &bi, DIB_RGB_COLORS, &statsBits, nullptr, 0);
        SelectObject(statsDc, statsBitmap);

        timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (!timer)
            timer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
        return true;
    }

    // Shaders de la vista rotada, compilados al arrancar (d3dcompiler_47 viene con Windows).
    // Si fallan, la vista no rota pero el lienzo sigue andando.
    void initRotation()
    {
        ComPtr<ID3DBlob> vsCode, psCode, errors;
        if (FAILED(D3DCompile(kRotateShader, sizeof(kRotateShader) - 1, "rotate", nullptr, nullptr, "vs", "vs_5_0", 0, 0,
                              &vsCode, &errors)) ||
            FAILED(D3DCompile(kRotateShader, sizeof(kRotateShader) - 1, "rotate", nullptr, nullptr, "ps", "ps_5_0", 0, 0,
                              &psCode, &errors)))
            return;
        device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(), nullptr, &rotateVs);
        device->CreatePixelShader(psCode->GetBufferPointer(), psCode->GetBufferSize(), nullptr, &rotatePs);
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(ViewConstants);
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        device->CreateBuffer(&bd, nullptr, &viewConstants);
        D3D11_SAMPLER_DESC sd{};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        device->CreateSamplerState(&sd, &sampler);
    }

    // Dibuja la imagen girada en el back buffer; devuelve false si no hay shaders.
    bool drawRotated(ID3D11Texture2D* back, const ViewRotation& rotation)
    {
        if (!rotateVs || !rotatePs)
            return false;
        ComPtr<ID3D11RenderTargetView> target;
        if (FAILED(device->CreateRenderTargetView(back, nullptr, &target)))
            return false;
        const float outside[4] = {float((kOutsideColor >> 16) & 0xFF) / 255.0f, float((kOutsideColor >> 8) & 0xFF) / 255.0f,
                                  float(kOutsideColor & 0xFF) / 255.0f, 1.0f};
        context->ClearRenderTargetView(target.Get(), outside);
        const double a = rotation.degrees * 3.14159265358979323846 / 180.0;
        const ViewConstants c{{float(width), float(height)},
                              {float(rotation.cx), float(rotation.cy)},
                              float(std::cos(a)),
                              float(std::sin(a)),
                              {0, 0}};
        context->UpdateSubresource(viewConstants.Get(), 0, nullptr, &c, 0, 0);
        const D3D11_VIEWPORT viewport{0, 0, float(width), float(height), 0, 1};
        context->RSSetViewports(1, &viewport);
        context->OMSetRenderTargets(1, target.GetAddressOf(), nullptr);
        context->IASetInputLayout(nullptr);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        context->VSSetShader(rotateVs.Get(), nullptr, 0);
        context->VSSetConstantBuffers(0, 1, viewConstants.GetAddressOf());
        context->PSSetShader(rotatePs.Get(), nullptr, 0);
        context->PSSetShaderResources(0, 1, displayView.GetAddressOf());
        context->PSSetSamplers(0, 1, sampler.GetAddressOf());
        context->Draw(4, 0);
        // Soltar la textura y el destino: el próximo frame se copia y se sube a ellos.
        ID3D11ShaderResourceView* noView = nullptr;
        context->PSSetShaderResources(0, 1, &noView);
        context->OMSetRenderTargets(0, nullptr, nullptr);
        return true;
    }

    void sleepUntil(int64_t target)
    {
        const int64_t margin = qpcFrequency / 2000; // último medio ms: espera activa
        const int64_t now = qpcNow();
        if (target - margin > now && timer) {
            LARGE_INTEGER due;
            due.QuadPart = -int64_t(double(target - margin - now) * 1e7 / double(qpcFrequency));
            SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE);
            WaitForSingleObject(timer, 100);
        }
        while (qpcNow() < target)
            YieldProcessor();
    }

    // Une cada Present con el vsync en que se mostró; detecta vsyncs perdidos (dos frames
    // seguidos separados por más de 1,5 períodos) para el adelanto adaptativo.
    void resolve(double periodMs)
    {
        DXGI_FRAME_STATISTICS st{};
        if (FAILED(swapChain->GetFrameStatistics(&st)) || st.PresentCount == 0) {
            pending.clear();
            return;
        }
        while (!pending.empty() && pending.front().presentCount <= st.PresentCount) {
            const FrameRecord f = pending.front();
            pending.pop_front();
            const UINT behind = st.PresentCount - f.presentCount;
            const int64_t sync = st.SyncQPCTime.QuadPart - int64_t(double(behind) * periodMs * double(qpcFrequency) / 1000.0);
            if (lastSyncQpc && f.presentCount == lastSyncPresent + 1)
                lead.onFrame(double(sync - lastSyncQpc) * 1000.0 / double(qpcFrequency) > periodMs * 1.5);
            lastSyncQpc = sync;
            lastSyncPresent = f.presentCount;
            if (f.newestSampleUs) {
                const double ms = double(tabletinput::qpcToMicroseconds(sync, qpcFrequency) - f.newestSampleUs) / 1000.0;
                latencies.push_back(ms);
                allLatencies.push_back(ms);
                if (latencies.size() > 400)
                    latencies.pop_front();
            }
        }
    }

    void drawStats(const std::wstring& extra)
    {
        const std::vector<double> recent(latencies.begin(), latencies.end());
        wchar_t text[512];
        const int len = swprintf(text, 512,
                                 L"F3 oculta   muestra → vsync: mediana %.1f ms · p95 %.1f ms\n"
                                 L"adelanto %.2f ms · vsyncs perdidos %d\n%ls",
                                 percentile(recent, 0.5), percentile(recent, 0.95), lead.leadMs(), lead.missed(),
                                 extra.c_str());
        RECT box{0, 0, kStatsW, kStatsH};
        HBRUSH brush = CreateSolidBrush(RGB(34, 38, 43));
        FillRect(statsDc, &box, brush);
        DeleteObject(brush);
        SetBkMode(statsDc, TRANSPARENT);
        SetTextColor(statsDc, RGB(236, 238, 240));
        RECT inner{12, 10, kStatsW - 12, kStatsH - 10};
        DrawTextW(statsDc, text, len, &inner, DT_LEFT | DT_TOP | DT_NOPREFIX);
        GdiFlush();
        context->UpdateSubresource(stats.Get(), 0, nullptr, statsBits, kStatsW * 4, 0);
    }
};

Renderer::Renderer(HWND hwnd, DisplayImage& image)
    : m_hwnd(hwnd)
    , m_image(image)
{
}

Renderer::~Renderer() = default;

void Renderer::start()
{
    m_quit = false;
    m_thread = std::thread([this] { run(); });
}

void Renderer::stop()
{
    m_quit = true;
    if (!m_thread.joinable())
        return;
    const HANDLE handle = m_thread.native_handle();
    while (MsgWaitForMultipleObjects(1, &handle, FALSE, INFINITE, QS_ALLINPUT) == WAIT_OBJECT_0 + 1) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            DispatchMessageW(&msg);
    }
    m_thread.join();
}

std::string Renderer::summary() const
{
    return m_summary;
}

void Renderer::run()
{
    Impl d;
    if (!d.init(m_hwnd)) {
        m_summary = "Renderer: no se pudo crear el swapchain D3D11";
        return;
    }
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    uint64_t lastVersion = 0;
    bool overlayShown = false;
    while (!m_quit) {
        DWM_TIMING_INFO timing{};
        timing.cbSize = sizeof(timing);
        double periodMs = 1000.0 / 60.0;
        const bool haveTiming = SUCCEEDED(DwmGetCompositionTimingInfo(nullptr, &timing)) && timing.qpcRefreshPeriod;
        if (haveTiming)
            periodMs = double(timing.qpcRefreshPeriod) * 1000.0 / double(d.qpcFrequency);

        // 1. Justo a tiempo: después del waitable, dormir hasta (vsync − adelanto).
        WaitForSingleObjectEx(d.waitable, 1000, TRUE);
        if (haveTiming && timing.qpcVBlank) {
            const int64_t period = int64_t(timing.qpcRefreshPeriod), now = qpcNow();
            int64_t next = int64_t(timing.qpcVBlank);
            while (next <= now)
                next += period;
            const int64_t lead = int64_t(d.lead.leadMs() * double(d.qpcFrequency) / 1000.0);
            if (next - lead > now)
                d.sleepUntil(next - lead);
        }

        // 2. Tomar lo que cambió en la imagen (late latch) y subirlo a la GPU. El render nunca
        //    espera a la simulación: si la imagen está ocupada, presenta lo que ya tenía y la
        //    toma en el frame siguiente (16 ms más tarde, en vez de perder un vsync). Y sube a
        //    lo sumo kUploadBudget píxeles por frame: el resto queda para los siguientes.
        FrameRecord frame;
        const Stopwatch latch;
        {
            std::unique_lock lock(m_image.mutex, std::try_to_lock);
            if (m_timings)
                m_timings->renderLockBusy.add(lock.owns_lock() ? 0.0 : 1.0);
            if (lock.owns_lock() && m_image.hasDirty) {
                RECT& r = m_image.dirty;
                const LONG width = r.right - r.left;
                const LONG rows = std::max<LONG>(1, std::min<LONG>(r.bottom - r.top, kUploadBudget / std::max<LONG>(1, width)));
                if (m_timings)
                    m_timings->renderUploadPixels.add(double(width) * double(rows));
                const D3D11_BOX box{UINT(r.left), UINT(r.top), 0, UINT(r.right), UINT(r.top + rows), 1};
                d.context->UpdateSubresource(d.display.Get(), 0, &box,
                                             m_image.pixels.data() + size_t(r.top) * size_t(m_image.width) + size_t(r.left),
                                             UINT(m_image.width) * 4, 0);
                r.top += rows;
                m_image.hasDirty = r.top < r.bottom;
            }
            // La muestra más nueva cuenta como mostrada cuando ya no queda nada por subir.
            if (lock.owns_lock() && !m_image.hasDirty && m_image.sampleVersion != lastVersion) {
                lastVersion = m_image.sampleVersion;
                frame.newestSampleUs = m_image.newestSampleUs;
            }
        }

        // 3. Componer y presentar.
        const int64_t now = qpcNow();
        if (m_overlay && double(now - d.lastStats) * 1000.0 / double(d.qpcFrequency) > 250) {
            d.lastStats = now;
            d.drawStats(m_extra ? m_extra() : std::wstring());
        }
        ComPtr<ID3D11Texture2D> back;
        d.swapChain->GetBuffer(0, IID_PPV_ARGS(&back));
        ViewRotation rotation;
        {
            std::lock_guard rotationLock(m_rotationMutex);
            rotation = m_rotation;
        }
        if (rotation.identity() || !d.drawRotated(back.Get(), rotation))
            d.context->CopyResource(back.Get(), d.display.Get());
        if (m_overlay) {
            d.context->CopySubresourceRegion(back.Get(), 0, 16, 16, 0, d.stats.Get(), 0, nullptr);
            overlayShown = true;
        } else if (overlayShown) {
            overlayShown = false; // al ocultarlo, el próximo CopyResource ya lo tapa
        }
        if (m_timings)
            m_timings->renderLatchToPresent.add(latch.ms());
        d.swapChain->Present(1, 0);
        d.swapChain->GetLastPresentCount(&frame.presentCount);
        d.pending.push_back(frame);
        d.resolve(periodMs);
    }

    char text[256];
    snprintf(text, sizeof(text),
             "Sesión: %zu frames con trazo · muestra → vsync mediana %.1f ms, p95 %.1f ms · adelanto final %.2f ms · "
             "vsyncs perdidos %d",
             d.allLatencies.size(), percentile(d.allLatencies, 0.5), percentile(d.allLatencies, 0.95), d.lead.leadMs(),
             d.lead.missed());
    m_summary = text;
}

} // namespace lienzo
