#pragma once

#include "BocetosCore.h"

#include <appkit/Config.h>
#include <appkit/Shortcuts.h>

#include <QString>
#include <QWidget>

class NotebookDialog;
class NotebookView;
namespace appkit {
class ConfirmOverlay;
}

// La biblioteca de Bocetos (HU-135, mesa "Bocetos · biblioteca"): una ventana común con los
// cuadernos como libros apaisados en estantes por colección. Las flechas eligen, Enter
// o tocar abre el cuaderno (HU-137) encima, y Esc vuelve. Las teclas van al registro de atajos de
// appkit; las de la biblioteca no hacen nada con el cuaderno abierto, y al revés.
class LibraryWindow : public QWidget {
public:
    LibraryWindow(appkit::Shortcuts& shortcuts, appkit::Config& config);

    // Lee la biblioteca de `root` (queda en el log cuánto tardó, RNF-01) y elige el último
    // cuaderno elegido.
    void setLibrary(const QString& root, const QString& cartuchera);

    void move(bocetos::Move move);
    void openSelected();

    // HU-137: el cuaderno abierto, o nullptr si se ve la biblioteca.
    NotebookView* openNotebook() const;
    void closeNotebook();

    // HU-136: Ctrl+N cuaderno nuevo (o seguir en uno nuevo si el elegido está lleno), F2 editarlo
    // (Ctrl+M con el foco en la colección), Supr mandarlo a la Papelera de reciclaje (con
    // confirmación).
    void newNotebook();
    void editSelected(bool focusCollection = false);
    void deleteSelected();

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void relayout();
    void reload(const QString& selectFolder);
    bool dialogOpen() const;
    void select(int slot, bool log = true);
    void ensureVisible();
    const bocetos::Book* selectedBook() const;
    int contentTop() const;
    int viewBottom() const;

    appkit::Shortcuts& m_shortcuts;
    appkit::Config& m_config;
    QString m_root, m_cartuchera;
    QList<bocetos::Shelf> m_shelves;
    bocetos::Layout m_layout;
    int m_selected = -1;
    int m_scroll = 0;
    NotebookDialog* m_dialog = nullptr;
    NotebookView* m_view = nullptr;
    appkit::ConfirmOverlay* m_confirm = nullptr;
    QString m_editing; // la carpeta del cuaderno que se edita
};
