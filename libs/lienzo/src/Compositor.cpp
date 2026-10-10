#include "lienzo/Compositor.h"

#include "lienzo/Tone.h"

#include <d3dcompiler.h>

#include <algorithm>
#include <cmath>

using Microsoft::WRL::ComPtr;

namespace lienzo {

namespace {

// Un rectángulo (la hoja) con dos texturas de origen: la que se muestra, con su propio
// rectángulo de uv (la imagen de pantalla entera, o la textura de una hoja), y las de abajo,
// que son del tamaño de la hoja.
const char kLightTableShader[] = R"(
cbuffer LightTable : register(b0)
{
    float4 rect;      // la hoja en clip space: x0, y0, x1, y1
    float4 baseUv;    // uv de la hoja dentro de la textura que se muestra: offset xy, escala zw
    float4 paper;     // color del papel (rgb)
    float4 graphite;  // grafito saturado (rgb)
    float4 tint[6];   // rgb: color de la capa; a: opacidad
    uint count;
    uint3 pad;
};
Texture2D shown : register(t0);
Texture2D below[6] : register(t1);
SamplerState pointClamp : register(s0);
struct V { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
V vs(uint id : SV_VertexID)
{
    float2 uv = float2(id & 1, id >> 1);
    V o;
    o.pos = float4(lerp(rect.x, rect.z, uv.x), lerp(rect.y, rect.w, uv.y), 0, 1);
    o.uv = uv;
    return o;
}
float4 ps(V v) : SV_Target
{
    float3 color = shown.Sample(pointClamp, baseUv.xy + v.uv * baseUv.zw).rgb;
    float3 factor = 1;
    float range = dot(paper.rgb - graphite.rgb, 1.0 / 3);
    [unroll] for (uint i = 0; i < 6; ++i) {
        if (i < count) {
            float3 c = below[i].Sample(pointClamp, v.uv).rgb;
            float amount = saturate(dot(paper.rgb - c, 1.0 / 3) / range);
            factor *= lerp(float3(1, 1, 1), tint[i].rgb, amount * tint[i].a);
        }
    }
    return float4(color * factor, 1);
}
)";

struct Constants {
    float rect[4];
    float baseUv[4];
    float paper[4];
    float graphite[4];
    float tint[Compositor::kMaxLayers][4];
    uint32_t count;
    uint32_t pad[3];
};

void rgb(uint32_t c, float out[4])
{
    out[0] = float((c >> 16) & 0xFF) / 255.0f;
    out[1] = float((c >> 8) & 0xFF) / 255.0f;
    out[2] = float(c & 0xFF) / 255.0f;
    out[3] = 1;
}

} // namespace

bool Compositor::init(ID3D11Device* device)
{
    m_device = device;
    ComPtr<ID3DBlob> vsCode, psCode, errors;
    if (FAILED(D3DCompile(kLightTableShader, sizeof(kLightTableShader) - 1, "lighttable", nullptr, nullptr, "vs", "vs_5_0",
                          0, 0, &vsCode, &errors)) ||
        FAILED(D3DCompile(kLightTableShader, sizeof(kLightTableShader) - 1, "lighttable", nullptr, nullptr, "ps", "ps_5_0",
                          0, 0, &psCode, &errors)))
        return false;
    device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(), nullptr, &m_vs);
    device->CreatePixelShader(psCode->GetBufferPointer(), psCode->GetBufferSize(), nullptr, &m_ps);
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = sizeof(Constants);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    device->CreateBuffer(&bd, nullptr, &m_constants);
    // Sin filtrar: la hoja se compone píxel a píxel (las texturas son del tamaño en pantalla).
    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    device->CreateSamplerState(&sd, &m_sampler);
    return m_vs && m_ps && m_constants && m_sampler;
}

void Compositor::compose(ID3D11DeviceContext* context, ID3D11Texture2D* screen, ID3D11ShaderResourceView* screenView,
                         ID3D11ShaderResourceView* base, const RECT& sheet, const std::vector<LightTableLayer>& layers,
                         ID3D11Texture2D* out)
{
    D3D11_TEXTURE2D_DESC desc;
    out->GetDesc(&desc);
    const float w = float(desc.Width), h = float(desc.Height);
    context->CopyResource(out, screen); // lo de afuera de la hoja, tal cual

    Constants c{};
    c.rect[0] = float(sheet.left) / w * 2 - 1;
    c.rect[1] = 1 - float(sheet.top) / h * 2;
    c.rect[2] = float(sheet.right) / w * 2 - 1;
    c.rect[3] = 1 - float(sheet.bottom) / h * 2;
    if (base) { // la textura de otra hoja: ocupa toda su textura
        c.baseUv[0] = c.baseUv[1] = 0;
        c.baseUv[2] = c.baseUv[3] = 1;
    } else { // la activa: la región de la hoja dentro de la imagen de pantalla
        c.baseUv[0] = float(sheet.left) / w;
        c.baseUv[1] = float(sheet.top) / h;
        c.baseUv[2] = float(sheet.right - sheet.left) / w;
        c.baseUv[3] = float(sheet.bottom - sheet.top) / h;
    }
    rgb(kPaperColor, c.paper);
    rgb(kGraphiteColor, c.graphite);
    ID3D11ShaderResourceView* views[1 + kMaxLayers] = {base ? base : screenView};
    int count = 0;
    for (const LightTableLayer& layer : layers) {
        if (count == kMaxLayers || !layer.sheet)
            continue;
        rgb(layer.color, c.tint[count]);
        c.tint[count][3] = std::clamp(layer.opacity, 0.0f, 1.0f);
        views[1 + count] = layer.sheet;
        ++count;
    }
    c.count = uint32_t(count);
    context->UpdateSubresource(m_constants.Get(), 0, nullptr, &c, 0, 0);

    ComPtr<ID3D11RenderTargetView> target;
    if (FAILED(m_device->CreateRenderTargetView(out, nullptr, &target)))
        return;
    const D3D11_VIEWPORT viewport{0, 0, w, h, 0, 1};
    context->RSSetViewports(1, &viewport);
    context->OMSetRenderTargets(1, target.GetAddressOf(), nullptr);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    context->VSSetShader(m_vs.Get(), nullptr, 0);
    context->VSSetConstantBuffers(0, 1, m_constants.GetAddressOf());
    context->PSSetShader(m_ps.Get(), nullptr, 0);
    context->PSSetConstantBuffers(0, 1, m_constants.GetAddressOf());
    context->PSSetShaderResources(0, 1 + kMaxLayers, views);
    context->PSSetSamplers(0, 1, m_sampler.GetAddressOf());
    context->Draw(4, 0);
    ID3D11ShaderResourceView* none[1 + kMaxLayers] = {};
    context->PSSetShaderResources(0, 1 + kMaxLayers, none);
    context->OMSetRenderTargets(0, nullptr, nullptr);
}

uint32_t Compositor::reference(uint32_t shown, const uint32_t* below, const LightTableLayer* layers, int count)
{
    float paper[4], graphite[4], color[4];
    rgb(kPaperColor, paper);
    rgb(kGraphiteColor, graphite);
    rgb(shown, color);
    const float range = ((paper[0] - graphite[0]) + (paper[1] - graphite[1]) + (paper[2] - graphite[2])) / 3;
    float factor[3] = {1, 1, 1};
    for (int i = 0; i < std::min(count, kMaxLayers); ++i) {
        float c[4], tint[4];
        rgb(below[i], c);
        rgb(layers[i].color, tint);
        const float amount = std::clamp(((paper[0] - c[0]) + (paper[1] - c[1]) + (paper[2] - c[2])) / 3 / range, 0.0f, 1.0f);
        const float k = amount * std::clamp(layers[i].opacity, 0.0f, 1.0f);
        for (int ch = 0; ch < 3; ++ch)
            factor[ch] *= 1 + (tint[ch] - 1) * k;
    }
    const auto byte = [](float v) { return uint32_t(std::lround(std::clamp(v, 0.0f, 1.0f) * 255)); };
    return 0xFF000000u | (byte(color[0] * factor[0]) << 16) | (byte(color[1] * factor[1]) << 8) | byte(color[2] * factor[2]);
}

} // namespace lienzo
