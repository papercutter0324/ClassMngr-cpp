#include "features/sub_prep/ui/sub_prep_print_service.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QtTest>

#include <QColor>
#include <QFileInfo>
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

        if (day == QStringLiteral("Tuesday"))
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
}

class SubPrepPrintPdfTests : public QObject
{
    Q_OBJECT

private slots:
    void generatedPdfUsesA4PortraitWithNarrowMargins();
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
