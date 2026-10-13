#pragma once

#include "BocetosCore.h"

#include <appkit/Config.h>
#include <appkit/Shortcuts.h>

#include <QString>
#include <QWidget>

// La biblioteca de Bocetos (HU-135, mesa "Bocetos · biblioteca"): pantalla completa, sin bordes,
// con los cuadernos como libros apaisados en estantes por colección. Las flechas eligen, Enter
// o tocar abre (el visor llega con HU-137). Las teclas van al registro de atajos de appkit.
class LibraryWindow : public QWidget {
public:
    LibraryWindow(appkit::Shortcuts& shortcuts, appkit::Config& config);

    // Lee la biblioteca de `root` (queda en el log cuánto tardó, RNF-01) y elige el último
    // cuaderno elegido.
    void setLibrary(const QString& root, const QString& cartuchera);

    void move(bocetos::Move move);
    void openSelected();

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void relayout();
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
};
