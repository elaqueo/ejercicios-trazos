// Lámina de comparación del papel de animación (HU-97): lo mismo dibujado en el papel de
// Cartuchera (600 dpi) y en el de Mesa de animación (300 dpi) con varias profundidades de
// grano, a la escala de la pantalla (100 %) y al doble (200 %). Sin textura visual en
// ninguno, para comparar la línea. Escribe un PNG. Código descartable.
//   mesa-papel <salida.png> [dpi:grano ...]

#include <appkit/Theme.h>
#include <drymedia/Paper.h>
#include <drymedia/Pencil.h>
#include <lienzo/Media.h>
#include <lienzo/SheetMapping.h>
#include <lienzo/Tone.h>

#include <QGuiApplication>
#include <QImage>
#include <QPainter>

#include <cmath>
#include <cstdio>
#include <numbers>
#include <vector>

namespace {

constexpr double kSheetWidthMm = 46, kSheetHeightMm = 30;
constexpr double kScreenPxPerMm = 1734.0 / 325.1; // la hoja de la desktop: el área útil sobre la tableta

struct Variant {
    const char* title;
    int dpi;
    int grain;
    lienzo::MediumProfile profile;
    QColor paper;
};

void stroke(drymedia::Pencil& pencil, double c, auto at, int samples)
{
    pencil.beginStroke(at(0.0, c));
    for (int i = 1; i <= samples; ++i)
        pencil.strokeTo(at(double(i) / samples, c));
    pencil.endStroke();
}

QImage sheet(const Variant& v, double pxPerMm)
{
    drymedia::Paper paper({.seed = 11, .widthMm = kSheetWidthMm, .heightMm = kSheetHeightMm, .dpi = v.dpi, .grain = v.grain});
    const lienzo::Lead hb = lienzo::factoryGrades()[lienzo::kHbIndex];
    drymedia::Pencil pencil(paper, lienzo::forProfile(hb.medium(), v.profile));
    const double c = paper.cellsPerMm();
    using S = drymedia::PencilSample;
    // Línea firme, línea liviana y una curva de boceto con la presión que sube y baja.
    stroke(pencil, c, [](double t, double k) { return S{(4 + 38 * t) * k, 5 * k, 0.6f, 0.0f, 90.0f}; }, 160);
    stroke(pencil, c, [](double t, double k) { return S{(4 + 38 * t) * k, 9 * k, 0.25f, 0.0f, 90.0f}; }, 160);
    stroke(pencil, c,
           [](double t, double k) {
               return S{(4 + 38 * t) * k, (15 + 3 * std::sin(t * 2 * std::numbers::pi)) * k,
                        float(0.15 + 0.55 * std::sin(t * std::numbers::pi)), 30.0f, 70.0f};
           },
           200);
    // Sombreado de costado: zigzag con el lápiz inclinado.
    stroke(pencil, c,
           [](double t, double k) {
               const double x = 4 + 24 * t;
               const double y = 21 + 6 * std::abs(std::fmod(t * 14, 2.0) - 1);
               return S{x * k, y * k, 0.45f, 300.0f, 40.0f};
           },
           600);

    const int w = int(std::lround(kSheetWidthMm * pxPerMm)), h = int(std::lround(kSheetHeightMm * pxPerMm));
    const lienzo::SheetMapping m = lienzo::SheetMapping::fit(w, h, paper.width(), paper.height());
    std::vector<uint32_t> pixels(size_t(w) * size_t(h));
    lienzo::renderTone(paper, m, 0, 0, w, h, pixels.data(), nullptr, nullptr,
                       lienzo::bgra(v.paper.red(), v.paper.green(), v.paper.blue()));
    return QImage(reinterpret_cast<const uchar*>(pixels.data()), w, h, QImage::Format_ARGB32).copy();
}

} // namespace

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    const char* out = argc > 1 ? argv[1] : "papel.png";
    namespace theme = appkit::theme;
    // Variantes por línea de comandos: dpi:grano ... (sin argumentos, la primera lámina).
    std::vector<Variant> variants{{"Cartuchera · 600 dpi", 600, 2047, lienzo::MediumProfile::Illustration, theme::kHoja}};
    std::vector<QByteArray> titles;
    for (int a = 2; a < argc; ++a) {
        const QList<QByteArray> parts = QByteArray(argv[a]).split(':');
        titles.push_back("Animación · " + parts.value(0) + " dpi · grano " + parts.value(1));
    }
    for (int a = 2; a < argc; ++a) {
        const QList<QByteArray> parts = QByteArray(argv[a]).split(':');
        variants.push_back({titles[size_t(a - 2)].constData(), parts.value(0).toInt(), parts.value(1).toInt(),
                            lienzo::MediumProfile::Animation, theme::kHojaAnimacion});
    }
    const int gap = 24, label = 28;
    std::vector<std::vector<QImage>> grid; // [escala][variante]
    for (const double scale : {1.0, 2.0}) {
        grid.emplace_back();
        for (const Variant& v : variants)
            grid.back().push_back(sheet(v, kScreenPxPerMm * scale));
    }
    int width = gap, height = gap;
    for (const QImage& img : grid[1])
        width += img.width() + gap;
    for (const auto& row : grid)
        height += label + row[0].height() + gap;
    height += label;
    QImage page(width, height, QImage::Format_ARGB32);
    page.fill(theme::kFondo);
    QPainter p(&page);
    QFont f(QStringLiteral("Segoe UI"));
    f.setPixelSize(15);
    p.setFont(f);
    int y = gap;
    const char* scales[] = {"100 % (como en pantalla)", "200 %"};
    for (size_t r = 0; r < grid.size(); ++r) {
        p.setPen(theme::kTextoSecundario);
        p.drawText(gap, y + 18, QString::fromUtf8(scales[r]));
        y += label;
        int x = gap;
        for (size_t i = 0; i < variants.size(); ++i) {
            p.drawImage(x, y, grid[r][i]);
            if (r == 0) {
                p.setPen(theme::kTexto);
                p.drawText(x, y + grid[r][i].height() + 18, QString::fromUtf8(variants[i].title));
            }
            x += grid[1][i].width() + gap;
        }
        y += grid[r][0].height() + gap + (r == 0 ? label : 0);
    }
    p.end();
    const bool ok = page.save(QString::fromLocal8Bit(out));
    std::printf("%s %s (%d x %d)\n", ok ? "Escrita" : "No se pudo escribir", out, width, height);
    return ok ? 0 : 1;
}
