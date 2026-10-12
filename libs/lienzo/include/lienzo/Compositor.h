#pragma once

#include <windows.h>

#include <d3d11.h>
#include <wrl/client.h>

#include "lienzo/Tone.h"

#include <cstdint>
#include <vector>

namespace lienzo {

// Una hoja que se ve debajo de la que se muestra (mesa de luz, HU-83): su textura en la GPU
// (la región de la hoja), el color con que se tiñe su grafito y la opacidad.
struct LightTableLayer {
    ID3D11ShaderResourceView* sheet = nullptr;
    uint32_t color = 0; // BGRA, como los de Tone.h
    float opacity = 1;
};

// La marca de la esquina de la hoja (HU-104, rol y número): una textura chica cuya cobertura
// (el alfa) se pinta de `color` encima de la hoja que se muestra, en `rect` (píxeles desde la
// esquina de la hoja, del tamaño de la textura). Va antes del giro: gira con la hoja.
struct SheetMarkLayer {
    ID3D11ShaderResourceView* view = nullptr;
    RECT rect{};
    uint32_t color = 0; // BGRA
};

// Una guía del tamaño de la hoja (HU-103, guía de campo): su cobertura (el alfa) tiñe la hoja
// de `color` con `opacity`, multiplicada como una capa de la mesa de luz (queda bajo el
// grafito). Va antes del giro: gira con la hoja.
struct SheetGuideLayer {
    ID3D11ShaderResourceView* view = nullptr;
    uint32_t color = 0; // BGRA
    float opacity = 1;
};

// Arma la mesa de luz en la GPU (HU-83): la hoja que se muestra con las de abajo teñidas.
// De cada hoja de abajo se toma solo el grafito (cuánto más oscuro que el papel es cada
// píxel), se pinta de su color y se multiplica, como hojas sobre una mesa de luz; el papel
// sin trazo queda igual. La salida es del tamaño del cliente: lo de afuera de la hoja se
// copia de la imagen de pantalla. Corre en el hilo del render, con su device.
class Compositor {
public:
    static constexpr int kMaxLayers = 6;

    // false si no compilaron los shaders (sin mesa de luz; el render sigue andando).
    bool init(ID3D11Device* device);

    // screen: la imagen de pantalla (tamaño del cliente), de la que sale lo de afuera de la
    // hoja y, si base es nullptr, la hoja que se muestra (la activa). base: la textura de
    // otra hoja para mostrar en su lugar (el flip, HU-90). sheet: la hoja en píxeles del
    // cliente. out: textura del tamaño del cliente con D3D11_BIND_RENDER_TARGET.
    void compose(ID3D11DeviceContext* context, ID3D11Texture2D* screen, ID3D11ShaderResourceView* screenView,
                 ID3D11ShaderResourceView* base, const RECT& sheet, const std::vector<LightTableLayer>& layers,
                 ID3D11Texture2D* out, const SheetMarkLayer* mark = nullptr, const SheetGuideLayer* guide = nullptr);

    // Solo la marca (HU-104), sin girar ni capas: pinta su recuadro en `out` (que ya tiene la
    // imagen de pantalla copiada) leyendo la imagen de pantalla; el resto de out no se toca.
    // Mucho más barato que compose, que recorre toda la pantalla.
    void drawMark(ID3D11DeviceContext* context, ID3D11ShaderResourceView* screenView, const RECT& sheet,
                  const SheetMarkLayer& mark, ID3D11Texture2D* out);

    // La misma cuenta en la CPU, píxel por píxel (para las pruebas).
    static uint32_t reference(uint32_t shown, const uint32_t* below, const LightTableLayer* layers, int count,
                              uint32_t paperColor = kPaperColor);
    // El color de la hoja (HU-97): el tinte se calcula contra él.
    void setPaperColor(uint32_t color) { m_paperColor = color; }

private:
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_ps;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_markPs;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_constants;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampler;
    uint32_t m_paperColor = kPaperColor;
};

} // namespace lienzo
