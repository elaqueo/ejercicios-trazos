#include "Media.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>

namespace cartuchera {

Lead Lead::clamped() const
{
    return {std::clamp(softness, 1, 255), std::clamp(diameter, 30, 200), std::clamp(ceiling, 2000, 65535)};
}

drymedia::Medium Lead::medium() const
{
    const Lead l = clamped();
    drymedia::Medium m = drymedia::Medium::hb().withLeadDiameter(l.diameter / 100.0);
    m.softness = uint16_t(l.softness);
    m.ceiling = uint16_t(l.ceiling);
    return m;
}

uint64_t Lead::pack() const
{
    const Lead l = clamped();
    return uint64_t(l.softness) | uint64_t(l.diameter) << 16 | uint64_t(l.ceiling) << 32;
}

Lead Lead::unpack(uint64_t packed)
{
    return {int(packed & 0xffff), int(packed >> 16 & 0xffff), int(packed >> 32 & 0xffff)};
}

Grades factoryGrades()
{
    // Duras: manda el techo (se quedan en gris). Blandas: manda la blandura (llegan al
    // oscuro con menos presión y menos pasadas). Calibradas con la tableta el 10 de octubre
    // de 2026 (HU-57); B a 4B quedaron con los valores interpolados.
    return {{
        {13, 70, 25267},  // 2H
        {14, 75, 32768},  // H
        {15, 80, 40632},  // F
        {20, 87, 65535},  // HB (HU-51)
        {26, 90, 65535},  // B
        {33, 94, 65535},  // 2B
        {41, 98, 65535},  // 3B
        {50, 102, 65535}, // 4B
        {60, 122, 65535}, // 5B
        {70, 145, 65535}, // 6B
    }};
}

bool gradesFromJson(const QByteArray& json, Grades& grades, QString* error)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error)
            *error = parseError.error != QJsonParseError::NoError ? parseError.errorString()
                                                                  : QStringLiteral("no es un objeto JSON");
        return false;
    }
    Grades result = factoryGrades();
    const QJsonArray leads = doc.object().value(QStringLiteral("minas")).toArray();
    for (const QJsonValue& value : leads) {
        const QJsonObject o = value.toObject();
        const QString name = o.value(QStringLiteral("nombre")).toString();
        const auto it = std::find_if(kGradeNames.begin(), kGradeNames.end(),
                                     [&](const char* n) { return name == QLatin1String(n); });
        if (it == kGradeNames.end())
            continue; // dureza desconocida: se ignora
        Lead& lead = result[size_t(it - kGradeNames.begin())];
        lead.softness = o.value(QStringLiteral("blandura")).toInt(lead.softness);
        lead.diameter = int(std::lround(o.value(QStringLiteral("diametroMm")).toDouble(lead.diameter / 100.0) * 100));
        lead.ceiling = o.value(QStringLiteral("techo")).toInt(lead.ceiling);
        lead = lead.clamped();
    }
    grades = result;
    return true;
}

QByteArray gradesToJson(const Grades& grades)
{
    QJsonArray leads;
    for (int i = 0; i < kGradeCount; ++i) {
        const Lead& l = grades[size_t(i)];
        leads.append(QJsonObject{{QStringLiteral("nombre"), QLatin1String(kGradeNames[size_t(i)])},
                                 {QStringLiteral("blandura"), l.softness},
                                 {QStringLiteral("diametroMm"), l.diameter / 100.0},
                                 {QStringLiteral("techo"), l.ceiling}});
    }
    const QJsonObject root{{QStringLiteral("version"), 1},
                           {QStringLiteral("nota"), QStringLiteral("techo: depósito máximo, de 2000 a 65535 (negro)")},
                           {QStringLiteral("minas"), leads}};
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

} // namespace cartuchera
