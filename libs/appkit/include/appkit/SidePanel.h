#pragma once

#include <QString>
#include <QWidget>

#include <functional>

class QTabWidget;

namespace appkit {

// Panel lateral con pestañas (HU-12, mesa "Panel" del diseño). Ventana propia sin borde,
// como el menú: un widget hijo quedaría tapado por la ventana nativa del lienzo. Va pegado al
// borde derecho del área útil (lo ubica quien lo muestra). Esc o closeKey lo cierran.
class SidePanel : public QWidget {
public:
    SidePanel(QWidget* owner, QString title, Qt::Key closeKey);

    // La página pasa a ser hija del panel.
    void addTab(QWidget* page, const QString& title);
    QTabWidget* tabs() const { return m_tabs; }

    std::function<void()> onClose; // se cerró con la tecla

    static constexpr int kWidth = 380;

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    Qt::Key m_closeKey;
    QTabWidget* m_tabs;
};

// Hoja de estilo de los controles de los paneles con los colores del tema.
QString panelStyleSheet();

} // namespace appkit
