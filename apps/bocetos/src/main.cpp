// Bocetos (docs/bocetos/): la biblioteca de cuadernos de bocetos, en una ventana común. Cada hoja se dibuja en
// Cartuchera (--hoja, HU-133) y vuelve al cuaderno terminada. Una app Qt común: no enlaza el
// lienzo (RNF-06).
//   Ctrl+, atajos · F10 monitor siguiente · Ctrl+Q sale
//   --biblioteca <carpeta>: los cuadernos de esa carpeta en lugar de Documentos\Trazos\Bocetos
//   (la prueba de la app real, para no tocar los del usuario).
//   --tv <url>: T usa ese TV (la descripción UPnP) sin buscar en la red (el TV de mentira, HU-145).

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
    // La ventana común se guarda al salir; a pantalla completa (F11), la de antes de entrar.
    const auto saveGeometry = [&] { config.setValue(QStringLiteral("geometry"), QString::fromLatin1(window.saveGeometry().toBase64())); };
    QObject::connect(&app, &QApplication::aboutToQuit, [&] {
        if (!window.isFullScreen())
            saveGeometry();
    });
    window.setLibrary(root, cartuchera); // con el ancho de la ventana, para acomodar los libros
    if (const QUrl tv = bocetos::tvArgument(QApplication::arguments()); tv.isValid()) {
        window.setTvForced(tv);
        qInfo().noquote() << QStringLiteral("Bocetos: TV forzado en %1").arg(tv.toString());
    }

    // La biblioteca (HU-135): las flechas eligen, Enter abre.
    shortcuts.setGroup(QStringLiteral("Biblioteca"));
    shortcuts.add(QStringLiteral("Cuaderno anterior"), {{VK_LEFT}}, [&] { window.move(bocetos::Move::Left); });
    shortcuts.add(QStringLiteral("Cuaderno siguiente"), {{VK_RIGHT}}, [&] { window.move(bocetos::Move::Right); });
    shortcuts.add(QStringLiteral("Fila de arriba"), {{VK_UP}}, [&] { window.move(bocetos::Move::Up); });
    shortcuts.add(QStringLiteral("Fila de abajo"), {{VK_DOWN}}, [&] { window.move(bocetos::Move::Down); });
    shortcuts.add(QStringLiteral("Abrir el cuaderno"), {{VK_RETURN}}, [&] { window.openSelected(); });
    // HU-138: en la biblioteca y en el cuaderno abierto.
    shortcuts.add(QStringLiteral("Dibujar la siguiente hoja"), {{'D'}}, [&] { window.draw(); });
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
    // HU-145: la hoja en el TV de la red (DLNA).
    shortcuts.add(QStringLiteral("Mostrar en el TV"), {{'T'}}, [&] { window.toggleTv(); });

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
        const bool maximized = window.isMaximized(), full = window.isFullScreen();
        window.showNormal();
        place(next, window.size());
        if (full)
            window.showFullScreen();
        else if (maximized)
            window.showMaximized();
        qInfo().noquote() << "Monitor siguiente:" << appkit::ScreenId::of(next).describe();
    });
    // F11 (pedido del usuario del 13 de octubre de 2026): pantalla completa, y vuelta a la ventana
    // común como estaba; queda para la próxima vez. Esc no sale: en el cuaderno vuelve a la biblioteca.
    shortcuts.add(QStringLiteral("Pantalla completa"), {{VK_F11}}, [&] {
        const bool full = !window.isFullScreen();
        if (full) {
            saveGeometry();
            window.showFullScreen();
        } else {
            window.showNormal();
        }
        config.setValue(QStringLiteral("fullscreen"), full);
        qInfo().noquote() << (full ? "Bocetos: pantalla completa" : "Bocetos: ventana común");
    });
    shortcuts.add(QStringLiteral("Salir"), {{'Q', true}}, [] { QApplication::quit(); });

    if (config.value(QStringLiteral("fullscreen")).toBool())
        window.showFullScreen();
    else
        window.show();
    window.activateWindow();
    window.setFocus();
    return QApplication::exec();
}
