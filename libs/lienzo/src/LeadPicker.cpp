#include "lienzo/LeadPicker.h"

#include "lienzo/SheetMapping.h"
#include "lienzo/Tone.h"

#include <appkit/Theme.h>
#include <drymedia/Paper.h>
#include <drymedia/Pencil.h>

#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace lienzo {

namespace {

constexpr int kPadding = 12;
constexpr int kTitleHeight = 44;
constexpr int kSampleWidth = 300;

} // namespace

QImage leadSample(const Lead& lead, QSize size, double pixelsPerCell, double cellsPerMm, MediumProfile profile)
{
    return mediumSample(forProfile(lead.medium(), profile), size, pixelsPerCell, cellsPerMm);
}

QImage mediumSample(const drymedia::Medium& medium, QSize size, double pixelsPerCell, double cellsPerMm,
                    uint32_t paperColor, bool bold)
{
    // Papel del tamaño de la muestra a la escala de la hoja (la misma semilla siempre: las
    // diferencias entre minas son solo de la mina).
    const double cells = 1.0 / std::max(pixelsPerCell, 1e-3);
    const double widthMm = size.width() * cells / cellsPerMm;
    const double heightMm = size.height() * cells / cellsPerMm;
    drymedia::Paper paper({.seed = 7, .widthMm = widthMm, .heightMm = heightMm, .dpi = int(std::lround(cellsPerMm * 25.4))});
    drymedia::Pencil pencil(paper, medium);
    const double w = paper.width(), h = paper.height();
    const int samples = 80;
    const auto at = [&](double t, double shift) {
        // S de izquierda a derecha; la presión sube hasta el medio y baja.
        const double x = w * (0.08 + 0.84 * t);
        const double y = h * (0.5 + 0.28 * std::sin(t * 2 * std::numbers::pi)) + shift;
        const float pressure = bold ? float(0.5 + 0.45 * std::sin(t * std::numbers::pi))
                                    : float(0.15 + 0.7 * std::sin(t * std::numbers::pi));
        return drymedia::PencilSample{x, y, pressure, 30.0f, 65.0f};
    };
    for (int pass = 0; pass < (bold ? 3 : 1); ++pass) {
        const double shift = bold ? (pass - 1) * 1.5 : 0.0; // en celdas: un trazo apenas más ancho
        pencil.beginStroke(at(0, shift));
        for (int i = 1; i <= samples; ++i)
            pencil.strokeTo(at(double(i) / samples, shift));
        pencil.endStroke();
    }

    const SheetMapping mapping = SheetMapping::fit(size.width(), size.height(), paper.width(), paper.height());
    std::vector<uint32_t> pixels(size_t(size.width()) * size_t(size.height()));
    renderTone(paper, mapping, 0, 0, size.width(), size.height(), pixels.data(), nullptr, nullptr, paperColor);
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
            m_samples[size_t(i)] = leadSample(grades[size_t(i)], QSize(kSampleWidth, kRowHeight - 2 * 8), pixelsPerCell,
                                              m_cellsPerMm, m_profile);
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

// --- PencilPicker (HU-100) ---------------------------------------------------------------

namespace {

// Medidas de la mesa "Selector de lápices" (px lógicos).
constexpr int kPencilPad = 16, kPencilTitle = 28, kPencilGap = 12, kPencilRowGap = 6, kPencilHint = 36;
constexpr int kPencilSampleW = 150, kPencilSampleH = 52;

QFont pencilFont(double px, int weight = QFont::Normal, bool mono = false)
{
    QFont f;
    f.setFamilies(mono ? QStringList{QString::fromLatin1(appkit::theme::kFuenteMono), QStringLiteral("Consolas")}
                       : QStringList{QString::fromLatin1(appkit::theme::kFuente), QStringLiteral("Segoe UI")});
    f.setPixelSize(int(px));
    f.setWeight(QFont::Weight(weight));
    return f;
}

// Una tecla como chip (la de la mesa: fondo elevado, borde y 2 px abajo).
void drawKey(QPainter& p, const QRect& r, const QString& key, const QColor& text)
{
    namespace theme = appkit::theme;
    QPainterPath path;
    path.addRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), theme::kRadioTecla, theme::kRadioTecla);
    p.fillPath(path, theme::kPanelAlto);
    p.setPen(QPen(theme::kBorde, 1));
    p.setBrush(Qt::NoBrush); // si no, drawPath rellena con el pincel que haya quedado
    p.drawPath(path);
    p.fillRect(QRect(r.left() + 2, r.bottom() - 1, r.width() - 4, 1), theme::kBorde);
    p.setFont(pencilFont(13, QFont::Normal, true));
    p.setPen(text);
    p.drawText(r, Qt::AlignCenter, key);
}

QString pencilName(int row)
{
    return row == 0 ? QStringLiteral("Grafito HB") : QString::fromUtf8(kColorPencilNames[size_t(row - 1)].name);
}

QString pencilUse(int row)
{
    return row == 0 ? QStringLiteral("el dibujo") : QString::fromUtf8(kColorPencilNames[size_t(row - 1)].use);
}

QColor pencilColor(int row)
{
    return QColor::fromRgb(QRgb(row == 0 ? kGraphiteColor : kColorPencilColors[size_t(row - 1)]));
}

} // namespace

PencilPicker::PencilPicker(QWidget* owner)
    : QWidget(owner, Qt::Tool | Qt::FramelessWindowHint)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_TranslucentBackground); // esquinas redondeadas
    resize(kWidth, kPencilPad + kPencilTitle + kPencilGap + kRows * kRowHeight + (kRows - 1) * kPencilRowGap + kPencilGap +
                       kPencilHint + kPencilPad);
}

void PencilPicker::setPencils(const Lead& graphite, int active, double pixelsPerCell, double cellsPerMm,
                              MediumProfile profile, uint32_t paperColor)
{
    const bool rescale = pixelsPerCell != m_pixelsPerCell || paperColor != m_paperColor;
    m_pixelsPerCell = pixelsPerCell;
    m_paperColor = paperColor;
    const QSize size(kPencilSampleW, kPencilSampleH);
    if (rescale || m_graphiteOf != graphite) {
        m_samples[0] = mediumSample(forProfile(graphite.medium(), profile), size, pixelsPerCell, cellsPerMm, paperColor, true);
        m_graphiteOf = graphite;
    }
    if (rescale || m_samples[1].isNull())
        for (int c = 0; c < drymedia::kColorCount; ++c)
            m_samples[size_t(c + 1)] =
                mediumSample(forProfile(drymedia::Medium::colored(c), profile), size, pixelsPerCell, cellsPerMm, paperColor, true);
    m_active = m_cursor = std::clamp(active, -1, drymedia::kColorCount - 1) + 1;
    m_hover = -1;
    update();
}

QRect PencilPicker::rowRect(int row) const
{
    return {kPencilPad, kPencilPad + kPencilTitle + kPencilGap + row * (kRowHeight + kPencilRowGap), width() - 2 * kPencilPad,
            kRowHeight};
}

int PencilPicker::rowAt(QPoint point) const
{
    for (int row = 0; row < kRows; ++row)
        if (rowRect(row).contains(point))
            return row;
    return -1;
}

void PencilPicker::pick(int row)
{
    if (row < 0)
        return;
    m_active = row;
    hide();
    if (onPick)
        onPick(row - 1);
}

void PencilPicker::paintEvent(QPaintEvent*)
{
    namespace theme = appkit::theme;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    QPainterPath panel;
    panel.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), theme::kRadioPanel, theme::kRadioPanel);
    p.fillPath(panel, theme::kPanel);
    p.setPen(QPen(theme::kBorde, 1));
    p.drawPath(panel);

    // Título y las teclas que lo cierran.
    const int right = width() - kPencilPad;
    p.setFont(pencilFont(18, QFont::DemiBold));
    p.setPen(theme::kTexto);
    p.drawText(QRect(kPencilPad, kPencilPad, width(), kPencilTitle), Qt::AlignVCenter, QStringLiteral("Lápices"));
    drawKey(p, QRect(right - 40, kPencilPad, 40, 28), QStringLiteral("Esc"), theme::kTexto);
    drawKey(p, QRect(right - 40 - 6 - 36, kPencilPad, 36, 28), QStringLiteral("F5"), theme::kTexto);

    for (int row = 0; row < kRows; ++row) {
        const QRect r = rowRect(row);
        const bool selected = row == m_active;
        const QColor text = selected ? theme::kSeleccionTexto : theme::kTexto;
        if (selected || row == m_hover || row == m_cursor) {
            QPainterPath path;
            path.addRoundedRect(QRectF(r).adjusted(1, 1, -1, -1), theme::kRadioControl, theme::kRadioControl);
            p.fillPath(path, theme::kPanelAlto);
            if (selected || row == m_cursor) {
                p.setPen(QPen(selected ? theme::kEnfasis : theme::kTextoSecundario, 2));
                p.setBrush(Qt::NoBrush);
                p.drawPath(path);
            }
        }
        drawKey(p, QRect(r.left() + 10, r.center().y() - 14, 28, 28), QString::number(row + 1), text);
        const QRect sample(r.left() + 48, r.center().y() - kPencilSampleH / 2, kPencilSampleW, kPencilSampleH);
        QPainterPath clip;
        clip.addRoundedRect(QRectF(sample), 4, 4);
        p.save();
        p.setClipPath(clip);
        if (!m_samples[size_t(row)].isNull())
            p.drawImage(sample, m_samples[size_t(row)]);
        else
            p.fillRect(sample, QColor::fromRgb(QRgb(m_paperColor)));
        p.restore();
        const int x = sample.right() + 12;
        p.setPen(Qt::NoPen);
        p.setBrush(pencilColor(row));
        p.drawRoundedRect(QRectF(x, r.center().y() - 15, 10, 10), 3, 3);
        p.setFont(pencilFont(15, selected ? QFont::DemiBold : QFont::Medium));
        p.setPen(text);
        p.drawText(QRect(x + 16, r.center().y() - 22, right - x - 16, 22), Qt::AlignLeft | Qt::AlignVCenter, pencilName(row));
        p.setFont(pencilFont(12));
        p.setPen(theme::kTextoSecundario);
        p.drawText(QRect(x, r.center().y() + 2, right - x, 18), Qt::AlignLeft | Qt::AlignVCenter, pencilUse(row));
    }

    const int hintTop = rowRect(kRows - 1).bottom() + 1 + kPencilGap;
    p.setFont(pencilFont(12));
    p.setPen(theme::kTextoSecundario);
    p.drawText(QRect(kPencilPad, hintTop, width() - 2 * kPencilPad, kPencilHint), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
               QStringLiteral("1 a 5 eligen directo · ↑ ↓ y Enter · clic o el lápiz. La goma borra solo el lápiz elegido."));
}

void PencilPicker::mouseMoveEvent(QMouseEvent* event)
{
    const int row = rowAt(event->position().toPoint());
    if (row != m_hover) {
        m_hover = row;
        update();
    }
}

void PencilPicker::mousePressEvent(QMouseEvent* event)
{
    pick(rowAt(event->position().toPoint()));
}

void PencilPicker::leaveEvent(QEvent*)
{
    m_hover = -1;
    update();
}

void PencilPicker::keyPressEvent(QKeyEvent* event)
{
    const int key = event->key();
    if (key >= Qt::Key_1 && key < Qt::Key_1 + kRows) {
        pick(key - Qt::Key_1);
        return;
    }
    switch (key) {
    case Qt::Key_Up:
        m_cursor = (m_cursor + kRows - 1) % kRows;
        update();
        break;
    case Qt::Key_Down:
        m_cursor = (m_cursor + 1) % kRows;
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
