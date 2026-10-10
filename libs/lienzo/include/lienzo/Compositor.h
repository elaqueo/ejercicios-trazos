#pragma once

#include <windows.h>

#include <d3d11.h>
#include <wrl/client.h>

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
                 ID3D11Texture2D* out);

    // La misma cuenta en la CPU, píxel por píxel (para las pruebas).
    static uint32_t reference(uint32_t shown, const uint32_t* below, const LightTableLayer* layers, int count);

private:
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_ps;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_constants;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampler;
};

} // namespace lienzo
