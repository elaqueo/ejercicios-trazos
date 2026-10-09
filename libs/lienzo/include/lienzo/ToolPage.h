#pragma once

#include "lienzo/Media.h"

#include <appkit/Param.h>

#include <QWidget>

#include <functional>

class QLabel;
class QPushButton;

namespace appkit {
class ParamForm;
}

namespace lienzo {

// Los valores calibrables de la mina y de la goma como parámetros del panel (HU-12), en
// unidades para mirar: diámetro en mm y techo en %. Son los mismos que ajustan [ ] , . - =.
// leadFrom/eraserFrom parten de `base` y cambian solo lo que se movió en el panel: el techo
// en % redondea, y mover la blandura no tiene que tocar un techo calibrado fino.
QList<appkit::Param> leadParams();
QVariantMap leadValues(const Lead& lead);
Lead leadFrom(const QVariantMap& values, const Lead& base);
QList<appkit::Param> eraserParams();
QVariantMap eraserValues(const Eraser& eraser);
Eraser eraserFrom(const QVariantMap& values, const Eraser& base);

// Pestaña "Lápiz" del panel: la mina activa (se elige con F5) y la goma, cada una con su
// botón Guardar (Ctrl+S). Los cambios se aplican en el momento.
class ToolPage : public QWidget {
public:
    explicit ToolPage(QWidget* parent = nullptr);

    // Refresca los controles sin avisar por los callbacks.
    void setTools(const QString& leadName, const Lead& lead, bool leadUnsaved, const Eraser& eraser, bool eraserUnsaved);

    std::function<void(const Lead&)> onLeadChanged;
    std::function<void(const Eraser&)> onEraserChanged;
    std::function<void()> onSaveLead;
    std::function<void()> onSaveEraser;

    appkit::ParamForm* leadForm() const { return m_leadForm; }
    appkit::ParamForm* eraserForm() const { return m_eraserForm; }

private:
    Lead m_lead;
    Eraser m_eraser;
    QLabel* m_leadTitle;
    appkit::ParamForm* m_leadForm;
    QPushButton* m_saveLead;
    appkit::ParamForm* m_eraserForm;
    QPushButton* m_saveEraser;
};

} // namespace lienzo
