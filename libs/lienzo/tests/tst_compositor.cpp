#include <lienzo/Compositor.h>
#include <lienzo/Tone.h>

#include <QTest>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace lienzo;

namespace {

// Una textura BGRA con estos píxeles; bind: SRV y, si target, también render target.
struct Texture {
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> view;
};

Texture makeTexture(ID3D11Device* device, int w, int h, const std::vector<uint32_t>& pixels, bool target = false)
{
    D3D11_TEXTURE2D_DESC td{};
    td.Width = UINT(w);
    td.Height = UINT(h);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE | (target ? D3D11_BIND_RENDER_TARGET : 0);
    const D3D11_SUBRESOURCE_DATA init{pixels.data(), UINT(w) * 4, 0};
    Texture t;
    device->CreateTexture2D(&td, &init, &t.texture);
    device->CreateShaderResourceView(t.texture.Get(), nullptr, &t.view);
    return t;
}

std::vector<uint32_t> readBack(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* texture)
{
    D3D11_TEXTURE2D_DESC td;
    texture->GetDesc(&td);
    td.Usage = D3D11_USAGE_STAGING;
    td.BindFlags = 0;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    device->CreateTexture2D(&td, nullptr, &staging);
    context->CopyResource(staging.Get(), texture);
    D3D11_MAPPED_SUBRESOURCE mapped;
    context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    std::vector<uint32_t> out(size_t(td.Width) * td.Height);
    for (UINT y = 0; y < td.Height; ++y)
        std::memcpy(out.data() + size_t(y) * td.Width, static_cast<const uint8_t*>(mapped.pData) + size_t(y) * mapped.RowPitch,
                    td.Width * 4);
    context->Unmap(staging.Get(), 0);
    return out;
}

bool similar(uint32_t a, uint32_t b, int tolerance = 1)
{
    for (int shift : {0, 8, 16})
        if (std::abs(int((a >> shift) & 0xFF) - int((b >> shift) & 0xFF)) > tolerance)
            return false;
    return true;
}

constexpr uint32_t kRed = bgra(0xE0, 0x30, 0x30);
constexpr uint32_t kGreen = bgra(0x30, 0xB0, 0x40);

} // namespace

class TestCompositor : public QObject {
    Q_OBJECT

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    Compositor compositor;

    static constexpr int W = 64, H = 48;
    const RECT sheet{8, 4, 56, 44}; // 48 × 40
    static constexpr int SW = 48, SH = 40;

private slots:
    void initTestCase()
    {
        if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
                                     D3D11_SDK_VERSION, &device, nullptr, &context)))
            QSKIP("sin GPU con D3D11");
        QVERIFY(compositor.init(device.Get()));
    }

    // Sin capas ni otra hoja, la salida es la imagen de pantalla, byte a byte.
    void sinCapasEsLaImagen()
    {
        std::vector<uint32_t> screen(size_t(W) * H);
        for (size_t i = 0; i < screen.size(); ++i)
            screen[i] = 0xFF000000u | uint32_t(i * 2654435761u & 0xFFFFFF); // de todo un poco
        const Texture in = makeTexture(device.Get(), W, H, screen);
        const Texture out = makeTexture(device.Get(), W, H, std::vector<uint32_t>(size_t(W) * H), true);
        compositor.compose(context.Get(), in.texture.Get(), in.view.Get(), nullptr, sheet, {}, out.texture.Get());
        QCOMPARE(readBack(device.Get(), context.Get(), out.texture.Get()), screen);
    }

    // Una hoja de abajo: su grafito se tiñe de su color y se multiplica; su papel no cambia
    // nada. Igual que la cuenta de referencia, ±1. Lo de afuera de la hoja, intacto.
    void capasTenidas()
    {
        std::vector<uint32_t> screen(size_t(W) * H, kOutsideColor);
        for (int y = sheet.top; y < sheet.bottom; ++y)
            for (int x = sheet.left; x < sheet.right; ++x)
                screen[size_t(y) * W + x] = (x == 20 && y > 10) ? kGraphiteColor : kPaperColor; // un trazo de la activa
        std::vector<uint32_t> low(size_t(SW) * SH, kPaperColor), high(size_t(SW) * SH, kPaperColor);
        for (int x = 0; x < SW; ++x) {
            low[size_t(20) * SW + x] = kGraphiteColor;                // trazo lleno en la fila 20
            low[size_t(22) * SW + x] = toneOf(30000);                 // trazo a medias en la 22
            high[size_t(30) * SW + x] = kGraphiteColor;               // otra hoja, otra fila
        }
        const Texture in = makeTexture(device.Get(), W, H, screen);
        const Texture a = makeTexture(device.Get(), SW, SH, low), b = makeTexture(device.Get(), SW, SH, high);
        const Texture out = makeTexture(device.Get(), W, H, std::vector<uint32_t>(size_t(W) * H), true);
        const std::vector<LightTableLayer> layers{{a.view.Get(), kRed, 0.6f}, {b.view.Get(), kGreen, 0.4f}};
        compositor.compose(context.Get(), in.texture.Get(), in.view.Get(), nullptr, sheet, layers, out.texture.Get());
        const std::vector<uint32_t> got = readBack(device.Get(), context.Get(), out.texture.Get());

        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                const uint32_t px = got[size_t(y) * W + x];
                const bool inside = x >= sheet.left && x < sheet.right && y >= sheet.top && y < sheet.bottom;
                if (!inside) {
                    QCOMPARE(px, kOutsideColor);
                    continue;
                }
                const int sx = x - sheet.left, sy = y - sheet.top;
                const uint32_t below[2] = {low[size_t(sy) * SW + sx], high[size_t(sy) * SW + sx]};
                const uint32_t want = Compositor::reference(screen[size_t(y) * W + x], below, layers.data(), 2);
                QVERIFY2(similar(px, want), qPrintable(QStringLiteral("(%1, %2): %3 en vez de %4").arg(x).arg(y).arg(px, 8, 16).arg(want, 8, 16)));
            }
        // Donde las de abajo no tienen trazo, la hoja queda como estaba.
        QCOMPARE(got[size_t(sheet.top + 5) * W + sheet.left + 5], kPaperColor);
        // El trazo de abajo se ve teñido: más rojo que verde y que azul.
        const uint32_t tinted = got[size_t(sheet.top + 20) * W + sheet.left + 30];
        QVERIFY(((tinted >> 16) & 0xFF) > ((tinted >> 8) & 0xFF) + 40);
    }

    // Otra hoja en lugar de la activa (el flip): se ve su textura; afuera, la imagen.
    void otraHojaEnLugarDeLaActiva()
    {
        const std::vector<uint32_t> screen(size_t(W) * H, kPaperColor);
        std::vector<uint32_t> other(size_t(SW) * SH, kPaperColor);
        other[size_t(7) * SW + 9] = kGraphiteColor;
        const Texture in = makeTexture(device.Get(), W, H, screen);
        const Texture base = makeTexture(device.Get(), SW, SH, other);
        const Texture out = makeTexture(device.Get(), W, H, std::vector<uint32_t>(size_t(W) * H), true);
        compositor.compose(context.Get(), in.texture.Get(), in.view.Get(), base.view.Get(), sheet, {}, out.texture.Get());
        const std::vector<uint32_t> got = readBack(device.Get(), context.Get(), out.texture.Get());
        for (int sy = 0; sy < SH; ++sy)
            for (int sx = 0; sx < SW; ++sx)
                QCOMPARE(got[size_t(sheet.top + sy) * W + sheet.left + sx], other[size_t(sy) * SW + sx]);
    }

    // HU-104: la marca se pinta de su color según su cobertura, en su lugar de la hoja; el resto
    // queda igual.
    void marcaEncima()
    {
        const std::vector<uint32_t> screen(size_t(W) * H, kPaperColor);
        std::vector<uint32_t> cover(8 * 8, 0x00FFFFFFu);
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 4; ++x)
                cover[size_t(y) * 8 + x] = 0xFFFFFFFFu; // la mitad izquierda, tinta llena
        const Texture in = makeTexture(device.Get(), W, H, screen);
        const Texture mark = makeTexture(device.Get(), 8, 8, cover);
        const Texture out = makeTexture(device.Get(), W, H, std::vector<uint32_t>(size_t(W) * H), true);
        const SheetMarkLayer layer{mark.view.Get(), {10, 5, 18, 13}, kRed};
        compositor.compose(context.Get(), in.texture.Get(), in.view.Get(), nullptr, sheet, {}, out.texture.Get(), &layer);
        const std::vector<uint32_t> got = readBack(device.Get(), context.Get(), out.texture.Get());
        for (int sy = 0; sy < SH; ++sy)
            for (int sx = 0; sx < SW; ++sx) {
                const bool inked = sx >= 10 && sx < 14 && sy >= 5 && sy < 13;
                const uint32_t px = got[size_t(sheet.top + sy) * W + sheet.left + sx];
                QVERIFY2(similar(px, inked ? kRed : kPaperColor), qPrintable(QStringLiteral("(%1, %2): %3").arg(sx).arg(sy).arg(px, 8, 16)));
            }
    }

    // HU-104: drawMark pinta lo mismo que compose con la marca, pero solo en su recuadro.
    void marcaSola()
    {
        std::vector<uint32_t> screen(size_t(W) * H, kPaperColor);
        screen[size_t(sheet.top + 7) * W + sheet.left + 12] = kGraphiteColor; // grafito bajo la marca, sin tinta
        std::vector<uint32_t> cover(8 * 8, 0x00FFFFFFu);
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 4; ++x)
                cover[size_t(y) * 8 + x] = 0xFFFFFFFFu;
        const Texture in = makeTexture(device.Get(), W, H, screen);
        const Texture mark = makeTexture(device.Get(), 8, 8, cover);
        const Texture viaCompose = makeTexture(device.Get(), W, H, std::vector<uint32_t>(size_t(W) * H), true);
        const Texture viaMark = makeTexture(device.Get(), W, H, screen, true);
        const SheetMarkLayer layer{mark.view.Get(), {10, 5, 18, 13}, kRed};
        compositor.compose(context.Get(), in.texture.Get(), in.view.Get(), nullptr, sheet, {}, viaCompose.texture.Get(), &layer);
        compositor.drawMark(context.Get(), in.view.Get(), sheet, layer, viaMark.texture.Get());
        const std::vector<uint32_t> a = readBack(device.Get(), context.Get(), viaCompose.texture.Get());
        const std::vector<uint32_t> b = readBack(device.Get(), context.Get(), viaMark.texture.Get());
        for (size_t i = 0; i < a.size(); ++i)
            QVERIFY2(similar(a[i], b[i]), qPrintable(QStringLiteral("%1: %2 y %3").arg(i).arg(a[i], 8, 16).arg(b[i], 8, 16)));
        QCOMPARE(b[size_t(sheet.top + 7) * W + sheet.left + 12], kRed); // tinta encima del grafito
        QCOMPARE(b[size_t(sheet.top + 7) * W + sheet.left + 16], kPaperColor);
    }

    // HU-103: la guía multiplica la hoja donde tiene cobertura (el papel y el grafito), como una
    // capa de la mesa de luz; donde no tiene, la hoja queda igual.
    void guiaDebajo()
    {
        std::vector<uint32_t> screen(size_t(W) * H, kPaperColor);
        screen[size_t(sheet.top + 10) * W + sheet.left + 5] = kGraphiteColor; // grafito sobre la guía
        std::vector<uint32_t> cover(size_t(SW) * SH, 0x00FFFFFFu);
        for (int y = 0; y < SH; ++y)
            for (int x = 0; x < SW / 2; ++x)
                cover[size_t(y) * SW + x] = 0xFFFFFFFFu; // la mitad izquierda
        const Texture in = makeTexture(device.Get(), W, H, screen);
        const Texture guide = makeTexture(device.Get(), SW, SH, cover);
        const Texture out = makeTexture(device.Get(), W, H, std::vector<uint32_t>(size_t(W) * H), true);
        const SheetGuideLayer layer{guide.view.Get(), kRed, 0.5f};
        compositor.compose(context.Get(), in.texture.Get(), in.view.Get(), nullptr, sheet, {}, out.texture.Get(), nullptr,
                           &layer);
        const std::vector<uint32_t> got = readBack(device.Get(), context.Get(), out.texture.Get());
        const auto times = [](uint32_t c, uint32_t tint, float k) {
            uint32_t r = 0xFF000000u;
            for (int shift : {0, 8, 16}) {
                const float a = float((c >> shift) & 0xFF) / 255, t = float((tint >> shift) & 0xFF) / 255;
                r |= uint32_t(std::lround(a * (1 + (t - 1) * k) * 255)) << shift;
            }
            return r;
        };
        const auto at = [&](int sx, int sy) { return got[size_t(sheet.top + sy) * W + sheet.left + sx]; };
        QVERIFY(similar(at(3, 3), times(kPaperColor, kRed, 0.5f)));
        QVERIFY(similar(at(5, 10), times(kGraphiteColor, kRed, 0.5f)));
        QVERIFY(similar(at(SW - 3, 3), kPaperColor));
        QCOMPARE(got[0], kPaperColor); // fuera de la hoja, tal cual
    }

    // HU-105: la carta de timing tiñe solo el contorno de su rectángulo (de `line` px hacia
    // adentro); adentro y afuera la hoja queda igual.
    void cartaDeTiming()
    {
        const std::vector<uint32_t> screen(size_t(W) * H, kPaperColor);
        const Texture in = makeTexture(device.Get(), W, H, screen);
        const Texture out = makeTexture(device.Get(), W, H, std::vector<uint32_t>(size_t(W) * H), true);
        const SheetChartLayer chart{{4, 3, 20, 13}, 2, kRed, 1.0f};
        compositor.compose(context.Get(), in.texture.Get(), in.view.Get(), nullptr, sheet, {}, out.texture.Get(), nullptr,
                           nullptr, &chart);
        const std::vector<uint32_t> got = readBack(device.Get(), context.Get(), out.texture.Get());
        for (int sy = 0; sy < SH; ++sy)
            for (int sx = 0; sx < SW; ++sx) {
                const bool inRect = sx >= 4 && sx < 20 && sy >= 3 && sy < 13;
                const bool edge = inRect && (sx < 6 || sx >= 18 || sy < 5 || sy >= 11);
                const uint32_t px = got[size_t(sheet.top + sy) * W + sheet.left + sx];
                const uint32_t want = edge ? bgra(uint8_t(0xF5 * 0xE0 / 255.0 + 0.5), uint8_t(0xF0 * 0x30 / 255.0 + 0.5),
                                                  uint8_t(0xE6 * 0x30 / 255.0 + 0.5))
                                           : kPaperColor;
                QVERIFY2(similar(px, want, 2), qPrintable(QStringLiteral("(%1, %2): %3").arg(sx).arg(sy).arg(px, 8, 16)));
            }
    }

    // HU-105: drawChart pinta lo mismo que compose con la carta, pero solo en su recuadro.
    void cartaSola()
    {
        std::vector<uint32_t> screen(size_t(W) * H, kPaperColor);
        screen[size_t(sheet.top + 3) * W + sheet.left + 8] = kGraphiteColor; // grafito sobre el borde
        const Texture in = makeTexture(device.Get(), W, H, screen);
        const Texture viaCompose = makeTexture(device.Get(), W, H, std::vector<uint32_t>(size_t(W) * H), true);
        const Texture viaChart = makeTexture(device.Get(), W, H, screen, true);
        const SheetChartLayer chart{{4, 3, 20, 13}, 2, kRed, 0.45f};
        compositor.compose(context.Get(), in.texture.Get(), in.view.Get(), nullptr, sheet, {}, viaCompose.texture.Get(),
                           nullptr, nullptr, &chart);
        compositor.drawChart(context.Get(), in.view.Get(), sheet, chart, viaChart.texture.Get());
        const std::vector<uint32_t> a = readBack(device.Get(), context.Get(), viaCompose.texture.Get());
        const std::vector<uint32_t> b = readBack(device.Get(), context.Get(), viaChart.texture.Get());
        for (size_t i = 0; i < a.size(); ++i)
            QVERIFY2(similar(a[i], b[i]), qPrintable(QStringLiteral("%1: %2 y %3").arg(i).arg(a[i], 8, 16).arg(b[i], 8, 16)));
    }

    // Seis hojas de abajo a tamaño real (1734 × 1080) en menos de 8 ms de GPU.
    void seisCapasATamanoReal()
    {
        constexpr int cw = 2560, ch = 1080;
        const RECT big{0, 0, 1734, 1080};
        const Texture in = makeTexture(device.Get(), cw, ch, std::vector<uint32_t>(size_t(cw) * ch, kPaperColor));
        std::vector<uint32_t> drawn(size_t(1734) * 1080, kPaperColor);
        for (size_t i = 0; i < drawn.size(); i += 7)
            drawn[i] = kGraphiteColor;
        std::vector<Texture> sheets;
        std::vector<LightTableLayer> layers;
        for (int i = 0; i < Compositor::kMaxLayers; ++i) {
            sheets.push_back(makeTexture(device.Get(), 1734, 1080, drawn));
            layers.push_back({sheets.back().view.Get(), i % 2 ? kGreen : kRed, 0.5f});
        }
        const Texture out = makeTexture(device.Get(), cw, ch, std::vector<uint32_t>(size_t(cw) * ch), true);

        D3D11_QUERY_DESC qd{D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
        ComPtr<ID3D11Query> disjoint, t0, t1;
        device->CreateQuery(&qd, &disjoint);
        qd.Query = D3D11_QUERY_TIMESTAMP;
        device->CreateQuery(&qd, &t0);
        device->CreateQuery(&qd, &t1);
        double best = 1e9;
        for (int run = 0; run < 10; ++run) {
            context->Begin(disjoint.Get());
            context->End(t0.Get());
            compositor.compose(context.Get(), in.texture.Get(), in.view.Get(), nullptr, big, layers, out.texture.Get());
            context->End(t1.Get());
            context->End(disjoint.Get());
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj{};
            while (context->GetData(disjoint.Get(), &dj, sizeof(dj), 0) == S_FALSE) {
            }
            UINT64 a = 0, b = 0;
            context->GetData(t0.Get(), &a, sizeof(a), 0);
            context->GetData(t1.Get(), &b, sizeof(b), 0);
            if (!dj.Disjoint)
                best = std::min(best, double(b - a) * 1000.0 / double(dj.Frequency));
        }
        qInfo() << "mesa de luz, 6 hojas a 1734 × 1080:" << best << "ms de GPU";
        QVERIFY(best < 8);
    }

};

QTEST_GUILESS_MAIN(TestCompositor)
#include "tst_compositor.moc"
