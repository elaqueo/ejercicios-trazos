#include "lienzo/Renderer.h"

#include "lienzo/Compositor.h"
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
#include <unordered_map>
#include <vector>

using Microsoft::WRL::ComPtr;

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace lienzo {

namespace {

constexpr int kStatsW = 760, kStatsH = 150;
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
cbuffer View : register(b0) { float2 size; float2 center; float cosA; float sinA; float scale; float taps; float2 pan; float2 pad; };
Texture2D image : register(t0);
SamplerState linearClamp : register(s0);
struct V { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
V vs(uint id : SV_VertexID)
{
    float2 uv = float2(id & 1, id >> 1);
    float2 d = uv * size - center;
    float2 p = center + pan + scale * float2(cosA * d.x - sinA * d.y, sinA * d.x + cosA * d.y);
    V o;
    o.pos = float4(p.x / size.x * 2 - 1, 1 - p.y / size.y * 2, 0, 1);
    o.uv = uv;
    return o;
}
// Al alejar (HU-101), cada píxel promedia los 1/scale × 1/scale texeles que cubre (taps × taps
// muestras bilineales), así una línea fina se aclara en vez de desaparecer.
float4 ps(V v) : SV_Target
{
    if (taps <= 1)
        return image.Sample(linearClamp, v.uv);
    float2 cell = 1.0 / (scale * size);
    float3 sum = 0;
    for (float j = 0; j < taps; j += 1)
        for (float i = 0; i < taps; i += 1)
            sum += image.SampleLevel(linearClamp, v.uv + ((float2(i, j) + 0.5) / taps - 0.5) * cell, 0).rgb;
    return float4(sum / (taps * taps), 1);
}
)";

// Miniatura del panel de hojas (HU-114): cada píxel de la miniatura promedia el rectángulo
// de la hoja que le toca (taps × taps muestras bilineales, cada una de 2 × 2 texeles) y
// oscurece el grafito `boost` veces (3), para que una línea fina se siga viendo chiquita.
const char kThumbShader[] = R"(
cbuffer Thumb : register(b0) { float2 origin; float2 extent; float2 cell; float taps; float boost; float4 paper; };
Texture2D image : register(t0);
SamplerState linearClamp : register(s0);
struct V { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
V vs(uint id : SV_VertexID)
{
    float2 uv = float2(id & 1, id >> 1);
    V o;
    o.pos = float4(uv.x * 2 - 1, 1 - uv.y * 2, 0, 1);
    o.uv = uv;
    return o;
}
float4 ps(V v) : SV_Target
{
    float2 center = origin + v.uv * extent;
    float3 sum = 0;
    for (float j = 0; j < taps; j += 1)
        for (float i = 0; i < taps; i += 1)
            sum += image.SampleLevel(linearClamp, center + (float2(i, j) + 0.5) / taps * cell - cell * 0.5, 0).rgb;
    float3 avg = sum / (taps * taps);
    return float4(saturate(paper.rgb - (paper.rgb - avg) * boost), 1);
}
)";

struct ThumbConstants {
    float origin[2];
    float extent[2];
    float cell[2];
    float taps, boost;
    float paper[4];
};

struct ViewConstants {
    float size[2];
    float center[2];
    float cosA, sinA;
    float scale, taps;
    float pan[2];
    float pad[2];
};

struct FrameRecord {
    UINT presentCount = 0;
    int64_t newestSampleUs = 0; // 0 = el frame no trajo muestras nuevas
};

} // namespace

// La textura de una hoja de la pila (HU-83): la región de la hoja, en BGRA.
struct SheetTexture {
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> view;
    int width = 0, height = 0;
};

// La miniatura de una hoja en el panel (HU-114).
struct Thumbnail {
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11RenderTargetView> target;
    int width = 0, height = 0;
    bool valid = false;
    bool fromImage = false;    // salió de la imagen de pantalla (la activa)
    uint64_t imageVersion = 0; // de qué versión de la imagen
    int64_t madeQpc = 0;
};

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
    // Mesa de luz (HU-83): texturas por hoja y la vista compuesta.
    Compositor compositor;
    bool compositorReady = false;
    ComPtr<ID3D11Texture2D> composed;
    ComPtr<ID3D11ShaderResourceView> composedView;
    std::unordered_map<uint64_t, SheetTexture> sheets;
    SheetTexture blank; // una hoja que nunca estuvo activa: papel liso
    // Panel de hojas (HU-114).
    ComPtr<ID3D11Texture2D> panel;
    int panelW = 0, panelH = 0;
    std::unordered_map<uint64_t, Thumbnail> thumbs;
    // La marca de la esquina (HU-104).
    ComPtr<ID3D11Texture2D> mark;
    ComPtr<ID3D11ShaderResourceView> markView;
    int markSize = 0;
    SheetMarkLayer markLayer;
    ComPtr<ID3D11VertexShader> thumbVs;
    ComPtr<ID3D11PixelShader> thumbPs;
    ComPtr<ID3D11Buffer> thumbConstants;
    uint64_t imageVersion = 0; // sube cada vez que se sube algo de la imagen de pantalla
    SheetSource sheetSource; // la imagen o la textura de la hoja (HU-90)
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
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET; // la mesa de luz compone acá
        device->CreateTexture2D(&td, nullptr, &composed);
        device->CreateShaderResourceView(composed.Get(), nullptr, &composedView);
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        compositorReady = compositor.init(device.Get());
        td.BindFlags = 0;
        td.Width = kStatsW;
        td.Height = kStatsH;
        device->CreateTexture2D(&td, nullptr, &stats);
        initRotation();
        initThumbs();

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

    void initThumbs()
    {
        ComPtr<ID3DBlob> vsCode, psCode, errors;
        if (FAILED(D3DCompile(kThumbShader, sizeof(kThumbShader) - 1, "thumb", nullptr, nullptr, "vs", "vs_5_0", 0, 0,
                              &vsCode, &errors)) ||
            FAILED(D3DCompile(kThumbShader, sizeof(kThumbShader) - 1, "thumb", nullptr, nullptr, "ps", "ps_5_0", 0, 0,
                              &psCode, &errors)))
            return;
        device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(), nullptr, &thumbVs);
        device->CreatePixelShader(psCode->GetBufferPointer(), psCode->GetBufferSize(), nullptr, &thumbPs);
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(ThumbConstants);
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        device->CreateBuffer(&bd, nullptr, &thumbConstants);
    }

    // La miniatura t (w × h) desde la región (x, y, sw, sh) de una textura de texW × texH; sin
    // fuente, papel liso.
    void makeThumb(Thumbnail& t, ID3D11ShaderResourceView* source, int texW, int texH, int x, int y, int sw, int sh)
    {
        const float paper[4] = {float((paperColor >> 16) & 0xFF) / 255.0f, float((paperColor >> 8) & 0xFF) / 255.0f,
                                float(paperColor & 0xFF) / 255.0f, 1.0f};
        if (!source || !thumbVs || !thumbPs || !sampler) {
            context->ClearRenderTargetView(t.target.Get(), paper);
            return;
        }
        const float scaleX = float(sw) / float(t.width), scaleY = float(sh) / float(t.height);
        ThumbConstants c{};
        c.origin[0] = float(x) / float(texW);
        c.origin[1] = float(y) / float(texH);
        c.extent[0] = float(sw) / float(texW);
        c.extent[1] = float(sh) / float(texH);
        c.cell[0] = c.extent[0] / float(t.width);
        c.cell[1] = c.extent[1] / float(t.height);
        c.taps = std::clamp(std::ceil(std::max(scaleX, scaleY) / 2.0f), 1.0f, 16.0f);
        c.boost = 3.0f;
        std::copy(paper, paper + 4, c.paper);
        context->UpdateSubresource(thumbConstants.Get(), 0, nullptr, &c, 0, 0);
        const D3D11_VIEWPORT viewport{0, 0, float(t.width), float(t.height), 0, 1};
        context->RSSetViewports(1, &viewport);
        context->OMSetRenderTargets(1, t.target.GetAddressOf(), nullptr);
        context->IASetInputLayout(nullptr);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        context->VSSetShader(thumbVs.Get(), nullptr, 0);
        context->PSSetShader(thumbPs.Get(), nullptr, 0);
        context->PSSetConstantBuffers(0, 1, thumbConstants.GetAddressOf());
        context->PSSetShaderResources(0, 1, &source);
        context->PSSetSamplers(0, 1, sampler.GetAddressOf());
        context->Draw(4, 0);
        ID3D11ShaderResourceView* noView = nullptr;
        context->PSSetShaderResources(0, 1, &noView);
        context->OMSetRenderTargets(0, nullptr, nullptr);
    }

    // Panel de hojas (HU-114): la imagen del panel y las miniaturas, sobre el back buffer.
    void drawPanel(ID3D11Texture2D* back, int x, int y, const std::vector<PanelThumb>& panelThumbs, const RECT& sheetRect,
                   int64_t now)
    {
        if (!panel)
            return;
        const auto clipped = [&](int left, int top, int w, int h, D3D11_BOX& box, UINT& dx, UINT& dy) {
            const int x0 = std::max(left, 0), y0 = std::max(top, 0);
            const int x1 = std::min(left + w, int(width)), y1 = std::min(top + h, int(height));
            if (x0 >= x1 || y0 >= y1)
                return false;
            box = {UINT(x0 - left), UINT(y0 - top), 0, UINT(x1 - left), UINT(y1 - top), 1};
            dx = UINT(x0);
            dy = UINT(y0);
            return true;
        };
        D3D11_BOX box;
        UINT dx, dy;
        if (clipped(x, y, panelW, panelH, box, dx, dy))
            context->CopySubresourceRegion(back, 0, dx, dy, 0, panel.Get(), 0, &box);
        const int sheetW = int(sheetRect.right - sheetRect.left), sheetH = int(sheetRect.bottom - sheetRect.top);
        const int64_t refresh = qpcFrequency / 4; // la activa, a lo sumo cada 250 ms mientras se dibuja
        for (const PanelThumb& p : panelThumbs) {
            const int w = int(p.rect.right - p.rect.left), h = int(p.rect.bottom - p.rect.top);
            if (!p.sheet || w <= 0 || h <= 0)
                continue;
            Thumbnail& t = thumbs[p.sheet];
            if (!t.texture || t.width != w || t.height != h) {
                D3D11_TEXTURE2D_DESC td{};
                td.Width = UINT(w);
                td.Height = UINT(h);
                td.MipLevels = 1;
                td.ArraySize = 1;
                td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                td.SampleDesc.Count = 1;
                td.Usage = D3D11_USAGE_DEFAULT;
                td.BindFlags = D3D11_BIND_RENDER_TARGET;
                t = {};
                if (FAILED(device->CreateTexture2D(&td, nullptr, &t.texture)))
                    continue;
                device->CreateRenderTargetView(t.texture.Get(), nullptr, &t.target);
                t.width = w;
                t.height = h;
            }
            const bool fromImage = p.sheet == sheetSource.uploaded() && sheetW > 0 && sheetH > 0;
            if (fromImage) {
                if (!t.valid || !t.fromImage || (t.imageVersion != imageVersion && now - t.madeQpc > refresh)) {
                    makeThumb(t, displayView.Get(), int(width), int(height), int(sheetRect.left), int(sheetRect.top), sheetW,
                              sheetH);
                    t.imageVersion = imageVersion;
                    t.madeQpc = now;
                    t.valid = t.fromImage = true;
                }
            } else if (!t.valid || t.fromImage) {
                // Desde su textura (la que dejó al salir); si nunca estuvo activa, papel liso.
                ID3D11ShaderResourceView* view = sheetW > 0 ? sheetView(p.sheet, sheetW, sheetH) : nullptr;
                makeThumb(t, view, sheetW, sheetH, 0, 0, sheetW, sheetH);
                t.madeQpc = now;
                t.valid = true;
                t.fromImage = false;
            }
            if (clipped(int(p.rect.left), int(p.rect.top), w, h, box, dx, dy))
                context->CopySubresourceRegion(back, 0, dx, dy, 0, t.texture.Get(), 0, &box);
        }
    }

    // Sube la imagen de una hoja a su textura (la crea si no hay o cambió el tamaño).
    void uploadSheet(SheetTexture& sheet, int w, int h, const uint32_t* pixels)
    {
        if (!sheet.texture || sheet.width != w || sheet.height != h) {
            D3D11_TEXTURE2D_DESC td{};
            td.Width = UINT(w);
            td.Height = UINT(h);
            td.MipLevels = 1;
            td.ArraySize = 1;
            td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            td.SampleDesc.Count = 1;
            td.Usage = D3D11_USAGE_DEFAULT;
            td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            sheet = {};
            if (FAILED(device->CreateTexture2D(&td, nullptr, &sheet.texture)))
                return;
            device->CreateShaderResourceView(sheet.texture.Get(), nullptr, &sheet.view);
            sheet.width = w;
            sheet.height = h;
        }
        context->UpdateSubresource(sheet.texture.Get(), 0, nullptr, pixels, UINT(w) * 4, 0);
    }

    // La textura de la hoja `id`, o nullptr si no hay (o es de otro tamaño de hoja).
    ID3D11ShaderResourceView* sheetView(uint64_t id, int w, int h)
    {
        const auto it = sheets.find(id);
        if (it == sheets.end() || it->second.width != w || it->second.height != h)
            return nullptr;
        return it->second.view.Get();
    }

    uint32_t paperColor = kPaperColor;
    ID3D11ShaderResourceView* blankView(int w, int h)
    {
        if (!blank.texture || blank.width != w || blank.height != h) {
            const std::vector<uint32_t> paper(size_t(w) * size_t(h), paperColor);
            uploadSheet(blank, w, h, paper.data());
        }
        return blank.view.Get();
    }

    // Dibuja la imagen girada en el back buffer; devuelve false si no hay shaders.
    bool drawRotated(ID3D11Texture2D* back, const ViewRotation& rotation, ID3D11ShaderResourceView* source)
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
        // Al alejar, una muestra por texel cubierto (cada una bilineal: 2 × 2), hasta 4 × 4.
        const float taps = rotation.scale < 1 ? float(std::min(4.0, std::ceil(0.5 / rotation.scale))) : 1.0f;
        const ViewConstants c{{float(width), float(height)},
                              {float(rotation.cx), float(rotation.cy)},
                              float(std::cos(a)),
                              float(std::sin(a)),
                              float(rotation.scale),
                              taps,
                              {float(rotation.panX), float(rotation.panY)},
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
        context->PSSetShaderResources(0, 1, &source);
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
        wchar_t text[1024];
        const int len = swprintf(text, 1024,
                                 L"Trazos %hs   ·   F3 oculta   ·   muestra → vsync: mediana %.1f ms · p95 %.1f ms\n"
                                 L"adelanto %.2f ms · vsyncs perdidos %d\n%ls",
                                 ET_VERSION, percentile(recent, 0.5), percentile(recent, 0.95), lead.leadMs(), lead.missed(),
                                 extra.c_str());
        RECT box{0, 0, kStatsW, kStatsH};
        HBRUSH brush = CreateSolidBrush(RGB(34, 38, 43));
        FillRect(statsDc, &box, brush);
        DeleteObject(brush);
        SetBkMode(statsDc, TRANSPARENT);
        SetTextColor(statsDc, RGB(236, 238, 240));
        RECT inner{12, 10, kStatsW - 12, kStatsH - 10};
        DrawTextW(statsDc, text, len, &inner, DT_LEFT | DT_TOP | DT_NOPREFIX | DT_WORDBREAK); // la línea de la herramienta es larga
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
    d.paperColor = m_paperColor;
    d.compositor.setPaperColor(m_paperColor);
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
        std::vector<RetiredSheet> retired;
        {
            std::unique_lock lock(m_image.mutex, std::try_to_lock);
            if (m_timings)
                m_timings->renderLockBusy.add(lock.owns_lock() ? 0.0 : 1.0);
            const bool pendingBefore = lock.owns_lock() && m_image.hasDirty;
            if (pendingBefore) {
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
                ++d.imageVersion; // la miniatura de la activa (HU-114) se rehace
            }
            if (lock.owns_lock())
                d.sheetSource.latched(m_image.sheet, pendingBefore, m_image.hasDirty);
            if (lock.owns_lock() && !m_image.retired.empty())
                retired.swap(m_image.retired); // se suben afuera del candado
            // La muestra más nueva cuenta como mostrada cuando ya no queda nada por subir.
            if (lock.owns_lock() && !m_image.hasDirty && m_image.sampleVersion != lastVersion) {
                lastVersion = m_image.sampleVersion;
                frame.newestSampleUs = m_image.newestSampleUs;
            }
        }

        // Pila de hojas (HU-83): las hojas que salieron, a sus texturas; las quitadas, afuera.
        RECT sheetRect;
        uint64_t shown;
        bool untilActive;
        int64_t flipQpc, lightQpc;
        std::vector<SheetLayer> below;
        bool panelVisible;
        int panelX, panelY;
        std::vector<PanelThumb> panelThumbs;
        {
            std::lock_guard layersLock(m_layersMutex);
            panelVisible = m_panelVisible;
            panelX = m_panelX;
            panelY = m_panelY;
            if (panelVisible)
                panelThumbs = m_panelThumbs;
            if (m_panelChanged) { // la imagen del panel (HU-114), a su textura
                m_panelChanged = false;
                if (!d.panel || d.panelW != m_panelW || d.panelH != m_panelH) {
                    D3D11_TEXTURE2D_DESC td{};
                    td.Width = UINT(std::max(1, m_panelW));
                    td.Height = UINT(std::max(1, m_panelH));
                    td.MipLevels = 1;
                    td.ArraySize = 1;
                    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                    td.SampleDesc.Count = 1;
                    td.Usage = D3D11_USAGE_DEFAULT;
                    d.panel.Reset();
                    d.device->CreateTexture2D(&td, nullptr, &d.panel);
                    d.panelW = m_panelW;
                    d.panelH = m_panelH;
                }
                if (d.panel && m_panelPixels.size() == size_t(m_panelW) * size_t(m_panelH))
                    d.context->UpdateSubresource(d.panel.Get(), 0, nullptr, m_panelPixels.data(), UINT(m_panelW) * 4, 0);
            }
            if (m_markChanged) { // la marca (HU-104), a su textura
                m_markChanged = false;
                const int size = m_markSize;
                if (m_markPixels.size() != size_t(size) * size_t(size) || size <= 0) {
                    d.markLayer.view = nullptr;
                } else {
                    if (!d.mark || d.markSize != size) {
                        D3D11_TEXTURE2D_DESC td{};
                        td.Width = td.Height = UINT(size);
                        td.MipLevels = 1;
                        td.ArraySize = 1;
                        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                        td.SampleDesc.Count = 1;
                        td.Usage = D3D11_USAGE_DEFAULT;
                        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                        d.mark.Reset();
                        d.markView.Reset();
                        if (SUCCEEDED(d.device->CreateTexture2D(&td, nullptr, &d.mark)))
                            d.device->CreateShaderResourceView(d.mark.Get(), nullptr, &d.markView);
                        d.markSize = size;
                    }
                    if (d.mark)
                        d.context->UpdateSubresource(d.mark.Get(), 0, nullptr, m_markPixels.data(), UINT(size) * 4, 0);
                    d.markLayer = {d.markView.Get(), {m_markX, m_markY, m_markX + size, m_markY + size}, m_markColor};
                }
            }
            sheetRect = m_sheetRect;
            shown = m_shown;
            untilActive = m_untilActive;
            flipQpc = m_flipQpc;
            m_flipQpc = 0;
            lightQpc = m_lightQpc;
            m_lightQpc = 0;
            below = m_below;
            for (const uint64_t id : m_dropped) {
                d.sheets.erase(id);
                d.thumbs.erase(id);
            }
            m_dropped.clear();
        }
        for (RetiredSheet& r : retired) {
            d.uploadSheet(d.sheets[r.sheet], r.width, r.height, r.pixels.data());
            if (const auto t = d.thumbs.find(r.sheet); t != d.thumbs.end())
                t->second.valid = false; // la miniatura sale de la textura nueva (HU-114)
        }
        // La hoja soltada (HU-90) ya está entera en la imagen: de acá en más se ve la activa.
        if (untilActive && shown == d.sheetSource.uploaded()) {
            std::lock_guard layersLock(m_layersMutex);
            if (m_shown == shown && m_untilActive) {
                m_shown = 0;
                m_untilActive = false;
            }
            shown = 0;
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
        // Mesa de luz o flip (HU-83, HU-90): la hoja que se muestra (de la imagen o de su
        // textura) con las de abajo, compuesta en la GPU; si no hay ni otra hoja ni capas, la
        // imagen de pantalla tal cual, como siempre.
        ID3D11Texture2D* source = d.display.Get();
        ID3D11ShaderResourceView* sourceView = d.displayView.Get();
        const int sheetW = int(sheetRect.right - sheetRect.left), sheetH = int(sheetRect.bottom - sheetRect.top);
        if (d.compositorReady && sheetW > 0 && sheetH > 0) {
            ID3D11ShaderResourceView* texture = shown ? d.sheetView(shown, sheetW, sheetH) : nullptr;
            ID3D11ShaderResourceView* base = nullptr;
            switch (d.sheetSource.pick(shown, texture != nullptr)) {
            case SheetSource::Image: break;
            case SheetSource::Texture: base = texture; break;
            case SheetSource::Blank: base = d.blankView(sheetW, sheetH); break;
            }
            std::vector<LightTableLayer> layers;
            for (const SheetLayer& l : below)
                if (ID3D11ShaderResourceView* v = d.sheetView(l.sheet, sheetW, sheetH))
                    layers.push_back({v, l.color, l.opacity});
            // La marca sola (HU-104), con la vista sin girar, va después sobre su recuadro (drawMark).
            const SheetMarkLayer* mark = d.markLayer.view ? &d.markLayer : nullptr;
            if (base || !layers.empty() || (mark && !rotation.identity())) {
                d.compositor.compose(d.context.Get(), d.display.Get(), d.displayView.Get(), base, sheetRect, layers,
                                     d.composed.Get(), mark);
                source = d.composed.Get();
                sourceView = d.composedView.Get();
            }
        }
        if (rotation.identity() || !d.drawRotated(back.Get(), rotation, sourceView)) {
            d.context->CopyResource(back.Get(), source);
            if (source == d.display.Get() && d.markLayer.view && d.compositorReady)
                d.compositor.drawMark(d.context.Get(), d.displayView.Get(), sheetRect, d.markLayer, back.Get());
        }
        if (panelVisible)
            d.drawPanel(back.Get(), panelX, panelY, panelThumbs, sheetRect, now);
        if (m_overlay) {
            d.context->CopySubresourceRegion(back.Get(), 0, 16, 16, 0, d.stats.Get(), 0, nullptr);
            overlayShown = true;
        } else if (overlayShown) {
            overlayShown = false; // al ocultarlo, el próximo CopyResource ya lo tapa
        }
        if (m_timings)
            m_timings->renderLatchToPresent.add(latch.ms());
        d.swapChain->Present(1, 0);
        if (flipQpc && m_onFlip)
            m_onFlip(double(qpcNow() - flipQpc) * 1000.0 / double(d.qpcFrequency));
        if (lightQpc && m_onLightTable)
            m_onLightTable(double(qpcNow() - lightQpc) * 1000.0 / double(d.qpcFrequency));
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
