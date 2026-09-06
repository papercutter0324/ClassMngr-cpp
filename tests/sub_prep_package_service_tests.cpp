#include "features/sub_prep/services/sub_prep_package_service.h"

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/theme_service.h"
#include "data/database/database_session.h"
#include "domain/models/teacher.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QTemporaryDir>

#include <utility>

namespace
{
QList<Classroom> g_classes;
QHash<int, ClassInfo> g_classInfo;
QHash<int, Roster> g_rosters;
QHash<int, Teacher> g_teachers;
PdfPrintService::Status g_batchPrintStatus =
    PdfPrintService::Status::Canceled;
QStringList g_batchPrintPaths;

ApplicationServices* fakeApplicationServices()
{
    static ApplicationServices services;
    return &services;
}

void resetData()
{
    g_classes.clear();
    g_classInfo.clear();
    g_rosters.clear();
    g_teachers.clear();
    g_batchPrintStatus = PdfPrintService::Status::Canceled;
    g_batchPrintPaths.clear();

    Classroom classroom;
    classroom.id = 42;
    classroom.name = QStringLiteral("Hercules");
    g_classes.append(classroom);

    ClassInfo info;
    info.classId = 42;
    info.teacherId = 7;
    info.classGrade = QStringLiteral("E4");
    info.classLevel = QStringLiteral("Hercules");
    info.classTimes.append(
        {
            QStringLiteral("Tuesday"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("4:50 PM")
        }
        );
    g_classInfo.insert(42, info);

    Roster roster;
    roster.columns = {
        QStringLiteral("English"),
        QStringLiteral("Korean"),
        QStringLiteral("Allergies")
    };
    roster.rows.append(
        {
            QStringLiteral("Alex"),
            QStringLiteral("알렉스"),
            QStringLiteral("None")
        }
        );
    g_rosters.insert(42, roster);

    Teacher teacher;
    teacher.id = 7;
    teacher.teacherEn = QStringLiteral("Susan");
    g_teachers.insert(7, teacher);
}

SubPrepPrintService::Request subPrepRequest()
{
    SubPrepPrintService::Request request;
    request.schedule.days = {QStringLiteral("Tuesday")};

    ScheduleRowView row;
    row.timeLabel = QStringLiteral("16:00");
    row.timeRangeLabel = QStringLiteral("4:00 - 4:50");

    ScheduleCellView cell;
    cell.day = QStringLiteral("Tuesday");
    cell.timeLabel = row.timeLabel;
    ScheduleEntry entry;
    entry.classId = 42;
    entry.teacherEn = QStringLiteral("Susan");
    entry.classGrade = QStringLiteral("E4");
    entry.classLevel = QStringLiteral("Hercules");
    cell.entries.append(entry);
    row.cells.append(cell);
    request.schedule.rows.append(row);
    request.subNotes = QStringLiteral("Please leave a handover note.");
    return request;
}

SubPrepPackageService::Request packageRequest(
    const QString& targetRoot,
    const QString& userName
    )
{
    SubPrepPackageService::Request request;
    request.services = fakeApplicationServices();
    request.subPrep = subPrepRequest();
    request.selectedDates = {QDate(2026, 7, 21)};
    request.classIds = {42};
    request.createFolder = true;
    request.targetRoot = targetRoot;
    request.userName = userName;
    request.openFolderAfterGeneration = false;
    return request;
}
}

ApplicationServices::ApplicationServices() = default;

ApplicationServices::~ApplicationServices() = default;

bool ApplicationServices::hasOpenDatabase() const
{
    return true;
}

FeatureService::FeatureService(DatabaseSession*)
{
}

bool FeatureService::isAvailable() const
{
    return true;
}

ClassService* ApplicationServices::classService() const
{
    static ClassService service(nullptr);
    return &service;
}

TeacherService* ApplicationServices::teacherService() const
{
    static TeacherService service(nullptr);
    return &service;
}

RosterService* ApplicationServices::rosterService() const
{
    static RosterService service(nullptr);
    return &service;
}

Result<Classroom> ClassService::classroom(
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
    return std::unexpected(QStringLiteral("Class not found."));
}

Result<ClassInfo> ClassService::classInfo(
    int classId
    ) const
{
    return g_classInfo.value(classId);
}

Result<Roster> RosterService::roster(
    int classId
    ) const
{
    return g_rosters.value(classId);
}

Result<Teacher> TeacherService::teacher(
    int teacherId
    ) const
{
    return g_teachers.value(teacherId);
}

namespace PdfPrintService
{
Result printPdfDocument(
    const Request&
    )
{
    return {Status::Canceled, {}};
}

Result printPdfDocuments(
    const BatchRequest& request
    )
{
    g_batchPrintPaths = request.documentPaths;
    return {
        g_batchPrintStatus,
        g_batchPrintStatus == Status::Failed
            ? QStringLiteral("Printer failed")
            : QString()
    };
}
}

class SubPrepPackageServiceTests : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void byDayPackageCreatesExpectedTreeAndReplacesTransactionally();
    void dailyPackageUsesDailyRosterTemplate();
    void perClassPackagePlacesRosterInsideClassFolder();
    void intensiveScheduleControlsSelectedDayFiltering();
    void printOnlyUsesPacketOrderAndReportsCancellation();
};

void SubPrepPackageServiceTests::init()
{
    resetData();
}

void SubPrepPackageServiceTests
    ::byDayPackageCreatesExpectedTreeAndReplacesTransactionally()
{
    QTemporaryDir targetRoot;
    QVERIFY(targetRoot.isValid());

    SubPrepPackageService::Request request =
        packageRequest(targetRoot.path(), QStringLiteral("Alex"));
    const SubPrepPackageService::Result first =
        SubPrepPackageService::generate(request);

    QCOMPARE(first.status, SubPrepPackageService::Status::Completed);
    QVERIFY(first.folderCreated);
    QCOMPARE(
        QFileInfo(first.outputDirectory).fileName(),
        QStringLiteral("Alex (21 Jul 2026)")
        );
    QVERIFY(QFileInfo::exists(
        QDir(first.outputDirectory).filePath(QStringLiteral("Sub Prep.pdf"))
        ));
    QVERIFY(QFileInfo::exists(
        QDir(first.outputDirectory).filePath(
            QStringLiteral("Rosters - By Day.pdf")
            )
        ));

    const QStringList classFolders =
        QDir(first.outputDirectory).entryList(
            QDir::Dirs | QDir::NoDotAndDotDot
            );
    QCOMPARE(classFolders.size(), 1);

    QFile sentinel(
        QDir(first.outputDirectory).filePath(QStringLiteral("keep-me.txt"))
        );
    QVERIFY(sentinel.open(QIODevice::WriteOnly));
    sentinel.write("old");
    sentinel.close();

    const SubPrepPackageService::Result declined =
        SubPrepPackageService::generate(request);
    QCOMPARE(declined.status, SubPrepPackageService::Status::Failed);
    QVERIFY(QFileInfo::exists(sentinel.fileName()));

    request.replaceExisting = true;
    const SubPrepPackageService::Result replaced =
        SubPrepPackageService::generate(request);
    QCOMPARE(replaced.status, SubPrepPackageService::Status::Completed);
    QVERIFY(!QFileInfo::exists(sentinel.fileName()));
    QVERIFY(QFileInfo::exists(
        QDir(replaced.outputDirectory).filePath(QStringLiteral("Sub Prep.pdf"))
        ));
}

void SubPrepPackageServiceTests::dailyPackageUsesDailyRosterTemplate()
{
    QTemporaryDir targetRoot;
    QVERIFY(targetRoot.isValid());

    SubPrepPackageService::Request request =
        packageRequest(targetRoot.path(), QStringLiteral("Daily"));
    request.rosterTemplate = RosterTemplatePrintService::TemplateId::Daily;

    const SubPrepPackageService::Result result =
        SubPrepPackageService::generate(request);

    QCOMPARE(result.status, SubPrepPackageService::Status::Completed);
    QVERIFY(QFileInfo::exists(
        QDir(result.outputDirectory).filePath(
            QStringLiteral("Rosters - Daily.pdf")
            )
        ));
    QVERIFY(!QFileInfo::exists(
        QDir(result.outputDirectory).filePath(
            QStringLiteral("Rosters - By Day.pdf")
            )
        ));
}

void SubPrepPackageServiceTests
    ::perClassPackagePlacesRosterInsideClassFolder()
{
    QTemporaryDir targetRoot;
    QVERIFY(targetRoot.isValid());

    SubPrepPackageService::Request request =
        packageRequest(targetRoot.path(), QStringLiteral("Jamie"));
    request.rosterTemplate =
        RosterTemplatePrintService::TemplateId::PerClassWithExtraInfo;
    request.selectedExtraColumns = {QStringLiteral("Allergies")};

    const SubPrepPackageService::Result result =
        SubPrepPackageService::generate(request);
    QCOMPARE(result.status, SubPrepPackageService::Status::Completed);
    QVERIFY(!QFileInfo::exists(
        QDir(result.outputDirectory).filePath(
            QStringLiteral("Rosters - By Day.pdf")
            )
        ));

    const QStringList classFolders =
        QDir(result.outputDirectory).entryList(
            QDir::Dirs | QDir::NoDotAndDotDot
            );
    QCOMPARE(classFolders.size(), 1);
    QVERIFY(QFileInfo::exists(
        QDir(result.outputDirectory).filePath(
            QDir(classFolders.first()).filePath(QStringLiteral("Roster.pdf"))
            )
        ));
}

void SubPrepPackageServiceTests
    ::intensiveScheduleControlsSelectedDayFiltering()
{
    ClassInfo info = g_classInfo.value(42);
    info.intensiveTimes = info.classTimes;
    info.classTimes.first().day = QStringLiteral("Monday");
    g_classInfo.insert(42, info);

    QTemporaryDir targetRoot;
    QVERIFY(targetRoot.isValid());

    SubPrepPackageService::Request intensiveRequest =
        packageRequest(targetRoot.path(), QStringLiteral("Intensive"));
    intensiveRequest.useIntensiveSchedule = true;
    QCOMPARE(
        SubPrepPackageService::generate(intensiveRequest).status,
        SubPrepPackageService::Status::Completed
        );

    SubPrepPackageService::Request regularRequest =
        packageRequest(targetRoot.path(), QStringLiteral("Regular"));
    regularRequest.useIntensiveSchedule = false;
    QCOMPARE(
        SubPrepPackageService::generate(regularRequest).status,
        SubPrepPackageService::Status::Failed
        );
}

void SubPrepPackageServiceTests
    ::printOnlyUsesPacketOrderAndReportsCancellation()
{
    SubPrepPackageService::Request request =
        packageRequest(QString(), QString());
    request.createFolder = false;
    request.printPaperCopies = true;

    g_batchPrintStatus = PdfPrintService::Status::Sent;
    const SubPrepPackageService::Result sent =
        SubPrepPackageService::generate(request);
    QCOMPARE(sent.status, SubPrepPackageService::Status::Completed);
    QVERIFY(!sent.folderCreated);
    QCOMPARE(g_batchPrintPaths.size(), 2);
    QCOMPARE(
        QFileInfo(g_batchPrintPaths.at(0)).fileName(),
        QStringLiteral("Sub Prep.pdf")
        );
    QCOMPARE(
        QFileInfo(g_batchPrintPaths.at(1)).fileName(),
        QStringLiteral("Rosters - By Day.pdf")
        );

    g_batchPrintStatus = PdfPrintService::Status::Canceled;
    const SubPrepPackageService::Result canceled =
        SubPrepPackageService::generate(request);
    QCOMPARE(canceled.status, SubPrepPackageService::Status::Canceled);
    QVERIFY(canceled.printCanceled);

    QTemporaryDir targetRoot;
    QVERIFY(targetRoot.isValid());
    SubPrepPackageService::Request failureRequest =
        packageRequest(targetRoot.path(), QStringLiteral("Printer Failure"));
    failureRequest.printPaperCopies = true;
    g_batchPrintStatus = PdfPrintService::Status::Failed;
    const SubPrepPackageService::Result failedPrint =
        SubPrepPackageService::generate(failureRequest);
    QCOMPARE(failedPrint.status, SubPrepPackageService::Status::Failed);
    QVERIFY(failedPrint.folderCreated);
    QVERIFY(QFileInfo::exists(
        QDir(failedPrint.outputDirectory).filePath(
            QStringLiteral("Sub Prep.pdf")
            )
        ));
}

QTEST_MAIN(SubPrepPackageServiceTests)

#include "sub_prep_package_service_tests.moc"
