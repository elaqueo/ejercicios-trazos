#include "exercises/StackExercise.h"

#include <algorithm>

namespace ejercicios {

appkit::Param sheetsParam(int defaultSheets)
{
    return {.key = kSheetsParam, .label = QStringLiteral("Hojas"), .type = appkit::Param::Type::Integer,
            .minimum = kMinSheets, .maximum = kMaxSheets, .step = 1,
            .defaultValue = std::clamp(defaultSheets, kMinSheets, kMaxSheets)};
}

int sheetCount(const QVariantMap& params)
{
    return std::clamp(params.value(kSheetsParam, kMinSheets).toInt(), kMinSheets, kMaxSheets);
}

appkit::Param keysParam()
{
    return {.key = kKeysParam, .label = QStringLiteral("Ghost solo en las keys (la primera, la del medio y la última)"),
            .type = appkit::Param::Type::Toggle, .defaultValue = false};
}

bool keysOnly(const QVariantMap& params)
{
    return params.value(kKeysParam, false).toBool();
}

QList<int> keySheets(int count)
{
    QList<int> keys;
    for (const int i : {0, count / 2, count - 1})
        if (i >= 0 && !keys.contains(i))
            keys.append(i);
    return keys;
}

SameOnEverySheet::SameOnEverySheet(const Exercise& exercise, int defaultSheets)
    : m_exercise(exercise)
    , m_defaultSheets(defaultSheets)
{
}

QList<appkit::Param> SameOnEverySheet::params() const
{
    QList<appkit::Param> all{sheetsParam(m_defaultSheets), keysParam()};
    all.append(m_exercise.params());
    return all;
}

StackGenerated SameOnEverySheet::generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const
{
    const Generated one = m_exercise.generate(params, seed, zone);
    StackGenerated out;
    for (int i = 0; i < sheetCount(params); ++i)
        out.sheets.append(one);
    return out;
}

} // namespace ejercicios
