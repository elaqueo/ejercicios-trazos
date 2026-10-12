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
constexpr int kInfo = 125;   // lápiz, mesa de luz, hoja fija, vista, con su raya arriba
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

std::vector<PanelSlot> panelSlots(const SheetOrder& order, int center, int around, const SheetRoles& roles,
                                  uint64_t fixed)
{
    // Las posiciones del flip (sin la fija) y dónde cae el centro entre ellas.
    const int skip = fixedIndex(order, fixed);
    std::vector<int> positions;
    int c = -1;
    for (int i = 0; i < order.count; ++i) {
        if (i == skip)
            continue;
        if (i == center)
            c = int(positions.size());
        positions.push_back(i);
    }
    std::vector<PanelSlot> strip;
    for (int k = c - around; k <= c + around; ++k) {
        PanelSlot s;
        if (c >= 0 && k >= 0 && k < int(positions.size())) {
            s.id = order.ids[size_t(positions[size_t(k)])];
            s.number = k + 1;
            s.role = roleOf(roles, s.id);
        }
        s.center = c >= 0 && k == c;
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
    const QString number = info.number > 0 ? QString::number(info.number) : QStringLiteral("F"); // HU-89
    double numberW = QFontMetricsF(big).horizontalAdvance(number);
    if (info.role == SheetRole::None) {
        text(QRectF(left, y, numberW + 1, px(40)), number, big, accent, Qt::AlignLeft | Qt::AlignBottom);
    } else { // HU-104: con su marca, como en la hoja (círculo de la altura del número y su aire)
        numberW = px(kNumber - 6);
        drawRoleMark(p, QRectF(left, y, numberW, numberW), info.number, info.role, font(true, px(26), QFont::Medium), accent,
                     px(2));
    }
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
        const QColor numberColor = slot.center ? accent : theme::kTextoSecundario;
        if (slot.role == SheetRole::None) {
            text(QRectF(left, box.top(), px(kNumberColumn), box.height()), QString::number(slot.number), mono, numberColor,
                 Qt::AlignRight | Qt::AlignVCenter);
        } else { // HU-104: con su marca, contra el borde derecho de la columna
            const double d = std::min(px(kNumberColumn), box.height());
            drawRoleMark(p, QRectF(left + px(kNumberColumn) - d, box.center().y() - d / 2, d, d), slot.number, slot.role,
                         font(true, px(12)), numberColor, px(1.5));
        }
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
    if (info.leadColor.isValid()) { // HU-99: un cuadrito con el color, antes del nombre
        const double leadW = QFontMetricsF(body).horizontalAdvance(info.lead);
        p.setPen(Qt::NoPen);
        p.setBrush(info.leadColor);
        p.drawRoundedRect(QRectF(right - leadW - px(18), y + px(4), px(12), px(12)), px(3), px(3));
    }
    row(QStringLiteral("Lápiz"), info.lead);
    // Mesa de luz: un cuadrito por capa, con su color, y "sí" o "no".
    const QString onOff = info.light.empty() ? QStringLiteral("no")
                          : info.keysOnly    ? QStringLiteral("solo keys") // HU-93
                                             : QStringLiteral("sí");
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
    row(QStringLiteral("Hoja fija"), !info.hasFixed ? QStringLiteral("no") // HU-89
                                     : info.onFixed ? QStringLiteral("dibujando · Ctrl+F")
                                                    : QStringLiteral("sí · Ctrl+F"));
    row(QStringLiteral("Vista"),
        info.zoom == 100 ? QStringLiteral("%1°").arg(info.degrees) : QStringLiteral("%1° · %2 %").arg(info.degrees).arg(info.zoom),
        true);

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
