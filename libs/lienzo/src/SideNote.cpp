#include "lienzo/SideNote.h"

#include <appkit/Theme.h>

#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace lienzo {

namespace {

// Medidas de la mesa "Cartuchera en modo cuaderno" (px a escala 1).
constexpr int kWidth = 460, kMinWidth = 320, kGap = 48, kTop = 40;
constexpr int kLabel = 18, kTitle = 36, kDetail = 24, kRow = 38, kKey = 28, kFooter = 40;
constexpr int kPill = 36, kPillPad = 14, kMargin = 16;

QFont font(bool mono, double px, int weight = QFont::Normal)
{
    QFont f;
    f.setFamilies(mono ? QStringList{QString::fromLatin1(appkit::theme::kFuenteMono), QStringLiteral("Consolas")}
                       : QStringList{QString::fromLatin1(appkit::theme::kFuente), QStringLiteral("Segoe UI")});
    f.setPixelSize(std::max(1, int(std::lround(px))));
    f.setWeight(QFont::Weight(weight));
    return f;
}

int fullHeight(const SideNote& note)
{
    return kLabel + 6 + kTitle + kDetail + 16 + int(note.keys.size()) * kRow + 8 + kFooter;
}

// Una tecla como las de la página de atajos: caja redondeada, borde inferior más grueso.
double drawKey(QPainter& p, double x, double y, const QString& key, bool accent, double scale)
{
    namespace theme = appkit::theme;
    const QFont mono = font(true, 13 * scale);
    const double w = std::max(kKey * scale, QFontMetricsF(mono).horizontalAdvance(key) + 16 * scale);
    const QRectF box(x, y, w, kKey * scale);
    QPainterPath path;
    path.addRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), theme::kRadioTecla * scale, theme::kRadioTecla * scale);
    p.fillPath(path, accent ? theme::kSeleccion : theme::kPanelAlto);
    p.setPen(QPen(accent ? theme::kSeleccionTexto : theme::kBorde, 1));
    p.drawPath(path);
    p.setFont(mono);
    p.setPen(accent ? theme::kSeleccionTexto : theme::kTexto);
    p.drawText(box, Qt::AlignCenter, key);
    return w;
}

} // namespace

QRect sideNoteRect(const SideNote& note, int clientWidth, int clientHeight, const QRect& sheet, double scale, bool* compact)
{
    const auto px = [&](double v) { return int(std::lround(v * scale)); };
    const int gap = clientWidth - (sheet.right() + 1);
    if (gap >= px(kMinWidth) + 2 * px(kGap)) {
        if (compact)
            *compact = false;
        const int w = std::min(px(kWidth), gap - 2 * px(kGap));
        return QRect(sheet.right() + 1 + px(kGap), px(kTop), w, std::min(clientHeight, px(fullHeight(note))));
    }
    if (compact)
        *compact = true;
    const QFont body = font(false, 14 * scale);
    const int w = int(std::ceil(QFontMetricsF(body).horizontalAdvance(note.compact))) + 2 * px(kPillPad);
    const int left = std::max(0, sheet.left()) + px(kMargin), top = std::max(0, sheet.top()) + px(kMargin);
    return QRect(left, top, std::min(w, std::max(1, clientWidth - left - px(kMargin))), px(kPill));
}

QImage paintSideNote(const SideNote& note, const QSize& size, double scale, bool compact, const QColor& outside)
{
    namespace theme = appkit::theme;
    const auto px = [&](double v) { return v * scale; };
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(outside);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    const double w = size.width();

    if (compact) { // una línea sobre la hoja, en una píldora del panel
        image.fill(theme::kPanel);
        p.setPen(QPen(theme::kBorde, 1));
        p.drawRect(QRectF(0.5, 0.5, w - 1, size.height() - 1));
        p.setFont(font(false, px(14)));
        p.setPen(theme::kTexto);
        p.drawText(QRectF(px(kPillPad), 0, w - 2 * px(kPillPad), size.height()), Qt::AlignLeft | Qt::AlignVCenter, note.compact);
        return image;
    }

    double y = 0;
    p.setFont(font(false, px(12), QFont::DemiBold));
    p.setPen(theme::kTextoSecundario);
    p.drawText(QRectF(0, y, w, px(kLabel)), Qt::AlignLeft | Qt::AlignVCenter, note.label);
    y += px(kLabel + 6);

    double x = 0;
    if (note.swatch.isValid()) {
        QPainterPath tapa;
        tapa.addRoundedRect(QRectF(0, y + px(10), px(22), px(16)), px(3), px(3));
        p.fillPath(tapa, note.swatch);
        x = px(34);
    }
    p.setFont(font(false, px(26), QFont::DemiBold));
    p.setPen(theme::kTexto);
    const QFontMetricsF titleMetrics(p.font());
    p.drawText(QRectF(x, y, w - x, px(kTitle)), Qt::AlignLeft | Qt::AlignVCenter,
               titleMetrics.elidedText(note.title, Qt::ElideRight, w - x));
    y += px(kTitle);

    p.setFont(font(true, px(15)));
    p.setPen(theme::kSeleccionTexto);
    p.drawText(QRectF(0, y, w, px(kDetail)), Qt::AlignLeft | Qt::AlignVCenter, note.detail);
    y += px(kDetail + 16);

    for (size_t i = 0; i < note.keys.size(); ++i) {
        const double kw = drawKey(p, 0, y, note.keys[i].first, i == 0, scale);
        p.setFont(font(false, px(14)));
        p.setPen(i == 0 ? theme::kTexto : theme::kTextoSecundario);
        p.drawText(QRectF(kw + px(10), y, w - kw - px(10), px(kKey)), Qt::AlignLeft | Qt::AlignVCenter, note.keys[i].second);
        y += px(kRow);
    }

    y += px(8);
    p.setPen(QPen(theme::kBorde, 1));
    p.drawLine(QPointF(0, y + 0.5), QPointF(w, y + 0.5));
    p.setFont(font(true, px(13)));
    p.setPen(theme::kTextoSecundario);
    p.drawText(QRectF(0, y, w, px(kFooter)), Qt::AlignLeft | Qt::AlignVCenter, note.footer);
    return image;
}

} // namespace lienzo
