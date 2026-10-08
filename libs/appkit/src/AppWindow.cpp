#include "appkit/AppWindow.h"

#include <paintcore/CanvasWidget.h>

#include <QVBoxLayout>

namespace appkit {

AppWindow::AppWindow(QWidget* parent)
    : QWidget(parent)
    , m_canvas(new paintcore::CanvasWidget(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_canvas);
}

} // namespace appkit
