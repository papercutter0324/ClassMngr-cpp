#include "features/roster/ui/roster_template_print_service.h"

#include "core/application_services.h"
#include "data/data_service.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QtTest>

#include <QColor>
#include <QHash>
#include <QImage>
#include <QPageSize>
#include <QPdfDocument>
#include <QRect>
#include <QRectF>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
bool g_hasOpenDatabase = false;
bool g_hasDataService = false;
QList<Classroom> g_classes;
QHash<int, ClassInfo> g_classInfo;
QHash<int, Roster> g_rosters;

alignas(ApplicationServices) unsigned char
    g_fakeApplicationServicesStorage[sizeof(ApplicationServices)];
alignas(DataService) unsigned char
    g_fakeDataServiceStorage[sizeof(DataService)];

ApplicationServices* fakeApplicationServices()
{
    return reinterpret_cast<ApplicationServices*>(
        g_fakeApplicationServicesStorage
        );
}

DataService* fakeDataService()
{
    return reinterpret_cast<DataService*>(
        g_fakeDataServiceStorage
        );
}

void resetServiceStubs()
{
    g_hasOpenDatabase = false;
    g_hasDataService = false;
    g_classes.clear();
    g_classInfo.clear();
    g_rosters.clear();
}
}

bool ApplicationServices::hasOpenDatabase() const
{
    return g_hasOpenDatabase;
}

DataService* ApplicationServices::dataService() const
{
    return g_hasDataService
        ? fakeDataService()
        : nullptr;
}

QList<Classroom> DataService::getClasses()
{
    return g_classes;
}

Classroom DataService::getClassById(
    int classId
    )
{
    for (const Classroom& classroom : std::as_const(g_classes))
    {
        if (classroom.id == classId)
        {
            return classroom;
        }
    }

    Classroom classroom;
    classroom.id = classId;
    return classroom;
}

ClassInfo DataService::loadClassInfo(
    int classId
    )
{
    if (g_classInfo.contains(classId))
    {
        return g_classInfo.value(classId);
    }

    ClassInfo info;
    info.classId = classId;
    return info;
}

Roster DataService::loadRoster(
    int classId
    )
{
    return g_rosters.value(classId);
}

namespace PdfPrintService
{
Result printPdfDocument(
    const Request& request
    )
{
    Q_UNUSED(request);
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
constexpr qreal ColumnWidthInches = 0.6833;
constexpr qreal TitleRowHeightInches = 0.3000;
constexpr qreal NormalRowHeightInches = 0.2000;
constexpr qreal SectionEndRowHeightInches = 0.2083;
constexpr qreal BorderWidthPoints = 2.0;
constexpr int RowCount = 33;
constexpr int ColumnCount = 13;
constexpr int LevelRow = 3;
constexpr int TeacherRoomRow = 4;
constexpr int FirstStudentRow = 5;
constexpr int LastStudentRow = 29;
constexpr int WifiRow = 30;

RosterTemplatePrintService::RosterClassData sampleRosterClass(
    int classId,
    const QString& day,
    const QString& startTime,
    const QString& grade = QStringLiteral("E4"),
    const QString& level = QStringLiteral("Hercules"),
    const QString& teacherEn = QStringLiteral("Emma"),
    const QString& teacherKr = QString(),
    const QString& room = QStringLiteral("506")
    )
{
    RosterTemplatePrintService::RosterClassData data;
    data.classroom.id = classId;
    data.classroom.name =
        QStringLiteral("Class %1").arg(classId);
    data.info.classId = classId;
    data.info.classGrade = grade;
    data.info.classLevel = level;
    data.info.teacherEn = teacherEn;
    data.info.teacherKr = teacherKr;
    data.info.roomNumber = room;
    data.info.wifiName = QStringLiteral("DYB");
    data.info.wifiPassword = QStringLiteral("pw");
    data.info.zoomId = QStringLiteral("zoom");
    data.info.zoomPassword = QStringLiteral("zoom-pw");
    data.info.classTimes.append(
        {
            day,
            startTime,
            QStringLiteral("4:50 PM")
        }
        );
    data.roster.columns = {
        QStringLiteral("English"),
        QStringLiteral("Korean")
    };
    data.roster.rows = {
        {
            QStringLiteral("Lily"),
            QStringLiteral("Lily KR")
        },
        {
            QStringLiteral("Jay"),
            QStringLiteral("Jay KR")
        }
    };
    return data;
}

bool hasCellValue(
    const QList<RosterTemplatePrintService::RosterCellValue>& values,
    const QString& day,
    int row,
    int column,
    const QString& expectedValue
    )
{
    for (const auto& value : values)
    {
        if (
            value.day == day
            && value.row == row
            && value.column == column
            && value.value == expectedValue
            )
        {
            return true;
        }
    }

    return false;
}

QString savePdf(
    QTemporaryDir& temporaryDirectory,
    const QList<RosterTemplatePrintService::RosterClassData>& classes,
    const QString& fileName
    )
{
    const QString path =
        temporaryDirectory.filePath(fileName);
    const RosterTemplatePrintService::Result result =
        RosterTemplatePrintService::saveRostersPdf(
            classes,
            path
            );

    return result.status == RosterTemplatePrintService::Status::Sent
        ? path
        : QString();
}

void loadDocument(
    QPdfDocument& document,
    const QString& path,
    int expectedPageCount
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
    QCOMPARE(document.pageCount(), expectedPageCount);
}

QImage renderPage(
    QPdfDocument& document,
    int pageIndex = 0
    )
{
    const QSizeF pointSize =
        document.pagePointSize(pageIndex);
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

    return document.render(
        pageIndex,
        renderSize
        );
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

QRectF rosterContentRect(
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

qreal baseColumnWidth()
{
    return ColumnWidthInches * RenderDpi;
}

qreal adjustedColumnWidth(
    const QImage& image,
    int column
    )
{
    const qreal extraWidthPerEnglishColumn =
        std::max(
            0.0,
            rosterContentRect(image).width()
                - (ColumnCount * baseColumnWidth())
            )
        / 6.0;

    const bool englishColumn =
        column == 2
        || column == 4
        || column == 6
        || column == 8
        || column == 10
        || column == 12;

    return baseColumnWidth()
        + (englishColumn ? extraWidthPerEnglishColumn : 0.0);
}

qreal adjustedColumnLeft(
    const QImage& image,
    int column
    )
{
    qreal left =
        rosterContentRect(image).left();

    for (int index = 1; index < column; ++index)
    {
        left += adjustedColumnWidth(
            image,
            index
            );
    }

    return left;
}

qreal baseRowHeight(
    int row
    )
{
    qreal inches =
        NormalRowHeightInches;

    if (row == 1)
    {
        inches =
            TitleRowHeightInches;
    }
    else if (
        row == TeacherRoomRow
        || row == LastStudentRow
        || row == RowCount
        )
    {
        inches =
            SectionEndRowHeightInches;
    }

    return inches * RenderDpi;
}

qreal adjustedRowHeight(
    const QImage& image,
    int row
    )
{
    qreal baseTableHeight = 0.0;

    for (int index = 1; index <= RowCount; ++index)
    {
        baseTableHeight += baseRowHeight(index);
    }

    const qreal extraHeightPerRow =
        std::max(
            0.0,
            rosterContentRect(image).height() - baseTableHeight
            )
        / RowCount;

    return baseRowHeight(row) + extraHeightPerRow;
}

qreal adjustedRowTop(
    const QImage& image,
    int row
    )
{
    qreal top =
        rosterContentRect(image).top();

    for (int index = 1; index < row; ++index)
    {
        top += adjustedRowHeight(
            image,
            index
            );
    }

    return top;
}

QPoint emptyStudentCellCenter(
    const QImage& image
    )
{
    return QPoint(
        qRound(
            adjustedColumnLeft(image, 4)
            + (adjustedColumnWidth(image, 4) / 2.0)
            ),
        qRound(
            adjustedRowTop(image, FirstStudentRow)
            + (adjustedRowHeight(image, FirstStudentRow) / 2.0)
            )
        );
}

QPoint emptyStudentCellBorderPoint(
    const QImage& image
    )
{
    const qreal borderWidth =
        BorderWidthPoints * RenderDpi / PointsPerInch;

    return QPoint(
        qRound(
            adjustedColumnLeft(image, 4)
            + (borderWidth / 2.0)
            ),
        qRound(
            adjustedRowTop(image, FirstStudentRow)
            + (adjustedRowHeight(image, FirstStudentRow) / 2.0)
            )
        );
}

bool hasNonWhitePixelNear(
    const QImage& image,
    const QPoint& point,
    int radius
    )
{
    for (int y = std::max(0, point.y() - radius);
         y <= std::min(image.height() - 1, point.y() + radius);
         ++y)
    {
        for (int x = std::max(0, point.x() - radius);
             x <= std::min(image.width() - 1, point.x() + radius);
             ++x)
        {
            if (!isWhitePixel(image.pixelColor(x, y)))
            {
                return true;
            }
        }
    }

    return false;
}

QPoint cellInteriorPoint(
    const QImage& image,
    int row,
    int column,
    int columnSpan = 1,
    qreal xFraction = 0.5,
    qreal yFraction = 0.5
    )
{
    qreal width = 0.0;
    for (int index = 0; index < columnSpan; ++index)
    {
        width += adjustedColumnWidth(
            image,
            column + index
            );
    }

    return QPoint(
        qRound(
            adjustedColumnLeft(image, column)
            + (width * xFraction)
            ),
        qRound(
            adjustedRowTop(image, row)
            + (adjustedRowHeight(image, row) * yFraction)
            )
        );
}

bool colorNear(
    const QColor& color,
    const QColor& expected,
    int tolerance = 8
    )
{
    return std::abs(color.red() - expected.red()) <= tolerance
        && std::abs(color.green() - expected.green()) <= tolerance
        && std::abs(color.blue() - expected.blue()) <= tolerance;
}
}

class RosterTemplatePrintServiceTests : public QObject
{
    Q_OBJECT

private slots:
    void resolveClassIdsUsesRequestedScope();
    void requestSaveRostersPdfRejectsEmptyPath();
    void requestSaveRostersPdfRejectsMissingDatabase();
    void requestSaveRostersPdfUsesSelectedClassScope();
    void buildByDayCellValuesMapsClassToTimeBlock();
    void buildByDayCellValuesWritesTwentyFiveStudentRows();
    void buildByDayCellValuesUsesTeacherFallback();
    void buildByDayCellValuesRejectsDuplicateSlots();
    void generatedPdfUsesA4LandscapeAndFilledWeekdayPages();
    void generatedPdfStaysInsideHalfInchMarginsAndKeepsEmptyCells();
};

void RosterTemplatePrintServiceTests::resolveClassIdsUsesRequestedScope()
{
    QList<Classroom> classes;
    for (int classId : {10, 20, 30})
    {
        Classroom classroom;
        classroom.id = classId;
        classes.append(classroom);
    }

    QCOMPARE(
        RosterTemplatePrintService::resolveClassIds(
            RosterTemplatePrintService::Scope::AllClasses,
            20,
            {},
            classes
            ),
        QList<int>({10, 20, 30})
        );
    QCOMPARE(
        RosterTemplatePrintService::resolveClassIds(
            RosterTemplatePrintService::Scope::CurrentClass,
            20,
            {},
            classes
            ),
        QList<int>({20})
        );
    QCOMPARE(
        RosterTemplatePrintService::resolveClassIds(
            RosterTemplatePrintService::Scope::SelectedClasses,
            20,
            {30, 10, 30},
            classes
            ),
        QList<int>({30, 10})
        );
}

void RosterTemplatePrintServiceTests::requestSaveRostersPdfRejectsEmptyPath()
{
    resetServiceStubs();

    const RosterTemplatePrintService::Result result =
        RosterTemplatePrintService::saveRostersPdf(
            RosterTemplatePrintService::Request(),
            QString()
            );

    QCOMPARE(result.status, RosterTemplatePrintService::Status::Failed);
    QVERIFY(
        result.message.contains(
            QStringLiteral("No roster print file path")
            )
        );
}

void RosterTemplatePrintServiceTests::requestSaveRostersPdfRejectsMissingDatabase()
{
    resetServiceStubs();

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    RosterTemplatePrintService::Request request;
    request.services =
        fakeApplicationServices();

    const RosterTemplatePrintService::Result result =
        RosterTemplatePrintService::saveRostersPdf(
            request,
            temporaryDirectory.filePath(QStringLiteral("rosters.pdf"))
            );

    QCOMPARE(result.status, RosterTemplatePrintService::Status::Failed);
    QVERIFY(result.message.contains(QStringLiteral("No database")));
}

void RosterTemplatePrintServiceTests::requestSaveRostersPdfUsesSelectedClassScope()
{
    resetServiceStubs();
    g_hasOpenDatabase = true;
    g_hasDataService = true;

    Classroom currentClass;
    currentClass.id = 10;
    currentClass.name = QStringLiteral("Current");

    const RosterTemplatePrintService::RosterClassData selectedClass =
        sampleRosterClass(
            20,
            QStringLiteral("Monday"),
            QStringLiteral("4:00 PM")
            );
    const RosterTemplatePrintService::RosterClassData duplicateClass =
        sampleRosterClass(
            30,
            QStringLiteral("Monday"),
            QStringLiteral("4:00 PM")
            );

    g_classes = {
        currentClass,
        selectedClass.classroom,
        duplicateClass.classroom
    };
    g_classInfo.insert(
        selectedClass.classroom.id,
        selectedClass.info
        );
    g_classInfo.insert(
        duplicateClass.classroom.id,
        duplicateClass.info
        );
    g_rosters.insert(
        selectedClass.classroom.id,
        selectedClass.roster
        );
    g_rosters.insert(
        duplicateClass.classroom.id,
        duplicateClass.roster
        );

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString path =
        temporaryDirectory.filePath(QStringLiteral("selected-roster.pdf"));

    RosterTemplatePrintService::Request request;
    request.services =
        fakeApplicationServices();
    request.currentClassId =
        currentClass.id;
    request.scope =
        RosterTemplatePrintService::Scope::SelectedClasses;
    request.selectedClassIds = {
        selectedClass.classroom.id
    };

    const RosterTemplatePrintService::Result result =
        RosterTemplatePrintService::saveRostersPdf(
            request,
            path
            );

    QCOMPARE(result.status, RosterTemplatePrintService::Status::Sent);

    QPdfDocument document;
    loadDocument(
        document,
        path,
        1
        );
}

void RosterTemplatePrintServiceTests::buildByDayCellValuesMapsClassToTimeBlock()
{
    QString error;
    const QList<RosterTemplatePrintService::RosterCellValue> values =
        RosterTemplatePrintService::buildByDayCellValues(
            {
                sampleRosterClass(
                    1,
                    QStringLiteral("Monday"),
                    QStringLiteral("4:00 PM")
                    )
            },
            &error
            );

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(hasCellValue(values, QStringLiteral("Monday"), LevelRow, 2, QStringLiteral("E4 Hercules")));
    QVERIFY(hasCellValue(values, QStringLiteral("Monday"), TeacherRoomRow, 2, QStringLiteral("Emma (506)")));
    QVERIFY(hasCellValue(values, QStringLiteral("Monday"), FirstStudentRow, 2, QStringLiteral("Lily")));
    QVERIFY(hasCellValue(values, QStringLiteral("Monday"), FirstStudentRow, 3, QStringLiteral("Lily KR")));
    QVERIFY(hasCellValue(values, QStringLiteral("Monday"), FirstStudentRow + 1, 2, QStringLiteral("Jay")));
    QVERIFY(hasCellValue(values, QStringLiteral("Monday"), FirstStudentRow + 1, 3, QStringLiteral("Jay KR")));
    QVERIFY(hasCellValue(values, QStringLiteral("Monday"), WifiRow, 2, QStringLiteral("DYB")));
}

void RosterTemplatePrintServiceTests::buildByDayCellValuesWritesTwentyFiveStudentRows()
{
    RosterTemplatePrintService::RosterClassData data =
        sampleRosterClass(
            1,
            QStringLiteral("Monday"),
            QStringLiteral("4:00 PM")
            );
    data.roster.rows.clear();

    for (int index = 1; index <= 26; ++index)
    {
        data.roster.rows.append(
            {
                QStringLiteral("Student %1").arg(index),
                QStringLiteral("Korean %1").arg(index)
            }
            );
    }

    QString error;
    const QList<RosterTemplatePrintService::RosterCellValue> values =
        RosterTemplatePrintService::buildByDayCellValues(
            {data},
            &error
            );

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(hasCellValue(values, QStringLiteral("Monday"), FirstStudentRow, 2, QStringLiteral("Student 1")));
    QVERIFY(hasCellValue(values, QStringLiteral("Monday"), LastStudentRow, 2, QStringLiteral("Student 25")));
    QVERIFY(!hasCellValue(values, QStringLiteral("Monday"), LastStudentRow + 1, 2, QStringLiteral("Student 26")));
}

void RosterTemplatePrintServiceTests::buildByDayCellValuesUsesTeacherFallback()
{
    QString error;
    const QList<RosterTemplatePrintService::RosterCellValue> values =
        RosterTemplatePrintService::buildByDayCellValues(
            {
                sampleRosterClass(
                    1,
                    QStringLiteral("Tuesday"),
                    QStringLiteral("5:00 PM"),
                    QStringLiteral("M2"),
                    QStringLiteral("Tigris"),
                    QString(),
                    QStringLiteral("Emma KR"),
                    QStringLiteral("601")
                    )
            },
            &error
            );

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(hasCellValue(values, QStringLiteral("Tuesday"), LevelRow, 4, QStringLiteral("M2 Tigris")));
    QVERIFY(hasCellValue(values, QStringLiteral("Tuesday"), TeacherRoomRow, 4, QStringLiteral("Emma KR (601)")));
}

void RosterTemplatePrintServiceTests::buildByDayCellValuesRejectsDuplicateSlots()
{
    QString error;
    const QList<RosterTemplatePrintService::RosterCellValue> values =
        RosterTemplatePrintService::buildByDayCellValues(
            {
                sampleRosterClass(1, QStringLiteral("Monday"), QStringLiteral("4:00 PM")),
                sampleRosterClass(2, QStringLiteral("Monday"), QStringLiteral("4:30 PM"))
            },
            &error
            );

    QVERIFY(values.isEmpty());
    QVERIFY(error.contains(QStringLiteral("Multiple selected classes")));
}

void RosterTemplatePrintServiceTests::generatedPdfUsesA4LandscapeAndFilledWeekdayPages()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString path =
        savePdf(
            temporaryDirectory,
            {
                sampleRosterClass(1, QStringLiteral("Monday"), QStringLiteral("4:00 PM")),
                sampleRosterClass(2, QStringLiteral("Wednesday"), QStringLiteral("5:00 PM"))
            },
            QStringLiteral("rosters.pdf")
            );
    QVERIFY(!path.isEmpty());

    QPdfDocument document;
    loadDocument(
        document,
        path,
        2
        );

    const QSizeF a4Points =
        QPageSize(QPageSize::A4).size(
            QPageSize::Point
            );
    const QSizeF pageSize =
        document.pagePointSize(0);
    const QSizeF expectedSize(
        a4Points.height(),
        a4Points.width()
        );

    QVERIFY(std::abs(pageSize.width() - expectedSize.width()) < 1.0);
    QVERIFY(std::abs(pageSize.height() - expectedSize.height()) < 1.0);
}

void RosterTemplatePrintServiceTests::
    generatedPdfStaysInsideHalfInchMarginsAndKeepsEmptyCells()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString path =
        savePdf(
            temporaryDirectory,
            {
                sampleRosterClass(1, QStringLiteral("Monday"), QStringLiteral("4:00 PM"))
            },
            QStringLiteral("roster.pdf")
            );
    QVERIFY(!path.isEmpty());

    QPdfDocument document;
    loadDocument(
        document,
        path,
        1
        );

    const QImage image =
        renderPage(document);
    QVERIFY(!image.isNull());

    const QRect bounds =
        nonWhiteBounds(image);
    QVERIFY(!bounds.isNull());

    const int marginPixels =
        qRound(MarginInches * RenderDpi);
    constexpr int TolerancePixels = 6;

    QVERIFY(std::abs(bounds.left() - marginPixels) <= TolerancePixels);
    QVERIFY(std::abs(bounds.top() - marginPixels) <= TolerancePixels);
    QVERIFY(
        std::abs(bounds.right() - (image.width() - marginPixels))
            <= TolerancePixels
        );
    QVERIFY(
        std::abs(bounds.bottom() - (image.height() - marginPixels))
            <= TolerancePixels
        );

    QVERIFY(
        isWhitePixel(
            image.pixelColor(
                emptyStudentCellCenter(image)
                )
            )
        );
    QVERIFY(
        hasNonWhitePixelNear(
            image,
            emptyStudentCellBorderPoint(image),
            3
            )
        );

    QVERIFY(
        colorNear(
            image.pixelColor(
                cellInteriorPoint(image, 2, 4, 2, 0.2)
                ),
            QColor(QStringLiteral("#95B3D7"))
            )
        );
    QVERIFY(
        colorNear(
            image.pixelColor(
                cellInteriorPoint(image, LevelRow, 4, 2)
                ),
            QColor(QStringLiteral("#B8CCE4"))
            )
        );
    QVERIFY(
        colorNear(
            image.pixelColor(
                cellInteriorPoint(image, WifiRow, 4, 2)
                ),
            QColor(QStringLiteral("#DCE6F1"))
            )
        );
}

QTEST_MAIN(RosterTemplatePrintServiceTests)

#include "roster_template_print_service_tests.moc"
