#include "lienzo/Media.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>

namespace lienzo {

Lead Lead::clamped() const
{
    return {std::clamp(softness, 1, 255), std::clamp(diameter, 30, 200), std::clamp(ceiling, 2000, 65535),
            std::clamp(halfAngle, 5, 30), std::clamp(minAltitude, 10, 80), (std::clamp(burnish, 0, 20000) + 50) / 100 * 100,
            std::clamp(wear, 0, 60)};
}

drymedia::Medium Lead::medium() const
{
    const Lead l = clamped();
    drymedia::Medium m = drymedia::Medium::hb().withLeadDiameter(l.diameter / 100.0);
    m.softness = uint16_t(l.softness);
    m.ceiling = uint16_t(l.ceiling);
    m.coneHalfAngleDeg = l.halfAngle;
    m.minTabletAltitudeDeg = l.minAltitude;
    m.burnishRate = uint16_t(l.burnish);
    m.wearRate = uint16_t(l.wear);
    return m;
}

uint64_t Lead::pack() const
{
    const Lead l = clamped();
    // Blandura ≤ 255, diámetro ≤ 200 y bruñido / 100 ≤ 200 entran en 8 bits.
    return uint64_t(l.softness) | uint64_t(l.diameter) << 8 | uint64_t(l.ceiling) << 16 | uint64_t(l.halfAngle) << 32 |
           uint64_t(l.minAltitude) << 40 | uint64_t(l.burnish / 100) << 48 | uint64_t(l.wear) << 56;
}

Lead Lead::unpack(uint64_t packed)
{
    return {int(packed & 0xff),       int(packed >> 8 & 0xff),  int(packed >> 16 & 0xffff),
            int(packed >> 32 & 0xff), int(packed >> 40 & 0xff), int(packed >> 48 & 0xff) * 100,
            int(packed >> 56 & 0xff)};
}

Grades factoryGrades()
{
    // Duras: manda el techo (se quedan en gris). Blandas: manda la blandura (llegan al
    // oscuro con menos presión y menos pasadas). Calibradas con la tableta el 10 de octubre
    // de 2026 (HU-57); B a 4B quedaron con los valores interpolados.
    // Bruñido (HU-60): de la 2H, que bruñe mucho, a la 6B; a calibrar con la tableta.
    constexpr std::array<int, kGradeCount> burnish{6000, 5000, 4000, 3000, 2500, 2000, 1500, 1200, 1000, 800};
    Grades grades{{
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
    for (int g = 0; g < kGradeCount; ++g)
        grades[size_t(g)].burnish = burnish[size_t(g)];
    return grades;
}

Eraser Eraser::clamped() const
{
    return {std::clamp(strength, 1, 255), std::clamp(diameter, 200, 800)};
}

drymedia::Medium Eraser::medium() const
{
    const Eraser e = clamped();
    return drymedia::Medium::eraser(e.diameter / 100.0, uint16_t(e.strength));
}

uint64_t Eraser::pack() const
{
    const Eraser e = clamped();
    return uint64_t(e.strength) | uint64_t(e.diameter) << 16;
}

Eraser Eraser::unpack(uint64_t packed)
{
    return {int(packed & 0xffff), int(packed >> 16 & 0xffff)};
}

bool mediaFromJson(const QByteArray& json, MediaSet& media, QString* error)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error)
            *error = parseError.error != QJsonParseError::NoError ? parseError.errorString()
                                                                  : QStringLiteral("no es un objeto JSON");
        return false;
    }
    MediaSet result;
    const QJsonArray leads = doc.object().value(QStringLiteral("minas")).toArray();
    for (const QJsonValue& value : leads) {
        const QJsonObject o = value.toObject();
        const QString name = o.value(QStringLiteral("nombre")).toString();
        const auto it = std::find_if(kGradeNames.begin(), kGradeNames.end(),
                                     [&](const char* n) { return name == QLatin1String(n); });
        if (it == kGradeNames.end())
            continue; // dureza desconocida: se ignora
        Lead& lead = result.grades[size_t(it - kGradeNames.begin())];
        lead.softness = o.value(QStringLiteral("blandura")).toInt(lead.softness);
        lead.diameter = int(std::lround(o.value(QStringLiteral("diametroMm")).toDouble(lead.diameter / 100.0) * 100));
        lead.ceiling = o.value(QStringLiteral("techo")).toInt(lead.ceiling);
        lead.burnish = o.value(QStringLiteral("bruñido")).toInt(lead.burnish);
        lead = lead.clamped();
    }
    const QJsonObject goma = doc.object().value(QStringLiteral("goma")).toObject();
    result.eraser.strength = goma.value(QStringLiteral("fuerza")).toInt(result.eraser.strength);
    result.eraser.diameter =
        int(std::lround(goma.value(QStringLiteral("diametroMm")).toDouble(result.eraser.diameter / 100.0) * 100));
    result.eraser = result.eraser.clamped();
    const QJsonObject punta = doc.object().value(QStringLiteral("punta")).toObject();
    for (Lead& lead : result.grades) {
        lead.halfAngle = punta.value(QStringLiteral("semianguloGrados")).toInt(lead.halfAngle);
        lead.minAltitude = punta.value(QStringLiteral("altitudMinimaTableta")).toInt(lead.minAltitude);
        lead.wear = punta.value(QStringLiteral("desgaste")).toInt(lead.wear);
        lead = lead.clamped();
    }
    media = result;
    return true;
}

QByteArray mediaToJson(const MediaSet& media)
{
    QJsonArray leads;
    for (int i = 0; i < kGradeCount; ++i) {
        const Lead& l = media.grades[size_t(i)];
        leads.append(QJsonObject{{QStringLiteral("nombre"), QLatin1String(kGradeNames[size_t(i)])},
                                 {QStringLiteral("blandura"), l.softness},
                                 {QStringLiteral("diametroMm"), l.diameter / 100.0},
                                 {QStringLiteral("techo"), l.ceiling},
                                 {QStringLiteral("bruñido"), l.burnish}});
    }
    const QJsonObject root{{QStringLiteral("version"), 1},
                           {QStringLiteral("nota"), QStringLiteral("techo: depósito máximo, de 2000 a 65535 (negro)")},
                           {QStringLiteral("minas"), leads},
                           {QStringLiteral("punta"),
                            QJsonObject{{QStringLiteral("semianguloGrados"), media.grades[kHbIndex].halfAngle},
                                        {QStringLiteral("altitudMinimaTableta"), media.grades[kHbIndex].minAltitude},
                                        {QStringLiteral("desgaste"), media.grades[kHbIndex].wear}}},
                           {QStringLiteral("goma"), QJsonObject{{QStringLiteral("fuerza"), media.eraser.strength},
                                                                {QStringLiteral("diametroMm"), media.eraser.diameter / 100.0}}}};
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

} // namespace lienzo
