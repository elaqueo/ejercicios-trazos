#include "appkit/Param.h"

#include <algorithm>
#include <cmath>

namespace appkit {

QVariant Param::clamp(const QVariant& value) const
{
    const QVariant v = value.isValid() ? value : defaultValue;
    switch (type) {
    case Type::Toggle:
        return v.toBool();
    case Type::Integer: {
        const double s = step >= 1 ? std::round(step) : 1.0;
        const double snapped = minimum + std::round((v.toDouble() - minimum) / s) * s;
        return int(std::lround(std::clamp(snapped, minimum, maximum)));
    }
    case Type::Real:
    default: {
        const double snapped = step > 0 ? minimum + std::round((v.toDouble() - minimum) / step) * step : v.toDouble();
        return std::clamp(snapped, minimum, maximum);
    }
    }
}

QVariantMap defaultValues(const QList<Param>& params)
{
    QVariantMap values;
    for (const Param& p : params)
        values.insert(p.key, p.clamp(p.defaultValue));
    return values;
}

QVariantMap clampValues(const QList<Param>& params, const QVariantMap& values)
{
    QVariantMap out;
    for (const Param& p : params)
        out.insert(p.key, p.clamp(values.value(p.key)));
    return out;
}

} // namespace appkit
