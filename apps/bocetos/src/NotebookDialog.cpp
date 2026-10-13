#include "NotebookDialog.h"

#include <appkit/Theme.h>

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace theme = appkit::theme;

namespace {

constexpr int kPanelWidth = 680;

QString css(const QColor& c)
{
    return c.name();
}

QString paperLabel(const QString& preset)
{
    if (preset == QLatin1String("crema"))
        return QStringLiteral("Crema");
    if (preset == QLatin1String("blanco"))
        return QStringLiteral("Blanco");
    if (preset == QLatin1String("kraft"))
        return QStringLiteral("Kraft");
    if (preset == QLatin1String("tonadoTostado"))
        return QStringLiteral("Tonado tostado");
    if (preset == QLatin1String("tonadoGris"))
        return QStringLiteral("Tonado gris");
    return preset;
}

QString grainLabel(int grain)
{
    return grain < 900 ? QStringLiteral("suave") : grain < 1700 ? QStringLiteral("medio") : QStringLiteral("marcado");
}

QLabel* fieldLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("field"));
    return label;
}

// La muestra del papel: su color con un grano de ruido (aproximado: no es la simulación de
// Cartuchera) y dos trazos de grafito, como en la mesa.
class PaperSample : public QWidget {
public:
    explicit PaperSample(QWidget* parent)
        : QWidget(parent)
    {
        setMinimumSize(260, 180);
        QRandomGenerator rng(7);
        m_noise = QImage(300, 196, QImage::Format_Grayscale8);
        for (int y = 0; y < m_noise.height(); ++y) {
            uchar* line = m_noise.scanLine(y);
            for (int x = 0; x < m_noise.width(); ++x)
                line[x] = uchar(rng.bounded(256));
        }
    }
    void set(const QColor& color, int grain, int texture)
    {
        m_color = color;
        m_grain = grain;
        m_texture = texture;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QImage image(m_noise.size(), QImage::Format_RGB32);
        const double amount = 0.02 + 0.10 * m_texture / 100.0 + 0.04 * m_grain / 2047.0;
        for (int y = 0; y < image.height(); ++y) {
            const uchar* n = m_noise.constScanLine(y);
            auto* out = reinterpret_cast<QRgb*>(image.scanLine(y));
            for (int x = 0; x < image.width(); ++x) {
                const double k = 1.0 - amount * (n[x] / 255.0);
                out[x] = qRgb(int(m_color.red() * k), int(m_color.green() * k), int(m_color.blue() * k));
            }
        }
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath clip;
        clip.addRoundedRect(QRectF(rect()), 6, 6);
        p.setClipPath(clip);
        p.drawImage(rect(), image);
        p.scale(width() / 300.0, height() / 196.0);
        QColor graphite(0x2A, 0x2A, 0x2A);
        graphite.setAlphaF(0.85f);
        p.setPen(QPen(graphite, 3, Qt::SolidLine, Qt::RoundCap));
        QPainterPath a(QPointF(30, 150));
        a.cubicTo(80, 110, 140, 90, 260, 60);
        p.drawPath(a);
        graphite.setAlphaF(0.45f);
        p.setPen(QPen(graphite, 5, Qt::SolidLine, Qt::RoundCap));
        QPainterPath b(QPointF(40, 170));
        b.cubicTo(100, 140, 170, 125, 250, 110);
        p.drawPath(b);
    }

private:
    QImage m_noise;
    QColor m_color = theme::kHoja;
    int m_grain = 2047, m_texture = 20;
};

} // namespace

NotebookDialog::NotebookDialog(QWidget* parent)
    : QWidget(parent)
{
    hide();
    setFocusPolicy(Qt::StrongFocus);
    m_panel = new QFrame(this);
    m_panel->setObjectName(QStringLiteral("panel"));
    m_panel->setFixedWidth(kPanelWidth);
    m_panel->setStyleSheet(QStringLiteral(R"(
        QFrame#panel { background: %1; border: 1px solid %2; border-radius: 12px; }
        QLabel { color: %3; font-size: 14px; background: transparent; }
        QLabel#title { font-size: 22px; font-weight: 600; }
        QLabel#hint, QLabel#value { color: %4; font-size: 12px; }
        QLabel#value { font-family: "%7"; color: %5; font-size: 13px; }
        QLineEdit, QComboBox, QSpinBox { min-height: 42px; padding: 0 12px; border-radius: 8px; background: %6;
            border: 1px solid %2; color: %3; font-size: 15px; }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border-color: %5; }
        QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled { color: %4; }
        QComboBox QAbstractItemView { background: %6; color: %3; selection-background-color: %8; selection-color: %5; }
        QSpinBox { font-family: "%7"; }
        QSlider::groove:horizontal { height: 4px; background: %2; border-radius: 2px; }
        QSlider::sub-page:horizontal { background: #E08A1E; border-radius: 2px; }
        QSlider::handle:horizontal { width: 16px; margin: -6px 0; border-radius: 8px; background: #E08A1E; }
        QSlider::sub-page:horizontal:disabled, QSlider::handle:horizontal:disabled { background: %4; }
        QPushButton#app { background: %6; border: 1px solid %2; border-radius: 8px; text-align: left; }
        QPushButton#app:checked { background: %8; border-color: %5; }
        QPushButton#app:focus { border: 2px solid %3; }
        QPushButton#app:checked:focus { border: 2px solid %5; }
    )")
                               .arg(css(theme::kPanel), css(theme::kBorde), css(theme::kTexto), css(theme::kTextoSecundario),
                                    css(theme::kSeleccionTexto), css(theme::kPanelAlto), QString::fromLatin1(theme::kFuenteMono),
                                    css(theme::kSeleccion)));
    QFont f = font();
    f.setFamilies({QString::fromLatin1(theme::kFuente), f.family()});
    m_panel->setFont(f);

    auto* layout = new QVBoxLayout(m_panel);
    layout->setContentsMargins(28, 24, 28, 22);
    layout->setSpacing(18);
    m_title = new QLabel(m_panel);
    m_title->setObjectName(QStringLiteral("title"));
    layout->addWidget(m_title);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(24);
    grid->setVerticalSpacing(6);
    grid->addWidget(fieldLabel(QStringLiteral("Nombre"), m_panel), 0, 0, 1, 2);
    m_name = new QLineEdit(m_panel);
    m_name->setMaxLength(80);
    grid->addWidget(m_name, 1, 0, 1, 2);
    grid->setRowMinimumHeight(2, 12);
    grid->addWidget(fieldLabel(QStringLiteral("Colección"), m_panel), 3, 0);
    grid->addWidget(fieldLabel(QStringLiteral("Hojas"), m_panel), 3, 1);
    m_collection = new QComboBox(m_panel);
    m_collection->setEditable(true);
    m_collection->setInsertPolicy(QComboBox::NoInsert);
    m_collection->lineEdit()->setPlaceholderText(QStringLiteral("General (o escribí una nueva)"));
    grid->addWidget(m_collection, 4, 0);
    m_sheets = new QSpinBox(m_panel);
    m_sheets->setRange(bocetos::kMinSheets, bocetos::kMaxSheets);
    m_sheets->setButtonSymbols(QAbstractSpinBox::NoButtons);
    grid->addWidget(m_sheets, 4, 1);
    auto* sheetsHint = new QLabel(QStringLiteral("Al llenarse, el cuaderno se cierra."), m_panel);
    sheetsHint->setObjectName(QStringLiteral("hint"));
    grid->addWidget(sheetsHint, 5, 1);
    layout->addLayout(grid);

    layout->addWidget(fieldLabel(QStringLiteral("Tapa"), m_panel));
    auto* covers = new QHBoxLayout;
    covers->setSpacing(14);
    m_covers = new QButtonGroup(this);
    m_covers->setExclusive(true);
    for (int i = 0; i < int(std::size(theme::kTapas)); ++i) {
        auto* b = new QToolButton(m_panel);
        b->setCheckable(true);
        b->setFixedSize(44, 44);
        b->setToolTip(QString::fromLatin1(theme::kTapasNombres[i]));
        b->setStyleSheet(QStringLiteral("QToolButton { background: %1; border: 0; border-radius: 7px; }"
                                        "QToolButton:checked { border: 2px solid %2; }"
                                        "QToolButton:focus { border: 2px solid %3; }")
                             .arg(css(theme::kTapas[i]), css(theme::kSeleccionTexto), css(theme::kTexto)));
        m_covers->addButton(b, i);
        covers->addWidget(b);
    }
    covers->addStretch();
    layout->addLayout(covers);

    // HU-142: con qué app se dibuja (mesa "Bocetos · cuaderno nuevo").
    auto* appsCol = new QVBoxLayout;
    appsCol->setSpacing(8);
    appsCol->addWidget(fieldLabel(QStringLiteral("Dibujar en"), m_panel));
    auto* appsRow = new QHBoxLayout;
    appsRow->setSpacing(10);
    m_apps = new QButtonGroup(this);
    m_apps->setExclusive(true);
    const QString notes[] = {QStringLiteral("grafito, con el motor de la suite"), QStringLiteral("sus pinceles, sobre el papel del cuaderno")};
    for (int i = 0; i < 2; ++i) {
        auto* b = new QPushButton(m_panel);
        b->setObjectName(QStringLiteral("app"));
        b->setCheckable(true);
        b->setMinimumHeight(58);
        auto* inner = new QVBoxLayout(b);
        inner->setContentsMargins(14, 8, 14, 8);
        inner->setSpacing(2);
        auto* name = new QLabel(i == 0 ? QStringLiteral("Cartuchera") : QStringLiteral("Krita"), b);
        auto* note = new QLabel(notes[i], b);
        note->setObjectName(QStringLiteral("hint"));
        for (QLabel* l : {name, note}) {
            l->setAttribute(Qt::WA_TransparentForMouseEvents);
            inner->addWidget(l);
        }
        m_apps->addButton(b, i);
        m_appButtons.append(b);
        m_appNames.append(name);
        m_appNotes.append(note);
        appsRow->addWidget(b, 1);
    }
    appsCol->addLayout(appsRow);
    auto* appsHint = new QLabel(QStringLiteral("Un cuaderno es de una sola app: no cambia después."), m_panel);
    appsHint->setObjectName(QStringLiteral("hint"));
    appsCol->addWidget(appsHint);
    layout->addLayout(appsCol);

    auto* line = new QFrame(m_panel);
    line->setFixedHeight(1);
    line->setStyleSheet(QStringLiteral("background: %1;").arg(css(theme::kBorde)));
    layout->addWidget(line);

    auto* paperRow = new QHBoxLayout;
    paperRow->setSpacing(24);
    auto* paperCol = new QVBoxLayout;
    paperCol->setSpacing(6);
    paperCol->addWidget(fieldLabel(QStringLiteral("Papel"), m_panel));
    auto* paperHint = new QLabel(QStringLiteral("El de Cartuchera; en Krita, el fondo de cada hoja."), m_panel);
    paperHint->setObjectName(QStringLiteral("hint"));
    paperCol->addWidget(paperHint);
    m_paper = new QComboBox(m_panel);
    for (const notebooks::Paper& paper : notebooks::paperPresets()) {
        QPixmap swatch(18, 18);
        swatch.fill(paper.color);
        m_paper->addItem(QIcon(swatch), paperLabel(paper.preset), paper.preset);
    }
    paperCol->addWidget(m_paper);
    paperCol->addSpacing(8);
    const auto sliderRow = [&](const QString& name, QSlider*& slider, QLabel*& value, int max) {
        auto* row = new QHBoxLayout;
        row->addWidget(fieldLabel(name, m_panel));
        row->addStretch();
        value = new QLabel(m_panel);
        value->setObjectName(QStringLiteral("value"));
        row->addWidget(value);
        paperCol->addLayout(row);
        slider = new QSlider(Qt::Horizontal, m_panel);
        slider->setRange(0, max);
        paperCol->addWidget(slider);
    };
    sliderRow(QStringLiteral("Grano"), m_grain, m_grainValue, 2047);
    sliderRow(QStringLiteral("Textura"), m_texture, m_textureValue, 100);
    paperCol->addStretch();
    paperRow->addLayout(paperCol, 1);
    auto* sampleCol = new QVBoxLayout;
    sampleCol->setSpacing(8);
    sampleCol->addWidget(fieldLabel(QStringLiteral("Muestra"), m_panel));
    m_sample = new PaperSample(m_panel);
    sampleCol->addWidget(m_sample, 1);
    paperRow->addLayout(sampleCol, 1);
    layout->addLayout(paperRow);

    m_footer = new QLabel(m_panel);
    m_footer->setTextFormat(Qt::RichText);
    layout->addWidget(m_footer);

    connect(m_paper, &QComboBox::activated, this, [this](int index) { applyPreset(index); });
    connect(m_apps, &QButtonGroup::idToggled, this, [this] { updateApps(); });
    connect(m_grain, &QSlider::valueChanged, this, [this] { updateSample(); });
    connect(m_texture, &QSlider::valueChanged, this, [this] { updateSample(); });
}

void NotebookDialog::open(Mode mode, const bocetos::Draft& draft, const QStringList& collections, bool focusCollection)
{
    m_mode = mode;
    const bool create = mode == Mode::Create;
    m_title->setText(create ? QStringLiteral("Cuaderno nuevo") : QStringLiteral("Editar cuaderno"));
    const QString key = QStringLiteral("<span style=\"font-family:'%1'; color:%2; background:%3; border:1px solid %2;\">&nbsp;%4&nbsp;</span>");
    m_footer->setText(key.arg(QString::fromLatin1(theme::kFuenteMono), css(theme::kSeleccionTexto), css(theme::kSeleccion), QStringLiteral("Enter")) +
                      QStringLiteral(" <span style=\"color:%1\">%2</span> &nbsp;·&nbsp; ")
                          .arg(css(theme::kTexto), create ? QStringLiteral("crea el cuaderno") : QStringLiteral("guarda los cambios")) +
                      key.arg(QString::fromLatin1(theme::kFuenteMono), css(theme::kTexto), css(theme::kPanelAlto), QStringLiteral("Esc")) +
                      QStringLiteral(" <span style=\"color:%1\">cancela</span>").arg(css(theme::kTextoSecundario)));
    m_name->setText(draft.name);
    m_collection->clear();
    m_collection->addItems(collections);
    m_collection->setEditText(draft.collection);
    m_sheets->setValue(draft.maxSheets);
    const int cover = int(std::find_if(std::begin(theme::kTapas), std::end(theme::kTapas),
                                       [&](const QColor& c) { return c == draft.cover; }) -
                          std::begin(theme::kTapas));
    if (QAbstractButton* b = m_covers->button(cover < int(std::size(theme::kTapas)) ? cover : 0))
        b->setChecked(true);
    m_preset = draft.paper.preset;
    m_paper->setCurrentIndex(std::max(0, m_paper->findData(draft.paper.preset)));
    m_grain->setValue(draft.paper.grain);
    m_texture->setValue(draft.paper.texture);
    m_apps->button(draft.app == notebooks::App::Krita ? 1 : 0)->setChecked(true);
    // Al editar, el papel, las hojas y la app no cambian (las hojas ya dibujadas son de ese papel).
    for (QWidget* w : std::initializer_list<QWidget*>{m_sheets, m_paper, m_grain, m_texture, m_appButtons[0], m_appButtons[1]})
        w->setEnabled(create);
    updateApps();
    updateSample();
    setGeometry(parentWidget()->rect());
    show();
    raise();
    QWidget* first = focusCollection ? static_cast<QWidget*>(m_collection) : static_cast<QWidget*>(m_name);
    first->setFocus();
    if (first == m_name)
        m_name->selectAll();
}

bocetos::Draft NotebookDialog::draft() const
{
    bocetos::Draft d;
    d.name = m_name->text().trimmed();
    QString collection = m_collection->currentText().trimmed();
    if (collection.compare(QStringLiteral("General"), Qt::CaseInsensitive) == 0)
        collection.clear();
    d.collection = collection;
    d.cover = theme::kTapas[std::max(0, m_covers->checkedId())];
    d.paper = notebooks::paperPreset(m_paper->currentData().toString());
    d.paper.grain = m_grain->value();
    d.paper.texture = m_texture->value();
    d.maxSheets = m_sheets->value();
    d.app = m_apps->checkedId() == 1 ? notebooks::App::Krita : notebooks::App::Cartuchera;
    return d;
}

void NotebookDialog::setKritaFound(bool found)
{
    m_appNotes[1]->setText(found ? QStringLiteral("sus pinceles, sobre el papel del cuaderno")
                                 : QStringLiteral("no está instalado: no se va a poder dibujar"));
}

void NotebookDialog::updateApps()
{
    for (int i = 0; i < m_appNames.size(); ++i)
        m_appNames[i]->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: 500; color: %1;")
                                         .arg(css(m_appButtons[i]->isChecked() ? theme::kSeleccionTexto : theme::kTexto)));
}

void NotebookDialog::applyPreset(int index)
{
    const notebooks::Paper paper = notebooks::paperPreset(m_paper->itemData(index).toString());
    m_grain->setValue(paper.grain);
    m_texture->setValue(paper.texture);
    updateSample();
}

void NotebookDialog::updateSample()
{
    const notebooks::Paper paper = notebooks::paperPreset(m_paper->currentData().toString());
    m_grainValue->setText(grainLabel(m_grain->value()));
    m_textureValue->setText(QString::number(m_texture->value()));
    static_cast<PaperSample*>(m_sample)->set(paper.color, m_grain->value(), m_texture->value());
}

void NotebookDialog::accept()
{
    if (m_name->text().trimmed().isEmpty()) {
        m_name->setFocus();
        m_name->setPlaceholderText(QStringLiteral("El cuaderno necesita un nombre"));
        return;
    }
    hide();
    if (onAccept)
        onAccept(draft());
}

void NotebookDialog::cancel()
{
    hide();
    if (onCancel)
        onCancel();
}

void NotebookDialog::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(23, 25, 28, 184)); // la biblioteca atenuada
}

void NotebookDialog::keyPressEvent(QKeyEvent* event)
{
    // Lo que los campos no usan llega acá; nada sigue a la biblioteca de atrás.
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        accept();
    else if (event->key() == Qt::Key_Escape)
        cancel();
    event->accept();
}

void NotebookDialog::resizeEvent(QResizeEvent*)
{
    m_panel->adjustSize();
    const QSize size = m_panel->sizeHint();
    m_panel->setGeometry((width() - kPanelWidth) / 2, std::max(16, (height() - size.height()) / 2), kPanelWidth,
                         std::min(size.height(), height() - 32));
}
