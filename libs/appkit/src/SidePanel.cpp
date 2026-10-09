#include "appkit/SidePanel.h"

#include "appkit/Theme.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QPainter>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>

namespace appkit {

QString panelStyleSheet()
{
    using namespace theme;
    return QStringLiteral(R"(
QWidget { color: %1; background: %2; }
QLabel#secundario { color: %3; }
QTabWidget::pane { border: none; border-top: 1px solid %4; }
QTabBar::tab { background: %2; color: %3; padding: 10px 14px; border: none; min-height: 24px; }
QTabBar::tab:selected { color: %6; border-bottom: 2px solid %6; }
QSlider::groove:horizontal { height: 6px; background: %5; border-radius: 3px; }
QSlider::sub-page:horizontal { background: %6; border-radius: 3px; }
QSlider::handle:horizontal { background: %1; width: 18px; margin: -7px 0; border-radius: 9px; }
QPushButton { background: %5; border: 1px solid %4; border-radius: 8px; padding: 10px 14px; min-height: 24px; }
QPushButton:hover { border-color: %6; }
QPushButton:disabled { color: %3; }
QCheckBox { spacing: 10px; min-height: 32px; }
QScrollArea { border: none; }
)")
        .arg(kTexto.name(), kPanel.name(), kTextoSecundario.name(), kBorde.name(), kPanelAlto.name(),
             kEnfasis.name());
}

SidePanel::SidePanel(QWidget* owner, QString title, Qt::Key closeKey)
    : QWidget(owner, Qt::Tool | Qt::FramelessWindowHint)
    , m_closeKey(closeKey)
    , m_tabs(new QTabWidget(this))
{
    setFocusPolicy(Qt::StrongFocus);
    setStyleSheet(panelStyleSheet());
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);

    auto* header = new QWidget(this);
    header->setFixedHeight(44);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(16, 0, 16, 0);
    auto* titleLabel = new QLabel(title, header);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.15);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    auto* hint = new QLabel(QKeySequence(closeKey).toString() + QStringLiteral(" o Esc cierra"), header);
    hint->setObjectName(QStringLiteral("secundario"));
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(hint);
    layout->addWidget(header);

    m_tabs->setDocumentMode(true);
    m_tabs->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(m_tabs);
    resize(kWidth, 560);
}

void SidePanel::addTab(QWidget* page, const QString& title)
{
    // Cada página con margen y desplazamiento, por si no entra en el alto del área útil.
    auto* scroll = new QScrollArea(m_tabs);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* holder = new QWidget(scroll);
    auto* layout = new QVBoxLayout(holder);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->addWidget(page);
    layout->addStretch();
    scroll->setWidget(holder);
    m_tabs->addTab(scroll, title);
}

void SidePanel::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape || event->key() == m_closeKey) {
        hide();
        if (onClose)
            onClose();
        return;
    }
    QWidget::keyPressEvent(event);
}

void SidePanel::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), theme::kPanel);
    p.setPen(theme::kBorde);
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

} // namespace appkit
