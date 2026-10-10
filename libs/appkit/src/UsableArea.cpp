#include "appkit/UsableArea.h"

#include "appkit/Config.h"

#include <QVariantList>

#include <cmath>

namespace appkit {

namespace {

const QString kAreasKey = QStringLiteral("usableAreas");
const QString kMonitorKey = QStringLiteral("monitor");
const QString kRectKey = QStringLiteral("rect");

bool sameMonitor(const QVariant& stored, const ScreenId& monitor)
{
    const auto id = ScreenId::fromVariant(stored);
    return id && findScreen({monitor}, *id).has_value();
}

} // namespace

QRect rectFromCorners(QPointF a, QPointF b)
{
    const int left = int(std::floor(qMin(a.x(), b.x())));
    const int top = int(std::floor(qMin(a.y(), b.y())));
    const int right = int(std::ceil(qMax(a.x(), b.x())));
    const int bottom = int(std::ceil(qMax(a.y(), b.y())));
    return QRect(QPoint(left, top), QPoint(right, bottom));
}

QRect scaledRect(const QRect& rect, double factor)
{
    const int left = int(std::lround(rect.x() * factor)), top = int(std::lround(rect.y() * factor));
    const int right = int(std::lround((rect.x() + rect.width()) * factor));
    const int bottom = int(std::lround((rect.y() + rect.height()) * factor));
    return QRect(left, top, right - left, bottom - top);
}

QRect sidePanelRect(const QRect& area, int width, int margin)
{
    const int w = qMax(0, qMin(width, area.width() - 2 * margin));
    return QRect(area.right() + 1 - margin - w, area.top() + margin, w, qMax(0, area.height() - 2 * margin));
}

std::optional<QRect> loadUsableArea(const Config& config, const ScreenId& monitor)
{
    const QVariantList areas = config.value(kAreasKey, {}, Config::Scope::Common).toList();
    for (const QVariant& entry : areas) {
        const QVariantMap map = entry.toMap();
        const QVariantList r = map.value(kRectKey).toList();
        if (r.size() == 4 && sameMonitor(map.value(kMonitorKey), monitor)) {
            const QRect rect(r[0].toInt(), r[1].toInt(), r[2].toInt(), r[3].toInt());
            if (rect.isValid())
                return rect;
        }
    }
    return std::nullopt;
}

void saveUsableArea(Config& config, const ScreenId& monitor, const QRect& rect)
{
    QVariantList areas = config.value(kAreasKey, {}, Config::Scope::Common).toList();
    const QVariantMap entry{{kMonitorKey, monitor.toVariant()},
                            {kRectKey, QVariantList{rect.x(), rect.y(), rect.width(), rect.height()}}};
    bool replaced = false;
    for (QVariant& existing : areas) {
        if (sameMonitor(existing.toMap().value(kMonitorKey), monitor)) {
            existing = entry;
            replaced = true;
        }
    }
    if (!replaced)
        areas.append(entry);
    config.setValue(kAreasKey, areas, Config::Scope::Common);
}

} // namespace appkit
