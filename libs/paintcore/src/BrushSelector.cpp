#include "paintcore/BrushSelector.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QSet>
#include <QVBoxLayout>

namespace paintcore {

namespace {

constexpr int kIconSize = 64;
constexpr int kNameRole = Qt::UserRole;
constexpr int kFolderRole = Qt::UserRole + 1;

QString folderOf(const QString& name)
{
    const qsizetype slash = name.lastIndexOf(u'/');
    return slash < 0 ? QString() : name.left(slash);
}

// Miniatura del pincel por defecto, que no tiene _prev.png: un trazo negro.
QIcon defaultIcon()
{
    QPixmap pixmap(kIconSize, kIconSize);
    pixmap.fill(Qt::white);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::black, 3, Qt::SolidLine, Qt::RoundCap));
    QPainterPath path(QPointF(10, 46));
    path.cubicTo(24, 10, 40, 54, 54, 18);
    painter.drawPath(path);
    return QIcon(pixmap);
}

} // namespace

BrushSelector::BrushSelector(QWidget* parent)
    : QWidget(parent)
    , m_folder(new QComboBox(this))
    , m_search(new QLineEdit(this))
    , m_list(new QListWidget(this))
{
    m_search->setPlaceholderText(tr("Buscar pincel"));
    m_search->setClearButtonEnabled(true);

    m_list->setViewMode(QListView::IconMode);
    m_list->setIconSize(QSize(kIconSize, kIconSize));
    m_list->setGridSize(QSize(kIconSize + 32, kIconSize + 32));
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setUniformItemSizes(true);
    m_list->setWordWrap(true);

    auto* filters = new QHBoxLayout;
    filters->addWidget(m_folder);
    filters->addWidget(m_search, 1);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(filters);
    layout->addWidget(m_list, 1);

    connect(m_folder, &QComboBox::currentIndexChanged, this, &BrushSelector::applyFilter);
    connect(m_search, &QLineEdit::textChanged, this, &BrushSelector::applyFilter);
    connect(m_list, &QListWidget::itemClicked, this, &BrushSelector::choose);
    connect(m_list, &QListWidget::itemActivated, this, &BrushSelector::choose);
}

void BrushSelector::setLibrary(const BrushLibrary* library)
{
    m_library = library;
    rebuild();
}

void BrushSelector::rebuild()
{
    m_list->clear();
    m_folder->blockSignals(true);
    m_folder->clear();
    m_folder->addItem(tr("Todas las carpetas"), QString());

    auto* defaultItem = new QListWidgetItem(defaultIcon(), defaultBrushPreset().name, m_list);
    defaultItem->setData(kNameRole, defaultBrushPreset().name);
    defaultItem->setToolTip(tr("Pincel por defecto: birome con presión"));

    QStringList folders;
    if (m_library) {
        for (const BrushPreset& preset : m_library->brushes()) {
            const QString folder = folderOf(preset.name);
            const QString label = folder.isEmpty() ? preset.name : preset.name.mid(folder.size() + 1);
            const QIcon icon = preset.previewPath.isEmpty() ? QIcon() : QIcon(preset.previewPath);
            auto* item = new QListWidgetItem(icon, label, m_list);
            item->setData(kNameRole, preset.name);
            item->setData(kFolderRole, folder);
            item->setToolTip(preset.name);
            if (!folder.isEmpty() && !folders.contains(folder))
                folders.append(folder);
        }
    }
    folders.sort(Qt::CaseInsensitive);
    for (const QString& folder : folders)
        m_folder->addItem(folder, folder);
    m_folder->blockSignals(false);
    applyFilter();
}

void BrushSelector::applyFilter()
{
    const QString folder = m_folder->currentData().toString();
    const QString text = m_search->text().trimmed();
    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem* item = m_list->item(row);
        const QString name = item->data(kNameRole).toString();
        const bool folderOk = folder.isEmpty() || item->data(kFolderRole).toString() == folder;
        const bool textOk = text.isEmpty() || name.contains(text, Qt::CaseInsensitive);
        item->setHidden(!(folderOk && textOk));
    }
}

void BrushSelector::setCurrentBrush(const QString& name)
{
    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem* item = m_list->item(row);
        if (item->data(kNameRole).toString() == name) {
            m_list->setCurrentItem(item);
            m_list->scrollToItem(item);
            return;
        }
    }
}

int BrushSelector::visibleCount() const
{
    int count = 0;
    for (int row = 0; row < m_list->count(); ++row)
        count += m_list->item(row)->isHidden() ? 0 : 1;
    return count;
}

void BrushSelector::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    m_search->setFocus();
    if (m_list->currentItem())
        m_list->scrollToItem(m_list->currentItem());
}

void BrushSelector::choose(QListWidgetItem* item)
{
    if (item)
        emit brushSelected(item->data(kNameRole).toString());
}

} // namespace paintcore
