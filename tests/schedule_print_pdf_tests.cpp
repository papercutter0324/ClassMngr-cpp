#include "features/schedule/services/schedule_print_service.h"
#include "features/schedule/ui/schedule_print_document_adapter.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QtTest>

#include <QColor>
#include <QFileInfo>
#include <QFont>
#include <QImage>
#include <QPageSize>
#include <QPdfDocument>
#include <QRegularExpression>
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

SchedulePrintService::Document emptyScheduleDocument()
{
    SchedulePrintService::Document document;
    document.days = {
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday")
    };

    for (const QString& timeLabel :
         { QStringLiteral("09:00"), QStringLiteral("10:00") })
    {
        SchedulePrintService::Row row;
        row.timeLabel = timeLabel;
        row.timeRangeLabel =
            timeLabel == QStringLiteral("09:00")
                ? QStringLiteral("9:00 -\n9:50")
                : QStringLiteral("10:00 -\n10:50");

        for (const QString& day : document.days)
        {
            SchedulePrintService::Cell cell;
            cell.day = day;
            cell.timeLabel = timeLabel;
            cell.slotState = SchedulePrintService::SlotState::Empty;
            row.cells.append(cell);
        }

        document.rows.append(row);
    }

    return document;
}

SchedulePrintService::Request requestFor(
    SchedulePrintStyle style,
    QPageLayout::Orientation orientation,
    Theme currentTheme = Theme::Dark
    )
{
    SchedulePrintService::Request request;
    request.document = emptyScheduleDocument();
    request.style = style;
    request.currentTheme = currentTheme;
    request.englishFont.setFamily(QStringLiteral("Arial"));
    request.koreanFont.setFamily(QStringLiteral("Pretendard"));
    request.userName = QStringLiteral("Warren");
    request.pageOrientation = orientation;
    return request;
}

QString compactPdfText(const QString& text)
{
    QString compact = text;
    compact.remove(QRegularExpression(QStringLiteral("\\s+")));
    return compact;
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
    const SchedulePrintService::Document& model,
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
    const SchedulePrintService::Document& model
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
    const SchedulePrintService::Document& model
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
    void legacyAdapterConvertsToValueDocument();
    void generatedPdfUsesA4Orientation();
    void savePdfFailsWithoutCreatingFileForMissingParent();
    void contentStaysInsideHalfInchMargins();
    void themedEmptyCellsAndOffTableAreaStayWhite();
    void excelTimeBackgroundRequiresAnExactValid24HourLabel();
    void rendersTestingCellAndRoom();
    void rendersTestingClassCardInformation();
};

void SchedulePrintPdfTests::legacyAdapterConvertsToValueDocument()
{
    ScheduleViewModel legacy;
    legacy.days = {QStringLiteral("Monday"), QStringLiteral("Tuesday")};
    legacy.summary.essayBlocks = 4;
    legacy.summary.scheduledBlocks = 9;
    ScheduleRowView row;
    row.timeLabel = QStringLiteral("16:00");
    row.timeRangeLabel = QStringLiteral("4:00 - 4:50 PM");
    ScheduleCellView testingClassCell;
    testingClassCell.day = QStringLiteral("Monday");
    testingClassCell.timeLabel = row.timeLabel;
    testingClassCell.slotState = scheduleTestingSlotState();
    testingClassCell.testingClassAssignment = true;
    ScheduleEntry entry;
    entry.kind = ScheduleEntryKind::TestingClass;
    entry.className = QStringLiteral("Writing Lab");
    entry.teacherKr = QStringLiteral("김선생");
    entry.teacherEn = QStringLiteral("Ms Han");
    entry.teacherPreferredName = QStringLiteral("Sam");
    entry.roomNumber = QStringLiteral("Library");
    entry.classGrade = QStringLiteral("M2");
    entry.classLevel = QStringLiteral("Mixed (All)");
    entry.classColor = QStringLiteral("#C9D8A6");
    entry.fontColor = QStringLiteral("#102030");
    testingClassCell.entries.append(entry);
    ScheduleCellView legacyPlainCell;
    legacyPlainCell.day = QStringLiteral("Tuesday");
    legacyPlainCell.timeLabel = row.timeLabel;
    legacyPlainCell.slotState = scheduleTestingSlotState();
    legacyPlainCell.testingRoom = QStringLiteral("Room 7");
    row.cells = {testingClassCell, legacyPlainCell};
    legacy.rows.append(row);

    const SchedulePrintService::Document document =
        schedulePrintDocumentFromLegacyModel(legacy);
    QCOMPARE(document.days, legacy.days);
    QCOMPARE(document.summary.essayBlocks, 4);
    QCOMPARE(document.summary.scheduledBlocks, 9);
    QCOMPARE(document.rows.size(), 1);
    QCOMPARE(document.rows.first().timeLabel, QStringLiteral("16:00"));
    QCOMPARE(document.rows.first().cells.size(), 2);
    const auto& classCell = document.rows.first().cells.first();
    QCOMPARE(classCell.slotState, SchedulePrintService::SlotState::Testing);
    QCOMPARE(classCell.entries.size(), 1);
    QCOMPARE(classCell.entries.first().className, QStringLiteral("Writing Lab"));
    QCOMPARE(classCell.entries.first().teacherKoreanName, QStringLiteral("김선생"));
    QCOMPARE(classCell.entries.first().teacherEnglishName, QStringLiteral("Ms Han"));
    QCOMPARE(classCell.entries.first().teacherPreferredName, QStringLiteral("Sam"));
    QCOMPARE(classCell.entries.first().room, QStringLiteral("Library"));
    QCOMPARE(classCell.entries.first().grade, QStringLiteral("M2"));
    QCOMPARE(classCell.entries.first().level, QStringLiteral("Mixed (All)"));
    QVERIFY(classCell.entries.first().testingClass);
    const auto& plainCell = document.rows.first().cells.at(1);
    QCOMPARE(plainCell.testingRoom, QStringLiteral("Room 7"));
    QCOMPARE(plainCell.slotState, SchedulePrintService::SlotState::Testing);
}

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

void SchedulePrintPdfTests::savePdfFailsWithoutCreatingFileForMissingParent()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    SchedulePrintService::Request request =
        requestFor(
            SchedulePrintStyle::LightTheme,
            QPageLayout::Landscape
            );
    const QString path = temporaryDirectory.filePath(
        QStringLiteral("missing-parent/schedule.pdf"));
    QVERIFY(!QFileInfo::exists(path));

    const SchedulePrintService::Result result =
        SchedulePrintService::saveSchedulePdf(request, path);

    QCOMPARE(result.status, SchedulePrintService::Status::Failed);
    QVERIFY(!QFileInfo::exists(path));
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
    SchedulePrintService::Cell& cell =
        request.document.rows.first().cells.first();
    cell.slotState = SchedulePrintService::SlotState::Testing;
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
    const QString pdfText = compactPdfText(document.getAllText(0).text());
    QVERIFY(pdfText.contains(QStringLiteral("OralTesting")));
    QVERIFY(pdfText.contains(QStringLiteral("Library")));
    const QImage image =
        renderFirstPage(document);
    QVERIFY(!image.isNull());
    const QRectF targetRect =
        scheduleTargetRect(
            image,
            request.document,
            false
            );
    const QPoint testingCenter =
        scaledSchedulePoint(
            targetRect,
            TimeColumnWidth + (DayColumnWidth / 2.0),
            HeaderHeight + (RowHeight / 2.0),
            request.document
            );
    const QPoint markerPoint =
        scaledSchedulePoint(
            targetRect,
            TimeColumnWidth + DayColumnWidth - 5.0,
            HeaderHeight + 5.0,
            request.document
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
    SchedulePrintService::Cell& cell =
        request.document.rows.first().cells.first();

    SchedulePrintService::Entry entry;
    entry.className = QStringLiteral("Writing Lab");
    entry.grade = QStringLiteral("M2");
    entry.level = QStringLiteral("Mixed (All)");
    entry.teacherEnglishName = QStringLiteral("Ms Han");
    entry.room = QStringLiteral("Library");
    entry.classColor = QStringLiteral("#C9D8A6");
    entry.fontColor = QStringLiteral("#000000");
    entry.testingClass = true;
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
    const QString pdfText = compactPdfText(document.getAllText(0).text());
    QVERIFY(pdfText.contains(QStringLiteral("WritingLab")));
    QVERIFY(pdfText.contains(QStringLiteral("M2")));
    QVERIFY(pdfText.contains(QStringLiteral("Mixed(All)")));
    QVERIFY(pdfText.contains(QStringLiteral("Library")));
    const QImage image =
        renderFirstPage(document);
    QVERIFY(!image.isNull());

    const QRectF targetRect =
        scheduleTargetRect(
            image,
            request.document,
            false
            );
    const QPoint markerPoint =
        scaledSchedulePoint(
            targetRect,
            TimeColumnWidth + DayColumnWidth - 5.0,
            HeaderHeight + 5.0,
            request.document
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
            request.document
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
            Theme::Light
            ),
        requestFor(
            SchedulePrintStyle::CurrentAppearance,
            QPageLayout::Portrait,
            Theme::Dark
            ),
        requestFor(
            SchedulePrintStyle::Excel,
            QPageLayout::Landscape,
            Theme::Light
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
                request.document,
                request.style == SchedulePrintStyle::Excel
                );

        const QPoint headerPoint =
            scaledSchedulePoint(
                targetRect,
                TimeColumnWidth + 10.0,
                10.0,
                request.document
                );
        const QColor headerColor =
            image.pixelColor(headerPoint);
        if (request.style == SchedulePrintStyle::Excel)
        {
            QCOMPARE(headerColor, QColor(Qt::white));

            const QPoint timePoint =
                scaledSchedulePoint(
                    targetRect,
                    10.0,
                    HeaderHeight + (RowHeight / 2.0),
                    request.document
                    );
            QCOMPARE(
                image.pixelColor(timePoint),
                QColor(QStringLiteral("#D9D9D9"))
                );

            const QPoint alternatingTimePoint =
                scaledSchedulePoint(
                    targetRect,
                    10.0,
                    HeaderHeight
                        + RowHeight
                        + (RowHeight / 2.0),
                    request.document
                    );
            QCOMPARE(
                image.pixelColor(alternatingTimePoint),
                QColor(QStringLiteral("#FFFF99"))
                );
        }
        else
        {
            const bool light =
                request.style == SchedulePrintStyle::LightTheme
                || (
                    request.style
                        == SchedulePrintStyle::CurrentAppearance
                    && request.currentTheme == Theme::Light
                    );
            QCOMPARE(
                headerColor,
                light
                    ? QColor(QStringLiteral("#deded8"))
                    : QColor(QStringLiteral("#303030"))
                );
        }

        const QPoint emptyCellCenter =
            scaledSchedulePoint(
                targetRect,
                TimeColumnWidth + (DayColumnWidth / 2.0),
                HeaderHeight + (RowHeight / 2.0),
                request.document
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

void SchedulePrintPdfTests::excelTimeBackgroundRequiresAnExactValid24HourLabel()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    SchedulePrintService::Request request =
        requestFor(
            SchedulePrintStyle::Excel,
            QPageLayout::Landscape
            );

    SchedulePrintService::Row paddedEvenHour =
        request.document.rows.at(1);
    paddedEvenHour.timeLabel =
        QStringLiteral("10:00 ");
    request.document.rows.append(paddedEvenHour);

    SchedulePrintService::Row malformedLabel =
        request.document.rows.at(1);
    malformedLabel.timeLabel =
        QStringLiteral("not-a-time");
    request.document.rows.append(malformedLabel);

    const QString path =
        savePdf(
            temporaryDirectory,
            request,
            QStringLiteral("excel-exact-time-labels.pdf")
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
            request.document,
            true
            );
    const auto timeCellColor =
        [&image, &targetRect, &request](int rowIndex)
        {
            const QPoint point =
                scaledSchedulePoint(
                    targetRect,
                    10.0,
                    HeaderHeight
                        + (rowIndex * RowHeight)
                        + (RowHeight / 2.0),
                    request.document
                    );
            return image.pixelColor(point);
        };

    QCOMPARE(timeCellColor(0), QColor(QStringLiteral("#D9D9D9")));
    QCOMPARE(timeCellColor(1), QColor(QStringLiteral("#FFFF99")));
    QCOMPARE(timeCellColor(2), QColor(QStringLiteral("#D9D9D9")));
    QCOMPARE(timeCellColor(3), QColor(QStringLiteral("#D9D9D9")));
}

QTEST_MAIN(SchedulePrintPdfTests)

#include "schedule_print_pdf_tests.moc"
