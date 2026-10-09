#include <appkit/ParamForm.h>

#include <QCheckBox>
#include <QSlider>
#include <QTest>

using appkit::Param;
using appkit::ParamForm;

namespace {

QList<Param> params()
{
    return {
        {.key = QStringLiteral("largo"), .label = QStringLiteral("Largo"), .type = Param::Type::Real, .minimum = 0.1, .maximum = 0.9, .step = 0.1, .defaultValue = 0.5},
        {.key = QStringLiteral("puntos"), .label = QStringLiteral("Puntos"), .type = Param::Type::Integer, .minimum = 2, .maximum = 6, .step = 1, .defaultValue = 3},
        {.key = QStringLiteral("guia"), .label = QStringLiteral("Mostrar guía"), .type = Param::Type::Toggle, .defaultValue = true},
    };
}

} // namespace

class TestParamForm : public QObject {
    Q_OBJECT

private slots:
    // Valores llevados al tipo, al rango y al paso; lo que falta toma el de defecto.
    void recortaAlRango()
    {
        const QVariantMap v = appkit::clampValues(params(), {{QStringLiteral("largo"), 1.7},
                                                             {QStringLiteral("puntos"), 0},
                                                             {QStringLiteral("ajena"), 1}});
        QCOMPARE(v.value(QStringLiteral("largo")).toDouble(), 0.9);
        QCOMPARE(v.value(QStringLiteral("puntos")).toInt(), 2);
        QCOMPARE(v.value(QStringLiteral("guia")).toBool(), true);
        QVERIFY(!v.contains(QStringLiteral("ajena")));
        QCOMPARE(params()[0].clamp(0.33).toDouble(), 0.3);
        QCOMPARE(appkit::defaultValues(params()).value(QStringLiteral("puntos")).toInt(), 3);
    }

    // Un control por parámetro, con el rango declarado; mover uno avisa con todos los valores.
    void unControlPorParametro()
    {
        ParamForm form;
        form.setParams(params(), {});
        QSlider* largo = form.slider(QStringLiteral("largo"));
        QSlider* puntos = form.slider(QStringLiteral("puntos"));
        QCheckBox* guia = form.toggle(QStringLiteral("guia"));
        QVERIFY(largo && puntos && guia);
        QCOMPARE(largo->maximum(), 8); // 0,1 a 0,9 en pasos de 0,1
        QCOMPARE(puntos->maximum(), 4);
        QCOMPARE(largo->value(), 4);

        QVariantMap avisado;
        form.onChanged = [&](const QVariantMap& v) { avisado = v; };
        largo->setValue(largo->maximum());
        QCOMPARE(avisado.value(QStringLiteral("largo")).toDouble(), 0.9);
        QCOMPARE(avisado.value(QStringLiteral("puntos")).toInt(), 3);
        guia->setChecked(false);
        QCOMPARE(avisado.value(QStringLiteral("guia")).toBool(), false);
    }

    // setValues mueve los controles sin avisar.
    void refrescarNoAvisa()
    {
        ParamForm form;
        form.setParams(params(), {});
        int avisos = 0;
        form.onChanged = [&](const QVariantMap&) { ++avisos; };
        form.setValues({{QStringLiteral("puntos"), 5}});
        QCOMPARE(avisos, 0);
        QCOMPARE(form.slider(QStringLiteral("puntos"))->value(), 3);
        QCOMPARE(form.values().value(QStringLiteral("puntos")).toInt(), 5);
    }
};

QTEST_MAIN(TestParamForm)
#include "tst_paramform.moc"
