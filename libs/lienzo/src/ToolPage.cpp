#include "lienzo/ToolPage.h"

#include <appkit/ParamForm.h>

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <cmath>

namespace lienzo {

namespace {

const QString kSoftness = QStringLiteral("blandura");
const QString kDiameter = QStringLiteral("diametro");
const QString kCeiling = QStringLiteral("techo");
const QString kStrength = QStringLiteral("fuerza");

using Type = appkit::Param::Type;

QLabel* sectionTitle(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    return label;
}

} // namespace

QList<appkit::Param> leadParams()
{
    // Los mismos rangos que las teclas de calibración (Lienzo::registerShortcuts).
    return {
        {.key = kSoftness, .label = QStringLiteral("Blandura"), .type = Type::Integer, .minimum = 1, .maximum = 255, .step = 1, .defaultValue = 20},
        {.key = kDiameter, .label = QStringLiteral("Diámetro"), .type = Type::Real, .minimum = 0.30, .maximum = 2.00, .step = 0.01, .defaultValue = 0.87, .decimals = 2, .suffix = QStringLiteral(" mm")},
        {.key = kCeiling, .label = QStringLiteral("Techo de tono"), .type = Type::Integer, .minimum = 4, .maximum = 100, .step = 1, .defaultValue = 100, .suffix = QStringLiteral(" %")},
    };
}

QVariantMap leadValues(const Lead& lead)
{
    return {{kSoftness, lead.softness},
            {kDiameter, lead.diameter / 100.0},
            {kCeiling, int(std::lround(lead.ceiling * 100.0 / 65535))}};
}

Lead leadFrom(const QVariantMap& values, const Lead& base)
{
    const QVariantMap v = appkit::clampValues(leadParams(), values);
    const QVariantMap before = appkit::clampValues(leadParams(), leadValues(base));
    Lead lead = base;
    if (v.value(kSoftness) != before.value(kSoftness))
        lead.softness = v.value(kSoftness).toInt();
    if (v.value(kDiameter) != before.value(kDiameter))
        lead.diameter = int(std::lround(v.value(kDiameter).toDouble() * 100));
    if (v.value(kCeiling) != before.value(kCeiling))
        lead.ceiling = int(std::lround(v.value(kCeiling).toInt() * 65535.0 / 100));
    return lead.clamped();
}

QList<appkit::Param> eraserParams()
{
    return {
        {.key = kStrength, .label = QStringLiteral("Fuerza"), .type = Type::Integer, .minimum = 1, .maximum = 255, .step = 1, .defaultValue = 20},
        {.key = kDiameter, .label = QStringLiteral("Diámetro"), .type = Type::Real, .minimum = 2.0, .maximum = 8.0, .step = 0.1, .defaultValue = 5.0, .decimals = 1, .suffix = QStringLiteral(" mm")},
    };
}

QVariantMap eraserValues(const Eraser& eraser)
{
    return {{kStrength, eraser.strength}, {kDiameter, eraser.diameter / 100.0}};
}

Eraser eraserFrom(const QVariantMap& values, const Eraser& base)
{
    const QVariantMap v = appkit::clampValues(eraserParams(), values);
    const QVariantMap before = appkit::clampValues(eraserParams(), eraserValues(base));
    Eraser eraser = base;
    if (v.value(kStrength) != before.value(kStrength))
        eraser.strength = v.value(kStrength).toInt();
    if (v.value(kDiameter) != before.value(kDiameter))
        eraser.diameter = int(std::lround(v.value(kDiameter).toDouble() * 100));
    return eraser.clamped();
}

ToolPage::ToolPage(QWidget* parent)
    : QWidget(parent)
    , m_leadTitle(sectionTitle(QString(), this))
    , m_leadForm(new appkit::ParamForm(this))
    , m_saveLead(new QPushButton(QStringLiteral("Guardar la mina (Ctrl+S)"), this))
    , m_eraserForm(new appkit::ParamForm(this))
    , m_saveEraser(new QPushButton(QStringLiteral("Guardar la goma"), this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->addWidget(m_leadTitle);
    auto* hint = new QLabel(QStringLiteral("La mina se elige con F5."), this);
    hint->setObjectName(QStringLiteral("secundario"));
    layout->addWidget(hint);
    layout->addWidget(m_leadForm);
    layout->addWidget(m_saveLead);
    layout->addSpacing(16);
    layout->addWidget(sectionTitle(QStringLiteral("Goma"), this));
    layout->addWidget(m_eraserForm);
    layout->addWidget(m_saveEraser);

    m_leadForm->setParams(leadParams(), leadValues(Lead{}));
    m_eraserForm->setParams(eraserParams(), eraserValues(Eraser{}));
    m_leadForm->onChanged = [this](const QVariantMap& values) {
        m_lead = leadFrom(values, m_lead);
        if (onLeadChanged)
            onLeadChanged(m_lead);
    };
    m_eraserForm->onChanged = [this](const QVariantMap& values) {
        m_eraser = eraserFrom(values, m_eraser);
        if (onEraserChanged)
            onEraserChanged(m_eraser);
    };
    connect(m_saveLead, &QPushButton::clicked, this, [this] {
        if (onSaveLead)
            onSaveLead();
    });
    connect(m_saveEraser, &QPushButton::clicked, this, [this] {
        if (onSaveEraser)
            onSaveEraser();
    });
}

void ToolPage::setTools(const QString& leadName, const Lead& lead, bool leadUnsaved, const Eraser& eraser,
                        bool eraserUnsaved)
{
    m_lead = lead;
    m_eraser = eraser;
    m_leadTitle->setText(QStringLiteral("Mina %1%2").arg(leadName, leadUnsaved ? QStringLiteral(" *") : QString()));
    m_leadForm->setValues(leadValues(lead));
    m_saveLead->setEnabled(leadUnsaved);
    m_eraserForm->setValues(eraserValues(eraser));
    m_saveEraser->setEnabled(eraserUnsaved);
}

} // namespace lienzo
