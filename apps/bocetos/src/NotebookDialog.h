#pragma once

#include "BocetosCore.h"

#include <QWidget>

#include <functional>

class QButtonGroup;
class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QSlider;
class QSpinBox;

// "Cuaderno nuevo" y "Editar cuaderno" (HU-136, mesa "Bocetos · cuaderno nuevo"): un panel sobre la
// biblioteca atenuada, dentro de la ventana. Nombre, colección (un desplegable editable: escribir
// una nueva la crea), hojas, tapa y papel (desplegable con muestra, grano, textura y una muestra
// grande). Al editar, el papel y las hojas se ven pero no cambian. Enter acepta, Esc cancela.
class NotebookDialog : public QWidget {
public:
    explicit NotebookDialog(QWidget* parent);

    enum class Mode { Create, Edit };
    // `focusCollection`: Ctrl+M (mover de colección) abre con el foco ahí.
    void open(Mode mode, const bocetos::Draft& draft, const QStringList& collections, bool focusCollection = false);
    bocetos::Draft draft() const;

    std::function<void(const bocetos::Draft&)> onAccept;
    std::function<void()> onCancel;

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void applyPreset(int index);
    void updateSample();
    void accept();
    void cancel();

    Mode m_mode = Mode::Create;
    QFrame* m_panel = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_footer = nullptr;
    QLineEdit* m_name = nullptr;
    QComboBox* m_collection = nullptr;
    QSpinBox* m_sheets = nullptr;
    QButtonGroup* m_covers = nullptr;
    QComboBox* m_paper = nullptr;
    QSlider* m_grain = nullptr;
    QSlider* m_texture = nullptr;
    QLabel* m_grainValue = nullptr;
    QLabel* m_textureValue = nullptr;
    QWidget* m_sample = nullptr;
    QString m_preset;
};
