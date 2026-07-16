#include "features/schedule/services/schedule_print_service.h"
#include "features/schedule/ui/schedule_view_model.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QtTest>

#include <QColor>
#include <QImage>
#include <QPageSize>
#include <QPdfDocument>
#include <QRect>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>

namespace PdfPrintService
{
Result printPdfDocument(
    const Request&
    )
{
    return {
        Status::Failed,
        QStringLiteral("Printing is not used by the schedule PDF tests.")
    };
}
}

namespace
{
constexpr int RenderDpi = 300;
constexpr qreal PointsPerInch = 72.0;
constexpr qreal MarginInches = 0.5;
constexpr qreal TimeColumnWidth = 118.0;
constexpr qreal DayColumnWidth = 142.0;
constexpr qreal HeaderHeight = 52.0;
constexpr qreal RowHeight = 58.0;
constexpr qreal FooterHeight = 52.0;

ScheduleViewModel emptyScheduleModel()
{
    ScheduleViewModel model;
    model.days = {
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday")
    };

    for (const QString& timeLabel :
         { QStringLiteral("09:00"), QStringLiteral("10:00") })
    {
        ScheduleRowView row;
        row.timeLabel = timeLabel;
        row.timeRangeLabel =
            timeLabel == QStringLiteral("09:00")
                ? QStringLiteral("9:00 -\n9:50")
                : QStringLiteral("10:00 -\n10:50");

        for (const QString& day : model.days)
        {
            ScheduleCellView cell;
            cell.day = day;
            cell.timeLabel = timeLabel;
            cell.defaultSlotState = scheduleEmptySlotState();
            cell.slotState = scheduleEmptySlotState();
            row.cells.append(cell);
        }

        model.rows.append(row);
    }

    return model;
}

SchedulePrintService::Request requestFor(
    SchedulePrintStyle style,
    QPageLayout::Orientation orientation,
    Theme currentTheme = Theme::Dark
    )
{
    SchedulePrintService::Request request;
    request.model = emptyScheduleModel();
    request.style = style;
    request.currentTheme = currentTheme;
    request.userName = QStringLiteral("Warren");
    request.pageOrientation = orientation;
    return request;
}

QString savePdf(
    QTemporaryDir& temporaryDirectory,
    const SchedulePrintService::Request& request,
    const QString& fileName
    )
{
    const QString path =
        temporaryDirectory.filePath(fileName);
    const SchedulePrintService::Result result =
        SchedulePrintService::saveSchedulePdf(
            request,
            path
            );

    return result.status == SchedulePrintService::Status::Sent
        ? path
        : QString();
}

void loadDocument(
    QPdfDocument& document,
    const QString& path
    )
{
    QCOMPARE(
        document.load(path),
        QPdfDocument::Error::None
        );
    QCOMPARE(
        document.status(),
        QPdfDocument::Status::Ready
        );
    QCOMPARE(document.pageCount(), 1);
}

QImage renderFirstPage(
    QPdfDocument& document
    )
{
    const QSizeF pointSize =
        document.pagePointSize(0);
    const QSize renderSize(
        std::max(
            1,
            qRound(pointSize.width() * RenderDpi / PointsPerInch)
            ),
        std::max(
            1,
            qRound(pointSize.height() * RenderDpi / PointsPerInch)
            )
        );

    QImage image =
        document.render(
            0,
            renderSize
            );
    return image;
}

bool isWhitePixel(
    const QColor& color
    )
{
    return color.red() >= 245
        && color.green() >= 245
        && color.blue() >= 245;
}

QRect nonWhiteBounds(
    const QImage& image
    )
{
    int left =
        image.width();
    int top =
        image.height();
    int right =
        -1;
    int bottom =
        -1;

    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            if (isWhitePixel(image.pixelColor(x, y)))
            {
                continue;
            }

            left =
                std::min(
                    left,
                    x
                    );
            top =
                std::min(
                    top,
                    y
                    );
            right =
                std::max(
                    right,
                    x
                    );
            bottom =
                std::max(
                    bottom,
                    y
                    );
        }
    }

    if (right < left || bottom < top)
    {
        return {};
    }

    return QRect(
        QPoint(
            left,
            top
            ),
        QPoint(
            right,
            bottom
            )
        );
}

QRectF scheduleContentRect(
    const QImage& image
    )
{
    const qreal margin =
        MarginInches * RenderDpi;

    return QRectF(
        margin,
        margin,
        image.width() - (2.0 * margin),
        image.height() - (2.0 * margin)
        );
}

QRectF scheduleTargetRect(
    const QImage& image,
    const ScheduleViewModel& model,
    bool excel
    )
{
    const int rowCount =
        std::max(
            1,
            static_cast<int>(model.rows.size())
            );
    const QSizeF sourceSize(
        TimeColumnWidth + (DayColumnWidth * model.days.size()),
        HeaderHeight
            + (RowHeight * rowCount)
            + (excel ? FooterHeight : 0.0)
        );
    const QRectF contentRect =
        scheduleContentRect(image);

    QSizeF targetSize =
        sourceSize;
    targetSize.scale(
        contentRect.size(),
        Qt::KeepAspectRatio
        );

    return QRectF(
        contentRect.left()
            + ((contentRect.width() - targetSize.width()) / 2.0),
        contentRect.top(),
        targetSize.width(),
        targetSize.height()
        );
}

QPoint scaledSchedulePoint(
    const QRectF& targetRect,
    qreal sourceX,
    qreal sourceY,
    const ScheduleViewModel& model
    )
{
    const qreal sourceWidth =
        TimeColumnWidth + (DayColumnWidth * model.days.size());
    const qreal scale =
        targetRect.width() / sourceWidth;

    return QPoint(
        qRound(targetRect.left() + (sourceX * scale)),
        qRound(targetRect.top() + (sourceY * scale))
        );
}
}

class SchedulePrintPdfTests : public QObject
{
    Q_OBJECT

private slots:
    void generatedPdfUsesA4Orientation();
    void contentStaysInsideHalfInchMargins();
    void themedEmptyCellsAndOffTableAreaStayWhite();
};

void SchedulePrintPdfTests::generatedPdfUsesA4Orientation()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QSizeF a4Points =
        QPageSize(QPageSize::A4).size(
            QPageSize::Point
            );

    for (const QPageLayout::Orientation orientation :
         { QPageLayout::Portrait, QPageLayout::Landscape })
    {
        const QString path =
            savePdf(
                temporaryDirectory,
                requestFor(
                    SchedulePrintStyle::Excel,
                    orientation
                    ),
                orientation == QPageLayout::Portrait
                    ? QStringLiteral("portrait.pdf")
                    : QStringLiteral("landscape.pdf")
                );
        QVERIFY(!path.isEmpty());
        QPdfDocument document;
        loadDocument(
            document,
            path
            );
        const QSizeF pageSize =
            document.pagePointSize(0);

        const QSizeF expectedSize =
            orientation == QPageLayout::Portrait
                ? a4Points
                : QSizeF(
                    a4Points.height(),
                    a4Points.width()
                    );

        QVERIFY(
            std::abs(pageSize.width() - expectedSize.width()) < 1.0
            );
        QVERIFY(
            std::abs(pageSize.height() - expectedSize.height()) < 1.0
            );
    }
}

void SchedulePrintPdfTests::contentStaysInsideHalfInchMargins()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const int marginPixels =
        qRound(MarginInches * RenderDpi);
    constexpr int TolerancePixels = 3;

    for (const QPageLayout::Orientation orientation :
         { QPageLayout::Portrait, QPageLayout::Landscape })
    {
        const QString path =
            savePdf(
                temporaryDirectory,
                requestFor(
                    SchedulePrintStyle::Excel,
                    orientation
                    ),
                orientation == QPageLayout::Portrait
                    ? QStringLiteral("excel-portrait.pdf")
                    : QStringLiteral("excel-landscape.pdf")
                );
        QVERIFY(!path.isEmpty());
        QPdfDocument document;
        loadDocument(
            document,
            path
            );
        const QImage image =
            renderFirstPage(document);
        QVERIFY(!image.isNull());
        const QRect bounds =
            nonWhiteBounds(image);
        QVERIFY(!bounds.isNull());

        QVERIFY(bounds.left() >= marginPixels - TolerancePixels);
        QVERIFY(bounds.top() >= marginPixels - TolerancePixels);
        QVERIFY(bounds.right() <= image.width() - marginPixels + TolerancePixels);
        QVERIFY(bounds.bottom() <= image.height() - marginPixels + TolerancePixels);
    }
}

void SchedulePrintPdfTests::themedEmptyCellsAndOffTableAreaStayWhite()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QList<SchedulePrintService::Request> requests{
        requestFor(
            SchedulePrintStyle::LightTheme,
            QPageLayout::Landscape,
            Theme::Dark
            ),
        requestFor(
            SchedulePrintStyle::DarkTheme,
            QPageLayout::Landscape,
            Theme::Light
            ),
        requestFor(
            SchedulePrintStyle::CurrentAppearance,
            QPageLayout::Portrait,
            Theme::Dark
            )
    };

    for (int index = 0; index < requests.size(); ++index)
    {
        const SchedulePrintService::Request& request =
            requests.at(index);
        const QString path =
            savePdf(
                temporaryDirectory,
                request,
                QStringLiteral("theme-%1.pdf").arg(index)
                );
        QVERIFY(!path.isEmpty());
        QPdfDocument document;
        loadDocument(
            document,
            path
            );
        const QImage image =
            renderFirstPage(document);
        QVERIFY(!image.isNull());
        const QRectF targetRect =
            scheduleTargetRect(
                image,
                request.model,
                false
                );

        const QPoint emptyCellCenter =
            scaledSchedulePoint(
                targetRect,
                TimeColumnWidth + (DayColumnWidth / 2.0),
                HeaderHeight + (RowHeight / 2.0),
                request.model
                );
        QVERIFY(
            isWhitePixel(
                image.pixelColor(emptyCellCenter)
                )
            );

        const QRectF contentRect =
            scheduleContentRect(image);
        const int belowTableY =
            qRound(
                std::min(
                    contentRect.bottom() - 5.0,
                    targetRect.bottom() + 30.0
                    )
                );
        QVERIFY(belowTableY > targetRect.bottom());

        const QPoint belowTablePoint(
            qRound(targetRect.center().x()),
            belowTableY
            );
        QVERIFY(
            isWhitePixel(
                image.pixelColor(belowTablePoint)
                )
            );
    }
}

QTEST_MAIN(SchedulePrintPdfTests)

#include "schedule_print_pdf_tests.moc"
