#include "features/sub_prep/services/sub_prep_package_service.h"

#include "core/application_services.h"
#include "data/data_service.h"
#include "domain/models/teacher.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPdfDocument>
#include <QTemporaryDir>

#include <algorithm>
#include <utility>

namespace
{
constexpr int HeavyClassCount = 96;
constexpr int HeavyClassesPerTeacher = 4;
constexpr int HeavyTeacherCount =
    HeavyClassCount / HeavyClassesPerTeacher;

QList<Classroom> g_classes;
QHash<int, ClassInfo> g_classInfo;
QHash<int, Roster> g_rosters;
QHash<int, Teacher> g_teachers;
PdfPrintService::Status g_batchPrintStatus =
    PdfPrintService::Status::Canceled;
QStringList g_batchPrintPaths;

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
    return reinterpret_cast<DataService*>(g_fakeDataServiceStorage);
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

QList<int> heavyClassIds()
{
    QList<int> ids;
    ids.reserve(HeavyClassCount);

    for (int index = 0; index < HeavyClassCount; ++index)
    {
        ids.append(1000 + index);
    }

    return ids;
}

QString heavyClassLabel(
    int classIndex
    )
{
    return QStringLiteral("E%1 Level %2")
        .arg((classIndex / 16) + 1)
        .arg((classIndex % 4) + 1);
}

void populateHeavyOutputData()
{
    g_classes.clear();
    g_classInfo.clear();
    g_rosters.clear();
    g_teachers.clear();
    g_batchPrintStatus = PdfPrintService::Status::Canceled;
    g_batchPrintPaths.clear();

    for (int classIndex = 0; classIndex < HeavyClassCount; ++classIndex)
    {
        const int classId = 1000 + classIndex;
        const int teacherIndex = classIndex / HeavyClassesPerTeacher;
        const int teacherId = 200 + teacherIndex;
        const QString label = heavyClassLabel(classIndex);
        const QString teacherName = QStringLiteral("Teacher %1")
            .arg(teacherIndex + 1, 2, 10, QLatin1Char('0'));
        const QString startTime =
            QStringLiteral("%1:00 PM").arg(4 + (classIndex % 8));
        const QString endTime =
            QStringLiteral("%1:50 PM").arg(4 + (classIndex % 8));

        g_classes.append(
            Classroom(
                QStringLiteral("Heavy Class %1").arg(classIndex + 1),
                classId
                )
            );

        Teacher teacher;
        teacher.id = teacherId;
        teacher.teacherEn = teacherName;
        teacher.roomNumber = QStringLiteral("%1").arg(401 + teacherIndex);
        teacher.wifiName = QStringLiteral("%1 WiFi").arg(teacherName);
        teacher.wifiPassword = QStringLiteral("wifi-%1").arg(teacherIndex + 1);
        teacher.internetType = QStringLiteral("WiFi");
        teacher.zoomId = QStringLiteral("zoom.%1").arg(teacherIndex + 1);
        teacher.zoomPassword = QStringLiteral("zoom-%1").arg(teacherIndex + 1);
        teacher.projectionType = QStringLiteral("HDMI");
        teacher.notes = QStringLiteral("Call %1 before class.").arg(teacherName);
        g_teachers.insert(teacherId, teacher);

        ClassInfo info;
        info.classId = classId;
        info.teacherId = teacherId;
        info.teacherEn = teacherName;
        info.roomNumber = teacher.roomNumber;
        info.wifiName = teacher.wifiName;
        info.wifiPassword = teacher.wifiPassword;
        info.internetType = teacher.internetType;
        info.zoomId = teacher.zoomId;
        info.zoomPassword = teacher.zoomPassword;
        info.projectionType = teacher.projectionType;
        info.classGrade = QStringLiteral("E%1").arg((classIndex / 16) + 1);
        info.classLevel = QStringLiteral("Level %1").arg((classIndex % 4) + 1);
        info.classColor = classIndex % 2 == 0
            ? QStringLiteral("#dbeafe")
            : QStringLiteral("#dcfce7");
        info.fontColor = QStringLiteral("#1e3a5f");
        info.notes = QStringLiteral(
            "Read chapter %1 and discuss the vocabulary."
            ).arg((classIndex % 6) + 1);
        info.classTimes.append(
            {
                QStringLiteral("Tuesday"),
                startTime,
                endTime
            }
            );
        info.intensiveTimes = info.classTimes;
        g_classInfo.insert(classId, info);

        Roster roster;
        roster.columns = {
            QStringLiteral("English"),
            QStringLiteral("Korean"),
            QStringLiteral("Allergies")
        };
        for (int studentIndex = 0; studentIndex < 25; ++studentIndex)
        {
            roster.rows.append(
                {
                    QStringLiteral("Student %1-%2")
                        .arg(classIndex + 1)
                        .arg(studentIndex + 1),
                    QStringLiteral("Student Korean %1-%2")
                        .arg(classIndex + 1)
                        .arg(studentIndex + 1),
                    studentIndex % 5 == 0
                        ? QStringLiteral("Check allergy plan")
                        : QStringLiteral("None")
                }
                );
        }
        g_rosters.insert(classId, roster);
    }
}

SubPrepPrintService::Request heavySubPrepRequest()
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
    request.classMaterials = QStringLiteral(
        "Materials are in the blue folder.\n"
        "Use the projector remote from the desk."
        );
    request.gradingInstructions = QStringLiteral(
        "Score each book report out of 100 and leave actionable feedback."
        );
    request.specialInstructions = QStringLiteral(
        "Collect completed reports before students leave."
        );
    request.schedule.days = {QStringLiteral("Tuesday")};

    for (int rowIndex = 0; rowIndex < 8; ++rowIndex)
    {
        ScheduleRowView row;
        row.timeLabel = QStringLiteral("%1:00").arg(16 + rowIndex);
        row.timeRangeLabel = QStringLiteral(
            "%1:00 -\n%1:50"
            ).arg(4 + rowIndex);

        ScheduleCellView cell;
        cell.day = QStringLiteral("Tuesday");
        cell.timeLabel = row.timeLabel;

        for (int classIndex = rowIndex * 12;
             classIndex < (rowIndex + 1) * 12;
             ++classIndex)
        {
            const int classId = 1000 + classIndex;
            const Teacher teacher =
                g_teachers.value(200 + classIndex / HeavyClassesPerTeacher);
            const ClassInfo info = g_classInfo.value(classId);

            ScheduleEntry entry;
            entry.classId = classId;
            entry.teacherEn = teacher.teacherEn;
            entry.roomNumber = teacher.roomNumber;
            entry.classGrade = info.classGrade;
            entry.classLevel = info.classLevel;
            entry.classColor = info.classColor;
            entry.fontColor = info.fontColor;
            cell.entries.append(entry);
        }

        row.cells.append(cell);
        request.schedule.rows.append(row);
    }

    for (int teacherIndex = 0;
         teacherIndex < HeavyTeacherCount;
         ++teacherIndex)
    {
        const Teacher teacher = g_teachers.value(200 + teacherIndex);
        SubPrepClassInformation::TeacherGroup group;
        group.teacher = teacher;
        group.displayName = teacher.preferredDisplayName();

        QStringList classLabels;
        for (int offset = 0; offset < HeavyClassesPerTeacher; ++offset)
        {
            const int classIndex =
                teacherIndex * HeavyClassesPerTeacher + offset;
            const int classId = 1000 + classIndex;
            const ClassInfo info = g_classInfo.value(classId);

            SubPrepClassInformation::ClassDetails details;
            details.classId = classId;
            details.info = info;
            details.studentCount = g_rosters.value(classId).rows.size();
            details.classLabel = heavyClassLabel(classIndex);
            details.timeText = QStringLiteral("Tues %1")
                .arg(info.classTimes.first().startTime);
            group.classes.append(details);
            classLabels.append(details.classLabel);
        }
        group.classListText = classLabels.join(QStringLiteral(" / "));
        request.classInformation.append(group);
    }

    request.subNotes = QStringLiteral(
        "Thank you for covering these classes. Please leave a short handover note."
        );
    return request;
}

SubPrepPackageService::Request heavyPackageRequest(
    const QString& targetRoot
    )
{
    SubPrepPackageService::Request request;
    request.services = fakeApplicationServices();
    request.subPrep = heavySubPrepRequest();
    request.selectedDates = {QDate(2026, 7, 21)};
    request.classIds = heavyClassIds();
    request.createFolder = true;
    request.targetRoot = targetRoot;
    request.userName = QStringLiteral("96-class reference");
    request.replaceExisting = true;
    request.openFolderAfterGeneration = false;
    request.rosterTemplate = RosterTemplatePrintService::TemplateId::Daily;
    return request;
}

bool inspectGeneratedPdf(
    const QString& pdfPath,
    const QString& capturePath,
    QJsonObject* metadata,
    QString* errorMessage
    )
{
    QPdfDocument document;
    if (document.load(pdfPath) != QPdfDocument::Error::None
        || document.status() != QPdfDocument::Status::Ready
        || document.pageCount() <= 0)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Generated PDF could not be loaded.");
        }
        return false;
    }

    const QSizeF pagePoints = document.pagePointSize(0);
    const QSize renderSize(
        std::max(1, qRound(pagePoints.width() * 150.0 / 72.0)),
        std::max(1, qRound(pagePoints.height() * 150.0 / 72.0))
        );
    const QImage firstPage = document.render(0, renderSize);
    if (firstPage.isNull())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Generated PDF first page could not be rendered.");
        }
        return false;
    }

    if (!capturePath.isEmpty() && !firstPage.save(capturePath, "PNG"))
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Generated PDF first page could not be captured.");
        }
        return false;
    }

    if (metadata)
    {
        metadata->insert(QStringLiteral("pageCount"), document.pageCount());
        metadata->insert(QStringLiteral("fileSizeBytes"), QFileInfo(pdfPath).size());
        metadata->insert(QStringLiteral("firstPageWidth"), firstPage.width());
        metadata->insert(QStringLiteral("firstPageHeight"), firstPage.height());
    }

    return true;
}
}

DataService* ApplicationServices::dataService() const
{
    return fakeDataService();
}

bool ApplicationServices::hasOpenDatabase() const
{
    return true;
}

bool DataService::isOpen() const
{
    return true;
}

Result<QList<Classroom>> DataService::getClasses()
{
    return g_classes;
}

Result<Classroom> DataService::getClassById(
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
    return std::unexpected(QStringLiteral("Class not found."));
}

Result<ClassInfo> DataService::loadClassInfo(
    int classId
    )
{
    return g_classInfo.value(classId);
}

Result<Roster> DataService::loadRoster(
    int classId
    )
{
    return g_rosters.value(classId);
}

Result<Teacher> DataService::getTeacher(
    int teacherId
    )
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
    void largePackageGeneratesOutputReferenceWhenConfigured();
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

void SubPrepPackageServiceTests
    ::largePackageGeneratesOutputReferenceWhenConfigured()
{
    populateHeavyOutputData();

    QTemporaryDir temporaryRoot;
    QVERIFY(temporaryRoot.isValid());

    const QString configuredRoot =
        qEnvironmentVariable(
            "CLASSMNGR_SUB_PREP_OUTPUT_REFERENCE_DIR"
            ).trimmed();
    const QString artifactRoot =
        configuredRoot.isEmpty()
            ? temporaryRoot.path()
            : QFileInfo(configuredRoot).absoluteFilePath();
    const QString retentionRoot =
        configuredRoot.isEmpty()
            ? QString()
            : QDir(artifactRoot).filePath(QStringLiteral("reference"));

    if (!configuredRoot.isEmpty())
    {
        QVERIFY2(
            QDir().mkpath(artifactRoot),
            qPrintable(
                QStringLiteral("Could not create output reference root: %1")
                    .arg(artifactRoot)
                )
            );
        QVERIFY2(
            QDir().mkpath(retentionRoot),
            qPrintable(
                QStringLiteral("Could not create output reference directory: %1")
                    .arg(retentionRoot)
                )
            );
    }

    const QString packageRoot =
        QDir(temporaryRoot.path()).filePath(QStringLiteral("package"));
    const SubPrepPackageService::Result result =
        SubPrepPackageService::generate(
            heavyPackageRequest(packageRoot)
            );

    QVERIFY2(
        result.status == SubPrepPackageService::Status::Completed,
        qPrintable(result.message)
        );
    QVERIFY(result.folderCreated);
    QCOMPARE(result.documentPaths.size(), 2);
    QCOMPARE(
        QDir(result.outputDirectory).entryList(
            QDir::Dirs | QDir::NoDotAndDotDot
            ).size(),
        HeavyClassCount
        );

    QJsonArray documents;
    for (const QString& documentPath : result.documentPaths)
    {
        QVERIFY2(
            QFileInfo(documentPath).size() > 0,
            qPrintable(
                QStringLiteral("Generated document is empty: %1")
                    .arg(documentPath)
                )
            );

        QJsonObject documentMetadata;
        const QString captureName =
            QFileInfo(documentPath).completeBaseName()
            + QStringLiteral("-first-page.png");
        const QString retainedDocumentPath =
            configuredRoot.isEmpty()
                ? QString()
                : QDir(retentionRoot).filePath(
                    QFileInfo(documentPath).fileName()
                    );
        if (!retainedDocumentPath.isEmpty())
        {
            if (QFileInfo::exists(retainedDocumentPath))
            {
                QVERIFY(QFile::remove(retainedDocumentPath));
            }
            QVERIFY(QFile::copy(documentPath, retainedDocumentPath));
        }
        const QString capturePath =
            configuredRoot.isEmpty()
                ? QString()
                : QDir(retentionRoot).filePath(captureName);
        QString inspectError;
        QVERIFY2(
            inspectGeneratedPdf(
                documentPath,
                capturePath,
                &documentMetadata,
                &inspectError
                ),
            qPrintable(inspectError)
            );

        documentMetadata.insert(
            QStringLiteral("relativePath"),
            QDir(artifactRoot)
                .relativeFilePath(
                    retainedDocumentPath.isEmpty()
                        ? documentPath
                        : retainedDocumentPath
                    )
                .replace(QLatin1Char('\\'), QLatin1Char('/'))
            );
        if (!capturePath.isEmpty())
        {
            documentMetadata.insert(
                QStringLiteral("firstPageCapture"),
                QDir(artifactRoot)
                    .relativeFilePath(capturePath)
                    .replace(QLatin1Char('\\'), QLatin1Char('/'))
                );
        }
        documents.append(documentMetadata);
    }

    if (!configuredRoot.isEmpty())
    {
        QJsonObject manifest;
        manifest.insert(
            QStringLiteral("fixtureScale"),
            QStringLiteral("large_startup_cardinality")
            );
        manifest.insert(QStringLiteral("teacherCount"), HeavyTeacherCount);
        manifest.insert(QStringLiteral("classCount"), HeavyClassCount);
        manifest.insert(
            QStringLiteral("rosterRowCount"),
            HeavyClassCount * 25
            );
        manifest.insert(
            QStringLiteral("rosterCellCount"),
            HeavyClassCount * 25 * 3
            );
        manifest.insert(
            QStringLiteral("selectedDate"),
            QStringLiteral("2026-07-21")
            );
        manifest.insert(QStringLiteral("documentCount"), documents.size());
        manifest.insert(QStringLiteral("documents"), documents);

        QFile manifestFile(
            QDir(artifactRoot).filePath(QStringLiteral("manifest.json"))
            );
        QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(
            manifestFile.write(
                QJsonDocument(manifest).toJson(QJsonDocument::Indented)
                ) > 0
            );
        manifestFile.close();
    }
}

QTEST_MAIN(SubPrepPackageServiceTests)

#include "sub_prep_package_service_tests.moc"
