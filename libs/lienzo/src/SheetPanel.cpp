#include "lienzo/SheetPanel.h"

#include <appkit/Theme.h>

#include <QFont>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace lienzo {

namespace {

// Medidas del diseño (px a escala 1), las de la mesa "Panel de hojas".
constexpr int kWidth = 240, kMargin = 16, kPad = 16, kGap = 16;
constexpr int kHeader = 44;  // "TOMA" y el nombre
constexpr int kNumber = 55;  // el número grande, con su raya abajo
constexpr int kInfo = 95;    // lápiz, mesa de luz, vista, con su raya arriba
constexpr int kFooter = 28;  // la tecla
constexpr int kRowGap = 6;   // entre miniaturas
constexpr int kRowMax = 84, kRowMin = 40;
constexpr int kNumberColumn = 28, kNumberGap = 8;
constexpr int kBorder = 2; // marco de la miniatura (la activa, 2 px de acento; las demás, 1 px y 1 de fondo)

int stripTop()
{
    return kPad + kHeader + kGap + kNumber + kGap;
}

QFont font(bool mono, double px, int weight = QFont::Normal)
{
    QFont f;
    f.setFamilies(mono ? QStringList{QString::fromLatin1(appkit::theme::kFuenteMono), QStringLiteral("Consolas")}
                       : QStringList{QString::fromLatin1(appkit::theme::kFuente), QStringLiteral("Segoe UI")});
    f.setPixelSize(std::max(1, int(std::lround(px))));
    f.setWeight(QFont::Weight(weight));
    return f;
}

} // namespace

std::vector<PanelSlot> panelSlots(const SheetOrder& order, int center, int around)
{
    std::vector<PanelSlot> strip;
    for (int i = center - around; i <= center + around; ++i) {
        PanelSlot s;
        if (order.valid(i)) {
            s.id = order.ids[size_t(i)];
            s.number = i + 1;
        }
        s.center = i == center;
        strip.push_back(s);
    }
    return strip;
}

PanelLayout panelLayout(int clientWidth, int clientHeight, double scale, double sheetAspect, int count)
{
    PanelLayout l;
    l.scale = scale > 0 ? scale : 1;
    const auto px = [&](double v) { return int(std::lround(v * l.scale)); };
    const int width = px(kWidth), margin = px(kMargin);
    l.panel = QRect(clientWidth - margin - width, margin, width, std::max(0, clientHeight - 2 * margin));
    // La tira toma lo que dejan los textos, hasta kRowMax por miniatura.
    const double height = l.panel.height() / l.scale;
    const double fixed = stripTop() + kGap + kInfo + kGap + kFooter + kPad + (count - 1) * kRowGap;
    const double row = std::clamp((height - fixed) / std::max(1, count), double(kRowMin), double(kRowMax));
    l.rowHeight = px(row);
    const int innerH = std::max(1, px(row - 4) - 2 * kBorder);
    const int maxW = width - px(kPad + kNumberColumn + kNumberGap + kPad) - 2 * kBorder;
    const int innerW = std::clamp(int(std::lround(innerH * sheetAspect)), 1, std::max(1, maxW));
    const int x = px(kPad + kNumberColumn + kNumberGap) + kBorder;
    for (int i = 0; i < count; ++i) {
        const int top = px(stripTop() + i * (row + kRowGap));
        l.thumbs.emplace_back(x, top + (l.rowHeight - innerH) / 2, innerW, innerH);
    }
    return l;
}

QImage paintPanel(const PanelInfo& info, const PanelLayout& layout, const QColor& paper, const QColor& outside)
{
    namespace theme = appkit::theme;
    const QColor accent = theme::kSeleccionTexto;
    QImage image(layout.panel.size(), QImage::Format_ARGB32);
    image.fill(outside);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    const double s = layout.scale;
    const auto px = [s](double v) { return v * s; };
    const double w = image.width(), h = image.height();

    QPainterPath shape;
    shape.addRoundedRect(QRectF(0.5, 0.5, w - 1, h - 1), px(theme::kRadioPanel), px(theme::kRadioPanel));
    p.fillPath(shape, theme::kPanel);
    p.setPen(QPen(theme::kBorde, 1));
    p.drawPath(shape);

    const double left = px(kPad), right = w - px(kPad);
    const auto text = [&](const QRectF& r, const QString& t, const QFont& f, const QColor& c, int flags) {
        p.setFont(f);
        p.setPen(c);
        p.drawText(r, flags, t);
    };
    // La toma y si está guardada.
    double y = px(kPad);
    text(QRectF(left, y, right - left, px(16)), QStringLiteral("TOMA"), font(false, px(12), QFont::DemiBold),
         theme::kTextoSecundario, Qt::AlignLeft | Qt::AlignVCenter);
    const QString state = info.saved ? QStringLiteral("guardado") : QStringLiteral("guardando…");
    const QFont small = font(false, px(13));
    const double stateW = QFontMetricsF(small).horizontalAdvance(state);
    text(QRectF(right - stateW, y, stateW, px(16)), state, small, theme::kTextoSecundario, Qt::AlignRight | Qt::AlignVCenter);
    p.setPen(Qt::NoPen);
    p.setBrush(info.saved ? theme::kGuardado : accent);
    p.drawEllipse(QPointF(right - stateW - px(10), y + px(8)), px(4), px(4));
    y += px(22);
    text(QRectF(left, y, right - left, px(22)),
         QFontMetricsF(font(false, px(16))).elidedText(info.take, Qt::ElideMiddle, right - left),
         font(false, px(16), QFont::DemiBold), theme::kTexto, Qt::AlignLeft | Qt::AlignVCenter);

    // La hoja del centro y el total.
    y = px(kPad + kHeader + kGap);
    const QFont big = font(true, px(40), QFont::Medium);
    const QString number = QString::number(info.number);
    const double numberW = QFontMetricsF(big).horizontalAdvance(number);
    text(QRectF(left, y, numberW + 1, px(40)), number, big, accent, Qt::AlignLeft | Qt::AlignBottom);
    text(QRectF(left + numberW + px(10), y, right - left - numberW - px(10), px(40) - px(4)),
         QStringLiteral("de %1 hojas").arg(info.total), font(false, px(14)), theme::kTextoSecundario,
         Qt::AlignLeft | Qt::AlignBottom);
    p.fillRect(QRectF(left, px(kPad + kHeader + kGap + kNumber) - 1, right - left, 1), theme::kBorde);

    // Los marcos de las miniaturas y sus números.
    const QFont mono = font(true, px(15));
    for (size_t i = 0; i < layout.thumbs.size() && i < info.strip.size(); ++i) {
        const PanelSlot& slot = info.strip[i];
        const QRectF inner = layout.thumbs[i];
        const QRectF box = inner.adjusted(-kBorder, -kBorder, kBorder, kBorder);
        if (!slot.id) { // fuera de la pila: punteado
            p.setBrush(Qt::NoBrush);
            QPen dashed(theme::kBorde, 1, Qt::DashLine);
            p.setPen(dashed);
            p.drawRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), px(4), px(4));
            continue;
        }
        p.setPen(Qt::NoPen);
        p.setBrush(slot.center ? accent : theme::kBorde);
        p.drawRoundedRect(box, px(4), px(4));
        if (!slot.center) // 1 px de marco y 1 de fondo
            p.fillRect(box.adjusted(1, 1, -1, -1), theme::kPanel);
        p.fillRect(inner, paper); // hasta que el render ponga la miniatura
        text(QRectF(left, box.top(), px(kNumberColumn), box.height()), QString::number(slot.number), mono,
             slot.center ? accent : theme::kTextoSecundario, Qt::AlignRight | Qt::AlignVCenter);
    }

    // Lápiz, mesa de luz y vista.
    const double stripBottom =
        layout.thumbs.empty() ? y : layout.thumbs.back().bottom() + kBorder + (layout.rowHeight - layout.thumbs.back().height()) / 2.0;
    y = stripBottom + px(kGap);
    p.fillRect(QRectF(left, y, right - left, 1), theme::kBorde);
    y += px(14);
    const QFont body = font(false, px(14));
    const auto row = [&](const QString& label, const QString& value, bool monoValue = false) {
        text(QRectF(left, y, right - left, px(20)), label, body, theme::kTextoSecundario, Qt::AlignLeft | Qt::AlignVCenter);
        text(QRectF(left, y, right - left, px(20)), value, monoValue ? font(true, px(14)) : body,
             theme::kTexto, Qt::AlignRight | Qt::AlignVCenter);
        y += px(30);
    };
    row(QStringLiteral("Lápiz"), info.lead);
    // Mesa de luz: un cuadrito por capa, con su color, y "sí" o "no".
    const QString onOff = info.light.empty() ? QStringLiteral("no") : QStringLiteral("sí");
    const double onOffW = QFontMetricsF(body).horizontalAdvance(onOff);
    text(QRectF(left, y, right - left, px(20)), QStringLiteral("Mesa de luz"), body, theme::kTextoSecundario,
         Qt::AlignLeft | Qt::AlignVCenter);
    text(QRectF(left, y, right - left, px(20)), onOff, body, theme::kTexto, Qt::AlignRight | Qt::AlignVCenter);
    double chipX = right - onOffW - px(6);
    for (auto it = info.light.rbegin(); it != info.light.rend(); ++it) {
        chipX -= px(12);
        p.setPen(Qt::NoPen);
        p.setBrush(*it);
        p.drawRoundedRect(QRectF(chipX, y + px(4), px(12), px(12)), px(3), px(3));
        chipX -= px(6);
    }
    y += px(30);
    row(QStringLiteral("Vista"), QStringLiteral("%1°").arg(info.degrees), true);

    // La tecla que lo oculta.
    const QFont keyFont = font(true, px(13));
    const double keyW = std::max(px(28), QFontMetricsF(keyFont).horizontalAdvance(info.key) + px(16));
    const QRectF key(left, h - px(kPad) - px(28), keyW, px(28));
    p.setPen(QPen(theme::kBorde, 1));
    p.setBrush(theme::kPanelAlto);
    p.drawRoundedRect(key.adjusted(0.5, 0.5, -0.5, -0.5), px(theme::kRadioTecla), px(theme::kRadioTecla));
    text(key, info.key, keyFont, theme::kTexto, Qt::AlignCenter);
    text(QRectF(key.right() + px(10), key.top(), right - key.right() - px(10), key.height()),
         QStringLiteral("oculta el panel"), small, theme::kTextoSecundario, Qt::AlignLeft | Qt::AlignVCenter);
    return image;
}

} // namespace lienzo
