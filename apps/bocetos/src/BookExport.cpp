#include "BookExport.h"

#include <appkit/Theme.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QImageReader>
#include <QPainter>
#include <QPdfWriter>
#include <QRegularExpression>

namespace bocetos {

namespace {

QString safeName(const QString& name)
{
    QString out = name.trimmed();
    out.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("-"));
    return out.isEmpty() ? QStringLiteral("Cuaderno") : out;
}

QFont font(int px, int weight, bool mono = false)
{
    // Como bocetos::themeFont: la de la suite y, si no está instalada, la de Windows.
    QFont f;
    f.setFamilies(mono ? QStringList{QString::fromLatin1(appkit::theme::kFuenteMono), QStringLiteral("Consolas")}
                       : QStringList{QString::fromLatin1(appkit::theme::kFuente), QStringLiteral("Segoe UI")});
    f.setPixelSize(px);
    f.setWeight(QFont::Weight(weight));
    return f;
}

// La tapa (como el libro de la biblioteca, a toda la página): el color con el lomo más oscuro, la
// colección en versalitas, el nombre y, al pie, las fechas y cuántas hojas. `page` en píxeles.
void paintCover(QPainter& p, const QRectF& page, const Book& book, int sheets)
{
    const double u = page.height() / 350.0; // la mesa mide 560 × 350
    p.fillRect(page, book.notebook.cover.isValid() ? book.notebook.cover : QColor(0x4A, 0x50, 0x59));
    p.fillRect(QRectF(page.left(), page.top(), 34 * u, page.height()), QColor(0, 0, 0, 56));
    const QColor text(0xF4, 0xF1, 0xEA);
    const double left = page.left() + 72 * u, width = page.width() - 120 * u;
    double y = page.top() + 56 * u;
    if (!book.notebook.collection.isEmpty()) {
        QFont label = font(int(12 * u), QFont::DemiBold);
        label.setLetterSpacing(QFont::AbsoluteSpacing, 1.5 * u);
        p.setFont(label);
        QColor dim = text;
        dim.setAlphaF(0.78f);
        p.setPen(dim);
        p.drawText(QRectF(left, y, width, 18 * u), Qt::AlignLeft | Qt::AlignVCenter, book.notebook.collection.toUpper());
        y += 28 * u;
    }
    p.setFont(font(int(34 * u), QFont::DemiBold));
    p.setPen(text);
    p.drawText(QRectF(left, y, width, 140 * u), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, book.notebook.name);
    p.setFont(font(int(13 * u), QFont::Normal, true));
    QColor foot = text;
    foot.setAlphaF(0.82f);
    p.setPen(foot);
    const QRectF bottom(left, page.bottom() - 62 * u, width, 20 * u);
    const QDate last = book.notebook.updated.isValid() ? book.notebook.updated : book.notebook.started;
    p.drawText(bottom, Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("%1 – %2").arg(longDate(book.notebook.started), longDate(last)));
    p.drawText(bottom, Qt::AlignRight | Qt::AlignVCenter, sheets == 1 ? QStringLiteral("1 hoja") : QStringLiteral("%1 hojas").arg(sheets));
}

} // namespace

QString sheetFileName(const notebooks::Notebook& notebook, int sheet)
{
    const int digits = notebook.maxSheets >= 100 ? 3 : 2;
    return QStringLiteral("%1 - hoja %2.png").arg(safeName(notebook.name)).arg(sheet, digits, 10, QLatin1Char('0'));
}

QString pdfFileName(const notebooks::Notebook& notebook)
{
    return safeName(notebook.name) + QStringLiteral(".pdf");
}

QString exportDirArgument(const QStringList& args)
{
    const qsizetype at = args.indexOf(QStringLiteral("--exportar-en"));
    return at >= 0 && at + 1 < args.size() ? QFileInfo(args[at + 1]).absoluteFilePath() : QString();
}

bool exportSheet(const Page& page, const QString& file, QString* error)
{
    if (page.image.isEmpty()) {
        if (error)
            *error = QStringLiteral("la hoja %1 no tiene imagen").arg(page.sheet);
        return false;
    }
    QFile::remove(file);
    if (!QFile::copy(page.image, file)) {
        if (error)
            *error = QStringLiteral("no se pudo escribir %1").arg(QDir::toNativeSeparators(file));
        return false;
    }
    QFile::setPermissions(file, QFile::permissions(file) | QFileDevice::WriteOwner | QFileDevice::WriteUser);
    return true;
}

QList<Page> pdfPages(const QString& dir, const notebooks::Notebook& notebook)
{
    QList<Page> out;
    for (const Page& p : pagesOf(dir, notebook))
        if (p.state == notebooks::SheetState::Finished)
            out.append(p);
    return out;
}

bool writeNotebookPdf(const QString& file, const Book& book, const std::function<void(int, int)>& progress, QString* error)
{
    const QList<Page> pages = pdfPages(book.dir, book.notebook);
    QPdfWriter pdf(file);
    pdf.setTitle(book.notebook.name);
    pdf.setCreator(QStringLiteral("Trazos · Bocetos"));
    pdf.setResolution(kPdfDpi);
    pdf.setPageSize(QPageSize(QSizeF(kPdfPageWidthMm, kPdfPageHeightMm), QPageSize::Millimeter, QString(), QPageSize::ExactMatch));
    pdf.setPageOrientation(QPageLayout::Portrait); // el tamaño ya es apaisado
    pdf.setPageMargins(QMarginsF(0, 0, 0, 0));
    QPainter p;
    if (!p.begin(&pdf)) {
        if (error)
            *error = QStringLiteral("no se pudo escribir %1").arg(QDir::toNativeSeparators(file));
        return false;
    }
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRectF page(0, 0, pdf.width(), pdf.height());
    paintCover(p, page, book, int(pages.size()));
    const QColor paper = book.notebook.paper.color.isValid() ? book.notebook.paper.color : appkit::theme::kHoja;
    for (int i = 0; i < pages.size(); ++i) {
        pdf.newPage();
        QImageReader reader(pages[i].image);
        const QSize size = reader.size();
        if (size.isValid())
            reader.setScaledSize(size.scaled(page.size().toSize(), Qt::KeepAspectRatio).boundedTo(size));
        const QImage image = reader.read();
        p.fillRect(page, paper);
        if (!image.isNull()) {
            QRectF target(QPointF(), QSizeF(image.size()).scaled(page.size(), Qt::KeepAspectRatio));
            target.moveCenter(page.center());
            p.drawImage(target, image);
        } else {
            qWarning().noquote() << QStringLiteral("PDF: no se pudo leer %1 (%2)").arg(pages[i].image, reader.errorString());
        }
        if (progress)
            progress(i + 1, int(pages.size()));
    }
    if (!p.end()) {
        if (error)
            *error = QStringLiteral("no se pudo terminar %1").arg(QDir::toNativeSeparators(file));
        return false;
    }
    return true;
}

} // namespace bocetos
