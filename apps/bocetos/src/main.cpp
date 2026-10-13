// Bocetos (docs/bocetos/): la biblioteca de cuadernos de bocetos. Cada hoja se dibuja en
// Cartuchera (--hoja, HU-133) y vuelve al cuaderno terminada. Una app Qt común: no enlaza el
// lienzo (RNF-06).
//   Ctrl+, atajos · F10 monitor siguiente · Ctrl+Q sale
//   --biblioteca <carpeta>: los cuadernos de esa carpeta en lugar de Documentos\Trazos\Bocetos
//   (la prueba de la app real, para no tocar los del usuario).

#include "BocetosCore.h"
#include "LibraryWindow.h"

#include <appkit/Config.h>
#include <appkit/Log.h>
#include <appkit/ScreenChoice.h>
#include <appkit/Shortcuts.h>
#include <appkit/ShortcutsOverlay.h>

#include <QApplication>
#include <QScreen>

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
    window.setWindowFlag(Qt::FramelessWindowHint);
    window.setWindowTitle(QApplication::applicationName());
    const auto place = [&](QScreen* on) {
        window.setScreen(on);
        window.setGeometry(on->geometry());
    };
    place(screen);
    window.setLibrary(root, cartuchera); // con el ancho de la pantalla, para acomodar los libros

    // La biblioteca (HU-135): las flechas eligen, Enter abre.
    shortcuts.setGroup(QStringLiteral("Biblioteca"));
    shortcuts.add(QStringLiteral("Cuaderno anterior"), {{VK_LEFT}}, [&] { window.move(bocetos::Move::Left); });
    shortcuts.add(QStringLiteral("Cuaderno siguiente"), {{VK_RIGHT}}, [&] { window.move(bocetos::Move::Right); });
    shortcuts.add(QStringLiteral("Fila de arriba"), {{VK_UP}}, [&] { window.move(bocetos::Move::Up); });
    shortcuts.add(QStringLiteral("Fila de abajo"), {{VK_DOWN}}, [&] { window.move(bocetos::Move::Down); });
    shortcuts.add(QStringLiteral("Abrir el cuaderno"), {{VK_RETURN}}, [&] { window.openSelected(); });

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
    // F10 (HU-66): sin lienzo nativo, la ventana se muda sin reiniciar.
    shortcuts.add(QStringLiteral("Monitor siguiente"), {{VK_F10}}, [&] {
        QScreen* next = appkit::nextScreen(config, window.screen());
        if (next == window.screen())
            return;
        place(next);
        qInfo().noquote() << "Monitor siguiente:" << appkit::ScreenId::of(next).describe();
    });
    shortcuts.add(QStringLiteral("Salir"), {{'Q', true}}, [] { QApplication::quit(); });

    window.show();
    window.activateWindow();
    window.setFocus();
    return QApplication::exec();
}
