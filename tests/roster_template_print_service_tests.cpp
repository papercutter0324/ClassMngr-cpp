#include "features/roster/ui/roster_template_print_service.h"

#include "core/application_services.h"
#include "data/data_service.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QDir>
#include <QTest>

bool ApplicationServices::hasOpenDatabase() const
{
    return false;
}

DataService* ApplicationServices::dataService() const
{
    return nullptr;
}

QList<Classroom> DataService::getClasses()
{
    return {};
}

Classroom DataService::getClassById(
    int classId
    )
{
    Classroom classroom;
    classroom.id = classId;
    return classroom;
}

ClassInfo DataService::loadClassInfo(
    int classId
    )
{
    ClassInfo info;
    info.classId = classId;
    return info;
}

Roster DataService::loadRoster(
    int classId
    )
{
    Q_UNUSED(classId);
    return {};
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
RosterTemplatePrintService::RosterClassData sampleRosterClass(
    int classId,
    const QString& day,
    const QString& startTime
    )
{
    RosterTemplatePrintService::RosterClassData data;
    data.classroom.id = classId;
    data.classroom.name =
        QStringLiteral("Class %1").arg(classId);
    data.info.classId = classId;
    data.info.classGrade = QStringLiteral("E4");
    data.info.classLevel = QStringLiteral("Theseus");
    data.info.teacherEn = QStringLiteral("Jenny");
    data.info.roomNumber = QStringLiteral("301");
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

bool hasOperation(
    const QList<RosterTemplatePrintService::FillOperation>& operations,
    const QString& sheet,
    const QString& cell,
    const QString& value
    )
{
    for (const auto& operation : operations)
    {
        if (
            operation.sheet == sheet
            && operation.cell == cell
            && operation.value == value
            )
        {
            return true;
        }
    }

    return false;
}
}

class RosterTemplatePrintServiceTests : public QObject
{
    Q_OBJECT

private slots:
    void preferredTemplateSuffixesFollowPlatform();
    void resolveClassIdsUsesRequestedScope();
    void buildByDayFillOperationsMapsClassToTimeBlock();
    void buildByDayFillOperationsRejectsDuplicateSlots();
    void detectsBundledByDayTemplates();
};

void RosterTemplatePrintServiceTests::preferredTemplateSuffixesFollowPlatform()
{
    QCOMPARE(
        RosterTemplatePrintService::preferredTemplateSuffixes(Platform::LINUX),
        QStringList({QStringLiteral("ods"), QStringLiteral("xlsx")})
        );
    QCOMPARE(
        RosterTemplatePrintService::preferredTemplateSuffixes(Platform::WINDOWS),
        QStringList({QStringLiteral("xlsx"), QStringLiteral("ods")})
        );
    QCOMPARE(
        RosterTemplatePrintService::preferredTemplateSuffixes(Platform::MAC),
        QStringList({QStringLiteral("xlsx"), QStringLiteral("ods")})
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

void RosterTemplatePrintServiceTests::buildByDayFillOperationsMapsClassToTimeBlock()
{
    QString error;
    const QList<RosterTemplatePrintService::FillOperation> operations =
        RosterTemplatePrintService::buildByDayFillOperations(
            {sampleRosterClass(1, QStringLiteral("Monday"), QStringLiteral("4:00 PM"))},
            &error
            );

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(hasOperation(operations, QStringLiteral("Monday"), QStringLiteral("B3"), QStringLiteral("E4 Theseus")));
    QVERIFY(hasOperation(operations, QStringLiteral("Monday"), QStringLiteral("B4"), QStringLiteral("Jenny")));
    QVERIFY(hasOperation(operations, QStringLiteral("Monday"), QStringLiteral("B5"), QStringLiteral("301")));
    QVERIFY(hasOperation(operations, QStringLiteral("Monday"), QStringLiteral("B7"), QStringLiteral("Lily")));
    QVERIFY(hasOperation(operations, QStringLiteral("Monday"), QStringLiteral("C7"), QStringLiteral("Lily KR")));
    QVERIFY(hasOperation(operations, QStringLiteral("Monday"), QStringLiteral("B8"), QStringLiteral("Jay")));
    QVERIFY(hasOperation(operations, QStringLiteral("Monday"), QStringLiteral("C8"), QStringLiteral("Jay KR")));
    QVERIFY(hasOperation(operations, QStringLiteral("Monday"), QStringLiteral("B30"), QStringLiteral("DYB")));
}

void RosterTemplatePrintServiceTests::buildByDayFillOperationsRejectsDuplicateSlots()
{
    QString error;
    const QList<RosterTemplatePrintService::FillOperation> operations =
        RosterTemplatePrintService::buildByDayFillOperations(
            {
                sampleRosterClass(1, QStringLiteral("Monday"), QStringLiteral("4:00 PM")),
                sampleRosterClass(2, QStringLiteral("Monday"), QStringLiteral("4:30 PM"))
            },
            &error
            );

    QVERIFY(operations.isEmpty());
    QVERIFY(error.contains(QStringLiteral("Multiple selected classes")));
}

void RosterTemplatePrintServiceTests::detectsBundledByDayTemplates()
{
    const QDir sourceDir(QStringLiteral(CLASSMNGR_TEST_SOURCE_DIR));
    const QString rosterDir =
        sourceDir.filePath(QStringLiteral("resources/assets/files/rosters"));

    QString error;
    QVERIFY2(
        RosterTemplatePrintService::isSupportedByDayTemplate(
            QDir(rosterDir).filePath(QStringLiteral("Roster Template 1 - By Day.xlsx")),
            &error
            ),
        qPrintable(error)
        );

    error.clear();
    QVERIFY2(
        RosterTemplatePrintService::isSupportedByDayTemplate(
            QDir(rosterDir).filePath(QStringLiteral("Roster Template 1 - By Day.ods")),
            &error
            ),
        qPrintable(error)
        );
}

QTEST_MAIN(RosterTemplatePrintServiceTests)

#include "roster_template_print_service_tests.moc"
