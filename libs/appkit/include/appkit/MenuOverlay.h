#pragma once

#include <QList>
#include <QString>
#include <QWidget>

#include <functional>

namespace appkit {

struct MenuItem {
    QString id;
    QString title;
    bool enabled = true;
    bool featured = false; // destacado (el modo mixto, arriba)
    QString note;          // texto secundario a la derecha ("llega en HU-20")
};

struct MenuGroup {
    QString title; // vacío: sin encabezado
    QList<MenuItem> items;
};

// Menú superpuesto (HU-11, mesa "Menú" del diseño): grupos de ítems, el actual en ámbar.
// Es una ventana propia sin borde, como el selector de lápices: un widget hijo quedaría
// tapado por la ventana nativa del lienzo (decisión 5 del 9 de octubre). Se elige con clic o
// el lápiz, o con flechas y Enter (los ítems deshabilitados se saltean); Esc o la tecla que
// lo abre (closeKey) lo cierran.
class MenuOverlay : public QWidget {
public:
    MenuOverlay(QWidget* owner, QString title, Qt::Key closeKey);

    void setGroups(const QList<MenuGroup>& groups);
    void setCurrent(const QString& id);
    QString current() const { return m_current; }

    std::function<void(const QString&)> onPick; // eligió un ítem habilitado
    std::function<void()> onClose;              // se cerró sin elegir

    static constexpr int kWidth = 420;
    static constexpr int kRowHeight = 48; // ≥ objetivo táctil

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    struct Row {
        int group = -1; // fila de encabezado si item < 0
        int item = -1;
        QRect rect;
    };
    void layoutRows();
    const MenuItem* itemOf(int row) const;
    int rowAt(QPoint point) const;
    void moveCursor(int step);
    void pick(int row);

    QString m_title;
    Qt::Key m_closeKey;
    QList<MenuGroup> m_groups;
    QList<Row> m_rows;
    QString m_current;
    int m_cursor = -1;
    int m_hover = -1;
};

} // namespace appkit
