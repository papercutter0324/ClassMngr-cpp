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

QRect scaledScheduleRect(
    const QRectF& targetRect,
    const QRectF& sourceRect,
    const ScheduleViewModel& model
    )
{
    const qreal sourceWidth =
        TimeColumnWidth + (DayColumnWidth * model.days.size());
    const qreal scale =
        targetRect.width() / sourceWidth;
    return QRectF(
        targetRect.left() + (sourceRect.left() * scale),
        targetRect.top() + (sourceRect.top() * scale),
        sourceRect.width() * scale,
        sourceRect.height() * scale
        )
        .toAlignedRect();
}

int darkPixelCount(
    const QImage& image,
    const QRect& area
    )
{
    int count = 0;
    const QRect bounded =
        area.intersected(image.rect());
    for (int y = bounded.top(); y <= bounded.bottom(); ++y)
    {
        for (int x = bounded.left(); x <= bounded.right(); ++x)
        {
            const QColor color =
                image.pixelColor(x, y);
            if (
                color.red() < 120
                && color.green() < 120
                && color.blue() < 120
                )
            {
                ++count;
            }
        }
    }
    return count;
}
}

class SchedulePrintPdfTests : public QObject
{
    Q_OBJECT

private slots:
    void generatedPdfUsesA4Orientation();
    void contentStaysInsideHalfInchMargins();
    void themedEmptyCellsAndOffTableAreaStayWhite();
    void rendersTestingCellAndRoom();
    void rendersTestingClassCardInformation();
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

void SchedulePrintPdfTests::rendersTestingCellAndRoom()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    SchedulePrintService::Request request =
        requestFor(
            SchedulePrintStyle::LightTheme,
            QPageLayout::Landscape
            );
    ScheduleCellView& cell =
        request.model.rows.first().cells.first();
    cell.slotState =
        scheduleTestingSlotState();
    cell.testingRoom =
        QStringLiteral("Library");

    const QString path =
        savePdf(
            temporaryDirectory,
            request,
            QStringLiteral("testing.pdf")
            );
    QVERIFY(!path.isEmpty());

    QPdfDocument document;
    loadDocument(document, path);
    const QImage image =
        renderFirstPage(document);
    QVERIFY(!image.isNull());
    const QRectF targetRect =
        scheduleTargetRect(
            image,
            request.model,
            false
            );
    const QPoint testingCenter =
        scaledSchedulePoint(
            targetRect,
            TimeColumnWidth + (DayColumnWidth / 2.0),
            HeaderHeight + (RowHeight / 2.0),
            request.model
            );
    const QPoint markerPoint =
        scaledSchedulePoint(
            targetRect,
            TimeColumnWidth + DayColumnWidth - 5.0,
            HeaderHeight + 5.0,
            request.model
            );

    const QColor centerColor =
        image.pixelColor(testingCenter);
    const QColor markerColor =
        image.pixelColor(markerPoint);
    QVERIFY(!isWhitePixel(centerColor));
    QVERIFY(markerColor != centerColor);
}

void SchedulePrintPdfTests::rendersTestingClassCardInformation()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    SchedulePrintService::Request request =
        requestFor(
            SchedulePrintStyle::LightTheme,
            QPageLayout::Landscape
            );
    ScheduleCellView& cell =
        request.model.rows.first().cells.first();
    cell.testingClassAssignment = true;

    ScheduleEntry entry;
    entry.kind = ScheduleEntryKind::TestingClass;
    entry.classId = 42;
    entry.className = QStringLiteral("Writing Lab");
    entry.classGrade = QStringLiteral("M2");
    entry.classLevel = QStringLiteral("Mixed (All)");
    entry.teacherEn = QStringLiteral("Ms Han");
    entry.roomNumber = QStringLiteral("Library");
    entry.classColor = QStringLiteral("#C9D8A6");
    entry.fontColor = QStringLiteral("#000000");
    cell.entries = {entry};

    const QString path =
        savePdf(
            temporaryDirectory,
            request,
            QStringLiteral("testing-class.pdf")
            );
    QVERIFY(!path.isEmpty());

    QPdfDocument document;
    loadDocument(document, path);
    const QImage image =
        renderFirstPage(document);
    QVERIFY(!image.isNull());

    const QRectF targetRect =
        scheduleTargetRect(
            image,
            request.model,
            false
            );
    const QPoint markerPoint =
        scaledSchedulePoint(
            targetRect,
            TimeColumnWidth + DayColumnWidth - 5.0,
            HeaderHeight + 5.0,
            request.model
            );
    const QColor markerColor =
        image.pixelColor(markerPoint);
    QVERIFY(markerColor.red() < 80);
    QVERIFY(markerColor.green() < 80);
    QVERIFY(markerColor.blue() < 80);

    QRect cardRect =
        scaledScheduleRect(
            targetRect,
            QRectF(
                TimeColumnWidth,
                HeaderHeight,
                DayColumnWidth,
                RowHeight
                ),
            request.model
            );
    cardRect.adjust(
        cardRect.width() / 12,
        2,
        -(cardRect.width() / 12),
        -2
        );

    const int thirdHeight =
        cardRect.height() / 3;
    for (int line = 0; line < 3; ++line)
    {
        const QRect lineRect(
            cardRect.left(),
            cardRect.top() + (line * thirdHeight),
            cardRect.width(),
            line == 2
                ? cardRect.bottom()
                    - (cardRect.top() + (line * thirdHeight))
                    + 1
                : thirdHeight
            );
        QVERIFY2(
            darkPixelCount(image, lineRect) > 5,
            "Each testing-class card line should be rendered in the PDF."
            );
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
