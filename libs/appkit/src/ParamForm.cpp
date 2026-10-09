#include "appkit/ParamForm.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>

#include <cmath>

namespace appkit {

ParamForm::ParamForm(QWidget* parent)
    : QWidget(parent)
    , m_layout(new QGridLayout(this))
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setHorizontalSpacing(12);
    m_layout->setVerticalSpacing(8);
    m_layout->setColumnStretch(0, 1);
}

int ParamForm::toSlider(const Param& param, const QVariant& value) const
{
    const double step = param.type == Param::Type::Integer ? std::max(1.0, std::round(param.step)) : param.step;
    return int(std::lround((param.clamp(value).toDouble() - param.minimum) / step));
}

QVariant ParamForm::fromSlider(const Param& param, int position) const
{
    const double step = param.type == Param::Type::Integer ? std::max(1.0, std::round(param.step)) : param.step;
    return param.clamp(param.minimum + position * step);
}

QString ParamForm::text(const Param& param, const QVariant& value) const
{
    const QString number = param.type == Param::Type::Integer ? QString::number(value.toInt())
                                                              : QString::number(value.toDouble(), 'f', param.decimals);
    return number + param.suffix;
}

void ParamForm::setParams(const QList<Param>& params, const QVariantMap& values)
{
    while (QLayoutItem* item = m_layout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    m_controls.clear();
    m_values = clampValues(params, values);
    m_updating = true;
    int row = 0;
    for (const Param& param : params) {
        Control control{param};
        if (param.type == Param::Type::Toggle) {
            control.toggle = new QCheckBox(param.label, this);
            control.toggle->setChecked(m_values.value(param.key).toBool());
            connect(control.toggle, &QCheckBox::toggled, this, [this, key = param.key](bool on) {
                if (m_updating)
                    return;
                m_values.insert(key, on);
                if (onChanged)
                    onChanged(m_values);
            });
            m_layout->addWidget(control.toggle, row++, 0, 1, 2);
        } else {
            m_layout->addWidget(new QLabel(param.label, this), row, 0);
            control.value = new QLabel(this);
            control.value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_layout->addWidget(control.value, row++, 1);
            control.slider = new QSlider(Qt::Horizontal, this);
            control.slider->setRange(0, toSlider(param, param.maximum));
            control.slider->setMinimumHeight(32); // cómodo con el lápiz
            control.slider->setValue(toSlider(param, m_values.value(param.key)));
            control.value->setText(text(param, m_values.value(param.key)));
            const int index = int(m_controls.size());
            connect(control.slider, &QSlider::valueChanged, this, [this, index](int position) {
                Control& c = m_controls[index];
                const QVariant value = fromSlider(c.param, position);
                c.value->setText(text(c.param, value));
                if (m_updating)
                    return;
                m_values.insert(c.param.key, value);
                if (onChanged)
                    onChanged(m_values);
            });
            m_layout->addWidget(control.slider, row++, 0, 1, 2);
        }
        m_controls.append(control);
    }
    m_layout->setRowStretch(row, 1);
    m_updating = false;
}

void ParamForm::setValues(const QVariantMap& values)
{
    m_updating = true;
    for (Control& control : m_controls) {
        const QVariant value = control.param.clamp(values.value(control.param.key, m_values.value(control.param.key)));
        m_values.insert(control.param.key, value);
        display(control);
    }
    m_updating = false;
}

void ParamForm::display(Control& control)
{
    const QVariant value = m_values.value(control.param.key);
    if (control.toggle)
        control.toggle->setChecked(value.toBool());
    if (control.slider) {
        control.slider->setValue(toSlider(control.param, value));
        control.value->setText(text(control.param, value));
    }
}

QSlider* ParamForm::slider(const QString& key) const
{
    for (const Control& control : m_controls)
        if (control.param.key == key)
            return control.slider;
    return nullptr;
}

QCheckBox* ParamForm::toggle(const QString& key) const
{
    for (const Control& control : m_controls)
        if (control.param.key == key)
            return control.toggle;
    return nullptr;
}

} // namespace appkit
