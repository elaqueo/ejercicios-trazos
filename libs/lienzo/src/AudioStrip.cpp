#include "lienzo/AudioStrip.h"

#include <appkit/Theme.h>

#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace lienzo {

namespace {

// Medidas de la mesa "Audio" (px a escala 1).
constexpr int kWidth = 520, kHeight = 130, kMargin = 16, kGap = 24, kMinWidth = 360;
constexpr int kPadX = 16, kPadY = 12, kHeader = 20, kHeaderGap = 8;

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

QRect audioStripRect(int clientWidth, int clientHeight, const QRect& sheet, int rightLimit, double scale)
{
    const auto px = [&](double v) { return int(std::lround(v * scale)); };
    const int h = px(kHeight);
    const int gap = rightLimit - (sheet.right() + 1);
    if (gap >= px(kMinWidth) + 2 * px(kGap)) {
        const int w = std::min(px(kWidth), gap - 2 * px(kGap));
        return QRect(sheet.right() + 1 + px(kGap), clientHeight - px(kMargin) - h, w, h);
    }
    const int w = std::min(px(kWidth), std::max(1, std::min(sheet.width(), clientWidth) - 2 * px(kMargin)));
    const int bottom = std::min(clientHeight, sheet.bottom() + 1);
    return QRect(sheet.left() + px(kMargin), bottom - px(kMargin) - h, w, h);
}

QRect audioWaveRect(const QSize& strip, double scale)
{
    const auto px = [&](double v) { return int(std::lround(v * scale)); };
    const int top = px(kPadY + kHeader + kHeaderGap);
    return QRect(px(kPadX), top, std::max(1, strip.width() - 2 * px(kPadX)), std::max(1, strip.height() - top - px(kPadY)));
}

QString audioTime(double seconds)
{
    const int tenths = int(std::lround(std::max(0.0, seconds) * 10));
    return QStringLiteral("%1:%2,%3")
        .arg(tenths / 600)
        .arg((tenths / 10) % 60, 2, 10, QLatin1Char('0'))
        .arg(tenths % 10);
}

int addMark(std::vector<double>& marks, double seconds)
{
    const auto it = std::upper_bound(marks.begin(), marks.end(), seconds);
    return int(marks.insert(it, seconds) - marks.begin());
}

int moveMark(std::vector<double>& marks, int index, double seconds)
{
    if (index < 0 || index >= int(marks.size()))
        return -1;
    marks.erase(marks.begin() + index);
    return addMark(marks, seconds);
}

int nearestMark(const std::vector<double>& marks, double seconds)
{
    int best = -1;
    for (int i = 0; i < int(marks.size()); ++i)
        if (best < 0 || std::abs(marks[size_t(i)] - seconds) < std::abs(marks[size_t(best)] - seconds))
            best = i;
    return best;
}

int markAt(const std::vector<double>& marks, double x, const QRect& wave, double duration, double tolerance)
{
    if (duration <= 0)
        return -1;
    int best = -1;
    double bestDistance = tolerance;
    for (int i = 0; i < int(marks.size()); ++i) {
        const double d = std::abs(wave.left() + wave.width() * marks[size_t(i)] / duration - x);
        if (d <= bestDistance) {
            best = i;
            bestDistance = d;
        }
    }
    return best;
}

double timeAt(double x, const QRect& wave, double duration)
{
    return std::clamp((x - wave.left()) / std::max(1, wave.width()), 0.0, 1.0) * std::max(0.0, duration);
}

QImage paintAudioStrip(const AudioStripInfo& info, const QSize& size, double scale, const QColor& outside)
{
    namespace theme = appkit::theme;
    const auto px = [&](double v) { return v * scale; };
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(outside);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    const double w = size.width(), h = size.height();
    QPainterPath shape;
    shape.addRoundedRect(QRectF(0.5, 0.5, w - 1, h - 1), px(theme::kRadioPanel), px(theme::kRadioPanel));
    p.fillPath(shape, theme::kPanel);
    p.setPen(QPen(theme::kBorde, 1));
    p.drawPath(shape);

    // "AUDIO", el nombre y la posición / duración.
    const double left = px(kPadX), right = w - px(kPadX), top = px(kPadY);
    const QFont label = font(false, px(12), QFont::DemiBold), body = font(false, px(13)), mono = font(true, px(13));
    p.setFont(label);
    p.setPen(theme::kTextoSecundario);
    const double labelW = QFontMetricsF(label).horizontalAdvance(QStringLiteral("AUDIO"));
    p.drawText(QRectF(left, top, labelW + 1, px(kHeader)), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("AUDIO"));
    QString time;
    if (!info.name.isEmpty() && info.problem.isEmpty())
        time = audioTime(info.position) + QStringLiteral(" / ") + audioTime(info.seconds);
    const double timeW = QFontMetricsF(mono).horizontalAdvance(time);
    p.setFont(mono);
    p.drawText(QRectF(right - timeW, top, timeW + 1, px(kHeader)), Qt::AlignRight | Qt::AlignVCenter, time);
    const double nameX = left + labelW + px(10), nameW = std::max(0.0, right - timeW - px(10) - nameX);
    p.setFont(body);
    p.setPen(theme::kTexto);
    p.drawText(QRectF(nameX, top, nameW, px(kHeader)), Qt::AlignLeft | Qt::AlignVCenter,
               QFontMetricsF(body).elidedText(info.name.isEmpty() ? QStringLiteral("sin audio") : info.name, Qt::ElideMiddle, nameW));

    // La onda, o por qué no está.
    const QRect wave = audioWaveRect(size, scale);
    if (info.name.isEmpty() || !info.problem.isEmpty() || info.peaks.empty() || info.loading) {
        p.setFont(body);
        p.setPen(theme::kTextoSecundario);
        p.drawText(wave, Qt::AlignCenter,
                   info.name.isEmpty()       ? QStringLiteral("A elige un WAV o MP3")
                   : info.loading            ? QStringLiteral("leyendo…")
                   : !info.problem.isEmpty() ? QStringLiteral("No se pudo leer: %1").arg(info.problem)
                                             : QString());
        return image;
    }
    const double mid = wave.top() + wave.height() / 2.0, half = wave.height() / 2.0 - 1;
    const double cursor = info.seconds > 0 ? wave.left() + wave.width() * std::clamp(info.position / info.seconds, 0.0, 1.0) : wave.left();
    const QColor ink = theme::kMarcaHoja, played = ink.lighter(140);
    p.setRenderHint(QPainter::Antialiasing, false);
    for (int i = 0; i < int(info.peaks.size()) && i < wave.width(); ++i) {
        const double x = wave.left() + i, a = std::max(0.5, double(info.peaks[size_t(i)]) * half);
        p.setPen(x < cursor ? played : ink);
        p.drawLine(QPointF(x, mid - a), QPointF(x, mid + a));
    }
    // Las marcas (HU-108): línea turquesa y su número arriba, a la derecha.
    const QFont number = font(true, px(11));
    p.setFont(number);
    for (int i = 0; i < int(info.marks.size()); ++i) {
        const double x = wave.left() + wave.width() * std::clamp(info.marks[size_t(i)] / info.seconds, 0.0, 1.0);
        p.setPen(QPen(theme::kMesaLuzSiguiente, std::max(1.0, px(1.5))));
        p.drawLine(QPointF(x, wave.top() + px(12)), QPointF(x, wave.bottom()));
        p.drawText(QRectF(x + px(3), wave.top(), px(30), px(12)), Qt::AlignLeft | Qt::AlignVCenter, QString::number(i + 1));
    }
    p.setPen(QPen(theme::kSeleccionTexto, std::max(1.0, px(2))));
    p.drawLine(QPointF(cursor, wave.top()), QPointF(cursor, wave.bottom()));
    return image;
}

} // namespace lienzo
