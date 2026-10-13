#pragma once

#include <QString>
#include <QTimer>
#include <QWidget>

// HU-138: el cartel de Bocetos, con el estilo de la confirmación de la suite. Ocupado ("Dibujando
// en Cartuchera"): tapa la ventana atenuada y no deja tocar nada hasta que se esconde. Aviso (un
// error): solo el cartel, se va solo a los 6 s o con cualquier tecla, y deja pasar el mouse.
class Notice : public QWidget {
public:
    explicit Notice(QWidget* parent);

    void busy(const QString& title, const QString& detail);
    void warn(const QString& title, const QString& detail);
    bool isBusy() const { return isVisible() && m_busy; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void present(const QString& title, const QString& detail, bool busy);

    QString m_title, m_detail;
    bool m_busy = false;
    QTimer m_timer;
};
