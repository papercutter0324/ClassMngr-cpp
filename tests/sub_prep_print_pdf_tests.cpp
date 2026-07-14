#include "features/sub_prep/ui/sub_prep_print_service.h"
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
}

class SubPrepPrintPdfTests : public QObject
{
    Q_OBJECT

private slots:
    void generatedPdfUsesA4PortraitWithNarrowMargins();
    void rendersEssayScheduleSlots();
    void omitsUnavailableZoomInformation();
    void rendersTeacherReferenceTableAndClassList();
    void teacherSectionBordersUseScheduleColor();
    void keepsTeacherSectionsTogetherAcrossPages();
    void longNotesFlowOntoAdditionalPages();
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

    const QString path =
        temporaryDirectory.filePath(QStringLiteral("Essay Sub Prep.pdf"));
    const SubPrepPrintService::Result result =
        SubPrepPrintService::saveSubPrepPdf(sampleRequest(), path);

    QCOMPARE(result.status, SubPrepPrintService::Status::Sent);

    QPdfDocument document;
    loadDocument(document, path);
    QVERIFY(documentText(document).contains(QStringLiteral("Essay")));
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
    QVERIFY(text.contains(QStringLiteral("Korean Teacher Name")));
    QVERIFY(text.contains(QStringLiteral("김선생")));
    QVERIFY(text.contains(QStringLiteral("Internet Type")));
    QVERIFY(text.contains(QStringLiteral("Projection Type")));
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
    QVERIFY(document.pageCount() >= 3);
    QVERIFY(pageText(document, 1).contains(QStringLiteral("First Teacher")));
    QVERIFY(!pageText(document, 1).contains(QStringLiteral("Second Teacher")));
    QVERIFY(pageText(document, 2).contains(QStringLiteral("Second Teacher")));
    QVERIFY(
        pageText(document, 2).contains(
            QStringLiteral("Second Teacher Class")
            )
        );
}

void SubPrepPrintPdfTests::longNotesFlowOntoAdditionalPages()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    SubPrepPrintService::Request request = sampleRequest();
    request.subNotes =
        QStringLiteral("Please record attendance and return materials to the cabinet. ")
            .repeated(900);

    const QString path =
        temporaryDirectory.filePath(QStringLiteral("Long Sub Prep.pdf"));
    const SubPrepPrintService::Result result =
        SubPrepPrintService::saveSubPrepPdf(request, path);

    QCOMPARE(result.status, SubPrepPrintService::Status::Sent);

    QPdfDocument document;
    loadDocument(document, path);
    QVERIFY(document.pageCount() > 1);

    const QImage secondPage = renderPage(document, 1);
    QVERIFY(!secondPage.isNull());

    const int marginPixels =
        qRound(MarginInches * RenderDpi);
    int bodyInkPixels = 0;

    for (
        int y = marginPixels + 80;
        y < secondPage.height() - marginPixels - 60;
        ++y
        )
    {
        for (int x = marginPixels; x < secondPage.width() - marginPixels; ++x)
        {
            if (!isWhitePixel(secondPage.pixelColor(x, y)))
            {
                ++bodyInkPixels;
            }
        }
    }

    QVERIFY(bodyInkPixels > 500);
}

QTEST_MAIN(SubPrepPrintPdfTests)

#include "sub_prep_print_pdf_tests.moc"
