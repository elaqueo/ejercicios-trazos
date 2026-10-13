#include "LibraryWindow.h"

#include <appkit/Theme.h>

#include <QFont>
#include <QKeyEvent>
#include <QPainter>

namespace {

QFont themeFont(bool mono, int px, int weight = QFont::Normal)
{
    QFont f;
    f.setFamilies(mono ? QStringList{QString::fromLatin1(appkit::theme::kFuenteMono), QStringLiteral("Consolas")}
                       : QStringList{QString::fromLatin1(appkit::theme::kFuente), QStringLiteral("Segoe UI")});
    f.setPixelSize(px);
    f.setWeight(QFont::Weight(weight));
    return f;
}

} // namespace

LibraryWindow::LibraryWindow(appkit::Shortcuts& shortcuts)
    : m_shortcuts(shortcuts)
{
    setFocusPolicy(Qt::StrongFocus);
}

void LibraryWindow::setStatus(const QString& root, int notebooks, const QString& cartuchera)
{
    m_root = root;
    m_notebooks = notebooks;
    m_cartuchera = cartuchera;
    update();
}

void LibraryWindow::paintEvent(QPaintEvent*)
{
    namespace theme = appkit::theme;
    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.fillRect(rect(), theme::kFondo);
    const int left = 64, width = this->width() - 2 * left;
    p.setFont(themeFont(false, 26, QFont::DemiBold));
    p.setPen(theme::kTexto);
    p.drawText(QRect(left, 40, width, 40), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Bocetos"));
    p.setFont(themeFont(false, 14));
    p.setPen(theme::kTextoSecundario);
    const QString count = m_notebooks == 1 ? QStringLiteral("1 cuaderno") : QStringLiteral("%1 cuadernos").arg(m_notebooks);
    p.drawText(QRect(left, 80, width, 24), Qt::AlignLeft | Qt::AlignVCenter, count);

    p.setFont(themeFont(true, 13));
    p.drawText(QRect(left, 132, width, 22), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("biblioteca · %1").arg(m_root));
    p.setPen(m_cartuchera.isEmpty() ? theme::kSeleccionTexto : theme::kTextoSecundario);
    p.drawText(QRect(left, 156, width, 22), Qt::AlignLeft | Qt::AlignVCenter,
               m_cartuchera.isEmpty() ? QStringLiteral("cartuchera · no está junto a bocetos.exe: no se va a poder dibujar")
                                      : QStringLiteral("cartuchera · %1").arg(m_cartuchera));

    p.setFont(themeFont(false, 14));
    p.setPen(theme::kTextoSecundario);
    p.drawText(QRect(left, height() - 64, width, 24), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("La biblioteca llega en la próxima versión  ·  Ctrl+, atajos  ·  Ctrl+Q salir"));
}

void LibraryWindow::keyPressEvent(QKeyEvent* event)
{
    // El registro va por la tecla virtual de Windows, como en el lienzo.
    if (!event->isAutoRepeat() && m_shortcuts.trigger(event->nativeVirtualKey(), event->modifiers() & Qt::ControlModifier))
        return;
    QWidget::keyPressEvent(event);
}
