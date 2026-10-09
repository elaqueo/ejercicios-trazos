// Cartuchera: dibujo con medios secos (docs/medios-secos/arquitectura.md). Fase 1:
// grafito HB sobre una hoja A4, sin interfaz.
//   Alt+F4 sale · Ctrl+N hoja nueva · F3 latencia en vivo · [ y ] blandura · , y . diámetro
//   de la mina (calibración)

#include "CanvasWindow.h"
#include "DisplayImage.h"
#include "Renderer.h"
#include "SampleQueue.h"
#include "SheetMapping.h"
#include "Simulation.h"

#include <appkit/Config.h>
#include <appkit/Log.h>
#include <appkit/ScreenChoice.h>
#include <appkit/UsableArea.h>
#include <drymedia/Paper.h>

#include <QApplication>
#include <QKeyEvent>
#include <QScreen>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <functional>

namespace {

// Cascarón Qt: sin bordes, cubre el monitor principal. Las teclas que llegan acá (cuando
// Qt tiene el foco) van al mismo manejador que las del lienzo.
class Shell : public QWidget {
public:
    std::function<void(UINT, bool)> onKey;

protected:
    void keyPressEvent(QKeyEvent* event) override
    {
        if (onKey)
            onKey(UINT(event->nativeVirtualKey()), event->modifiers().testFlag(Qt::ControlModifier));
    }
};

// Superficie activa de la Wacom Intuos4 Large (PTK-840), según el plan.
constexpr double kTabletWidthMm = 325.1;
constexpr double kTabletHeightMm = 203.2;

// El monitor guardado ("monitor", sección común) si está conectado; si no, el principal.
QScreen* savedScreen(const appkit::Config& config)
{
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (const auto saved =
            appkit::ScreenId::fromVariant(config.value(QStringLiteral("monitor"), {}, appkit::Config::Scope::Common))) {
        QList<appkit::ScreenId> ids;
        for (const QScreen* s : screens)
            ids.append(appkit::ScreenId::of(s));
        if (const auto index = appkit::findScreen(ids, *saved))
            return screens[*index];
    }
    return QGuiApplication::primaryScreen();
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Cartuchera"));
    appkit::installFileLog(QStringLiteral("cartuchera"));

    // Monitor y área útil: los de la familia de apps (sección común de config.json), los
    // mismos que se eligen y calibran en Ejercicios (F10 y F9).
    appkit::Config config(QStringLiteral("cartuchera"));
    QScreen* screen = savedScreen(config);
    const std::optional<QRect> area = appkit::loadUsableArea(config, appkit::ScreenId::of(screen));

    Shell shell;
    shell.setWindowFlag(Qt::FramelessWindowHint);
    shell.setWindowTitle(QApplication::applicationName());
    shell.setScreen(screen); // antes de crear la ventana nativa, o Windows elige el monitor
    shell.setGeometry(screen->geometry());
    shell.show();

    drymedia::Paper paper; // A4 apaisado recortado, 297 × 203 mm
    cartuchera::SampleQueue queue;
    cartuchera::Simulation* simulation = nullptr;
    cartuchera::Renderer* renderer = nullptr;

    const auto onKey = [&](UINT vk, bool ctrl) {
        if (ctrl && vk == 'N' && simulation)
            simulation->requestClear();
        else if (vk == VK_F3 && renderer)
            renderer->toggleOverlay();
        else if ((vk == VK_OEM_4 || vk == VK_OEM_6) && simulation) { // [ y ]
            const int s = simulation->softness();
            const int next = vk == VK_OEM_6 ? std::max(s + 1, int(std::lround(s * 1.25))) : int(std::lround(s / 1.25));
            simulation->setSoftness(std::clamp(next, 1, 255));
        } else if ((vk == VK_OEM_COMMA || vk == VK_OEM_PERIOD) && simulation) { // , y .
            // Diámetro de la mina en centésimas de mm: 0,30 a 2,00 mm (la punta mide 48 celdas).
            const int d = simulation->leadDiameter();
            const int next = vk == VK_OEM_PERIOD ? int(std::lround(d * 1.15)) : int(std::lround(d / 1.15));
            simulation->setLeadDiameter(std::clamp(next, 30, 200));
        }
    };
    shell.onKey = onKey;

    cartuchera::CanvasWindow canvas(reinterpret_cast<HWND>(shell.winId()), queue, onKey);
    // Hoja = tableta (HU-52): el área útil corresponde a toda la superficie activa, y la
    // hoja va centrada encima, a escala real. Sin área calibrada, ajustada a la ventana.
    const cartuchera::SheetMapping mapping =
        area ? cartuchera::SheetMapping::onTablet(canvas.width(), canvas.height(), area->x(), area->y(), area->width(),
                                                  area->height(), kTabletWidthMm, kTabletHeightMm,
                                                  paper.spec().widthMm, paper.spec().heightMm, paper.width(),
                                                  paper.height())
             : cartuchera::SheetMapping::fit(canvas.width(), canvas.height(), paper.width(), paper.height());

    cartuchera::DisplayImage image;
    image.width = canvas.width();
    image.height = canvas.height();
    image.pixels.resize(size_t(image.width) * size_t(image.height));

    cartuchera::Simulation sim(paper, queue, image, mapping);
    sim.renderAll();
    cartuchera::Renderer render(canvas.hwnd(), image);
    const bool calibrated = area.has_value();
    render.setExtraInfo([&sim, calibrated] {
        wchar_t text[256];
        swprintf(text, 256, L"blandura HB %d ([ ])   ·   mina %.2f mm (, .)%ls", sim.softness(),
                 sim.leadDiameter() / 100.0,
                 calibrated ? L"" : L"\nSin área calibrada en este monitor: calibrala con F9 en Ejercicios.");
        return std::wstring(text);
    });
    if (!calibrated)
        render.setOverlay(true); // el aviso tiene que verse sin apretar nada
    simulation = &sim;
    renderer = &render;
    qInfo() << "Monitor" << appkit::ScreenId::of(screen).describe() << (calibrated ? "· área útil" : "· SIN área útil")
            << (area ? *area : QRect());
    qInfo() << "Hoja" << paper.width() << "x" << paper.height() << "celdas en" << mapping.sheetWidth << "x"
            << mapping.sheetHeight << "px desde" << mapping.sheetX << mapping.sheetY << "(" << mapping.pixelsPerCellX
            << "x" << mapping.pixelsPerCellY << "px por celda)";

    sim.start();
    render.start();
    const int result = QApplication::exec();
    sim.stop();
    render.stop();
    qInfo().noquote() << QString::fromStdString(render.summary()) << "· blandura final" << sim.softness() << "· mina"
                      << sim.leadDiameter() / 100.0 << "mm";
    return result;
}
