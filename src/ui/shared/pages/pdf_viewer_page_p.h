#include "pdf_viewer_page.h"

#include "core/fontmanager.h"
#include "ui/shared/printing/pdf_print_service.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/role_style_registry.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <QApplication>
#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QDesktopServices>
#include <QEvent>
#include <QIntValidator>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QObject>
#include <QPalette>
#include <QPdfDocument>
#include <QPdfPageNavigator>
#include <QPdfView>
#include <QPushButton>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QStyle>
#include <QStringList>
#include <QtGlobal>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
constexpr qreal ZoomStep = 1.2;
constexpr qreal MinimumZoom = 0.25;
constexpr qreal MaximumZoom = 3.0;
constexpr qsizetype CopyBufferSize = 1024 * 1024;
constexpr int PageTotalReservedDigits = 3;
constexpr int PageTotalReservedPadding = 8;

QString zoomText(
    qreal zoom
    )
{
    return QStringLiteral("%1%").arg(
        std::clamp(
            qRound(zoom * 100.0),
            qRound(MinimumZoom * 100.0),
            qRound(MaximumZoom * 100.0)
            )
        );
}

qreal normalizedZoomFactor(
    qreal zoom
    )
{
    const int zoomPercent =
        std::clamp(
            qRound(zoom * 100.0),
            qRound(MinimumZoom * 100.0),
            qRound(MaximumZoom * 100.0)
            );

    return zoomPercent / 100.0;
}

QString exportFileFilter(
    const QString& suffix
    )
{
    if (suffix.trimmed().isEmpty())
    {
        return QObject::tr("All Files (*)");
    }

    return QObject::tr("%1 Files (*.%2);;All Files (*)")
        .arg(
            suffix.toUpper(),
            suffix
            );
}

QString defaultExportDirectory()
{
    QString directory =
        QStandardPaths::writableLocation(
            QStandardPaths::DocumentsLocation
            );

    if (directory.isEmpty())
    {
        directory =
            QDir::homePath();
    }

    return directory;
}

QString documentViewerBackgroundProperty(
    DocumentViewerBackground background
    )
{
    switch (background)
    {
    case DocumentViewerBackground::White:
        return QStringLiteral("white");

    case DocumentViewerBackground::Black:
        return QStringLiteral("black");

    case DocumentViewerBackground::Default:
    default:
        return QStringLiteral("default");
    }
}

void setPdfViewBackgroundColor(
    QWidget* widget,
    const QColor& color
    )
{
    if (!widget)
    {
        return;
    }

    QPalette palette =
        widget->palette();

    for (const QPalette::ColorGroup group : {
             QPalette::Active,
             QPalette::Inactive,
             QPalette::Disabled
         })
    {
        palette.setBrush(
            group,
            QPalette::Dark,
            QBrush(color)
            );
    }

    widget->setPalette(
        palette
        );
}

void resetPdfViewBackgroundColor(
    QWidget* widget
    )
{
    if (!widget)
    {
        return;
    }

    widget->setPalette(
        QApplication::palette(widget)
        );
}

void refreshStyle(
    QWidget* widget
    )
{
    if (!widget)
    {
        return;
    }

    if (widget->style())
    {
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
    }

    widget->update();
}
}

