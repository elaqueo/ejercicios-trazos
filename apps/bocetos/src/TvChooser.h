#pragma once

#include <cast/Dlna.h>

#include <QList>
#include <QWidget>

#include <functional>

// HU-145, mesa "Bocetos · elegir TV": con más de un TV en la red, la lista sobre el cuaderno
// atenuado. ↑ ↓ eligen, Enter muestra en el elegido, Esc cancela (tocar uno también lo elige).
class TvChooser : public QWidget {
public:
    explicit TvChooser(QWidget* parent);

    void choose(const QList<cast::Renderer>& renderers);

    std::function<void(const cast::Renderer&)> onPick;
    std::function<void()> onCancel;

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    QRectF box() const;
    QRectF row(int i) const;
    void finish(bool picked);

    QList<cast::Renderer> m_renderers;
    int m_selected = 0;
};
