#pragma once

#include <appkit/Shortcuts.h>

#include <QString>
#include <QWidget>

// La ventana de Bocetos (HU-134): pantalla completa, sin bordes, con el fondo del tema. Por ahora
// dice de dónde lee los cuadernos, cuántos hay y si encontró Cartuchera; la biblioteca (la grilla
// de libros) llega con HU-135. Las teclas van al registro de atajos de appkit.
class LibraryWindow : public QWidget {
public:
    explicit LibraryWindow(appkit::Shortcuts& shortcuts);

    void setStatus(const QString& root, int notebooks, const QString& cartuchera);

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    appkit::Shortcuts& m_shortcuts;
    QString m_root, m_cartuchera;
    int m_notebooks = 0;
};
