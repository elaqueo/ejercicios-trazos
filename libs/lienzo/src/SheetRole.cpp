#include "lienzo/SheetRole.h"

#include <appkit/Theme.h>

#include <QFont>
#include <QFontMetricsF>
#include <QPainter>

#include <cmath>

namespace lienzo {

SheetRole nextRole(SheetRole role)
{
    switch (role) {
    case SheetRole::None: return SheetRole::Key;
    case SheetRole::Key: return SheetRole::Breakdown;
    case SheetRole::Breakdown: return SheetRole::None;
    }
    return SheetRole::None;
}

QString roleName(SheetRole role)
{
    switch (role) {
    case SheetRole::Key: return QStringLiteral("key");
    case SheetRole::Breakdown: return QStringLiteral("breakdown");
    case SheetRole::None: break;
    }
    return {};
}

SheetRole parseRole(const QString& name)
{
    if (name == QLatin1String("key"))
        return SheetRole::Key;
    if (name == QLatin1String("breakdown"))
        return SheetRole::Breakdown;
    return SheetRole::None;
}

SheetRole roleOf(const SheetRoles& roles, uint64_t id)
{
    const auto it = roles.find(id);
    return it == roles.end() ? SheetRole::None : it->second;
}

void drawRoleMark(QPainter& p, const QRectF& box, int number, SheetRole role, const QFont& font, const QColor& color,
                  double line)
{
    const QString text = QString::number(number);
    p.setFont(font);
    p.setPen(color);
    p.drawText(box, Qt::AlignCenter, text);
    const QFontMetricsF metrics(font);
    if (role == SheetRole::Key) {
        const double r = std::min(box.width(), box.height()) / 2 - line / 2;
        p.setPen(QPen(color, line));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(box.center(), r, r);
    } else if (role == SheetRole::Breakdown) {
        const double w = metrics.horizontalAdvance(text) + metrics.averageCharWidth() * 0.6;
        const double y = box.center().y() + metrics.capHeight() / 2 + line * 2.5;
        p.setPen(QPen(color, line, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(box.center().x() - w / 2, y), QPointF(box.center().x() + w / 2, y));
    }
}

QImage paintSheetMark(int number, SheetRole role, int size)
{
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    QFont f;
    f.setFamilies({QString::fromLatin1(appkit::theme::kFuenteMono), QStringLiteral("Consolas")});
    f.setPixelSize(std::max(1, int(std::lround(size * 26.0 / 64))));
    f.setWeight(QFont::Medium);
    // Medidas de la mesa: caja de 64, círculo de radio 24, trazo de 2.
    const double k = size / 64.0;
    drawRoleMark(p, QRectF(8 * k, 8 * k, 48 * k, 48 * k), number, role, f, Qt::white, 2 * k);
    return image;
}

} // namespace lienzo
