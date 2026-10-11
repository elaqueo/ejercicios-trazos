#include "lienzo/PenMenu.h"

#include <appkit/Theme.h>

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

#include <windows.h>

#include <algorithm>
#include <cmath>

namespace lienzo {

namespace {

QFont menuFont(int pixelSize, QFont::Weight weight = QFont::Normal, bool mono = false)
{
    QFont f(QString::fromLatin1(mono ? appkit::theme::kFuenteMono : appkit::theme::kFuente));
    f.setPixelSize(pixelSize);
    f.setWeight(weight);
    return f;
}

QPointF polar(QPointF center, double radius, double degrees)
{
    const double t = qDegreesToRadians(degrees);
    return {center.x() + radius * std::cos(t), center.y() - radius * std::sin(t)};
}

// Sector de corona entre r1 y r2, de `degrees` ± half.
QPainterPath sector(QPointF c, double r1, double r2, double degrees, double half)
{
    QPainterPath path;
    const QRectF outer(c.x() - r2, c.y() - r2, 2 * r2, 2 * r2);
    const QRectF inner(c.x() - r1, c.y() - r1, 2 * r1, 2 * r1);
    path.arcMoveTo(outer, degrees + half);
    path.arcTo(outer, degrees + half, -2 * half);
    path.arcTo(inner, degrees - half, 2 * half);
    path.closeSubpath();
    return path;
}

// Índice de la opción cuyo ángulo es el más cercano (la primera arriba, en el sentido del reloj).
int slotOf(QPointF offset, int n)
{
    const double degrees = qRadiansToDegrees(std::atan2(-offset.y(), offset.x()));
    const double step = 360.0 / n;
    const int i = int(std::lround((90.0 - degrees) / step));
    return ((i % n) + n) % n;
}

} // namespace

PenMenu::PenMenu(QWidget* owner)
    : QWidget(owner, Qt::Tool | Qt::FramelessWindowHint)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_TranslucentBackground);
}

double PenMenu::angleOf(int i, int n)
{
    return 90.0 - i * 360.0 / n;
}

PenMenu::Hit PenMenu::hitTest(QPointF offset, int pencils, int actions)
{
    const double r = std::hypot(offset.x(), offset.y());
    if (r > kRadius)
        return {Hit::Outside, -1};
    if (r < kCenter)
        return {Hit::Center, -1};
    if (r <= kPencilRing)
        return pencils > 0 ? Hit{Hit::Pencil, slotOf(offset, pencils)} : Hit{Hit::Center, -1};
    return actions > 0 ? Hit{Hit::Action, slotOf(offset, actions)} : Hit{Hit::Center, -1};
}

QPoint PenMenu::centerFor(QPoint tip, QSize size)
{
    const int reach = kRadius + kMargin;
    const auto clampAxis = [reach](int v, int length) {
        if (length < 2 * reach)
            return length / 2;
        return std::clamp(v, reach, length - reach);
    };
    return {clampAxis(tip.x(), size.width()), clampAxis(tip.y(), size.height())};
}

void PenMenu::setPencils(std::vector<QColor> colors, int active)
{
    m_colors = std::move(colors);
    m_active = active;
    update();
}

void PenMenu::setActions(std::vector<PenMenuAction> actions)
{
    m_actions = std::move(actions);
    update();
}

void PenMenu::open(const QRect& area, QPoint tip)
{
    setGeometry(area);
    m_center = centerFor(tip, area.size());
    m_barrelDown = true;
    m_openedOn = hitAt(tip);
    m_hover = m_openedOn;
    show();
    raise();
    activateWindow();
    setFocus();
}

PenMenu::Hit PenMenu::hitAt(QPointF position) const
{
    return hitTest(position - QPointF(m_center), int(m_colors.size()), int(m_actions.size()));
}

void PenMenu::choose(const Hit& hit)
{
    if (hit.kind == Hit::Pencil) {
        hide();
        m_active = hit.index;
        if (onPencil)
            onPencil(hit.index);
    } else if (hit.kind == Hit::Action) {
        if (onAction)
            onAction(hit.index);
        update();
    }
}

void PenMenu::dismiss(const char* how)
{
    hide();
    if (onDismiss)
        onDismiss(how);
}

void PenMenu::tipDown(QPointF position)
{
    const Hit hit = hitAt(position);
    if (hit.kind == Hit::Outside)
        dismiss("fuera");
    else
        choose(hit);
}

void PenMenu::barrel(bool down, QPointF position)
{
    if (down == m_barrelDown)
        return;
    m_barrelDown = down;
    const Hit hit = hitAt(position);
    if (down) { // el botón otra vez
        dismiss("botón");
        return;
    }
    // Soltó el botón: si llevó la punta a otra opción, la elige; si no, el menú queda abierto.
    if (hit != m_openedOn && (hit.kind == Hit::Pencil || hit.kind == Hit::Action))
        choose(hit);
    m_openedOn = {};
}

void PenMenu::hover(QPointF position)
{
    const Hit hit = hitAt(position);
    if (hit != m_hover) {
        m_hover = hit;
        update();
    }
}

void PenMenu::paintEvent(QPaintEvent*)
{
    namespace theme = appkit::theme;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    // Casi transparente pero no del todo: así los toques fuera del anillo llegan acá y no a la hoja.
    p.fillRect(rect(), QColor(0, 0, 0, 1));

    const QPointF c(m_center);
    QColor disc = theme::kPanel;
    disc.setAlphaF(0.86f);
    p.setPen(QPen(theme::kBorde, 1.5));
    p.setBrush(disc);
    p.drawEllipse(c, kRadius, kRadius);

    // Acciones: sectores de corona con ícono, nombre y tecla.
    const int n = int(m_actions.size());
    for (int i = 0; i < n; ++i) {
        const double a = angleOf(i, n);
        const bool hot = m_hover.kind == Hit::Action && m_hover.index == i;
        QColor fill = hot ? QColor(0x3A, 0x32, 0x26) : theme::kPanelAlto;
        if (!hot)
            fill.setAlphaF(0.6f);
        const QPainterPath path = sector(c, kActionInner, kActionOuter, a, 180.0 / n - 2);
        p.setBrush(fill);
        p.setPen(hot ? QPen(theme::kEnfasis, 2) : Qt::NoPen);
        p.drawPath(path);
        const QPointF at = polar(c, 140, a);
        const QColor text = hot ? theme::kSeleccionTexto : theme::kTexto;
        p.setPen(text);
        p.setFont(menuFont(22));
        p.drawText(QRectF(at.x() - 50, at.y() - 30, 100, 26), Qt::AlignCenter, m_actions[size_t(i)].icon);
        p.setFont(menuFont(11, QFont::Medium));
        p.drawText(QRectF(at.x() - 50, at.y() - 2, 100, 16), Qt::AlignCenter, m_actions[size_t(i)].label);
        p.setFont(menuFont(10, QFont::Normal, true));
        p.setPen(theme::kTextoSecundario);
        p.drawText(QRectF(at.x() - 50, at.y() + 14, 100, 14), Qt::AlignCenter, m_actions[size_t(i)].key);
    }

    // Lápices: el disco de adentro, el activo con anillo ámbar.
    p.setPen(QPen(theme::kBorde, 1.5));
    p.setBrush(theme::kPanel);
    p.drawEllipse(c, kPencilRing, kPencilRing);
    const int m = int(m_colors.size());
    for (int i = 0; i < m; ++i) {
        const QPointF at = polar(c, kPencilOrbit, angleOf(i, m));
        const bool hot = m_hover.kind == Hit::Pencil && m_hover.index == i;
        if (i == m_active || hot) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(i == m_active ? theme::kEnfasis : theme::kTextoSecundario, 3));
            p.drawEllipse(at, kPencilDot + 7, kPencilDot + 7);
        }
        p.setBrush(m_colors[size_t(i)]);
        p.setPen(i == 0 ? QPen(QColor(0x8A, 0x90, 0x99), 1.5) : Qt::NoPen); // el grafito, oscuro sobre oscuro
        p.drawEllipse(at, kPencilDot, kPencilDot);
        p.setPen(theme::kHojaAnimacion);
        p.setFont(menuFont(12, QFont::Medium, true));
        p.drawText(QRectF(at.x() - kPencilDot, at.y() - kPencilDot, 2 * kPencilDot, 2 * kPencilDot), Qt::AlignCenter,
                   QString::number(i + 1));
    }

    // La punta.
    p.setPen(QPen(theme::kBorde, 1.5));
    p.setBrush(theme::kPanelAlto);
    p.drawEllipse(c, kCenter, kCenter);
    p.setPen(Qt::NoPen);
    p.setBrush(theme::kEnfasis);
    p.drawEllipse(c, 6, 6);
}

bool PenMenu::nativeEvent(const QByteArray&, void* message, qintptr* result)
{
    const MSG* msg = static_cast<const MSG*>(message);
    if (msg->message != WM_POINTERUPDATE && msg->message != WM_POINTERDOWN && msg->message != WM_POINTERUP)
        return false;
    const UINT32 id = GET_POINTERID_WPARAM(msg->wParam);
    POINTER_INPUT_TYPE type = PT_POINTER;
    POINTER_PEN_INFO info{};
    if (!GetPointerType(id, &type) || type != PT_PEN || !GetPointerPenInfo(id, &info))
        return false;
    POINT pt = info.pointerInfo.ptPixelLocation;
    ScreenToClient(reinterpret_cast<HWND>(winId()), &pt);
    const QPointF pos = QPointF(pt.x, pt.y) / devicePixelRatioF();
    hover(pos);
    if (msg->message == WM_POINTERDOWN)
        tipDown(pos);
    else
        barrel((info.penFlags & PEN_FLAG_BARREL) != 0, pos);
    *result = 0; // sin mouse sintetizado
    return true;
}

void PenMenu::mouseMoveEvent(QMouseEvent* event)
{
    hover(event->position());
}

void PenMenu::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton)
        dismiss("botón");
    else
        tipDown(event->position());
}

void PenMenu::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape)
        dismiss("Esc");
}

} // namespace lienzo
