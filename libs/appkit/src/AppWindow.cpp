#include "appkit/AppWindow.h"

#include "appkit/Config.h"
#include "appkit/ScreenChoice.h"

#include <paintcore/BrushLibrary.h>
#include <paintcore/BrushSelector.h>
#include <paintcore/CanvasWidget.h>

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QScreen>
#include <QShortcut>
#include <QVBoxLayout>

Q_LOGGING_CATEGORY(lcWindow, "appkit.window")

namespace appkit {

namespace {

constexpr int kSelectorWidth = 440;
constexpr int kSelectorMargin = 16;
const QString kBrushKey = QStringLiteral("brush");
const QString kMonitorKey = QStringLiteral("monitor");

} // namespace

AppWindow::AppWindow(QWidget* parent)
    : QWidget(parent)
    , m_canvas(new paintcore::CanvasWidget(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_canvas);
}

void AppWindow::setupBrushes(const paintcore::BrushLibrary* library, Config* config)
{
    m_library = library;
    m_config = config;

    // Panel superpuesto al lienzo, a la derecha. Cuando lleguen los overlays (HU-11/12)
    // pasa a ser la sección "pincel" del panel de configuración.
    m_brushSelector = new paintcore::BrushSelector(this);
    m_brushSelector->setAutoFillBackground(true);
    m_brushSelector->setLibrary(library);
    m_brushSelector->hide();
    connect(m_brushSelector, &paintcore::BrushSelector::brushSelected, this, [this](const QString& name) {
        applyBrush(name);
        m_config->setValue(kBrushKey, name);
        m_brushSelector->hide();
    });

    auto* toggle = new QShortcut(QKeySequence(kBrushSelectorKey), this);
    connect(toggle, &QShortcut::activated, this, &AppWindow::toggleBrushSelector);
    auto* close = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(close, &QShortcut::activated, m_brushSelector, &QWidget::hide);

    applyBrush(m_config->value(kBrushKey, paintcore::defaultBrushPreset().name).toString());
}

void AppWindow::applyBrush(const QString& name)
{
    const paintcore::BrushPreset* preset = m_library->find(name);
    if (!preset && name != paintcore::defaultBrushPreset().name)
        qCWarning(lcWindow) << "El pincel guardado" << name << "no está cargado; se usa el pincel por defecto";
    const paintcore::BrushPreset chosen = preset ? *preset : paintcore::defaultBrushPreset();
    m_canvas->setBrush(chosen);
    m_brushSelector->setCurrentBrush(chosen.name);
    qCInfo(lcWindow) << "Pincel:" << chosen.name;
}

void AppWindow::toggleBrushSelector()
{
    if (m_brushSelector->isVisible()) {
        m_brushSelector->hide();
        return;
    }
    placeBrushSelector();
    m_brushSelector->show();
    m_brushSelector->raise();
}

void AppWindow::placeBrushSelector()
{
    const int width = qMin(kSelectorWidth, this->width() - 2 * kSelectorMargin);
    m_brushSelector->setGeometry(this->width() - width - kSelectorMargin, kSelectorMargin,
                                 width, height() - 2 * kSelectorMargin);
}

void AppWindow::showOnSavedScreen(Config* config)
{
    m_config = config;

    const QList<QScreen*> screens = QGuiApplication::screens();
    QList<ScreenId> ids;
    for (const QScreen* screen : screens)
        ids.append(ScreenId::of(screen));
    QScreen* target = QGuiApplication::primaryScreen();
    if (const auto saved = ScreenId::fromVariant(m_config->value(kMonitorKey, {}, Config::Scope::Common))) {
        if (const auto index = findScreen(ids, *saved))
            target = screens[*index];
        else
            qCWarning(lcWindow) << "El monitor guardado" << saved->describe()
                                << "no está conectado; se usa el principal";
    }
    moveToScreen(target);

    auto* next = new QShortcut(QKeySequence(kNextScreenKey), this);
    connect(next, &QShortcut::activated, this, &AppWindow::moveToNextScreen);
    connect(qApp, &QGuiApplication::screenRemoved, this, [this](QScreen* removed) {
        if (removed == screen() || !isVisible())
            moveToScreen(QGuiApplication::primaryScreen());
    });
}

void AppWindow::moveToScreen(QScreen* target)
{
    // Para cambiar de monitor una ventana a pantalla completa: salir de pantalla
    // completa, asignar el monitor y una geometría normal dentro de él, y volver a
    // pantalla completa. Al arrancar, esto ocurre ANTES de crear la ventana nativa:
    // si se crea primero, Windows la ubica en el monitor que él elige y la pantalla
    // completa queda en ese, no en el pedido.
    if (isFullScreen())
        showNormal();
    setScreen(target);
    const QRect available = target->availableGeometry();
    setGeometry(QRect(QPoint(0, 0), available.size() / 2).translated(available.topLeft() + QPoint(40, 40)));
    showFullScreen();
    qCInfo(lcWindow) << "Monitor:" << ScreenId::of(target).describe() << target->geometry();
}

void AppWindow::moveToNextScreen()
{
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (screens.size() < 2)
        return;
    QScreen* target = screens[(screens.indexOf(screen()) + 1) % screens.size()];
    moveToScreen(target);
    m_config->setValue(kMonitorKey, ScreenId::of(target).toVariant(), Config::Scope::Common);
}

void AppWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_brushSelector)
        placeBrushSelector();
}

} // namespace appkit
