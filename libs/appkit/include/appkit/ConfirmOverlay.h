#pragma once

#include <QString>
#include <QWidget>

#include <functional>

namespace appkit {

// Confirmación de una tecla (HU-87, mesa "Confirmación" del canvas de diseño): un cartel
// sobre el lienzo con una pregunta, un detalle y la tecla que confirma; esa tecla confirma y
// cualquier otra (Esc incluida) cancela. Como el menú, es una ventana propia sin borde (un
// widget hijo quedaría tapado por la ventana nativa del lienzo).
class ConfirmOverlay : public QWidget {
public:
    explicit ConfirmOverlay(QWidget* owner);

    // title: "¿Borrar la hoja 7?" · detail: "Tiene trazos. Se puede deshacer con Z." · key:
    // la tecla que confirma, con su nombre ("Supr") y lo que hace ("la borra").
    void ask(const QString& title, const QString& detail, Qt::Key key, const QString& keyName, const QString& action);

    std::function<void()> onConfirm;
    std::function<void()> onCancel;

    static constexpr int kWidth = 520;

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    void finish(bool confirmed);

    QString m_title, m_detail, m_keyName, m_action;
    Qt::Key m_key = Qt::Key_unknown;
    bool m_finishing = false; // hide() pierde el foco y volvería a cancelar (HU-136)
};

} // namespace appkit
