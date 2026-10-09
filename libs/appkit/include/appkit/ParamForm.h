#pragma once

#include "appkit/Param.h"

#include <QWidget>

#include <functional>

class QCheckBox;
class QGridLayout;
class QLabel;
class QSlider;

namespace appkit {

// Controles generados a partir de una lista de parámetros (HU-12): un deslizador con su
// valor para los números y una casilla para sí/no. Todo dentro del rango declarado.
class ParamForm : public QWidget {
public:
    explicit ParamForm(QWidget* parent = nullptr);

    // Rehace los controles.
    void setParams(const QList<Param>& params, const QVariantMap& values);
    // Mueve los controles sin avisar por onChanged (refrescar desde afuera).
    void setValues(const QVariantMap& values);
    QVariantMap values() const { return m_values; }

    // El usuario cambió un valor; recibe todos los valores.
    std::function<void(const QVariantMap&)> onChanged;

    // Para los tests.
    QSlider* slider(const QString& key) const;
    QCheckBox* toggle(const QString& key) const;

private:
    struct Control {
        Param param;
        QSlider* slider = nullptr;
        QLabel* value = nullptr;
        QCheckBox* toggle = nullptr;
    };
    int toSlider(const Param& param, const QVariant& value) const;
    QVariant fromSlider(const Param& param, int position) const;
    QString text(const Param& param, const QVariant& value) const;
    void display(Control& control);

    QGridLayout* m_layout;
    QList<Control> m_controls;
    QVariantMap m_values;
    bool m_updating = false;
};

} // namespace appkit
