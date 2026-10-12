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
    uint hasMark;
    uint hasGuide;
    uint pad;
    float4 markRect;  // la marca (HU-104) en uv de la hoja: x0, y0, x1, y1
    float4 markColor; // rgb
    float4 guideColor; // la guía (HU-103): rgb; a: opacidad
    float4 chartRect;  // la carta de timing (HU-105), en px de la hoja: x0, y0, x1, y1
    float4 chartColor; // rgb; a: opacidad (0: sin carta)
    float4 sheetSize;  // px de la hoja: ancho, alto; z: grosor de la línea de la carta
};
Texture2D shown : register(t0);
Texture2D below[6] : register(t1);
Texture2D mark : register(t7);
Texture2D guide : register(t8);
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
    if (hasGuide != 0)
        factor *= lerp(float3(1, 1, 1), guideColor.rgb, guide.Sample(pointClamp, v.uv).a * guideColor.a);
    if (chartColor.a > 0) { // el contorno: dentro del rectángulo y a menos de `line` de un borde
        float2 p = v.uv * sheetSize.xy;
        float2 near = min(p - chartRect.xy, chartRect.zw - p);
        if (all(near >= 0) && min(near.x, near.y) < sheetSize.z)
            factor *= lerp(float3(1, 1, 1), chartColor.rgb, chartColor.a);
    }
    color *= factor;
    if (hasMark != 0) {
        float2 m = (v.uv - markRect.xy) / (markRect.zw - markRect.xy);
        if (all(m >= 0) && all(m < 1))
            color = lerp(color, markColor.rgb, mark.Sample(pointClamp, m).a);
    }
    return float4(color, 1);
}
// Solo la marca: rect es su recuadro, baseUv su región en la imagen de pantalla.
float4 psMark(V v) : SV_Target
{
    float3 color = shown.Sample(pointClamp, baseUv.xy + v.uv * baseUv.zw).rgb;
    return float4(lerp(color, markColor.rgb, mark.Sample(pointClamp, v.uv).a), 1);
}
// Solo la carta: rect es su recuadro, baseUv su región en la imagen de pantalla, sheetSize.xy
// su tamaño en px y z el grosor de la línea.
float4 psChart(V v) : SV_Target
{
    float3 color = shown.Sample(pointClamp, baseUv.xy + v.uv * baseUv.zw).rgb;
    float2 p = v.uv * sheetSize.xy;
    float2 near = min(p, sheetSize.xy - p);
    if (min(near.x, near.y) < sheetSize.z)
        color *= lerp(float3(1, 1, 1), chartColor.rgb, chartColor.a);
    return float4(color, 1);
}
)";

struct Constants {
    float rect[4];
    float baseUv[4];
    float paper[4];
    float graphite[4];
    float tint[Compositor::kMaxLayers][4];
    uint32_t count;
    uint32_t hasMark;
    uint32_t hasGuide;
    uint32_t pad;
    float markRect[4];
    float markColor[4];
    float guideColor[4];
    float chartRect[4];
    float chartColor[4];
    float sheetSize[4];
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
    ComPtr<ID3DBlob> vsCode, psCode, markCode, chartCode, errors;
    if (FAILED(D3DCompile(kLightTableShader, sizeof(kLightTableShader) - 1, "lighttable", nullptr, nullptr, "vs", "vs_5_0",
                          0, 0, &vsCode, &errors)) ||
        FAILED(D3DCompile(kLightTableShader, sizeof(kLightTableShader) - 1, "lighttable", nullptr, nullptr, "ps", "ps_5_0",
                          0, 0, &psCode, &errors)) ||
        FAILED(D3DCompile(kLightTableShader, sizeof(kLightTableShader) - 1, "lighttable", nullptr, nullptr, "psMark",
                          "ps_5_0", 0, 0, &markCode, &errors)) ||
        FAILED(D3DCompile(kLightTableShader, sizeof(kLightTableShader) - 1, "lighttable", nullptr, nullptr, "psChart",
                          "ps_5_0", 0, 0, &chartCode, &errors)))
        return false;
    device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(), nullptr, &m_vs);
    device->CreatePixelShader(psCode->GetBufferPointer(), psCode->GetBufferSize(), nullptr, &m_ps);
    device->CreatePixelShader(markCode->GetBufferPointer(), markCode->GetBufferSize(), nullptr, &m_markPs);
    device->CreatePixelShader(chartCode->GetBufferPointer(), chartCode->GetBufferSize(), nullptr, &m_chartPs);
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
    return m_vs && m_ps && m_markPs && m_chartPs && m_constants && m_sampler;
}

void Compositor::compose(ID3D11DeviceContext* context, ID3D11Texture2D* screen, ID3D11ShaderResourceView* screenView,
                         ID3D11ShaderResourceView* base, const RECT& sheet, const std::vector<LightTableLayer>& layers,
                         ID3D11Texture2D* out, const SheetMarkLayer* mark, const SheetGuideLayer* guide,
                         const SheetChartLayer* chart)
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
    rgb(m_paperColor, c.paper);
    rgb(kGraphiteColor, c.graphite);
    ID3D11ShaderResourceView* views[3 + kMaxLayers] = {base ? base : screenView};
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
    const float sheetW = float(sheet.right - sheet.left), sheetH = float(sheet.bottom - sheet.top);
    if (mark && mark->view && sheetW > 0 && sheetH > 0) {
        c.hasMark = 1;
        c.markRect[0] = float(mark->rect.left) / sheetW;
        c.markRect[1] = float(mark->rect.top) / sheetH;
        c.markRect[2] = float(mark->rect.right) / sheetW;
        c.markRect[3] = float(mark->rect.bottom) / sheetH;
        rgb(mark->color, c.markColor);
        views[1 + kMaxLayers] = mark->view;
    }
    if (guide && guide->view) { // HU-103
        c.hasGuide = 1;
        rgb(guide->color, c.guideColor);
        c.guideColor[3] = std::clamp(guide->opacity, 0.0f, 1.0f);
        views[2 + kMaxLayers] = guide->view;
    }
    if (chart && chart->rect.right > chart->rect.left && chart->rect.bottom > chart->rect.top) { // HU-105
        c.chartRect[0] = float(chart->rect.left);
        c.chartRect[1] = float(chart->rect.top);
        c.chartRect[2] = float(chart->rect.right);
        c.chartRect[3] = float(chart->rect.bottom);
        rgb(chart->color, c.chartColor);
        c.chartColor[3] = std::clamp(chart->opacity, 0.0f, 1.0f);
    }
    c.sheetSize[0] = sheetW;
    c.sheetSize[1] = sheetH;
    c.sheetSize[2] = chart ? std::max(1.0f, chart->line) : 1.0f;
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
    context->PSSetShaderResources(0, 3 + kMaxLayers, views);
    context->PSSetSamplers(0, 1, m_sampler.GetAddressOf());
    context->Draw(4, 0);
    ID3D11ShaderResourceView* none[3 + kMaxLayers] = {};
    context->PSSetShaderResources(0, 3 + kMaxLayers, none);
    context->OMSetRenderTargets(0, nullptr, nullptr);
}

void Compositor::drawMark(ID3D11DeviceContext* context, ID3D11ShaderResourceView* screenView, const RECT& sheet,
                          const SheetMarkLayer& mark, ID3D11Texture2D* out)
{
    if (!mark.view)
        return;
    D3D11_TEXTURE2D_DESC desc;
    out->GetDesc(&desc);
    const float w = float(desc.Width), h = float(desc.Height);
    const float left = float(sheet.left + mark.rect.left), top = float(sheet.top + mark.rect.top);
    const float right = float(sheet.left + mark.rect.right), bottom = float(sheet.top + mark.rect.bottom);
    Constants c{};
    c.rect[0] = left / w * 2 - 1;
    c.rect[1] = 1 - top / h * 2;
    c.rect[2] = right / w * 2 - 1;
    c.rect[3] = 1 - bottom / h * 2;
    c.baseUv[0] = left / w;
    c.baseUv[1] = top / h;
    c.baseUv[2] = (right - left) / w;
    c.baseUv[3] = (bottom - top) / h;
    rgb(mark.color, c.markColor);
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
    context->PSSetShader(m_markPs.Get(), nullptr, 0);
    context->PSSetConstantBuffers(0, 1, m_constants.GetAddressOf());
    ID3D11ShaderResourceView* views[2 + kMaxLayers] = {screenView};
    views[1 + kMaxLayers] = mark.view;
    context->PSSetShaderResources(0, 2 + kMaxLayers, views);
    context->PSSetSamplers(0, 1, m_sampler.GetAddressOf());
    context->Draw(4, 0);
    ID3D11ShaderResourceView* none[2 + kMaxLayers] = {};
    context->PSSetShaderResources(0, 2 + kMaxLayers, none);
    context->OMSetRenderTargets(0, nullptr, nullptr);
}

void Compositor::drawChart(ID3D11DeviceContext* context, ID3D11ShaderResourceView* screenView, const RECT& sheet,
                           const SheetChartLayer& chart, ID3D11Texture2D* out)
{
    if (chart.rect.right <= chart.rect.left || chart.rect.bottom <= chart.rect.top)
        return;
    D3D11_TEXTURE2D_DESC desc;
    out->GetDesc(&desc);
    const float w = float(desc.Width), h = float(desc.Height);
    const float left = float(sheet.left + chart.rect.left), top = float(sheet.top + chart.rect.top);
    const float right = float(sheet.left + chart.rect.right), bottom = float(sheet.top + chart.rect.bottom);
    Constants c{};
    c.rect[0] = left / w * 2 - 1;
    c.rect[1] = 1 - top / h * 2;
    c.rect[2] = right / w * 2 - 1;
    c.rect[3] = 1 - bottom / h * 2;
    c.baseUv[0] = left / w;
    c.baseUv[1] = top / h;
    c.baseUv[2] = (right - left) / w;
    c.baseUv[3] = (bottom - top) / h;
    rgb(chart.color, c.chartColor);
    c.chartColor[3] = std::clamp(chart.opacity, 0.0f, 1.0f);
    c.sheetSize[0] = right - left;
    c.sheetSize[1] = bottom - top;
    c.sheetSize[2] = std::max(1.0f, chart.line);
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
    context->PSSetShader(m_chartPs.Get(), nullptr, 0);
    context->PSSetConstantBuffers(0, 1, m_constants.GetAddressOf());
    ID3D11ShaderResourceView* views[1] = {screenView};
    context->PSSetShaderResources(0, 1, views);
    context->PSSetSamplers(0, 1, m_sampler.GetAddressOf());
    context->Draw(4, 0);
    ID3D11ShaderResourceView* none[1] = {};
    context->PSSetShaderResources(0, 1, none);
    context->OMSetRenderTargets(0, nullptr, nullptr);
}

uint32_t Compositor::reference(uint32_t shown, const uint32_t* below, const LightTableLayer* layers, int count,
                               uint32_t paperColor)
{
    float paper[4], graphite[4], color[4];
    rgb(paperColor, paper);
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
