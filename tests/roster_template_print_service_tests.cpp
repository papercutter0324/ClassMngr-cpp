#include "features/roster/services/roster_template_print_service.h"

#include "app/services/feature_services.h"
#include "core/application_services.h"
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
bool g_hasFeatureServices = false;
QList<Classroom> g_classes;
QHash<int, Classroom> g_classesById;
QHash<int, ClassInfo> g_classInfo;
QHash<int, Roster> g_rosters;

alignas(ApplicationServices) unsigned char
    g_fakeApplicationServicesStorage[sizeof(ApplicationServices)];
alignas(ClassService) unsigned char
    g_fakeClassServiceStorage[sizeof(ClassService)];
alignas(RosterService) unsigned char
    g_fakeRosterServiceStorage[sizeof(RosterService)];

ApplicationServices* fakeApplicationServices()
{
    return reinterpret_cast<ApplicationServices*>(
        g_fakeApplicationServicesStorage
        );
}

ClassService* fakeClassService()
{
    return reinterpret_cast<ClassService*>(
        g_fakeClassServiceStorage
        );
}

RosterService* fakeRosterService()
{
    return reinterpret_cast<RosterService*>(
        g_fakeRosterServiceStorage
        );
}

void resetServiceStubs()
{
    g_hasOpenDatabase = false;
    g_hasFeatureServices = false;
    g_classes.clear();
    g_classesById.clear();
    g_classInfo.clear();
    g_rosters.clear();
}
}

bool ApplicationServices::hasOpenDatabase() const
{
    return g_hasOpenDatabase;
}

ClassService* ApplicationServices::classService() const
{
    return g_hasFeatureServices
        ? fakeClassService()
        : nullptr;
}

RosterService* ApplicationServices::rosterService() const
{
    return g_hasFeatureServices
        ? fakeRosterService()
        : nullptr;
}

QList<Classroom> ClassService::classes() const
{
    return g_classes;
}

Classroom ClassService::classroom(
    int classId
    ) const
{
    for (const Classroom& classroom : std::as_const(g_classes))
    {
        if (classroom.id == classId)
        {
            return classroom;
        }
    }

    if (g_classesById.contains(classId))
    {
        return g_classesById.value(classId);
    }

    Classroom classroom;
    classroom.id = classId;
    return classroom;
}

ClassInfo ClassService::classInfo(
    int classId
    ) const
{
    if (g_classInfo.contains(classId))
    {
        return g_classInfo.value(classId);
    }

    ClassInfo info;
    info.classId = classId;
    return info;
}

Roster RosterService::roster(
    int classId
    ) const
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
constexpr int DailyFirstSectionRow = 3;
constexpr int DailyRowsPerSection = 7;
constexpr int DailyHeaderColumn = 1;
constexpr int DailyFirstStudentColumn = 2;
constexpr int DailyStudentColumnCount = 5;
constexpr int DailyMaxStudentsPerClass = 25;
constexpr int DailySectionsPerPage = 6;
constexpr int PerClassHeaderRow = 5;
constexpr int PerClassFirstStudentRow = 6;
constexpr int PerClassIndexColumn = 1;
constexpr int PerClassEnglishColumn = 2;
constexpr int PerClassKoreanColumn = 3;
constexpr int PerClassFirstExtraColumn = 4;
constexpr int PerClassStudentRowCount = 23;
constexpr qreal PerClassInfoRowHeightInches = 0.2700;
constexpr qreal PerClassInfoTableGapInches = 0.2500;
constexpr qreal PerClassHeaderRowHeightInches = 0.2800;
constexpr qreal PerClassStudentRowHeightInches = 0.2300;

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

QString dailyPageKey(
    const QString& day,
    int pageIndex
    )
{
    return pageIndex <= 0
        ? day
        : QStringLiteral("%1|%2")
            .arg(day)
            .arg(pageIndex);
}

QString perClassPageKey(
    int pageIndex,
    int classId
    )
{
    return QStringLiteral("class|%1|%2")
        .arg(pageIndex)
        .arg(classId);
}

QString savePdf(
    QTemporaryDir& temporaryDirectory,
    const QList<RosterTemplatePrintService::RosterClassData>& classes,
    const QString& fileName,
    RosterTemplatePrintService::TemplateId templateId =
        RosterTemplatePrintService::TemplateId::ByDay,
    const QStringList& selectedExtraColumns = {},
    QPageLayout::Orientation perClassExtraInfoOrientation =
        QPageLayout::Portrait
    )
{
    const QString path =
        temporaryDirectory.filePath(fileName);
    const RosterTemplatePrintService::Result result =
        RosterTemplatePrintService::saveRostersPdf(
            classes,
            path,
            templateId,
            selectedExtraColumns,
            perClassExtraInfoOrientation
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

bool isPerClassHeaderPixel(
    const QColor& color
    )
{
    return std::abs(color.red() - 184) <= 4
        && std::abs(color.green() - 204) <= 4
        && std::abs(color.blue() - 228) <= 4;
}

int perClassHeaderCellCount(
    const QImage& image,
    int y
    )
{
    int count = 0;
    bool inCell = false;

    for (int x = 0; x < image.width(); ++x)
    {
        const bool isHeader =
            isPerClassHeaderPixel(image.pixelColor(x, y));

        if (isHeader && !inCell)
        {
            ++count;
        }

        inCell = isHeader;
    }

    return count;
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
    void templateMetadataIncludesDailyTemplate();
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
    void buildDailyCellValuesMapsClassSectionsAndFallbackNames();
    void buildDailyCellValuesWritesTwentyFiveStudentRows();
    void buildDailyCellValuesOrdersByStartTimeAndAllowsDuplicateSlots();
    void availablePerClassExtraInfoColumnsFiltersRequiredAndEvaluationColumns();
    void buildPerClassExtraInfoCellValuesMapsSelectedColumnsAndMissingCells();
    void buildPerClassExtraInfoCellValuesCapsColumnsByOrientation();
    void requestSaveRostersPdfUsesSelectedTemplate();
    void currentClassPrintSupportsClassExcludedFromRegularList();
    void dailyPdfUsesA4PortraitAndContinuesOverflowPages();
    void perClassWithExtraInfoPdfHonorsPortraitAndLandscape();
};

void RosterTemplatePrintServiceTests::templateMetadataIncludesDailyTemplate()
{
    const QList<RosterTemplatePrintService::TemplateId> templates =
        RosterTemplatePrintService::availableTemplateIds();

    QVERIFY(
        templates.contains(
            RosterTemplatePrintService::TemplateId::Daily
            )
        );
    QVERIFY(
        templates.contains(
            RosterTemplatePrintService::TemplateId::PerClassWithExtraInfo
            )
        );
    QCOMPARE(
        RosterTemplatePrintService::templateDisplayName(
            RosterTemplatePrintService::TemplateId::Daily
            ),
        QStringLiteral("Daily")
        );
    QCOMPARE(
        RosterTemplatePrintService::templateOrientation(
            RosterTemplatePrintService::TemplateId::Daily
            ),
        QPageLayout::Portrait
        );
    QCOMPARE(
        RosterTemplatePrintService::templateDisplayName(
            RosterTemplatePrintService::TemplateId::PerClassWithExtraInfo
            ),
        QStringLiteral("Per Class with Extra Info")
        );
    QCOMPARE(
        RosterTemplatePrintService::templateOrientation(
            RosterTemplatePrintService::TemplateId::PerClassWithExtraInfo
            ),
        QPageLayout::Portrait
        );
}

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
    QVERIFY(result.message.contains(QStringLiteral("No Teacher Profile")));
}

void RosterTemplatePrintServiceTests::requestSaveRostersPdfUsesSelectedClassScope()
{
    resetServiceStubs();
    g_hasOpenDatabase = true;
    g_hasFeatureServices = true;

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

void RosterTemplatePrintServiceTests::
    buildDailyCellValuesMapsClassSectionsAndFallbackNames()
{
    RosterTemplatePrintService::RosterClassData data =
        sampleRosterClass(
            1,
            QStringLiteral("Friday"),
            QStringLiteral("2:00 PM"),
            QStringLiteral("M2"),
            QStringLiteral("Tigris"),
            QStringLiteral("Daniel"),
            QStringLiteral("Teacher KR"),
            QStringLiteral("407")
            );
    data.roster.rows = {
        {
            QStringLiteral("Yoongoo"),
            QStringLiteral("Yoongoo KR")
        },
        {
            QString(),
            QStringLiteral("Minseo KR")
        }
    };

    QString error;
    const QList<RosterTemplatePrintService::RosterCellValue> values =
        RosterTemplatePrintService::buildDailyCellValues(
            {data},
            &error
            );

    QVERIFY2(error.isEmpty(), qPrintable(error));

    const QString expectedHeader =
        QStringLiteral("M2 Tigris (2 p.m. / Daniel / Room 407 / Zoom: zoom");
    bool headerFound = false;
    for (const auto& value : values)
    {
        if (
            value.day == QStringLiteral("Friday")
            && value.row == DailyFirstSectionRow
            && value.column == DailyHeaderColumn
            && value.value.contains(expectedHeader)
            && value.value.contains(QStringLiteral("PW zoom-pw"))
            )
        {
            headerFound = true;
            break;
        }
    }

    QVERIFY(headerFound);
    QVERIFY(hasCellValue(values, QStringLiteral("Friday"), DailyFirstSectionRow + 1, DailyFirstStudentColumn, QStringLiteral("Yoongoo")));
    QVERIFY(hasCellValue(values, QStringLiteral("Friday"), DailyFirstSectionRow + 1, DailyFirstStudentColumn + 1, QStringLiteral("Minseo KR")));
}

void RosterTemplatePrintServiceTests::
    buildDailyCellValuesWritesTwentyFiveStudentRows()
{
    RosterTemplatePrintService::RosterClassData data =
        sampleRosterClass(
            1,
            QStringLiteral("Monday"),
            QStringLiteral("4:00 PM")
            );
    data.roster.rows.clear();

    for (int index = 1; index <= DailyMaxStudentsPerClass + 1; ++index)
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
        RosterTemplatePrintService::buildDailyCellValues(
            {data},
            &error
            );

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(hasCellValue(values, QStringLiteral("Monday"), DailyFirstSectionRow + 1, DailyFirstStudentColumn, QStringLiteral("Student 1")));
    QVERIFY(hasCellValue(values, QStringLiteral("Monday"), DailyFirstSectionRow + 5, DailyFirstStudentColumn + 4, QStringLiteral("Student 25")));
    QVERIFY(!hasCellValue(values, QStringLiteral("Monday"), DailyFirstSectionRow + 6, DailyFirstStudentColumn, QStringLiteral("Student 26")));
}

void RosterTemplatePrintServiceTests::
    buildDailyCellValuesOrdersByStartTimeAndAllowsDuplicateSlots()
{
    RosterTemplatePrintService::RosterClassData later =
        sampleRosterClass(
            1,
            QStringLiteral("Tuesday"),
            QStringLiteral("6:00 PM"),
            QStringLiteral("E6"),
            QStringLiteral("Poseidon")
            );
    RosterTemplatePrintService::RosterClassData earlier =
        sampleRosterClass(
            2,
            QStringLiteral("Tuesday"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("M2"),
            QStringLiteral("Tigris")
            );
    RosterTemplatePrintService::RosterClassData sameSlot =
        sampleRosterClass(
            3,
            QStringLiteral("Tuesday"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("E5"),
            QStringLiteral("Athena")
            );

    QString error;
    const QList<RosterTemplatePrintService::RosterCellValue> values =
        RosterTemplatePrintService::buildDailyCellValues(
            {
                later,
                earlier,
                sameSlot
            },
            &error
            );

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(hasCellValue(values, QStringLiteral("Tuesday"), DailyFirstSectionRow, DailyHeaderColumn, QStringLiteral("E5 Athena (4 p.m. / Emma / Room 506 / Zoom: zoom ") + QString(QChar(0x2022)) + QStringLiteral(" PW zoom-pw)")));
    QVERIFY(hasCellValue(values, QStringLiteral("Tuesday"), DailyFirstSectionRow + DailyRowsPerSection, DailyHeaderColumn, QStringLiteral("M2 Tigris (4 p.m. / Emma / Room 506 / Zoom: zoom ") + QString(QChar(0x2022)) + QStringLiteral(" PW zoom-pw)")));
    QVERIFY(hasCellValue(values, QStringLiteral("Tuesday"), DailyFirstSectionRow + (DailyRowsPerSection * 2), DailyHeaderColumn, QStringLiteral("E6 Poseidon (6 p.m. / Emma / Room 506 / Zoom: zoom ") + QString(QChar(0x2022)) + QStringLiteral(" PW zoom-pw)")));
}

void RosterTemplatePrintServiceTests::
    availablePerClassExtraInfoColumnsFiltersRequiredAndEvaluationColumns()
{
    RosterTemplatePrintService::RosterClassData first =
        sampleRosterClass(
            1,
            QStringLiteral("Monday"),
            QStringLiteral("4:00 PM")
            );
    first.roster.columns = {
        QStringLiteral("English"),
        QStringLiteral("Korean"),
        QStringLiteral("Birthday"),
        QStringLiteral("Fall"),
        QStringLiteral("Phone")
    };

    RosterTemplatePrintService::RosterClassData second =
        sampleRosterClass(
            2,
            QStringLiteral("Tuesday"),
            QStringLiteral("5:00 PM")
            );
    second.roster.columns = {
        QStringLiteral("English"),
        QStringLiteral("Korean"),
        QStringLiteral("Phone"),
        QStringLiteral("School"),
        QStringLiteral("Speech Contest")
    };

    QCOMPARE(
        RosterTemplatePrintService::availablePerClassExtraInfoColumns(
            {
                first,
                second
            }
            ),
        QStringList({
            QStringLiteral("Birthday"),
            QStringLiteral("Phone"),
            QStringLiteral("School")
        })
        );
}

void RosterTemplatePrintServiceTests::
    buildPerClassExtraInfoCellValuesMapsSelectedColumnsAndMissingCells()
{
    RosterTemplatePrintService::RosterClassData data =
        sampleRosterClass(
            7,
            QStringLiteral("Friday"),
            QStringLiteral("8:00 PM"),
            QStringLiteral("M3"),
            QStringLiteral("Major"),
            QStringLiteral("Hyewon"),
            QString(),
            QStringLiteral("414")
            );
    data.roster.columns = {
        QStringLiteral("English"),
        QStringLiteral("Korean"),
        QStringLiteral("Birthday")
    };
    data.roster.rows = {
        {
            QStringLiteral("Kaelyn"),
            QStringLiteral("Kaelyn KR"),
            QStringLiteral("10/15")
        }
    };

    QString error;
    const QList<RosterTemplatePrintService::RosterCellValue> values =
        RosterTemplatePrintService::buildPerClassExtraInfoCellValues(
            {data},
            {
                QStringLiteral("Birthday"),
                QStringLiteral("School"),
                QStringLiteral("Fall")
            },
            QPageLayout::Portrait,
            &error
            );
    const QString page =
        perClassPageKey(
            0,
            data.classroom.id
            );

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(hasCellValue(values, page, 1, 1, QStringLiteral("Level")));
    QVERIFY(hasCellValue(values, page, 1, 2, QStringLiteral("M3 Major")));
    QVERIFY(hasCellValue(values, page, 1, 3, QStringLiteral("Room")));
    QVERIFY(hasCellValue(values, page, 1, 4, QStringLiteral("414")));
    QVERIFY(hasCellValue(values, page, 2, 1, QStringLiteral("Days/Times")));
    QVERIFY(hasCellValue(values, page, 3, 1, QStringLiteral("Teacher")));
    QVERIFY(hasCellValue(values, page, 4, 1, QStringLiteral("ZOOM")));
    QVERIFY(hasCellValue(values, page, PerClassHeaderRow, PerClassIndexColumn, QStringLiteral("No.")));
    QVERIFY(hasCellValue(values, page, PerClassHeaderRow, PerClassEnglishColumn, QStringLiteral("English Name")));
    QVERIFY(hasCellValue(values, page, PerClassHeaderRow, PerClassKoreanColumn, QStringLiteral("Korean Name")));
    QVERIFY(hasCellValue(values, page, PerClassHeaderRow, PerClassFirstExtraColumn, QStringLiteral("Birthday")));
    QVERIFY(hasCellValue(values, page, PerClassHeaderRow, PerClassFirstExtraColumn + 1, QStringLiteral("School")));
    QVERIFY(!hasCellValue(values, page, PerClassHeaderRow, PerClassFirstExtraColumn + 2, QStringLiteral("Fall")));
    QVERIFY(hasCellValue(values, page, PerClassFirstStudentRow, PerClassEnglishColumn, QStringLiteral("Kaelyn")));
    QVERIFY(hasCellValue(values, page, PerClassFirstStudentRow, PerClassIndexColumn, QStringLiteral("1")));
    QVERIFY(hasCellValue(values, page, PerClassFirstStudentRow + 22, PerClassIndexColumn, QStringLiteral("23")));
    QVERIFY(hasCellValue(values, page, PerClassFirstStudentRow, PerClassKoreanColumn, QStringLiteral("Kaelyn KR")));
    QVERIFY(hasCellValue(values, page, PerClassFirstStudentRow, PerClassFirstExtraColumn, QStringLiteral("10/15")));
    QVERIFY(!hasCellValue(values, page, PerClassFirstStudentRow, PerClassFirstExtraColumn + 1, QStringLiteral("10/15")));
}

void RosterTemplatePrintServiceTests::
    buildPerClassExtraInfoCellValuesCapsColumnsByOrientation()
{
    RosterTemplatePrintService::RosterClassData data =
        sampleRosterClass(
            8,
            QStringLiteral("Monday"),
            QStringLiteral("4:00 PM")
            );
    data.roster.columns = {
        QStringLiteral("English"),
        QStringLiteral("Korean"),
        QStringLiteral("A"),
        QStringLiteral("B"),
        QStringLiteral("C"),
        QStringLiteral("D"),
        QStringLiteral("E"),
        QStringLiteral("F"),
        QStringLiteral("G"),
        QStringLiteral("H"),
        QStringLiteral("I")
    };
    const QStringList requested{
        QStringLiteral("A"),
        QStringLiteral("B"),
        QStringLiteral("C"),
        QStringLiteral("D"),
        QStringLiteral("E"),
        QStringLiteral("F"),
        QStringLiteral("G"),
        QStringLiteral("H"),
        QStringLiteral("I")
    };
    const QString page =
        perClassPageKey(
            0,
            data.classroom.id
            );

    const QList<RosterTemplatePrintService::RosterCellValue> portraitValues =
        RosterTemplatePrintService::buildPerClassExtraInfoCellValues(
            {data},
            requested,
            QPageLayout::Portrait
            );
    QVERIFY(hasCellValue(portraitValues, page, PerClassHeaderRow, PerClassFirstExtraColumn + 3, QStringLiteral("D")));
    QVERIFY(!hasCellValue(portraitValues, page, PerClassHeaderRow, PerClassFirstExtraColumn + 4, QStringLiteral("E")));

    const QList<RosterTemplatePrintService::RosterCellValue> landscapeValues =
        RosterTemplatePrintService::buildPerClassExtraInfoCellValues(
            {data},
            requested,
            QPageLayout::Landscape
            );
    QVERIFY(hasCellValue(landscapeValues, page, PerClassHeaderRow, PerClassFirstExtraColumn + 7, QStringLiteral("H")));
    QVERIFY(!hasCellValue(landscapeValues, page, PerClassHeaderRow, PerClassFirstExtraColumn + 8, QStringLiteral("I")));
    QCOMPARE(
        RosterTemplatePrintService::perClassExtraInfoMaxExtraColumns(QPageLayout::Portrait),
        4
        );
    QCOMPARE(
        RosterTemplatePrintService::perClassExtraInfoMaxExtraColumns(QPageLayout::Landscape),
        8
        );
}

void RosterTemplatePrintServiceTests::requestSaveRostersPdfUsesSelectedTemplate()
{
    resetServiceStubs();
    g_hasOpenDatabase = true;
    g_hasFeatureServices = true;

    const RosterTemplatePrintService::RosterClassData selectedClass =
        sampleRosterClass(
            20,
            QStringLiteral("Monday"),
            QStringLiteral("4:00 PM")
            );

    g_classes = {
        selectedClass.classroom
    };
    g_classInfo.insert(
        selectedClass.classroom.id,
        selectedClass.info
        );
    g_rosters.insert(
        selectedClass.classroom.id,
        selectedClass.roster
        );

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString path =
        temporaryDirectory.filePath(QStringLiteral("daily.pdf"));

    RosterTemplatePrintService::Request request;
    request.services =
        fakeApplicationServices();
    request.currentClassId =
        selectedClass.classroom.id;
    request.scope =
        RosterTemplatePrintService::Scope::CurrentClass;
    request.templateId =
        RosterTemplatePrintService::TemplateId::Daily;

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

    const QSizeF a4Points =
        QPageSize(QPageSize::A4).size(
            QPageSize::Point
            );
    const QSizeF pageSize =
        document.pagePointSize(0);

    QVERIFY(std::abs(pageSize.width() - a4Points.width()) < 1.0);
    QVERIFY(std::abs(pageSize.height() - a4Points.height()) < 1.0);
}

void RosterTemplatePrintServiceTests
    ::currentClassPrintSupportsClassExcludedFromRegularList()
{
    resetServiceStubs();
    g_hasOpenDatabase = true;
    g_hasFeatureServices = true;

    Classroom testingClass;
    testingClass.id = 50;
    testingClass.name = QStringLiteral("Writing Lab");
    g_classesById.insert(testingClass.id, testingClass);

    ClassInfo info;
    info.classId = testingClass.id;
    info.classGrade = QStringLiteral("M2");
    info.classLevel = QStringLiteral("Mixed (All)");
    g_classInfo.insert(testingClass.id, info);

    Roster roster;
    roster.columns = {
        QStringLiteral("English"),
        QStringLiteral("Korean")
    };
    roster.rows = {
        {
            QStringLiteral("Unique Student"),
            QStringLiteral("고유 학생")
        }
    };
    g_rosters.insert(testingClass.id, roster);

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    RosterTemplatePrintService::Request request;
    request.services = fakeApplicationServices();
    request.currentClassId = testingClass.id;
    request.scope =
        RosterTemplatePrintService::Scope::CurrentClass;
    request.templateId =
        RosterTemplatePrintService::TemplateId::PerClassWithExtraInfo;

    const QString currentPath =
        temporaryDirectory.filePath(
            QStringLiteral("testing-class-roster.pdf")
            );
    const RosterTemplatePrintService::Result currentResult =
        RosterTemplatePrintService::saveRostersPdf(
            request,
            currentPath
            );
    QCOMPARE(
        currentResult.status,
        RosterTemplatePrintService::Status::Sent
        );

    QPdfDocument document;
    loadDocument(document, currentPath, 1);

    request.scope =
        RosterTemplatePrintService::Scope::AllClasses;
    const RosterTemplatePrintService::Result bulkResult =
        RosterTemplatePrintService::saveRostersPdf(
            request,
            temporaryDirectory.filePath(
                QStringLiteral("bulk-rosters.pdf")
                )
            );
    QCOMPARE(
        bulkResult.status,
        RosterTemplatePrintService::Status::Failed
        );
}

void RosterTemplatePrintServiceTests::
    dailyPdfUsesA4PortraitAndContinuesOverflowPages()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    QList<RosterTemplatePrintService::RosterClassData> classes;
    for (int index = 1; index <= DailySectionsPerPage + 1; ++index)
    {
        classes.append(
            sampleRosterClass(
                index,
                QStringLiteral("Monday"),
                QStringLiteral("4:00 PM"),
                QStringLiteral("E%1").arg(index),
                QStringLiteral("Level")
                )
            );
    }

    const QString path =
        savePdf(
            temporaryDirectory,
            classes,
            QStringLiteral("daily.pdf"),
            RosterTemplatePrintService::TemplateId::Daily
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

    QVERIFY(std::abs(pageSize.width() - a4Points.width()) < 1.0);
    QVERIFY(std::abs(pageSize.height() - a4Points.height()) < 1.0);

    const QList<RosterTemplatePrintService::RosterCellValue> values =
        RosterTemplatePrintService::buildDailyCellValues(
            classes
            );
    QVERIFY(hasCellValue(values, dailyPageKey(QStringLiteral("Monday"), 1), DailyFirstSectionRow, DailyHeaderColumn, QStringLiteral("E7 Level (4 p.m. / Emma / Room 506 / Zoom: zoom ") + QString(QChar(0x2022)) + QStringLiteral(" PW zoom-pw)")));
}

void RosterTemplatePrintServiceTests::
    perClassWithExtraInfoPdfHonorsPortraitAndLandscape()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    RosterTemplatePrintService::RosterClassData data =
        sampleRosterClass(
            11,
            QStringLiteral("Friday"),
            QStringLiteral("8:00 PM")
            );
    data.roster.columns = {
        QStringLiteral("English"),
        QStringLiteral("Korean"),
        QStringLiteral("Birthday"),
        QStringLiteral("Phone")
    };
    data.roster.rows = {
        {
            QStringLiteral("Kaelyn"),
            QStringLiteral("Kaelyn KR"),
            QStringLiteral("10/15"),
            QStringLiteral("010")
        }
    };

    const QString portraitPath =
        savePdf(
            temporaryDirectory,
            {data},
            QStringLiteral("per-class-portrait.pdf"),
            RosterTemplatePrintService::TemplateId::PerClassWithExtraInfo,
            {
                QStringLiteral("Birthday"),
                QStringLiteral("Phone")
            },
            QPageLayout::Portrait
            );
    QVERIFY(!portraitPath.isEmpty());

    QPdfDocument portraitDocument;
    loadDocument(
        portraitDocument,
        portraitPath,
        1
        );

    const QSizeF a4Points =
        QPageSize(QPageSize::A4).size(
            QPageSize::Point
            );
    const QSizeF portraitSize =
        portraitDocument.pagePointSize(0);

    QVERIFY(std::abs(portraitSize.width() - a4Points.width()) < 1.0);
    QVERIFY(std::abs(portraitSize.height() - a4Points.height()) < 1.0);

    // The saved PDF must include the selected Birthday and Phone columns,
    // not only the on-screen preview.
    const QImage portraitImage = renderPage(portraitDocument);
    const int headerTopY =
        qRound((0.5 + (4.0 * 0.27) + 0.25 + 0.02) * RenderDpi);
    QCOMPARE(
        perClassHeaderCellCount(portraitImage, headerTopY),
        5
        );

    const int portraitNotesTopY =
        qRound(
            (MarginInches
             + (4.0 * PerClassInfoRowHeightInches)
             + PerClassInfoTableGapInches
             + PerClassHeaderRowHeightInches
             + (PerClassStudentRowCount * PerClassStudentRowHeightInches)
             + PerClassInfoTableGapInches)
            * RenderDpi
            );
    QVERIFY(
        !isWhitePixel(
            portraitImage.pixelColor(
                qRound(4.0 * RenderDpi),
                portraitNotesTopY
                )
            )
        );

    const QString landscapePath =
        savePdf(
            temporaryDirectory,
            {data},
            QStringLiteral("per-class-landscape.pdf"),
            RosterTemplatePrintService::TemplateId::PerClassWithExtraInfo,
            {
                QStringLiteral("Birthday"),
                QStringLiteral("Phone")
            },
            QPageLayout::Landscape
            );
    QVERIFY(!landscapePath.isEmpty());

    QPdfDocument landscapeDocument;
    loadDocument(
        landscapeDocument,
        landscapePath,
        1
        );

    const QSizeF landscapeSize =
        landscapeDocument.pagePointSize(0);
    const QSizeF expectedLandscape(
        a4Points.height(),
        a4Points.width()
        );

    QVERIFY(std::abs(landscapeSize.width() - expectedLandscape.width()) < 1.0);
    QVERIFY(std::abs(landscapeSize.height() - expectedLandscape.height()) < 1.0);

    const QImage landscapeImage = renderPage(landscapeDocument);
    QVERIFY(
        !isWhitePixel(
            landscapeImage.pixelColor(
                qRound(4.0 * RenderDpi),
                qRound(MarginInches * RenderDpi)
                )
            )
        );
}

QTEST_MAIN(RosterTemplatePrintServiceTests)

#include "roster_template_print_service_tests.moc"
