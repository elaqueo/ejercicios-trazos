#include "Paint.h"

#include <appkit/Theme.h>

#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace theme = appkit::theme;

namespace bocetos {

QFont themeFont(bool mono, double px, int weight)
{
    QFont f;
    f.setFamilies(mono ? QStringList{QString::fromLatin1(theme::kFuenteMono), QStringLiteral("Consolas")}
                       : QStringList{QString::fromLatin1(theme::kFuente), QStringLiteral("Segoe UI")});
    f.setPixelSize(std::max(1, int(std::lround(px))));
    f.setWeight(QFont::Weight(weight));
    return f;
}

double drawKey(QPainter& p, double x, double y, const QString& key)
{
    const QFont mono = themeFont(true, 13);
    const double w = std::max(28.0, QFontMetricsF(mono).horizontalAdvance(key) + 16);
    const QRectF box(x, y, w, 28);
    QPainterPath path;
    path.addRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), theme::kRadioTecla, theme::kRadioTecla);
    p.fillPath(path, theme::kPanelAlto);
    p.setPen(QPen(theme::kBorde, 1));
    p.drawPath(path);
    p.setFont(mono);
    p.setPen(theme::kTexto);
    p.drawText(box, Qt::AlignCenter, key);
    return w;
}

} // namespace bocetos
