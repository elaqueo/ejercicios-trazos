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

SameOnEverySheet::SameOnEverySheet(const Exercise& exercise, int defaultSheets)
    : m_exercise(exercise)
    , m_defaultSheets(defaultSheets)
{
}

QList<appkit::Param> SameOnEverySheet::params() const
{
    QList<appkit::Param> all{sheetsParam(m_defaultSheets)};
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
