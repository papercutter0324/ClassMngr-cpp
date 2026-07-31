#include "features/sub_prep/services/sub_prep_print_service.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QtTest>

#include <QColor>
#include <QFileInfo>
#include <QImage>
#include <QPageSize>
#include <QPdfDocument>
#include <QPdfSelection>
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
        Status::Canceled,
        QString()
    };
}
}

namespace
{
constexpr int RenderDpi = 300;
constexpr qreal PointsPerInch = 72.0;
constexpr qreal MarginInches = 0.5;

SubPrepPrintService::Request sampleRequest()
{
    SubPrepPrintService::Request request;
    request.campus = {
        QStringLiteral("02-555-1234"),
        QStringLiteral("DYB-Staff"),
        QStringLiteral("campus-password"),
        QStringLiteral("5678")
    };
    request.zoom = {
        QStringLiteral("teacher@example.com"),
        QStringLiteral("zoom-password")
    };
    request.classMaterials =
        QStringLiteral("Worksheets are in the blue folder.\nUse the projector remote from the desk.");
    request.gradingInstructions =
        QStringLiteral("Score each book report out of 100 and leave actionable feedback.");
    request.specialInstructions =
        QStringLiteral("Collect completed reports before students leave.");

    request.schedule.days = {
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday")
    };

    ScheduleRowView row;
    row.timeLabel = QStringLiteral("16:00");
    row.timeRangeLabel = QStringLiteral("4:00 -\n4:50");

    for (const QString& day : request.schedule.days)
    {
        ScheduleCellView cell;
        cell.day = day;
        cell.timeLabel = row.timeLabel;
        cell.defaultSlotState = scheduleEmptySlotState();
        cell.slotState = cell.defaultSlotState;

        if (day == QStringLiteral("Monday"))
        {
            cell.slotState = scheduleEssaySlotState();
        }
        else if (day == QStringLiteral("Tuesday"))
        {
            ScheduleEntry entry;
            entry.classId = 42;
            entry.teacherKr = QStringLiteral("김선생");
            entry.teacherEn = QStringLiteral("Susan");
            entry.roomNumber = QStringLiteral("413");
            entry.classGrade = QStringLiteral("E4");
            entry.classLevel = QStringLiteral("Hercules");
            entry.classColor = QStringLiteral("#dbeafe");
            entry.fontColor = QStringLiteral("#1e3a5f");
            cell.entries.append(entry);
        }

        row.cells.append(cell);
    }

    request.schedule.rows.append(row);

    SubPrepClassInformation::TeacherGroup group;
    group.displayName = QStringLiteral("Susan");
    group.classListText = QStringLiteral("E4 Hercules");
    group.teacher.id = 7;
    group.teacher.teacherKr = QStringLiteral("김선생");
    group.teacher.teacherEn = QStringLiteral("Susan");
    group.teacher.roomNumber = QStringLiteral("413");
    group.teacher.wifiName = QStringLiteral("Susan WiFi");
    group.teacher.wifiPassword = QStringLiteral("wifi secret");
    group.teacher.zoomId = QStringLiteral("susan.zoom");
    group.teacher.zoomPassword = QStringLiteral("zoom secret");
    group.teacher.internetType = QStringLiteral("WiFi");
    group.teacher.projectionType = QStringLiteral("HDMI");
    group.teacher.notes = QStringLiteral("Call the co-teacher before class.");

    SubPrepClassInformation::ClassDetails details;
    details.classId = 42;
    details.classLabel = QStringLiteral("E4 Hercules");
    details.info.classLevel = QStringLiteral("Hercules");
    details.info.classColor = QStringLiteral("#dbeafe");
    details.info.fontColor = QStringLiteral("#1e3a5f");
    details.info.notes = QStringLiteral("Read chapter three and discuss the vocabulary.");
    details.studentCount = 12;
    details.timeText = QStringLiteral("Tues 4pm");
    group.classes.append(details);
    request.classInformation.append(group);

    request.subNotes =
        QStringLiteral("Thank you for covering this class. Please leave a short handover note.");
    return request;
}

SubPrepPrintService::Request denseWeekdayScheduleRequest()
{
    SubPrepPrintService::Request request = sampleRequest();
    request.schedule.rows.clear();

    const QStringList timeLabels{
        QStringLiteral("16:00"),
        QStringLiteral("17:00"),
        QStringLiteral("18:00"),
        QStringLiteral("19:00"),
        QStringLiteral("20:00"),
        QStringLiteral("21:00")
    };
    const QStringList teacherNames{
        QStringLiteral("Jihye"),
        QStringLiteral("Hyeeyoung"),
        QStringLiteral("San"),
        QStringLiteral("Youjin"),
        QStringLiteral("Emma")
    };

    for (int rowIndex = 0; rowIndex < timeLabels.size(); ++rowIndex)
    {
        ScheduleRowView row;
        row.timeLabel = timeLabels.at(rowIndex);
        row.timeRangeLabel = QStringLiteral("%1:00 -\n%1:50")
            .arg(rowIndex + 4);

        for (int dayIndex = 0;
             dayIndex < request.schedule.days.size();
             ++dayIndex)
        {
            ScheduleCellView cell;
            cell.day = request.schedule.days.at(dayIndex);
            cell.timeLabel = row.timeLabel;
            cell.defaultSlotState = scheduleEmptySlotState();
            cell.slotState = cell.defaultSlotState;

            if (rowIndex >= 2 && rowIndex <= 4)
            {
                cell.slotState = scheduleEssaySlotState();
            }
            else
            {
                ScheduleEntry entry;
                entry.classId = (rowIndex * 10) + dayIndex + 1;
                entry.teacherEn = teacherNames.at(
                    (rowIndex + dayIndex) % teacherNames.size()
                    );
                entry.roomNumber = QString::number(
                    401 + (rowIndex * 10) + dayIndex
                    );
                entry.classGrade = QStringLiteral("E5");
                entry.classLevel =
                    dayIndex == 1
                        ? QStringLiteral("Extraordinary Advanced Course")
                        : QStringLiteral("Zeus");
                entry.classColor = QStringLiteral("#dbeafe");
                entry.fontColor = QStringLiteral("#1e3a5f");
                cell.entries.append(entry);
            }

            row.cells.append(cell);
        }

        request.schedule.rows.append(row);
    }

    return request;
}

void loadDocument(
    QPdfDocument& document,
    const QString& path
    )
{
    QCOMPARE(document.load(path), QPdfDocument::Error::None);
    QCOMPARE(document.status(), QPdfDocument::Status::Ready);
    QVERIFY(document.pageCount() > 0);
}

QImage renderPage(
    QPdfDocument& document,
    int pageIndex
    )
{
    const QSizeF pagePoints =
        document.pagePointSize(pageIndex);
    const QSize renderSize(
        std::max(
            1,
            qRound(pagePoints.width() * RenderDpi / PointsPerInch)
            ),
        std::max(
            1,
            qRound(pagePoints.height() * RenderDpi / PointsPerInch)
            )
        );

    return document.render(pageIndex, renderSize);
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
    int left = image.width();
    int top = image.height();
    int right = -1;
    int bottom = -1;

    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            if (isWhitePixel(image.pixelColor(x, y)))
            {
                continue;
            }

            left = std::min(left, x);
            top = std::min(top, y);
            right = std::max(right, x);
            bottom = std::max(bottom, y);
        }
    }

    if (right < left || bottom < top)
    {
        return {};
    }

    return QRect(QPoint(left, top), QPoint(right, bottom));
}

QString documentText(
    QPdfDocument& document
    )
{
    QString text;

    for (int pageIndex = 0; pageIndex < document.pageCount(); ++pageIndex)
    {
        text += document.getAllText(pageIndex).text();
    }

    return text;
}

QString pageText(
    QPdfDocument& document,
    int pageIndex
    )
{
    return document.getAllText(pageIndex).text();
}

QRectF textBounds(
    QPdfDocument& document,
    int pageIndex,
    const QString& text
    )
{
    const QString pageContents =
        pageText(document, pageIndex);
    const int index =
        pageContents.indexOf(text);

    if (index < 0)
    {
        return {};
    }

    const QPdfSelection selection =
        document.getSelectionAtIndex(
            pageIndex,
            index,
            text.size()
            );

    return selection.isValid()
        ? selection.boundingRectangle()
        : QRectF();
}

QList<QRectF> textBoundsForAllOccurrences(
    QPdfDocument& document,
    int pageIndex,
    const QString& text
    )
{
    QList<QRectF> bounds;
    const QString pageContents =
        pageText(document, pageIndex);
    int startIndex = 0;

    while (true)
    {
        const int index =
            pageContents.indexOf(text, startIndex);

        if (index < 0)
        {
            break;
        }

        const QPdfSelection selection =
            document.getSelectionAtIndex(
                pageIndex,
                index,
                text.size()
                );

        if (selection.isValid())
        {
            bounds.append(selection.boundingRectangle());
        }

        startIndex = index + text.size();
    }

    return bounds;
}

QRectF firstTextBounds(
    QPdfDocument& document,
    const QString& text
    )
{
    for (int pageIndex = 0; pageIndex < document.pageCount(); ++pageIndex)
    {
        const QRectF bounds =
            textBounds(document, pageIndex, text);

        if (!bounds.isEmpty())
        {
            return bounds;
        }
    }

    return {};
}

int longestVerticalInkRun(
    const QImage& image,
    const QRect& area
    )
{
    int longestRun = 0;

    for (int x = area.left(); x <= area.right(); ++x)
    {
        int currentRun = 0;

        for (int y = area.top(); y <= area.bottom(); ++y)
        {
            if (isWhitePixel(image.pixelColor(x, y)))
            {
                currentRun = 0;
                continue;
            }

            ++currentRun;
            longestRun = std::max(longestRun, currentRun);
        }
    }

    return longestRun;
}

bool isNearColor(
    const QColor& actual,
    const QColor& expected,
    int tolerance = 8
    )
{
    return std::abs(actual.red() - expected.red()) <= tolerance
        && std::abs(actual.green() - expected.green()) <= tolerance
        && std::abs(actual.blue() - expected.blue()) <= tolerance;
}

int longestVerticalColorRun(
    const QImage& image,
    const QRect& area,
    const QColor& expected
    )
{
    int longestRun = 0;

    for (int x = area.left(); x <= area.right(); ++x)
    {
        int currentRun = 0;

        for (int y = area.top(); y <= area.bottom(); ++y)
        {
            if (!isNearColor(image.pixelColor(x, y), expected))
            {
                currentRun = 0;
                continue;
            }

            ++currentRun;
            longestRun = std::max(longestRun, currentRun);
        }
    }

    return longestRun;
}

int horizontalColorLineCount(
    const QImage& image,
    const QRect& area,
    const QColor& expected,
    int minimumWidth
    )
{
    int lineCount = 0;
    bool previousRowHadLine = false;

    for (int y = area.top(); y <= area.bottom(); ++y)
    {
        int longestRun = 0;
        int currentRun = 0;

        for (int x = area.left(); x <= area.right(); ++x)
        {
            if (!isNearColor(image.pixelColor(x, y), expected))
            {
                currentRun = 0;
                continue;
            }

            ++currentRun;
            longestRun = std::max(longestRun, currentRun);
        }

        const bool rowHasLine = longestRun >= minimumWidth;

        if (rowHasLine && !previousRowHadLine)
        {
            ++lineCount;
        }

        previousRowHadLine = rowHasLine;
    }

    return lineCount;
}

int changedPixelCount(
    const QImage& first,
    const QImage& second
    )
{
    if (first.size() != second.size())
    {
        return 0;
    }

    int count = 0;

    for (int y = 0; y < first.height(); ++y)
    {
        for (int x = 0; x < first.width(); ++x)
        {
            const QColor left =
                first.pixelColor(x, y);
            const QColor right =
                second.pixelColor(x, y);

            if (
                std::abs(left.red() - right.red())
                    + std::abs(left.green() - right.green())
                    + std::abs(left.blue() - right.blue())
                > 20
                )
            {
                ++count;
            }
        }
    }

    return count;
}
}

class SubPrepPrintPdfTests : public QObject
{
    Q_OBJECT

private slots:
    void generatedPdfUsesA4PortraitWithNarrowMargins();
    void rendersEssayScheduleSlots();
    void rendersTestingScheduleSlots();
    void omitsUnavailableZoomInformation();
    void bookReportNotesUseCompactTextAndUnderlinedLabels();
    void rendersTeacherReferenceTableAndClassList();
    void teacherSectionBordersUseScheduleColor();
    void keepsTeacherSectionsTogetherAcrossPages();
    void subNotesUsePromptAndRuledWritingSpace();
    void subNotesArePresentForSelectedDayCombinations_data();
    void subNotesArePresentForSelectedDayCombinations();
    void denseWeekdayScheduleFitsSectionWidth();
    void centersSelectedWeekdayScheduleAtNominalColumnWidth();
    void usesSevenDayColumnMinimumWhenWeekendsAreShown();
};

void SubPrepPrintPdfTests::generatedPdfUsesA4PortraitWithNarrowMargins()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString path =
        temporaryDirectory.filePath(QStringLiteral("Sub Prep.pdf"));
    const SubPrepPrintService::Result result =
        SubPrepPrintService::saveSubPrepPdf(sampleRequest(), path);

    QCOMPARE(result.status, SubPrepPrintService::Status::Sent);
    QVERIFY(QFileInfo(path).size() > 0);

    QPdfDocument document;
    loadDocument(document, path);

    const QSizeF pageSize =
        document.pagePointSize(0);
    const QSizeF a4Points =
        QPageSize(QPageSize::A4).size(QPageSize::Point);
    QVERIFY(std::abs(pageSize.width() - a4Points.width()) < 1.0);
    QVERIFY(std::abs(pageSize.height() - a4Points.height()) < 1.0);

    const QRectF titleBounds =
        textBounds(document, 0, QStringLiteral("Sub Prep"));
    const QRectF subtitleBounds =
        textBounds(
            document,
            0,
            QStringLiteral("Thank you for subbing for me!")
            );
    QVERIFY(!titleBounds.isEmpty());
    QVERIFY(!subtitleBounds.isEmpty());
    constexpr qreal CenterTolerancePoints = 3.0;
    QVERIFY(
        std::abs(titleBounds.center().x() - (pageSize.width() / 2.0))
        <= CenterTolerancePoints
        );
    QVERIFY(
        std::abs(subtitleBounds.center().x() - (pageSize.width() / 2.0))
        <= CenterTolerancePoints
        );

    const QImage image = renderPage(document, 0);
    QVERIFY(!image.isNull());

    const QRect bounds = nonWhiteBounds(image);
    QVERIFY(!bounds.isNull());

    const int marginPixels =
        qRound(MarginInches * RenderDpi);
    constexpr int TolerancePixels = 4;
    QVERIFY(bounds.left() >= marginPixels - TolerancePixels);
    QVERIFY(bounds.top() >= marginPixels - TolerancePixels);
    QVERIFY(bounds.right() <= image.width() - marginPixels + TolerancePixels);
    QVERIFY(bounds.bottom() <= image.height() - marginPixels + TolerancePixels);

    const QRect rightBodyArea(
        image.width() / 2,
        image.height() / 8,
        image.width() / 2 - marginPixels + TolerancePixels,
        image.height() * 3 / 8
        );
    QVERIFY2(
        longestVerticalInkRun(image, rightBodyArea)
            >= qRound(RenderDpi * 0.25),
        "The printable tables should extend through the right side of the page."
        );
}

void SubPrepPrintPdfTests::rendersEssayScheduleSlots()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString essayPath =
        temporaryDirectory.filePath(QStringLiteral("Essay Sub Prep.pdf"));
    const SubPrepPrintService::Result essayResult =
        SubPrepPrintService::saveSubPrepPdf(sampleRequest(), essayPath);

    QCOMPARE(essayResult.status, SubPrepPrintService::Status::Sent);

    SubPrepPrintService::Request emptyRequest =
        sampleRequest();
    emptyRequest.schedule.rows.first().cells.first().slotState =
        scheduleEmptySlotState();
    const QString emptyPath =
        temporaryDirectory.filePath(QStringLiteral("Empty Schedule Sub Prep.pdf"));
    const SubPrepPrintService::Result emptyResult =
        SubPrepPrintService::saveSubPrepPdf(emptyRequest, emptyPath);

    QCOMPARE(emptyResult.status, SubPrepPrintService::Status::Sent);

    QPdfDocument essayDocument;
    loadDocument(essayDocument, essayPath);
    QPdfDocument emptyDocument;
    loadDocument(emptyDocument, emptyPath);
    QVERIFY(documentText(essayDocument).contains(QStringLiteral("ESSAY")));

    const QImage essayPage =
        renderPage(essayDocument, 0);
    const QImage emptyPage =
        renderPage(emptyDocument, 0);
    QVERIFY(!essayPage.isNull());
    QVERIFY(!emptyPage.isNull());
    QVERIFY(changedPixelCount(essayPage, emptyPage) > 100);
}

void SubPrepPrintPdfTests::rendersTestingScheduleSlots()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    SubPrepPrintService::Request request =
        sampleRequest();
    ScheduleCellView& cell =
        request.schedule.rows.first().cells.first();
    cell.slotState =
        scheduleTestingSlotState();
    cell.testingRoom =
        QStringLiteral("402");

    const QString path =
        temporaryDirectory.filePath(
            QStringLiteral("Testing Sub Prep.pdf")
            );
    const SubPrepPrintService::Result result =
        SubPrepPrintService::saveSubPrepPdf(
            request,
            path
            );
    QCOMPARE(result.status, SubPrepPrintService::Status::Sent);

    QPdfDocument document;
    loadDocument(document, path);
    const QString text =
        documentText(document);
    QVERIFY(text.contains(QStringLiteral("Oral Testing")));
    QVERIFY(text.contains(QStringLiteral("Rm: 402")));
}

void SubPrepPrintPdfTests::denseWeekdayScheduleFitsSectionWidth()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    SubPrepPrintService::Request request =
        denseWeekdayScheduleRequest();
    const QString path =
        temporaryDirectory.filePath(
            QStringLiteral("Dense Weekday Sub Prep.pdf")
            );
    const SubPrepPrintService::Result result =
        SubPrepPrintService::saveSubPrepPdf(request, path);
    QCOMPARE(result.status, SubPrepPrintService::Status::Sent);

    QPdfDocument document;
    loadDocument(document, path);

    const QRectF timeBounds =
        textBounds(document, 0, QStringLiteral("Time"));
    const QRectF mondayBounds =
        textBounds(document, 0, QStringLiteral("Monday"));
    const QRectF tuesdayBounds =
        textBounds(document, 0, QStringLiteral("Tuesday"));
    const QRectF fridayBounds =
        textBounds(document, 0, QStringLiteral("Friday"));
    QVERIFY(!timeBounds.isEmpty());
    QVERIFY(!mondayBounds.isEmpty());
    QVERIFY(!tuesdayBounds.isEmpty());
    QVERIFY(!fridayBounds.isEmpty());

    constexpr qreal TimeColumnWidthPoints = 64.0;
    constexpr int DayColumnCount = 5;
    constexpr qreal TolerancePoints = 4.0;
    const qreal pageWidth = document.pagePointSize(0).width();
    const qreal marginPoints = MarginInches * PointsPerInch;
    const qreal contentWidth =
        pageWidth - (2.0 * marginPoints);
    const qreal dayColumnWidth =
        (contentWidth - TimeColumnWidthPoints) / DayColumnCount;
    const qreal expectedTimeToDayCenterDistance =
        (TimeColumnWidthPoints + dayColumnWidth) / 2.0;

    QVERIFY(
        std::abs(
            mondayBounds.center().x() - timeBounds.center().x()
            - expectedTimeToDayCenterDistance
            )
        <= TolerancePoints
        );
    QVERIFY(
        std::abs(
            tuesdayBounds.center().x() - mondayBounds.center().x()
            - dayColumnWidth
            )
        <= TolerancePoints
        );

    const qreal tableLeft =
        timeBounds.center().x() - (TimeColumnWidthPoints / 2.0);
    const qreal tableRight =
        fridayBounds.center().x() + (dayColumnWidth / 2.0);
    QVERIFY(
        std::abs(
            tableLeft - marginPoints
            )
        <= TolerancePoints
        );
    QVERIFY(
        std::abs(
            tableRight - (pageWidth - marginPoints)
            )
        <= TolerancePoints
        );

    const QRectF longEntryBounds =
        textBounds(document, 0, QStringLiteral("Hyeeyoung 402"));
    QVERIFY(!longEntryBounds.isEmpty());
    QVERIFY(
        longEntryBounds.left()
        >= tuesdayBounds.center().x()
            - (dayColumnWidth / 2.0)
            - TolerancePoints
        );
    QVERIFY(
        longEntryBounds.right()
        <= tuesdayBounds.center().x()
            + (dayColumnWidth / 2.0)
            + TolerancePoints
        );

    const QRectF lastRowBounds =
        textBounds(document, 0, QStringLiteral("Emma 455"));
    QVERIFY(!lastRowBounds.isEmpty());

    const QImage image = renderPage(document, 0);
    QVERIFY(!image.isNull());
    const qreal pixelsPerPoint = RenderDpi / PointsPerInch;
    const QRect rightBorderArea(
        qRound((tableRight - 1.0) * pixelsPerPoint),
        qRound(timeBounds.top() * pixelsPerPoint),
        qRound(2.0 * pixelsPerPoint),
        qRound(
            (lastRowBounds.bottom() - timeBounds.top() + 10.0)
            * pixelsPerPoint
            )
        );
    QVERIFY(
        longestVerticalInkRun(image, rightBorderArea)
        >= qRound(RenderDpi * 0.5)
        );
}

void SubPrepPrintPdfTests
    ::centersSelectedWeekdayScheduleAtNominalColumnWidth()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    SubPrepPrintService::Request request = sampleRequest();
    request.schedule.days = {
        QStringLiteral("Monday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Friday")
    };
    request.schedule.rows.first().cells = {
        request.schedule.rows.first().cells.at(0),
        request.schedule.rows.first().cells.at(2),
        request.schedule.rows.first().cells.at(4)
    };

    const QString path =
        temporaryDirectory.filePath(
            QStringLiteral("Selected Weekdays Sub Prep.pdf")
            );
    const SubPrepPrintService::Result result =
        SubPrepPrintService::saveSubPrepPdf(request, path);
    QCOMPARE(result.status, SubPrepPrintService::Status::Sent);

    QPdfDocument document;
    loadDocument(document, path);

    const QRectF timeBounds =
        textBounds(document, 0, QStringLiteral("Time"));
    const QRectF mondayBounds =
        textBounds(document, 0, QStringLiteral("Monday"));
    const QRectF wednesdayBounds =
        textBounds(document, 0, QStringLiteral("Wednesday"));
    const QRectF fridayBounds =
        textBounds(document, 0, QStringLiteral("Friday"));
    QVERIFY(!timeBounds.isEmpty());
    QVERIFY(!mondayBounds.isEmpty());
    QVERIFY(!wednesdayBounds.isEmpty());
    QVERIFY(!fridayBounds.isEmpty());

    constexpr qreal TimeColumnWidthPoints = 64.0;
    constexpr int DayColumnCount = 5;
    constexpr qreal TolerancePoints = 4.0;
    const qreal pageWidth = document.pagePointSize(0).width();
    const qreal contentWidth =
        pageWidth - (2.0 * MarginInches * PointsPerInch);
    const qreal dayColumnWidth =
        (contentWidth - TimeColumnWidthPoints) / DayColumnCount;
    const qreal tableLeft =
        timeBounds.center().x() - (TimeColumnWidthPoints / 2.0);
    const qreal tableRight =
        fridayBounds.center().x() + (dayColumnWidth / 2.0);

    QVERIFY(
        std::abs(
            wednesdayBounds.center().x()
            - mondayBounds.center().x()
            - dayColumnWidth
            )
        <= TolerancePoints
        );
    QVERIFY(
        std::abs(
            fridayBounds.center().x()
            - wednesdayBounds.center().x()
            - dayColumnWidth
            )
        <= TolerancePoints
        );
    QVERIFY(
        std::abs(
            ((tableLeft + tableRight) / 2.0)
            - (pageWidth / 2.0)
            )
        <= TolerancePoints
        );
}

void SubPrepPrintPdfTests::usesSevenDayColumnMinimumWhenWeekendsAreShown()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    SubPrepPrintService::Request request = sampleRequest();
    request.schedule.days = {
        QStringLiteral("Monday"),
        QStringLiteral("Saturday")
    };
    request.schedule.rows.first().cells =
        request.schedule.rows.first().cells.mid(0, 2);
    request.schedule.rows.first().cells[1].day = QStringLiteral("Saturday");

    const QString path =
        temporaryDirectory.filePath(QStringLiteral("Weekend Sub Prep.pdf"));
    const SubPrepPrintService::Result result =
        SubPrepPrintService::saveSubPrepPdf(request, path);
    QCOMPARE(result.status, SubPrepPrintService::Status::Sent);

    QPdfDocument document;
    loadDocument(document, path);

    const QRectF mondayBounds =
        textBounds(document, 0, QStringLiteral("Monday"));
    const QRectF saturdayBounds =
        textBounds(document, 0, QStringLiteral("Saturday"));
    QVERIFY(!mondayBounds.isEmpty());
    QVERIFY(!saturdayBounds.isEmpty());

    constexpr qreal TimeColumnWidthPoints = 64.0;
    constexpr int WeekendDayColumnCount = 7;
    constexpr qreal TolerancePoints = 10.0;
    const qreal pageWidth = document.pagePointSize(0).width();
    const qreal contentWidth =
        pageWidth - (2.0 * MarginInches * PointsPerInch);
    const qreal dayColumnWidth =
        (contentWidth - TimeColumnWidthPoints) / WeekendDayColumnCount;

    QVERIFY(
        std::abs(
            saturdayBounds.center().x() - mondayBounds.center().x()
            - dayColumnWidth
            )
        <= TolerancePoints
        );
}

void SubPrepPrintPdfTests::omitsUnavailableZoomInformation()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    SubPrepPrintService::Request request = sampleRequest();
    request.zoom.loginId = QStringLiteral("N/A");
    request.zoom.password = QStringLiteral("N/A");

    const QString path =
        temporaryDirectory.filePath(QStringLiteral("No Zoom Sub Prep.pdf"));
    const SubPrepPrintService::Result result =
        SubPrepPrintService::saveSubPrepPdf(request, path);

    QCOMPARE(result.status, SubPrepPrintService::Status::Sent);

    QPdfDocument document;
    loadDocument(document, path);

    const QString text =
        documentText(document);
    QVERIFY(!text.contains(QStringLiteral("Personal Zoom Information")));
    QVERIFY(!text.contains(QStringLiteral("Zoom Login ID")));
}

void SubPrepPrintPdfTests::bookReportNotesUseCompactTextAndUnderlinedLabels()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    SubPrepPrintService::Request request =
        sampleRequest();
    const QString gradingInstructionsText =
        QStringLiteral("Grading Instructions");
    request.classMaterials = gradingInstructionsText;
    request.gradingInstructions = gradingInstructionsText;
    request.specialInstructions = gradingInstructionsText;
    const QString path =
        temporaryDirectory.filePath(
            QStringLiteral("Book Report Grading.pdf")
            );
    const SubPrepPrintService::Result result =
        SubPrepPrintService::saveSubPrepPdf(request, path);

    QCOMPARE(result.status, SubPrepPrintService::Status::Sent);

    QPdfDocument document;
    loadDocument(document, path);

    int bookReportPage = -1;

    for (int pageIndex = 0; pageIndex < document.pageCount(); ++pageIndex)
    {
        if (
            pageText(document, pageIndex).contains(
                QStringLiteral("Grading Instructions")
                )
            && pageText(document, pageIndex).contains(
                QStringLiteral("Special Instructions")
                )
            )
        {
            bookReportPage = pageIndex;
            break;
        }
    }

    QVERIFY(bookReportPage >= 0);

    const QList<QRectF> gradingInstructionsBounds =
        textBoundsForAllOccurrences(
            document,
            bookReportPage,
            gradingInstructionsText
            );

    QCOMPARE(gradingInstructionsBounds.size(), 4);

}

void SubPrepPrintPdfTests::rendersTeacherReferenceTableAndClassList()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    SubPrepPrintService::Request request = sampleRequest();
    SubPrepClassInformation::ClassDetails duplicate =
        request.classInformation.first().classes.first();
    duplicate.classId = 43;
    request.classInformation.first().classes.append(duplicate);

    const QString path =
        temporaryDirectory.filePath(QStringLiteral("Teacher Reference Sub Prep.pdf"));
    const SubPrepPrintService::Result result =
        SubPrepPrintService::saveSubPrepPdf(request, path);

    QCOMPARE(result.status, SubPrepPrintService::Status::Sent);

    QPdfDocument document;
    loadDocument(document, path);

    const QString text =
        documentText(document);
    QVERIFY(text.contains(QStringLiteral("English Name")));
    QVERIFY(!text.contains(QStringLiteral("Korean Teacher Name")));
    QVERIFY(text.contains(QStringLiteral("Susan")));
    QVERIFY(!text.contains(QStringLiteral("김선생")));
    QVERIFY(text.contains(QStringLiteral("Internet Type")));
    QVERIFY(text.contains(QStringLiteral("Projection Type")));
    QVERIFY(text.contains(QStringLiteral("Notes:")));
    QVERIFY(!text.contains(QStringLiteral("Class Notes")));
    QCOMPARE(text.count(QStringLiteral("E4 Hercules")), 2);
    QCOMPARE(text.count(QStringLiteral("Tues 4pm")), 2);
    QCOMPARE(text.count(QStringLiteral("12 Students")), 2);
}

void SubPrepPrintPdfTests::teacherSectionBordersUseScheduleColor()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    SubPrepPrintService::Request request = sampleRequest();
    const QString accent = QStringLiteral("#d05a7a");
    request.schedule.rows.first().cells[1].entries.first().classColor =
        accent;
    request.classInformation.first().classes.first().info.classColor =
        accent;

    const QString path =
        temporaryDirectory.filePath(QStringLiteral("Teacher Border Colors.pdf"));
    const SubPrepPrintService::Result result =
        SubPrepPrintService::saveSubPrepPdf(request, path);

    QCOMPARE(result.status, SubPrepPrintService::Status::Sent);

    QPdfDocument document;
    loadDocument(document, path);

    int teacherPage = -1;

    for (int pageIndex = 0; pageIndex < document.pageCount(); ++pageIndex)
    {
        if (pageText(document, pageIndex).contains(QStringLiteral("Susan")))
        {
            teacherPage = pageIndex;
            break;
        }
    }

    QVERIFY(teacherPage >= 0);

    const QImage image = renderPage(document, teacherPage);
    QVERIFY(!image.isNull());

    const int marginPixels =
        qRound(MarginInches * RenderDpi);
    constexpr int BorderSearchWidth = 24;
    const QRect leftBorderArea(
        marginPixels - 4,
        marginPixels,
        BorderSearchWidth,
        image.height() - (2 * marginPixels)
        );
    const QRect rightBorderArea(
        image.width() - marginPixels - BorderSearchWidth + 4,
        marginPixels,
        BorderSearchWidth,
        image.height() - (2 * marginPixels)
        );
    const int longestRun =
        std::max(
            longestVerticalColorRun(image, leftBorderArea, QColor(accent)),
            longestVerticalColorRun(image, rightBorderArea, QColor(accent))
            );

    QVERIFY2(
        longestRun >= qRound(RenderDpi * 0.5),
        qPrintable(
            QStringLiteral(
                "The schedule color should outline the teacher header and content; "
                "the longest detected edge was %1 px."
                )
                .arg(longestRun)
            )
        );
}

void SubPrepPrintPdfTests::keepsTeacherSectionsTogetherAcrossPages()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    SubPrepPrintService::Request request = sampleRequest();
    request.schedule.days = {QStringLiteral("Thursday")};
    request.schedule.rows.first().cells = {
        request.schedule.rows.first().cells.at(3)
    };
    SubPrepClassInformation::TeacherGroup firstGroup =
        request.classInformation.first();
    firstGroup.displayName = QStringLiteral("First Teacher");
    firstGroup.classes.clear();

    for (int classIndex = 0; classIndex < 14; ++classIndex)
    {
        SubPrepClassInformation::ClassDetails details =
            request.classInformation.first().classes.first();
        details.classId = 100 + classIndex;
        details.classLabel =
            QStringLiteral("First Class %1").arg(classIndex + 1);
        details.info.notes =
            QStringLiteral("Bring the required materials and complete the review activity.");
        firstGroup.classes.append(details);
    }

    SubPrepClassInformation::TeacherGroup secondGroup =
        request.classInformation.first();
    secondGroup.displayName = QStringLiteral("Second Teacher");
    secondGroup.classes.first().classLabel = QStringLiteral("Second Teacher Class");
    request.classInformation = {firstGroup, secondGroup};

    const QString path =
        temporaryDirectory.filePath(QStringLiteral("Teacher Page Breaks.pdf"));
    const SubPrepPrintService::Result result =
        SubPrepPrintService::saveSubPrepPdf(request, path);

    QCOMPARE(result.status, SubPrepPrintService::Status::Sent);

    QPdfDocument document;
    loadDocument(document, path);
    QCOMPARE(document.pageCount(), 3);
    QVERIFY(pageText(document, 1).contains(QStringLiteral("First Teacher")));
    QVERIFY(!pageText(document, 1).contains(QStringLiteral("Second Teacher")));
    QVERIFY(pageText(document, 2).contains(QStringLiteral("Second Teacher")));
    QVERIFY(
        pageText(document, 2).contains(
            QStringLiteral("Second Teacher Class")
            )
        );
    const QString text = documentText(document);
    QVERIFY(text.contains(QStringLiteral("Sub Notes")));
    QVERIFY(!text.contains(request.subNotes));
    QVERIFY(
        text.contains(
            QStringLiteral(
                "If there is anything important that you want me to know, "
                "please leave me some notes."
                )
            )
        );
    const int subNotesPageIndex = document.pageCount() - 1;
    QCOMPARE(subNotesPageIndex, 2);
    QVERIFY(
        pageText(document, subNotesPageIndex).contains(
            QStringLiteral("Sub Notes")
            )
        );
    const QRectF lastClassBounds =
        textBounds(
            document,
            subNotesPageIndex,
            QStringLiteral("Second Teacher Class")
            );
    const QRectF subNotesBounds =
        textBounds(
            document,
            subNotesPageIndex,
            QStringLiteral("Sub Notes")
            );
    QVERIFY(!lastClassBounds.isEmpty());
    QVERIFY(!subNotesBounds.isEmpty());
    QVERIFY(subNotesBounds.top() - lastClassBounds.bottom() >= 8.0);

    const QImage subNotesPage = renderPage(document, subNotesPageIndex);
    QVERIFY(!subNotesPage.isNull());
    const int marginPixels = qRound(MarginInches * RenderDpi);
    const QRect writingArea(
        marginPixels,
        subNotesPage.height() / 2,
        subNotesPage.width() - (2 * marginPixels),
        subNotesPage.height() / 2 - marginPixels
        );
    QVERIFY(
        horizontalColorLineCount(
            subNotesPage,
            writingArea,
            Qt::black,
            writingArea.width() * 9 / 10
            )
        >= 4
        );
}

void SubPrepPrintPdfTests::subNotesUsePromptAndRuledWritingSpace()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    SubPrepPrintService::Request request = sampleRequest();
    request.subNotes =
        QStringLiteral("This saved sub-note record must not be included in the PDF.");

    const QString path =
        temporaryDirectory.filePath(QStringLiteral("Sub Notes Writing Space.pdf"));
    const SubPrepPrintService::Result result =
        SubPrepPrintService::saveSubPrepPdf(request, path);

    QCOMPARE(result.status, SubPrepPrintService::Status::Sent);

    QPdfDocument document;
    loadDocument(document, path);

    const QString text =
        documentText(document);
    QVERIFY(text.contains(QStringLiteral("Thank you for subbing for me!")));
    QVERIFY(
        text.contains(
            QStringLiteral(
                "If there is anything important that you want me to know, "
                "please leave me some notes."
                )
            )
        );
    QVERIFY(!text.contains(request.subNotes));
    QVERIFY(!text.contains(QStringLiteral("ClassMngr")));
    QVERIFY(text.contains(QStringLiteral("Page 1")));

    const QSizeF firstPageSize =
        document.pagePointSize(0);
    const QRectF pageNumberBounds =
        textBounds(document, 0, QStringLiteral("Page 1"));
    QVERIFY(!pageNumberBounds.isEmpty());
    QVERIFY(pageNumberBounds.center().y() > firstPageSize.height() * 0.9);

    for (
        const QString& heading : {
            QStringLiteral("Important Information"),
            QStringLiteral("Schedule"),
            QStringLiteral("Class Information"),
            QStringLiteral("Sub Notes")
        }
        )
    {
        const QRectF headingBounds =
            firstTextBounds(document, heading);
        QVERIFY(!headingBounds.isEmpty());
        QVERIFY(
            std::abs(
                headingBounds.center().x()
                - (firstPageSize.width() / 2.0)
                )
            < 25.0
            );
    }

    const QImage subNotesPage =
        renderPage(document, document.pageCount() - 1);
    QVERIFY(!subNotesPage.isNull());

    const int marginPixels =
        qRound(MarginInches * RenderDpi);
    const QRect writingArea(
        marginPixels,
        subNotesPage.height() * 2 / 3,
        subNotesPage.width() - (2 * marginPixels),
        subNotesPage.height() / 3 - marginPixels
        );
    QVERIFY(
        horizontalColorLineCount(
            subNotesPage,
            writingArea,
            Qt::black,
            writingArea.width() * 9 / 10
            )
        >= 4
        );
}

void SubPrepPrintPdfTests
    ::subNotesArePresentForSelectedDayCombinations_data()
{
    QTest::addColumn<QStringList>("selectedDays");

    QTest::newRow("monday") << QStringList{QStringLiteral("Monday")};
    QTest::newRow("tuesday") << QStringList{QStringLiteral("Tuesday")};
    QTest::newRow("thursday") << QStringList{QStringLiteral("Thursday")};
    QTest::newRow("weekdays") << QStringList{
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday")
    };
    QTest::newRow("weekend") << QStringList{
        QStringLiteral("Saturday"),
        QStringLiteral("Sunday")
    };
    QTest::newRow("alternating-days") << QStringList{
        QStringLiteral("Monday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Friday"),
        QStringLiteral("Sunday")
    };
    QTest::newRow("full-week") << QStringList{
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday"),
        QStringLiteral("Saturday"),
        QStringLiteral("Sunday")
    };
}

void SubPrepPrintPdfTests
    ::subNotesArePresentForSelectedDayCombinations()
{
    QFETCH(QStringList, selectedDays);

    SubPrepPrintService::Request request = sampleRequest();
    request.subNotes =
        QStringLiteral("This saved note must stay out of every printed PDF.");
    request.schedule.days = selectedDays;
    request.schedule.rows.first().cells.clear();

    for (const QString& day : selectedDays)
    {
        ScheduleCellView cell;
        cell.day = day;
        cell.timeLabel = QStringLiteral("16:00");
        cell.defaultSlotState = scheduleEmptySlotState();
        cell.slotState = cell.defaultSlotState;
        request.schedule.rows.first().cells.append(cell);
    }

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString path =
        temporaryDirectory.filePath(QStringLiteral("Selected Days.pdf"));
    const SubPrepPrintService::Result result =
        SubPrepPrintService::saveSubPrepPdf(request, path);
    QCOMPARE(result.status, SubPrepPrintService::Status::Sent);

    QPdfDocument document;
    loadDocument(document, path);

    const QString text = documentText(document);
    QVERIFY(text.contains(QStringLiteral("Sub Notes")));
    QVERIFY(
        text.contains(
            QStringLiteral(
                "If there is anything important that you want me to know, "
                "please leave me some notes."
                )
            )
        );
    QVERIFY(!text.contains(request.subNotes));
}

QTEST_MAIN(SubPrepPrintPdfTests)

#include "sub_prep_print_pdf_tests.moc"
