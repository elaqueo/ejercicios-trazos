// Bocetos (docs/bocetos/): la biblioteca de cuadernos de bocetos, en una ventana común. Cada hoja se dibuja en
// Cartuchera (--hoja, HU-133) y vuelve al cuaderno terminada. Una app Qt común: no enlaza el
// lienzo (RNF-06).
//   Ctrl+, atajos · F10 monitor siguiente · Ctrl+Q sale
//   --biblioteca <carpeta>: los cuadernos de esa carpeta en lugar de Documentos\Trazos\Bocetos
//   (la prueba de la app real, para no tocar los del usuario).

#include "BocetosCore.h"
#include "LibraryWindow.h"
#include "NotebookView.h"

#include <appkit/Config.h>
#include <appkit/Log.h>
#include <appkit/ScreenChoice.h>
#include <appkit/Shortcuts.h>
#include <appkit/ShortcutsOverlay.h>

#include <QApplication>
#include <QScreen>
#include <QStyle>

#include <algorithm>

#include <windows.h>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Bocetos"));
    appkit::installFileLog(QStringLiteral("bocetos"));

    // El monitor de la familia (sección común de config.json), el que se elige con F10 en
    // cualquiera de las apps.
    appkit::Config config(QStringLiteral("bocetos"));
    QScreen* screen = appkit::savedScreen(config);

    const QString root = bocetos::libraryRoot(QApplication::arguments());
    const QString cartuchera = bocetos::findCartuchera(QApplication::applicationDirPath());
    qInfo().noquote() << QStringLiteral("Trazos %1 · Bocetos · escala de Windows %2").arg(QStringLiteral(ET_VERSION)).arg(screen->devicePixelRatio());
    qInfo().noquote() << QStringLiteral("Bocetos: biblioteca %1").arg(root);
    if (cartuchera.isEmpty())
        qWarning() << "Bocetos: no está cartuchera.exe junto a bocetos.exe";
    else
        qInfo().noquote() << QStringLiteral("Bocetos: Cartuchera en %1").arg(cartuchera);

    appkit::Shortcuts shortcuts;
    LibraryWindow window(shortcuts, config);
    window.setWindowTitle(QApplication::applicationName());
    window.setMinimumSize(800, 560);
    // Una ventana común, no a pantalla completa (pedido del usuario del 13 de octubre de 2026): con
    // la posición y el tamaño de la última vez ("geometry" en config.json); la primera vez, centrada
    // en el monitor de la familia, a tres cuartos de él.
    const auto place = [&](QScreen* on, QSize size) {
        const QRect area = on->availableGeometry();
        window.setScreen(on);
        window.setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size.boundedTo(area.size()), area));
    };
    if (!window.restoreGeometry(QByteArray::fromBase64(config.value(QStringLiteral("geometry")).toByteArray()))) {
        const QRect area = screen->availableGeometry();
        place(screen, QSize(std::max(800, area.width() * 3 / 4), std::max(560, area.height() * 3 / 4)));
    }
    QObject::connect(&app, &QApplication::aboutToQuit, [&] {
        config.setValue(QStringLiteral("geometry"), QString::fromLatin1(window.saveGeometry().toBase64()));
    });
    window.setLibrary(root, cartuchera); // con el ancho de la ventana, para acomodar los libros

    // La biblioteca (HU-135): las flechas eligen, Enter abre.
    shortcuts.setGroup(QStringLiteral("Biblioteca"));
    shortcuts.add(QStringLiteral("Cuaderno anterior"), {{VK_LEFT}}, [&] { window.move(bocetos::Move::Left); });
    shortcuts.add(QStringLiteral("Cuaderno siguiente"), {{VK_RIGHT}}, [&] { window.move(bocetos::Move::Right); });
    shortcuts.add(QStringLiteral("Fila de arriba"), {{VK_UP}}, [&] { window.move(bocetos::Move::Up); });
    shortcuts.add(QStringLiteral("Fila de abajo"), {{VK_DOWN}}, [&] { window.move(bocetos::Move::Down); });
    shortcuts.add(QStringLiteral("Abrir el cuaderno"), {{VK_RETURN}}, [&] { window.openSelected(); });
    // HU-136: crear, editar, mover de colección y borrar (a la papelera).
    shortcuts.add(QStringLiteral("Cuaderno nuevo"), {{'N', true}}, [&] { window.newNotebook(); });
    shortcuts.add(QStringLiteral("Editar el cuaderno"), {{VK_F2}}, [&] { window.editSelected(); });
    shortcuts.add(QStringLiteral("Mover a otra colección"), {{'M', true}}, [&] { window.editSelected(true); });
    shortcuts.add(QStringLiteral("Borrar el cuaderno"), {{VK_DELETE}}, [&] { window.deleteSelected(); });

    // El cuaderno abierto (HU-137): pasar hojas, zoom y volver.
    shortcuts.setGroup(QStringLiteral("Cuaderno"));
    const auto onBook = [&](auto action) {
        return [&window, action] {
            if (NotebookView* view = window.openNotebook())
                action(*view);
        };
    };
    shortcuts.add(QStringLiteral("Hoja anterior"), {{VK_PRIOR}}, onBook([](NotebookView& v) { v.step(-1); }));
    shortcuts.add(QStringLiteral("Hoja siguiente"), {{VK_NEXT}}, onBook([](NotebookView& v) { v.step(1); }));
    shortcuts.joinWithPrevious(QStringLiteral("Pasar de hoja"));
    shortcuts.add(QStringLiteral("Primera hoja"), {{VK_HOME}}, onBook([](NotebookView& v) { v.first(); }));
    shortcuts.add(QStringLiteral("Última hoja"), {{VK_END}}, onBook([](NotebookView& v) { v.last(); }));
    shortcuts.joinWithPrevious(QStringLiteral("Primera o última hoja"));
    shortcuts.add(QStringLiteral("Acercar"), {{VK_OEM_PLUS}, {VK_ADD}}, onBook([](NotebookView& v) { v.zoomIn(); }));
    shortcuts.add(QStringLiteral("Alejar"), {{VK_OEM_MINUS}, {VK_SUBTRACT}}, onBook([](NotebookView& v) { v.zoomOut(); }));
    shortcuts.joinWithPrevious(QStringLiteral("Acercar y alejar"));
    shortcuts.note(QStringLiteral("Arrastrar"), QStringLiteral("Mover la hoja acercada"));
    shortcuts.add(QStringLiteral("Volver a la biblioteca"), {{VK_ESCAPE}}, [&] { window.closeNotebook(); });

    // Ctrl+, (HU-78): la lista sale del registro, centrada en la pantalla.
    appkit::ShortcutsOverlay overlay(&window);
    overlay.onClose = [&] {
        qInfo() << "Atajos: cerrados";
        window.activateWindow();
        window.setFocus();
    };
    shortcuts.setGroup(QStringLiteral("Pantalla"));
    shortcuts.add(QStringLiteral("Atajos"), {{VK_OEM_COMMA, true}}, [&] {
        overlay.setSheet(QApplication::applicationName(), shortcuts.sheet(), window.size());
        overlay.move(window.mapToGlobal(window.rect().center() - QPoint(overlay.width() / 2, overlay.height() / 2)));
        overlay.show();
        overlay.raise();
        overlay.activateWindow();
        overlay.setFocus();
        qInfo() << "Atajos: abiertos";
    });
    // F10 (HU-66): la ventana se muda, centrada, al monitor siguiente, sin reiniciar.
    shortcuts.add(QStringLiteral("Monitor siguiente"), {{VK_F10}}, [&] {
        QScreen* next = appkit::nextScreen(config, window.screen());
        if (next == window.screen())
            return;
        const bool maximized = window.isMaximized();
        window.showNormal();
        place(next, window.size());
        if (maximized)
            window.showMaximized();
        qInfo().noquote() << "Monitor siguiente:" << appkit::ScreenId::of(next).describe();
    });
    shortcuts.add(QStringLiteral("Salir"), {{'Q', true}}, [] { QApplication::quit(); });

    window.show();
    window.activateWindow();
    window.setFocus();
    return QApplication::exec();
}
