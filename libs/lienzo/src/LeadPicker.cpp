#include "lienzo/LeadPicker.h"

#include "lienzo/SheetMapping.h"
#include "lienzo/Tone.h"

#include <appkit/Theme.h>
#include <drymedia/Paper.h>
#include <drymedia/Pencil.h>

#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>

#include <cmath>
#include <numbers>
#include <vector>

namespace lienzo {

namespace {

constexpr int kPadding = 12;
constexpr int kTitleHeight = 44;
constexpr int kSampleWidth = 300;

} // namespace

QImage leadSample(const Lead& lead, QSize size, double pixelsPerCell)
{
    // Papel del tamaño de la muestra a la escala de la hoja (la misma semilla siempre: las
    // diferencias entre minas son solo de la mina).
    const double cells = 1.0 / std::max(pixelsPerCell, 1e-3);
    const double widthMm = size.width() * cells / drymedia::kCellsPerMm;
    const double heightMm = size.height() * cells / drymedia::kCellsPerMm;
    drymedia::Paper paper({.seed = 7, .widthMm = widthMm, .heightMm = heightMm});
    drymedia::Pencil pencil(paper, lead.medium());
    const double w = paper.width(), h = paper.height();
    const int samples = 80;
    const auto at = [&](double t) {
        // S de izquierda a derecha; la presión sube hasta el medio y baja.
        const double x = w * (0.08 + 0.84 * t);
        const double y = h * (0.5 + 0.28 * std::sin(t * 2 * std::numbers::pi));
        const float pressure = float(0.15 + 0.7 * std::sin(t * std::numbers::pi));
        return drymedia::PencilSample{x, y, pressure, 30.0f, 65.0f};
    };
    pencil.beginStroke(at(0));
    for (int i = 1; i <= samples; ++i)
        pencil.strokeTo(at(double(i) / samples));
    pencil.endStroke();

    const SheetMapping mapping = SheetMapping::fit(size.width(), size.height(), paper.width(), paper.height());
    std::vector<uint32_t> pixels(size_t(size.width()) * size_t(size.height()));
    renderTone(paper, mapping, 0, 0, size.width(), size.height(), pixels.data());
    return QImage(reinterpret_cast<const uchar*>(pixels.data()), size.width(), size.height(), QImage::Format_RGB32)
        .copy();
}

LeadPicker::LeadPicker(QWidget* owner)
    : QWidget(owner, Qt::Tool | Qt::FramelessWindowHint)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    m_header = QImage(QStringLiteral(":/lienzo/grafito.jpg"));
    resize(kWidth, headerHeight() + kTitleHeight + kGradeCount * kRowHeight + kPadding);
}

int LeadPicker::headerHeight() const
{
    if (m_header.isNull())
        return 0;
    return int(std::lround((kWidth - 2) * double(m_header.height()) / m_header.width()));
}

void LeadPicker::setLeads(const Grades& grades, int active, double pixelsPerCell)
{
    if (pixelsPerCell != m_pixelsPerCell)
        m_sampleOf.fill(std::nullopt);
    m_pixelsPerCell = pixelsPerCell;
    m_grades = grades;
    m_active = m_cursor = active;
    for (int i = 0; i < kGradeCount; ++i)
        if (m_sampleOf[size_t(i)] != grades[size_t(i)]) {
            m_samples[size_t(i)] = leadSample(grades[size_t(i)], QSize(kSampleWidth, kRowHeight - 2 * 8), pixelsPerCell);
            m_sampleOf[size_t(i)] = grades[size_t(i)];
        }
    update();
}

QRect LeadPicker::rowRect(int row) const
{
    return {kPadding, headerHeight() + kTitleHeight + row * kRowHeight, width() - 2 * kPadding, kRowHeight};
}

int LeadPicker::rowAt(QPoint point) const
{
    for (int row = 0; row < kGradeCount; ++row)
        if (rowRect(row).contains(point))
            return row;
    return -1;
}

void LeadPicker::pick(int row)
{
    if (row < 0)
        return;
    m_active = row;
    hide();
    if (onPick)
        onPick(row);
}

void LeadPicker::paintEvent(QPaintEvent*)
{
    namespace theme = appkit::theme;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), theme::kPanel);
    p.setPen(theme::kBorde);
    p.drawRect(rect().adjusted(0, 0, -1, -1));
    const int top = headerHeight();
    if (top > 0) {
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawImage(QRect(1, 1, width() - 2, top), m_header);
    }

    QFont title = font();
    title.setPointSizeF(title.pointSizeF() * 1.15);
    title.setBold(true);
    p.setFont(title);
    p.setPen(theme::kTexto);
    p.drawText(QRect(kPadding + 4, top, width(), kTitleHeight), Qt::AlignVCenter, QStringLiteral("Lápices"));
    QFont hint = font();
    p.setFont(hint);
    p.setPen(theme::kTextoSecundario);
    p.drawText(QRect(0, top, width() - kPadding - 4, kTitleHeight), Qt::AlignVCenter | Qt::AlignRight,
               QStringLiteral("F5 o Esc cierra"));

    QFont name = font();
    name.setPointSizeF(name.pointSizeF() * 1.3);
    name.setBold(true);
    for (int row = 0; row < kGradeCount; ++row) {
        const QRect r = rowRect(row).adjusted(0, 2, 0, -2);
        const bool selected = row == m_active;
        if (selected || row == m_hover || row == m_cursor) {
            QPainterPath path;
            path.addRoundedRect(r, theme::kRadioControl, theme::kRadioControl);
            p.fillPath(path, selected ? theme::kSeleccion : theme::kPanelAlto);
        }
        p.setFont(name);
        p.setPen(selected ? theme::kSeleccionTexto : theme::kTexto);
        p.drawText(QRect(r.left() + 12, r.top(), 64, r.height()), Qt::AlignVCenter, QLatin1String(kGradeNames[size_t(row)]));
        const QImage& sample = m_samples[size_t(row)];
        if (!sample.isNull()) {
            const QRect target(r.right() - 12 - sample.width(), r.center().y() - sample.height() / 2, sample.width(),
                               sample.height());
            p.drawImage(target, sample);
            if (row == m_cursor && !selected) {
                p.setPen(theme::kTextoSecundario);
                p.drawRect(target.adjusted(0, 0, -1, -1));
            }
        }
    }
}

void LeadPicker::mouseMoveEvent(QMouseEvent* event)
{
    const int row = rowAt(event->position().toPoint());
    if (row != m_hover) {
        m_hover = row;
        update();
    }
}

void LeadPicker::mousePressEvent(QMouseEvent* event)
{
    pick(rowAt(event->position().toPoint()));
}

void LeadPicker::leaveEvent(QEvent*)
{
    m_hover = -1;
    update();
}

void LeadPicker::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Up:
        m_cursor = (m_cursor + kGradeCount - 1) % kGradeCount;
        update();
        break;
    case Qt::Key_Down:
        m_cursor = (m_cursor + 1) % kGradeCount;
        update();
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        pick(m_cursor);
        break;
    case Qt::Key_Escape:
    case Qt::Key_F5:
        hide();
        if (onClose)
            onClose();
        break;
    default:
        QWidget::keyPressEvent(event);
    }
}

} // namespace lienzo
