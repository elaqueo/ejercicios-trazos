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

#include <appkit/Log.h>
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

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Cartuchera"));
    appkit::installFileLog(QStringLiteral("cartuchera"));

    Shell shell;
    shell.setWindowFlag(Qt::FramelessWindowHint);
    shell.setWindowTitle(QApplication::applicationName());
    shell.setGeometry(QGuiApplication::primaryScreen()->geometry());
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
    const cartuchera::SheetMapping mapping =
        cartuchera::SheetMapping::fit(canvas.width(), canvas.height(), paper.width(), paper.height());

    cartuchera::DisplayImage image;
    image.width = canvas.width();
    image.height = canvas.height();
    image.pixels.resize(size_t(image.width) * size_t(image.height));

    cartuchera::Simulation sim(paper, queue, image, mapping);
    sim.renderAll();
    cartuchera::Renderer render(canvas.hwnd(), image);
    render.setExtraInfo([&sim] {
        wchar_t text[160];
        swprintf(text, 160, L"blandura HB %d ([ ])   ·   mina %.2f mm (, .)", sim.softness(), sim.leadDiameter() / 100.0);
        return std::wstring(text);
    });
    simulation = &sim;
    renderer = &render;
    qInfo() << "Hoja" << paper.width() << "x" << paper.height() << "celdas en" << mapping.sheetWidth << "x"
            << mapping.sheetHeight << "px (" << mapping.pixelsPerCell << "px por celda)";

    sim.start();
    render.start();
    const int result = QApplication::exec();
    sim.stop();
    render.stop();
    qInfo().noquote() << QString::fromStdString(render.summary()) << "· blandura final" << sim.softness() << "· mina"
                      << sim.leadDiameter() / 100.0 << "mm";
    return result;
}
