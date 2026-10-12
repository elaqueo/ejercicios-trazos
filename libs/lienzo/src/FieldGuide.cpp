#include "lienzo/FieldGuide.h"

#include <appkit/Theme.h>

#include <QFont>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace lienzo {

QRectF fieldRect(int field, int width, int height, double pxPerMm)
{
    const double w = field * kFieldInchMm * pxPerMm, h = w * 9 / 16;
    return QRectF(width / 2.0 - w / 2, height / 2.0 - h / 2, w, h);
}

QImage paintFieldGuide(int width, int height, double pxPerMm, double scale)
{
    QImage image(std::max(1, width), std::max(1, height), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    const double line = std::max(1.0, scale);
    p.setBrush(Qt::NoBrush);
    for (int n = 1; n <= kFieldCount; ++n) {
        p.setPen(QPen(Qt::white, n == kFieldCount ? line * 1.6 : line));
        p.drawRect(fieldRect(n, width, height, pxPerMm));
    }
    const QRectF outer = fieldRect(kFieldCount, width, height, pxPerMm);
    p.setPen(QPen(Qt::white, line));
    p.drawLine(outer.topLeft(), outer.bottomRight());
    p.drawLine(outer.bottomLeft(), outer.topRight());
    p.drawLine(QPointF(outer.left(), outer.center().y()), QPointF(outer.right(), outer.center().y()));
    p.drawLine(QPointF(outer.center().x(), outer.top()), QPointF(outer.center().x(), outer.bottom()));
    // El número de los campos pares, sobre el eje horizontal, a la izquierda de su borde derecho.
    QFont f;
    f.setFamilies({QString::fromLatin1(appkit::theme::kFuenteMono), QStringLiteral("Consolas")});
    f.setPixelSize(std::max(9, int(std::lround(2.2 * pxPerMm))));
    p.setFont(f);
    const double gap = 2 * line;
    for (int n = 2; n <= kFieldCount; n += 2) {
        const QRectF r = fieldRect(n, width, height, pxPerMm);
        p.drawText(QRectF(r.right() - 40 * line - gap, r.center().y() - f.pixelSize() - gap, 40 * line, f.pixelSize()),
                   Qt::AlignRight | Qt::AlignBottom, QString::number(n));
    }
    return image;
}

} // namespace lienzo
