#include "Renderer.h"

#include "DisplayImage.h"
#include "LeadController.h"

#include <tabletinput/PenReader.h>

#include <d3d11.h>
#include <dwmapi.h>
#include <dxgi1_5.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdio>
#include <deque>
#include <vector>

using Microsoft::WRL::ComPtr;

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace cartuchera {

namespace {

constexpr int kStatsW = 760, kStatsH = 110;

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
        device->CreateTexture2D(&td, nullptr, &display);
        td.Width = kStatsW;
        td.Height = kStatsH;
        device->CreateTexture2D(&td, nullptr, &stats);

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
                                 L"Cartuchera · F3 oculta   muestra → vsync: mediana %.1f ms · p95 %.1f ms\n"
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

        // 2. Tomar lo que cambió en la imagen (late latch) y subirlo a la GPU.
        FrameRecord frame;
        {
            std::lock_guard lock(m_image.mutex);
            if (m_image.hasDirty) {
                const RECT& r = m_image.dirty;
                const D3D11_BOX box{UINT(r.left), UINT(r.top), 0, UINT(r.right), UINT(r.bottom), 1};
                d.context->UpdateSubresource(d.display.Get(), 0, &box,
                                             m_image.pixels.data() + size_t(r.top) * size_t(m_image.width) + size_t(r.left),
                                             UINT(m_image.width) * 4, 0);
                m_image.hasDirty = false;
            }
            if (m_image.sampleVersion != lastVersion) {
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
        d.context->CopyResource(back.Get(), d.display.Get());
        if (m_overlay) {
            d.context->CopySubresourceRegion(back.Get(), 0, 16, 16, 0, d.stats.Get(), 0, nullptr);
            overlayShown = true;
        } else if (overlayShown) {
            overlayShown = false; // al ocultarlo, el próximo CopyResource ya lo tapa
        }
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

} // namespace cartuchera
