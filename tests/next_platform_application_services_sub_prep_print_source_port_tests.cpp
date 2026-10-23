#include "core/application_services.h"
#include "core/startup_profiler.h"
#include "app/services/feature_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/roster.h"
#include "domain/models/teacher.h"
#include "next/application/sub_prep_print_source_query.h"
#include "next/platform/application_services_sub_prep_class_details_port.h"
#include "next/platform/application_services_sub_prep_print_source_port.h"
#include "next/platform/application_services_sub_prep_schedule_summary_port.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>
#include <QVariant>

#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;
using ClassMngr::Next::Platform::
    ApplicationServicesSubPrepClassDetailsPort;
using ClassMngr::Next::Platform::
    ApplicationServicesSubPrepPrintSourcePort;
using ClassMngr::Next::Platform::
    ApplicationServicesSubPrepScheduleSummaryPort;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("sub-prep-print-source-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

bool openDatabase(
    ApplicationServices& services,
    QTemporaryDir& directory
    )
{
    return services.openDatabase(databasePath(directory)).has_value();
}

ClassId classId(const int id)
{
    return *ClassId::fromString(std::to_string(id));
}

TeacherId teacherId(const int id)
{
    return *TeacherId::fromString(std::to_string(id));
}

std::string utf8(const QString& value)
{
    const QByteArray encoded = value.toUtf8();
    return std::string(
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
        );
}

bool executeSql(
    ApplicationServices& services,
    const QString& statement
    )
{
    DataService* dataService = services.dataService();
    if (!dataService || !dataService->databaseSession())
    {
        return false;
    }

    QSqlQuery query(dataService->databaseSession()->database());
    return query.exec(statement);
}

bool updateTeacherDisplayNames(
    ApplicationServices& services,
    const int teacherId,
    const QString& koreanName,
    const QString& englishName,
    const QString& preferredRomanization,
    const QString& preferredName
    )
{
    DataService* dataService = services.dataService();
    if (!dataService || !dataService->databaseSession())
    {
        return false;
    }

    QSqlQuery query(dataService->databaseSession()->database());
    if (!query.prepare(QStringLiteral(
        "UPDATE teachers SET teacher_kr = ?, teacher_en = ?, "
        "preferred_romanization = ?, preferred_name = ? WHERE id = ?"
        )))
    {
        return false;
    }
    query.addBindValue(koreanName);
    query.addBindValue(englishName);
    query.addBindValue(preferredRomanization);
    query.addBindValue(preferredName);
    query.addBindValue(teacherId);
    return query.exec();
}

int createTeacher(
    ApplicationServices& services,
    const QString& englishName,
    const QString& preferredName,
    const QString& suffix
    )
{
    Teacher teacher;
    teacher.teacherKr = QString::fromUtf8("\xEA\xB0\x80\xEB\x82\x98");
    teacher.teacherEn = englishName;
    teacher.preferredRomanization = preferredName;
    teacher.preferredName = preferredName;
    teacher.roomNumber = QStringLiteral("Room ") + suffix;
    teacher.wifiName = QStringLiteral("Network ") + suffix;
    teacher.wifiPassword = QStringLiteral("wifi-password-") + suffix;
    teacher.internetType = QStringLiteral("LAN");
    teacher.zoomId = QStringLiteral("zoom-") + suffix;
    teacher.zoomPassword = QStringLiteral("zoom-password-") + suffix;
    teacher.projectionType = QStringLiteral("Zoom");
    teacher.notes = QString::fromUtf8("\xEA\xB5\x90\xEC\x82\xAC \xEB\x85\xB8\xED\x8A\xB8 ")
        + suffix;

    const auto created = services.teacherService()->create(teacher);
    return created ? *created : -1;
}

int createClass(
    ApplicationServices& services,
    const QString& name,
    const int assignedTeacherId,
    const QString& grade,
    const QString& level,
    const QString& notes,
    const QString& color,
    const QString& fontColor,
    const QList<ClassTime>& regularTimes,
    const QList<ClassTime>& intensiveTimes = {}
    )
{
    const auto created = services.classService()->create(name);
    if (!created)
    {
        return -1;
    }

    ClassInfo info;
    info.classId = *created;
    info.teacherId = assignedTeacherId;
    info.classGrade = grade;
    info.classLevel = level;
    info.classColor = color;
    info.fontColor = fontColor;
    info.notes = notes;
    info.classTimes = regularTimes;
    info.intensiveTimes = intensiveTimes;
    if (!services.classService()->saveClassInfo(info))
    {
        return -1;
    }

    return *created;
}

Roster rosterWithTwoStudents()
{
    Roster roster;
    roster.columns = Roster::BaseColumns;
    roster.rows = {
        {QStringLiteral("Alex"), QString::fromUtf8("\xEA\xB9\x80\xEC\x84\xA0\xEC\x83\x9D"),
         {}, {}, {}, {}},
        {QStringLiteral("Casey"), QString::fromUtf8("\xEC\x9D\xB4\xED\x95\x99\xEC\x83\x9D"),
         {}, {}, {}, {}}
    };
    return roster;
}

SubPrepPrintSourceRequest requestFor(
    const std::vector<ClassId>& selectedClasses,
    const std::vector<SubPrepWeekday>& selectedDays,
    const ScheduleViewMode mode
    )
{
    return {
        .selectedClassIds = selectedClasses,
        .selectedDays = selectedDays,
        .mode = mode
    };
}

SubPrepScheduleScopeRequest summaryRequestFor(
    const std::vector<ClassId>& visibleClasses,
    const std::vector<SubPrepWeekday>& selectedDays,
    const ScheduleViewMode mode
    )
{
    return {
        .visibleClassIds = visibleClasses,
        .selectedDays = selectedDays,
        .mode = mode
    };
}

bool insertScheduleMeeting(
    ApplicationServices& services,
    const QString& table,
    const int classId,
    const QString& day,
    const QString& startTime
    )
{
    DataService* dataService = services.dataService();
    if (!dataService || !dataService->databaseSession())
    {
        return false;
    }

    QSqlQuery query(dataService->databaseSession()->database());
    if (!query.prepare(QStringLiteral(
            "INSERT INTO %1 (class_id, day, start_time, end_time) "
            "VALUES (?, ?, ?, ?)"
            ).arg(table)))
    {
        return false;
    }
    query.addBindValue(classId);
    query.addBindValue(day);
    query.addBindValue(startTime);
    query.addBindValue(QStringLiteral("11:59 PM"));
    return query.exec();
}

} // namespace

class NextPlatformApplicationServicesSubPrepPrintSourcePortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void projectsVisibleScheduleSummariesAndAggregatesRosterCounts();
    void navigationSummaryOmitsRosterCountAndMatchesCountedSummary();
    void scheduleSummaryDoesNotAliasFractionalRosterRows();
    void wideAssignedTeacherIdRemainsInScheduleSummaries();
    void selectsModeAndOmitsUnusableOrOutOfScopeClasses();
    void rosterAggregateFailureKeepsLegacyZeroCountFallback();
    void emptyScopeDoesNotReadAndUnavailableSessionFails();
    void rejectsNoncanonicalIdsAndSupportsMaximumClassScope();
    void rejectsOversizedTeacherSummaryFields();
    void summaryPerClassMeetingOverflowSurfacesValidation();
    void summaryAggregateMeetingOverflowSurfacesValidation();
    void allEmptyTeacherNamesKeepAdapterFallbacks();
    void selectedClassDetailsReadUsesOnlyScopedSessionData();
    void selectedClassDetailsPreservesWideTeacherId();
    void selectedClassDetailsUnavailableSessionsAreTechnical();
    void selectedClassDetailsSqlRepositoryFailureIsTechnical();
    void selectedClassDetailsUsesMissingTeacherFallbackAndBoundsFields();
    void projectsSelectedClassesInRequestOrderAndCopiesFilteredSource();
    void printSourcePreservesWideTeacherIdAndCompleteProfile();
    void batchesTeacherProfilesInFirstReferenceOrderWithoutUnassignedReads();
    void batchesRosterCountsAfterPrintScopeAndPreservesLegacyMetrics();
    void subPrepCountDoesNotAliasFractionalRosterColumns();
    void subPrepCountDoesNotAliasFractionalRosterRows();
    void batchesRosterCountsWideRowIndicesAndSaturatesMetrics();
    void rosterBatchFailureFallsBackPerClassAndKeepsSuccessfulSibling();
    void omittedPrintClassesDoNotReadRosters();
    void selectsIntensiveTimesAndOmitsClassesOutsideSelectedDays();
    void supportsUnassignedAndMissingTeachersAndEmptyRosterFallback();
    void unavailableSessionsDoNotFallBackToDataService();
    void queryEmptyScopeDoesNotReadAndNonemptyReadFailurePropagates();
    void rejectsNoncanonicalClassIdAliasesBeforeReading();
    void maximumClassScopeFitsSqliteBindLimits();
    void ordersAllCanonicalWeekdaysInSummariesAndNavigation();
    void regularAllWeekNavigationMetadataWinsOverIntensiveSummary();
    void parsesRawScheduleTimesAndKeepsMalformedNavigationFallback();
    void unknownAndNoncanonicalRawDaysDoNotChangeNavigationOrPresentation();
    void perClassMeetingOverflowSurfacesValidation();
    void aggregateMeetingOverflowSurfacesValidation();
    void boundaryReturnsOwningTypedValues();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
projectsVisibleScheduleSummariesAndAggregatesRosterCounts()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    const int teacher = createTeacher(
        services,
        QStringLiteral("English Name"),
        QStringLiteral("Preferred Name"),
        QStringLiteral("summary")
    );
    QVERIFY(teacher > 0);
    QVERIFY(updateTeacherDisplayNames(
        services,
        teacher,
        QStringLiteral("  \uac00\ub098  "),
        QStringLiteral("  English Name  "),
        QStringLiteral("  Romanized Name  "),
        QStringLiteral("  Preferred Name  ")
        ));
    const int secondClass = createClass(
        services,
        QStringLiteral("Second class"),
        teacher,
        QStringLiteral("E5"),
        QStringLiteral("Apollo"),
        QStringLiteral("Second notes"),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("10:00 AM"), QStringLiteral("10:45 AM")}
        }
        );
    const int firstClass = createClass(
        services,
        QStringLiteral("First class"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("First notes"),
        QStringLiteral("#112233"),
        QStringLiteral("#AABBCC"),
        {
            {QStringLiteral("Wednesday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")},
            {QStringLiteral("Tuesday"), QStringLiteral("11:00 AM"), QStringLiteral("11:45 AM")},
            {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")}
        }
        );
    const int outsideSelectedDays = createClass(
        services,
        QStringLiteral("Outside selected days"),
        teacher,
        QStringLiteral("E6"),
        QStringLiteral("Helios"),
        QStringLiteral("Outside notes"),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Tuesday"), QStringLiteral("1:00 PM"), QStringLiteral("1:45 PM")}
        }
        );
    QVERIFY(secondClass > 0);
    QVERIFY(firstClass > 0);
    QVERIFY(outsideSelectedDays > 0);

    Roster firstClassRoster = rosterWithTwoStudents();
    const QString nonBreakingSpace = QString::fromUtf8("\xC2\xA0");
    firstClassRoster.rows.push_back({
        nonBreakingSpace, {}, {}, {}, {}, {}
    });
    firstClassRoster.rows.push_back({
        {}, nonBreakingSpace, {}, {}, {}, {}
    });
    QVERIFY(services.rosterService()->saveRoster(firstClass, firstClassRoster));
    Roster oneStudent;
    oneStudent.columns = Roster::BaseColumns;
    oneStudent.rows = {
        {QStringLiteral("Casey"), QString::fromUtf8("\xEC\x9D\xB4\xED\x95\x99\xEC\x83\x9D"), {}, {}, {}, {}}
    };
    QVERIFY(services.rosterService()->saveRoster(secondClass, oneStudent));

    ApplicationServicesSubPrepScheduleSummaryPort port(services);
    const SubPrepScheduleSummaryQuery query(port);
    const auto result = query.execute(summaryRequestFor(
        {
            classId(outsideSelectedDays),
            classId(secondClass),
            classId(firstClass)
        },
        {SubPrepWeekday::Monday, SubPrepWeekday::Wednesday},
        ScheduleViewMode::Regular
        ));

    QVERIFY(result);
    QCOMPARE(result.value().classes().size(), std::size_t(2));
    QCOMPARE(result.value().classes()[0].id, classId(secondClass));
    QCOMPARE(
        result.value().classes()[0].order.rawValue(),
        std::int32_t(1)
        );
    QCOMPARE(result.value().classes()[0].grade, std::string("E5"));
    QCOMPARE(result.value().classes()[0].level, std::string("Apollo"));
    QCOMPARE(result.value().classes()[0].displayLabel, std::string("E5 Apollo"));
    QCOMPARE(result.value().classes()[0].studentCount.rawValue(), std::size_t(1));
    QCOMPARE(result.value().classes()[1].id, classId(firstClass));
    QCOMPARE(
        result.value().classes()[1].order.rawValue(),
        std::int32_t(2)
        );
    QCOMPARE(result.value().classes()[1].studentCount.rawValue(), std::size_t(2));
    QCOMPARE(result.value().classes()[1].meetingText, std::string("MonWed 9am"));
    QCOMPARE(
        result.value().classes()[1].navigationLabel,
        utf8(QStringLiteral("Perseus • M/W 9:00; T 11:00"))
        );
    QCOMPARE(
        result.value().classes()[1].navigationTeacherLabel,
        std::string("English Name")
        );
    QCOMPARE(
        result.value().classes()[1].navigationFirstDayOrder.rawValue(),
        0
        );
    QCOMPARE(
        result.value().classes()[1].navigationFirstTimeOrder.rawValue(),
        9 * 60
        );
    QCOMPARE(result.value().teacherIndex().summaries().size(), std::size_t(1));
    const auto teacherSummary = result.value().teacherIndex().find(
        teacherId(teacher)
        );
    QVERIFY(teacherSummary.has_value());
    QCOMPARE(teacherSummary->displayName, std::string("Preferred Name"));
    QVERIFY(teacherSummary->facilities.find("Room: Room summary")
            != std::string::npos);
    QVERIFY(teacherSummary->facilities.find("WiFi Password: wifi-password-summary")
            != std::string::npos);
    QVERIFY(teacherSummary->notes.find("summary") != std::string::npos);

    const auto ownedCopy = result.value();
    services.closeDatabase();
    QVERIFY(ownedCopy == result.value());
    QCOMPARE(ownedCopy.classes()[1].meetingText, std::string("MonWed 9am"));
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
navigationSummaryOmitsRosterCountAndMatchesCountedSummary()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    const int teacher = createTeacher(
        services,
        QStringLiteral("English Name"),
        QStringLiteral("Preferred Name"),
        QStringLiteral("navigation-summary")
        );
    QVERIFY(teacher > 0);
    QVERIFY(updateTeacherDisplayNames(
        services,
        teacher,
        QString::fromUtf8("\xEA\xB0\x80\xEB\x82\x98"),
        QStringLiteral("English Display"),
        QStringLiteral("Romanized Display"),
        QStringLiteral("Preferred Display")
        ));

    const int selectedClass = createClass(
        services,
        QStringLiteral("Navigation summary class"),
        teacher,
        QStringLiteral("E5"),
        QStringLiteral("Apollo"),
        QStringLiteral("Class notes"),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")}
        }
        );
    QVERIFY(selectedClass > 0);
    QVERIFY(services.rosterService()->saveRoster(
        selectedClass,
        rosterWithTwoStudents()
        ));

    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    QVERIFY(store->isOpen());
    ClassInfoRepository* const repository = store->classInfoRepository();
    QVERIFY(repository);

    const QList<int> classIds{selectedClass};
    const QStringList selectedDays{QStringLiteral("Monday")};
    const auto counted = repository->loadSubPrepClassSummaries(
        classIds,
        selectedDays,
        ScheduleType::Regular,
        static_cast<int>(kSubPrepScheduleSummaryMaxMeetingsPerClass),
        static_cast<int>(kSubPrepScheduleSummaryMaxMeetings)
        );
    QVERIFY(counted);
    QCOMPARE(counted->size(), 1);
    const SubPrepClassSummaryRecord& selectedSummary = counted->front();
    QCOMPARE(selectedSummary.studentCount, qint64(2));

    const auto navigation = repository->loadSubPrepClassNavigationSummaries(
        classIds,
        selectedDays,
        ScheduleType::Regular,
        static_cast<int>(kSubPrepScheduleSummaryMaxMeetingsPerClass),
        static_cast<int>(kSubPrepScheduleSummaryMaxMeetings)
        );
    QVERIFY(navigation);
    QCOMPARE(navigation->size(), 1);
    const SubPrepClassSummaryNavigationReadRecord& navigationSummary =
        navigation->front();

    QCOMPARE(navigationSummary.classId, selectedSummary.classId);
    QCOMPARE(navigationSummary.teacherId, selectedSummary.teacherId);
    QCOMPARE(navigationSummary.teacherKr, selectedSummary.teacherKr);
    QCOMPARE(navigationSummary.teacherEn, selectedSummary.teacherEn);
    QCOMPARE(
        navigationSummary.teacherPreferredRomanization,
        selectedSummary.teacherPreferredRomanization
        );
    QCOMPARE(
        navigationSummary.teacherPreferredName,
        selectedSummary.teacherPreferredName
        );
    QCOMPARE(navigationSummary.classGrade, selectedSummary.classGrade);
    QCOMPARE(navigationSummary.classLevel, selectedSummary.classLevel);
    QCOMPARE(navigationSummary.teacherId, qint64(teacher));
    QCOMPARE(
        navigationSummary.teacherKr,
        QString::fromUtf8("\xEA\xB0\x80\xEB\x82\x98")
        );
    QCOMPARE(navigationSummary.teacherEn, QStringLiteral("English Display"));
    QCOMPARE(
        navigationSummary.teacherPreferredRomanization,
        QStringLiteral("Romanized Display")
        );
    QCOMPARE(
        navigationSummary.teacherPreferredName,
        QStringLiteral("Preferred Display")
        );
    QCOMPARE(navigationSummary.classGrade, QStringLiteral("E5"));
    QCOMPARE(navigationSummary.classLevel, QStringLiteral("Apollo"));
    QVERIFY(!navigationSummary.studentCount.has_value());

    QCOMPARE(
        navigationSummary.meetings.size(),
        selectedSummary.meetings.size()
        );
    QCOMPARE(navigationSummary.meetings.size(), 1);
    for (qsizetype index = 0;
         index < selectedSummary.meetings.size();
         ++index)
    {
        QCOMPARE(
            navigationSummary.meetings.at(index).day,
            selectedSummary.meetings.at(index).day
            );
        QCOMPARE(
            navigationSummary.meetings.at(index).startTime,
            selectedSummary.meetings.at(index).startTime
            );
    }
    QCOMPARE(
        navigationSummary.meetings.front().day,
        QStringLiteral("Monday")
        );
    QCOMPARE(
        navigationSummary.meetings.front().startTime,
        QStringLiteral("9:00 AM")
        );
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
scheduleSummaryDoesNotAliasFractionalRosterRows()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(
        services,
        QStringLiteral("Frac summary"),
        QStringLiteral("Frac summary"),
        QStringLiteral("fractional-summary-row")
        );
    QVERIFY(teacher > 0);

    const int selectedClass = createClass(
        services,
        QStringLiteral("Fractional summary row index"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QString(),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("8:00 AM"), QStringLiteral("8:45 AM")}
        }
        );
    QVERIFY(selectedClass > 0);

    Roster roster;
    roster.columns = {
        QStringLiteral("English"),
        QStringLiteral("Korean")
    };
    roster.rows = {{QString(), QString()}};
    RosterRepository* const rosterRepository = services.databaseSession()
        ? services.databaseSession()->rosterRepository()
        : nullptr;
    QVERIFY(rosterRepository);
    QVERIFY(rosterRepository->saveRoster(selectedClass, roster));

    DataService* const dataService = services.dataService();
    QVERIFY(dataService);
    QVERIFY(dataService->databaseSession());
    const QSqlDatabase database = dataService->databaseSession()->database();
    QSqlQuery insertCell(database);
    QVERIFY(insertCell.prepare(QStringLiteral(
        "INSERT INTO roster_data (class_id, row_index, col_index, value) "
        "VALUES (?, ?, ?, ?)"
        )));
    insertCell.addBindValue(selectedClass);
    insertCell.addBindValue(0);
    insertCell.addBindValue(0);
    insertCell.addBindValue(QString());
    QVERIFY2(insertCell.exec(), qPrintable(insertCell.lastError().text()));
    insertCell.finish();
    QVERIFY(insertCell.prepare(QStringLiteral(
        "INSERT INTO roster_data (class_id, row_index, col_index, value) "
        "VALUES (?, ?, ?, ?)"
        )));
    insertCell.addBindValue(selectedClass);
    insertCell.addBindValue(QVariant::fromValue(0.25));
    insertCell.addBindValue(0);
    insertCell.addBindValue(QStringLiteral("Fractional summary student"));
    QVERIFY2(insertCell.exec(), qPrintable(insertCell.lastError().text()));
    insertCell.finish();

    QSqlQuery storedCells(database);
    QVERIFY(storedCells.prepare(QStringLiteral(
        "SELECT row_index, typeof(row_index) AS storage_type, value "
        "FROM roster_data WHERE class_id=? AND col_index=0 ORDER BY row_index"
        )));
    storedCells.addBindValue(selectedClass);
    QVERIFY2(storedCells.exec(), qPrintable(storedCells.lastError().text()));
    QVERIFY(storedCells.next());
    QCOMPARE(storedCells.value("row_index").toLongLong(), qlonglong(0));
    QCOMPARE(storedCells.value("storage_type").toString(), QStringLiteral("integer"));
    QVERIFY(storedCells.value("value").toString().isEmpty());
    QVERIFY(storedCells.next());
    QCOMPARE(storedCells.value("row_index").toDouble(), 0.25);
    QCOMPARE(storedCells.value("storage_type").toString(), QStringLiteral("real"));
    QCOMPARE(
        storedCells.value("value").toString(),
        QStringLiteral("Fractional summary student")
        );
    QVERIFY(!storedCells.next());
    storedCells.finish();

    ApplicationServicesSubPrepScheduleSummaryPort port(services);
    const auto result = port.loadSummaries(summaryRequestFor(
        {classId(selectedClass)},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));

    QVERIFY(result);
    QCOMPARE(result.value().classes.size(), std::size_t(1));
    QCOMPARE(result.value().classes.front().id, classId(selectedClass));
    QCOMPARE(result.value().classes.front().studentCount.rawValue(), std::size_t(0));
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
wideAssignedTeacherIdRemainsInScheduleSummaries()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    constexpr qlonglong wideTeacherIdValue = 2'147'483'648LL;
    const auto wideTeacherIdResult = TeacherId::fromString("2147483648");
    QVERIFY(wideTeacherIdResult.has_value());
    const TeacherId wideTeacherId = *wideTeacherIdResult;

    DataService* const dataService = services.dataService();
    QVERIFY(dataService);
    QVERIFY(dataService->databaseSession());
    const QSqlDatabase database = dataService->databaseSession()->database();
    QSqlQuery insertTeacher(database);
    QVERIFY(insertTeacher.prepare(QStringLiteral(R"(
        INSERT INTO teachers (
            id, teacher_kr, teacher_en, preferred_romanization,
            preferred_name, room_number, birthday, phone_number,
            wifi_name, wifi_password, internet_type, zoom_id,
            zoom_password, projection_type, notes
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )")));
    insertTeacher.addBindValue(QVariant::fromValue<qlonglong>(wideTeacherIdValue));
    insertTeacher.addBindValue(QStringLiteral("Wide Teacher Korean"));
    insertTeacher.addBindValue(QStringLiteral("Wide Teacher English"));
    insertTeacher.addBindValue(QStringLiteral("Wide Teacher Romanization"));
    insertTeacher.addBindValue(QStringLiteral("Wide Preferred Name"));
    insertTeacher.addBindValue(QStringLiteral("Wide Room 214"));
    insertTeacher.addBindValue(QStringLiteral("2000-01-01"));
    insertTeacher.addBindValue(QStringLiteral("555-214-7483"));
    insertTeacher.addBindValue(QStringLiteral("Wide Teacher Network"));
    insertTeacher.addBindValue(QStringLiteral("wide-wifi-password"));
    insertTeacher.addBindValue(QStringLiteral("LAN"));
    insertTeacher.addBindValue(QStringLiteral("wide-zoom-id"));
    insertTeacher.addBindValue(QStringLiteral("wide-zoom-password"));
    insertTeacher.addBindValue(QStringLiteral("Zoom"));
    insertTeacher.addBindValue(QStringLiteral("Wide teacher notes"));
    QVERIFY2(insertTeacher.exec(), qPrintable(insertTeacher.lastError().text()));

    const int selectedClass = createClass(
        services,
        QStringLiteral("Wide teacher schedule"),
        -1,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Class notes for wide teacher"),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")}
        }
        );
    QVERIFY(selectedClass > 0);

    QSqlQuery assignTeacher(database);
    QVERIFY(assignTeacher.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id = ? WHERE class_id = ?")));
    assignTeacher.addBindValue(QVariant::fromValue<qlonglong>(wideTeacherIdValue));
    assignTeacher.addBindValue(selectedClass);
    QVERIFY2(assignTeacher.exec(), qPrintable(assignTeacher.lastError().text()));
    QCOMPARE(assignTeacher.numRowsAffected(), 1);

    QSqlQuery verifyStoredId(database);
    QVERIFY(verifyStoredId.prepare(QStringLiteral(R"(
        SELECT typeof(teachers.id), teachers.id,
               typeof(class_info.teacher_id), class_info.teacher_id
        FROM teachers
        INNER JOIN class_info ON class_info.teacher_id = teachers.id
        WHERE class_info.class_id = ?
    )")));
    verifyStoredId.addBindValue(selectedClass);
    QVERIFY2(verifyStoredId.exec(), qPrintable(verifyStoredId.lastError().text()));
    QVERIFY(verifyStoredId.next());
    QCOMPARE(verifyStoredId.value(0).toString(), QStringLiteral("integer"));
    QCOMPARE(verifyStoredId.value(1).toLongLong(), wideTeacherIdValue);
    QCOMPARE(verifyStoredId.value(2).toString(), QStringLiteral("integer"));
    QCOMPARE(verifyStoredId.value(3).toLongLong(), wideTeacherIdValue);
    QVERIFY(!verifyStoredId.next());

    ApplicationServicesSubPrepScheduleSummaryPort port(services);
    const SubPrepScheduleSummaryQuery query(port);
    const auto result = query.execute(summaryRequestFor(
        {classId(selectedClass)},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));
    QVERIFY(result);
    QCOMPARE(result.value().classes().size(), std::size_t(1));
    const auto& summaryClass = result.value().classes().front();
    QCOMPARE(summaryClass.id, classId(selectedClass));
    QVERIFY(summaryClass.teacherId.has_value());
    QVERIFY(*summaryClass.teacherId == wideTeacherId);
    QCOMPARE(summaryClass.displayLabel, std::string("E4 Perseus"));
    QCOMPARE(summaryClass.navigationTeacherLabel, std::string("Wide Teacher English"));

    const auto summaryTeacher = result.value().teacherIndex().find(wideTeacherId);
    QVERIFY(summaryTeacher.has_value());
    QCOMPARE(summaryTeacher->displayName, std::string("Wide Preferred Name"));
    QVERIFY(summaryTeacher->facilities.find("Room: Wide Room 214") != std::string::npos);
    QVERIFY(summaryTeacher->facilities.find("WiFi: Wide Teacher Network") != std::string::npos);
    QVERIFY(summaryTeacher->facilities.find("WiFi Password: wide-wifi-password")
            != std::string::npos);
    QVERIFY(summaryTeacher->facilities.find("Internet: LAN") != std::string::npos);
    QVERIFY(summaryTeacher->facilities.find("Zoom ID: wide-zoom-id") != std::string::npos);
    QVERIFY(summaryTeacher->facilities.find("Zoom Password: wide-zoom-password")
            != std::string::npos);
    QVERIFY(summaryTeacher->facilities.find("Projection: Zoom") != std::string::npos);
    QCOMPARE(summaryTeacher->notes, std::string("Wide teacher notes"));
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
selectsModeAndOmitsUnusableOrOutOfScopeClasses()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    DataService* dataService = services.dataService();
    QVERIFY(dataService);
    QVERIFY(dataService->databaseSession());
    const int teacher = createTeacher(
        services,
        QStringLiteral("Mode Teacher"),
        QStringLiteral("Mode Teacher"),
        QStringLiteral("mode-summary")
        );
    QVERIFY(teacher > 0);
    const int usable = createClass(
        services,
        QStringLiteral("Usable intensive"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")}
        },
        {
            {QStringLiteral("Wednesday"), QStringLiteral("2:00 PM"), QStringLiteral("2:45 PM")}
        }
        );
    const int unassigned = createClass(
        services,
        QStringLiteral("Unassigned"),
        -1,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Wednesday"), QStringLiteral("3:00 PM"), QStringLiteral("3:45 PM")}
        }
        );
    const int staleTeacher = createClass(
        services,
        QStringLiteral("Stale teacher"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {},
        {
            {QStringLiteral("Wednesday"), QStringLiteral("4:00 PM"), QStringLiteral("4:45 PM")}
        }
        );
    QVERIFY(usable > 0);
    QVERIFY(unassigned > 0);
    QVERIFY(staleTeacher > 0);

    const auto noInfo = services.classService()->create(
        QStringLiteral("Missing class info")
        );
    QVERIFY(noInfo.has_value());
    QVERIFY(insertScheduleMeeting(
        services,
        QStringLiteral("class_intensive_times"),
        *noInfo,
        QStringLiteral("Wednesday"),
        QStringLiteral("5:00 PM")
        ));

    const int testingClass = createClass(
        services,
        QStringLiteral("Testing class"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Odysseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Wednesday"), QStringLiteral("5:00 PM"), QStringLiteral("5:45 PM")}
        }
        );
    QVERIFY(testingClass > 0);
    QVERIFY(executeSql(
        services,
        QStringLiteral("INSERT INTO testing_classes (class_id, room) VALUES (%1, 'A')")
            .arg(testingClass)
        ));

    QVERIFY(executeSql(services, QStringLiteral("PRAGMA foreign_keys = OFF")));
    QSqlQuery staleAssignment(dataService->databaseSession()->database());
    QVERIFY(staleAssignment.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id = ? WHERE class_id = ?"
        )));
    staleAssignment.addBindValue(999999);
    staleAssignment.addBindValue(staleTeacher);
    QVERIFY2(
        staleAssignment.exec(),
        qPrintable(staleAssignment.lastError().text())
        );
    QVERIFY(executeSql(services, QStringLiteral("PRAGMA foreign_keys = ON")));
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE class_times")));

    ApplicationServicesSubPrepScheduleSummaryPort port(services);
    const auto result = port.loadSummaries(summaryRequestFor(
        {
            classId(usable),
            classId(unassigned),
            classId(staleTeacher),
            classId(*noInfo),
            classId(testingClass)
        },
        {SubPrepWeekday::Wednesday},
        ScheduleViewMode::Intensive
        ));

    QVERIFY(result);
    QCOMPARE(result.value().classes.size(), std::size_t(1));
    QCOMPARE(result.value().classes.front().id, classId(usable));
    QCOMPARE(result.value().classes.front().meetingText, std::string("Wed 2pm"));
    QCOMPARE(
        result.value().classes.front().navigationLabel,
        utf8(QStringLiteral("Theseus • Int W 2:00"))
        );
    QCOMPARE(
        result.value().classes.front().navigationFirstDayOrder.rawValue(),
        2
        );
    QCOMPARE(
        result.value().classes.front().navigationFirstTimeOrder.rawValue(),
        14 * 60
        );
    QCOMPARE(result.value().teachers.size(), std::size_t(1));
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
rosterAggregateFailureKeepsLegacyZeroCountFallback()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(
        services,
        QStringLiteral("Roster fallback"),
        QStringLiteral("Roster fallback"),
        QStringLiteral("roster-summary")
        );
    QVERIFY(teacher > 0);
    const int classWithSchedule = createClass(
        services,
        QStringLiteral("Missing roster storage"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")}
        }
        );
    QVERIFY(classWithSchedule > 0);
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE roster_data")));

    ApplicationServicesSubPrepScheduleSummaryPort port(services);
    const auto result = port.loadSummaries(summaryRequestFor(
        {classId(classWithSchedule)},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));

    QVERIFY(result);
    QCOMPARE(result.value().classes.size(), std::size_t(1));
    QCOMPARE(result.value().classes.front().studentCount.rawValue(), std::size_t(0));
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
emptyScopeDoesNotReadAndUnavailableSessionFails()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE classes")));

    ApplicationServicesSubPrepScheduleSummaryPort port(services);
    const auto empty = port.loadSummaries(summaryRequestFor(
        {},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));
    QVERIFY(empty);
    QVERIFY(empty.value().classes.empty());
    QVERIFY(empty.value().teachers.empty());

    const auto queryFailure = port.loadSummaries(summaryRequestFor(
        {classId(1)},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));
    QVERIFY(!queryFailure);
    QCOMPARE(queryFailure.error().code, ErrorCode::Technical);

    services.closeDatabase();
    const auto unavailable = port.loadSummaries(summaryRequestFor(
        {classId(1)},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, ErrorCode::NotFound);
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
rejectsNoncanonicalIdsAndSupportsMaximumClassScope()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    ApplicationServicesSubPrepScheduleSummaryPort port(services);

    const auto alias = ClassId::fromString("01");
    QVERIFY(alias.has_value());
    const auto invalidId = port.loadSummaries(summaryRequestFor(
        {*alias},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));
    QVERIFY(!invalidId);
    QCOMPARE(invalidId.error().code, ErrorCode::InvalidInput);

    std::vector<ClassId> maximumScope;
    maximumScope.reserve(kSubPrepScheduleScopeMaxVisibleClasses);
    for (std::size_t index = 0;
         index < kSubPrepScheduleScopeMaxVisibleClasses;
         ++index)
    {
        maximumScope.push_back(classId(static_cast<int>(100'000 + index)));
    }
    const auto maximum = port.loadSummaries(summaryRequestFor(
        maximumScope,
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));
    QVERIFY(maximum);
    QVERIFY(maximum.value().classes.empty());
    QVERIFY(maximum.value().teachers.empty());
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
rejectsOversizedTeacherSummaryFields()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(
        services,
        QStringLiteral("Oversized teacher"),
        QStringLiteral("Oversized teacher"),
        QStringLiteral("oversized-summary")
        );
    QVERIFY(teacher > 0);
    const int selectedClass = createClass(
        services,
        QStringLiteral("Oversized summary class"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")}
        }
        );
    QVERIFY(selectedClass > 0);

    DataService* dataService = services.dataService();
    QVERIFY(dataService);
    QVERIFY(dataService->databaseSession());
    QSqlQuery oversizedNotes(dataService->databaseSession()->database());
    QVERIFY(oversizedNotes.prepare(QStringLiteral(
        "UPDATE teachers SET notes = ? WHERE id = ?"
        )));
    oversizedNotes.addBindValue(QString(
        static_cast<qsizetype>(kTeacherSummaryMaxNotesLength + 1),
        QLatin1Char('N')
        ));
    oversizedNotes.addBindValue(teacher);
    QVERIFY2(oversizedNotes.exec(), qPrintable(oversizedNotes.lastError().text()));

    ApplicationServicesSubPrepScheduleSummaryPort port(services);
    const auto result = port.loadSummaries(summaryRequestFor(
        {classId(selectedClass)},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));
    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Validation);
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
summaryPerClassMeetingOverflowSurfacesValidation()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(
        services,
        QStringLiteral("Meeting overflow"),
        QStringLiteral("Meeting overflow"),
        QStringLiteral("meeting-overflow")
        );
    QVERIFY(teacher > 0);
    const int classWithTooManyMeetings = createClass(
        services,
        QStringLiteral("Too many meetings"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {}
        );
    QVERIFY(classWithTooManyMeetings > 0);
    for (std::size_t index = 0;
         index <= kSubPrepScheduleSummaryMaxMeetingsPerClass;
         ++index)
    {
        QVERIFY(insertScheduleMeeting(
            services,
            QStringLiteral("class_times"),
            classWithTooManyMeetings,
            QStringLiteral("Monday"),
            QStringLiteral("9:00 AM")
            ));
    }

    ApplicationServicesSubPrepScheduleSummaryPort port(services);
    const auto result = port.loadSummaries(summaryRequestFor(
        {classId(classWithTooManyMeetings)},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));
    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Validation);
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
summaryAggregateMeetingOverflowSurfacesValidation()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(
        services,
        QStringLiteral("Aggregate overflow"),
        QStringLiteral("Aggregate overflow"),
        QStringLiteral("aggregate-overflow")
        );
    QVERIFY(teacher > 0);

    constexpr int ClassCount = 257;
    constexpr int MeetingsPerClass = 64;
    std::vector<ClassId> classIds;
    classIds.reserve(ClassCount);
    DataService* dataService = services.dataService();
    QVERIFY(dataService);
    QVERIFY(dataService->databaseSession());
    QSqlDatabase database = dataService->databaseSession()->database();
    QVERIFY(database.transaction());

    QSqlQuery classInsert(database);
    QVERIFY(classInsert.prepare(QStringLiteral(
        "INSERT INTO classes (name) VALUES (?)"
        )));
    QSqlQuery infoInsert(database);
    QVERIFY(infoInsert.prepare(QStringLiteral(
        "INSERT INTO class_info "
        "(class_id, teacher_id, class_grade, class_level) "
        "VALUES (?, ?, 'E4', 'Perseus')"
        )));
    QSqlQuery meetingInsert(database);
    QVERIFY(meetingInsert.prepare(QStringLiteral(
        "INSERT INTO class_times (class_id, day, start_time, end_time) "
        "VALUES (?, 'Monday', '9:00 AM', '9:45 AM')"
        )));

    std::size_t remaining = kSubPrepScheduleSummaryMaxMeetings + 1;
    for (int index = 0; index < ClassCount && remaining > 0; ++index)
    {
        classInsert.bindValue(
            0,
            QStringLiteral("Aggregate class %1").arg(index)
            );
        QVERIFY2(
            classInsert.exec(),
            qPrintable(classInsert.lastError().text())
            );
        const int legacyClassId = classInsert.lastInsertId().toInt();
        QVERIFY(legacyClassId > 0);
        infoInsert.bindValue(0, legacyClassId);
        infoInsert.bindValue(1, teacher);
        QVERIFY2(infoInsert.exec(), qPrintable(infoInsert.lastError().text()));
        classIds.push_back(ClassMngr::Next::Domain::ClassId::fromString(
            std::to_string(legacyClassId)
            ).value());

        const std::size_t count = std::min<std::size_t>(
            MeetingsPerClass,
            remaining
            );
        for (std::size_t index = 0; index < count; ++index)
        {
            meetingInsert.bindValue(0, legacyClassId);
            QVERIFY2(
                meetingInsert.exec(),
                qPrintable(meetingInsert.lastError().text())
                );
        }
        remaining -= count;
    }
    QCOMPARE(remaining, std::size_t(0));
    QVERIFY(database.commit());

    ApplicationServicesSubPrepScheduleSummaryPort port(services);
    const auto result = port.loadSummaries(summaryRequestFor(
        classIds,
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));
    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Validation);
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
allEmptyTeacherNamesKeepAdapterFallbacks()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    const int teacher = createTeacher(
        services,
        QStringLiteral("English Name"),
        QStringLiteral("Preferred Name"),
        QStringLiteral("empty-display-name")
        );
    QVERIFY(teacher > 0);

    const int selectedClass = createClass(
        services,
        QStringLiteral("Empty teacher display name"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Class notes"),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")}
        }
        );
    QVERIFY(selectedClass > 0);
    QVERIFY(updateTeacherDisplayNames(
        services,
        teacher,
        {},
        {},
        {},
        {}
        ));

    ApplicationServicesSubPrepScheduleSummaryPort summaryPort(services);
    const SubPrepScheduleSummaryQuery summaryQuery(summaryPort);
    const auto summary = summaryQuery.execute(summaryRequestFor(
        {classId(selectedClass)},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));
    QVERIFY(summary);
    const auto summaryTeacher = summary.value().teacherIndex().find(
        teacherId(teacher)
        );
    QVERIFY(summaryTeacher.has_value());
    QCOMPARE(summaryTeacher->displayName, std::string("N/A"));

    ApplicationServicesSubPrepClassDetailsPort detailsPort(services);
    const auto details = detailsPort.loadDetails(classId(selectedClass));
    QVERIFY(details);
    QVERIFY(details.value().teacherId == teacherId(teacher));
    QVERIFY(details.value().teacherDisplayName.empty());
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
selectedClassDetailsReadUsesOnlyScopedSessionData()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    const int teacher = createTeacher(
        services,
        QStringLiteral("English Name"),
        QStringLiteral("Preferred Name"),
        QStringLiteral("details")
        );
    QVERIFY(teacher > 0);
    QVERIFY(updateTeacherDisplayNames(
        services,
        teacher,
        QStringLiteral("  \uad8c\uc601\uc0dd  "),
        QStringLiteral("  English Name  "),
        QStringLiteral("  Romanized Name  "),
        QStringLiteral("  Preferred Name  ")
        ));
    const int selectedClass = createClass(
        services,
        QStringLiteral("Selected details"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Selected class notes"),
        QStringLiteral("#112233"),
        QStringLiteral("#AABBCC"),
        {
            {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")}
        }
        );
    QVERIFY(selectedClass > 0);

    // The selected details query must not load either schedule relation.
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE class_times")));
    QVERIFY(executeSql(
        services,
        QStringLiteral("DROP TABLE class_intensive_times")
        ));

    ApplicationServicesSubPrepClassDetailsPort port(services);
    const auto result = port.loadDetails(classId(selectedClass));

    QVERIFY(result);
    const auto& details = result.value();
    QVERIFY(details.classId == classId(selectedClass));
    QVERIFY(details.teacherId == teacherId(teacher));
    QCOMPARE(details.classNotes, std::string("Selected class notes"));
    QCOMPARE(details.teacherDisplayName, std::string("Preferred Name"));
    QCOMPARE(details.teacherFacilities.room, std::string("Room details"));
    QCOMPARE(details.teacherFacilities.wifiName, std::string("Network details"));
    QCOMPARE(
        details.teacherFacilities.wifiPassword,
        std::string("wifi-password-details")
        );
    QCOMPARE(details.teacherFacilities.internetType, std::string("LAN"));
    QCOMPARE(details.teacherFacilities.zoomId, std::string("zoom-details"));
    QCOMPARE(
        details.teacherFacilities.zoomPassword,
        std::string("zoom-password-details")
        );
    QCOMPARE(details.teacherFacilities.projectionType, std::string("Zoom"));
    QCOMPARE(
        details.teacherNotes,
        utf8(QString::fromUtf8(
            "\xEA\xB5\x90\xEC\x82\xAC \xEB\x85\xB8\xED\x8A\xB8 details"
            ))
        );

    const auto ownedCopy = details;
    services.closeDatabase();
    QVERIFY(ownedCopy == details);
    QCOMPARE(ownedCopy.teacherFacilities.room, std::string("Room details"));
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
selectedClassDetailsPreservesWideTeacherId()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    constexpr qlonglong wideTeacherId = 2'147'483'648LL;
    const auto expectedTeacherId = TeacherId::fromString("2147483648");
    QVERIFY(expectedTeacherId.has_value());

    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    QVERIFY(store->isOpen());
    const QSqlDatabase database = store->database();

    QSqlQuery insertTeacher(database);
    QVERIFY(insertTeacher.prepare(QStringLiteral(R"(
        INSERT INTO teachers (
            id, teacher_kr, teacher_en, preferred_romanization,
            preferred_name, room_number, birthday, phone_number,
            wifi_name, wifi_password, internet_type, zoom_id,
            zoom_password, projection_type, notes
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )")));
    insertTeacher.addBindValue(QVariant::fromValue<qlonglong>(wideTeacherId));
    insertTeacher.addBindValue(QStringLiteral("Wide Teacher Korean"));
    insertTeacher.addBindValue(QStringLiteral("Wide Teacher English"));
    insertTeacher.addBindValue(QStringLiteral("Wide Teacher Romanization"));
    insertTeacher.addBindValue(QStringLiteral("Wide Preferred Name"));
    insertTeacher.addBindValue(QStringLiteral("Wide Room 214"));
    insertTeacher.addBindValue(QStringLiteral("2000-01-01"));
    insertTeacher.addBindValue(QStringLiteral("555-214-7483"));
    insertTeacher.addBindValue(QStringLiteral("Wide Teacher Network"));
    insertTeacher.addBindValue(QStringLiteral("wide-wifi-password"));
    insertTeacher.addBindValue(QStringLiteral("LAN"));
    insertTeacher.addBindValue(QStringLiteral("wide-zoom-id"));
    insertTeacher.addBindValue(QStringLiteral("wide-zoom-password"));
    insertTeacher.addBindValue(QStringLiteral("Zoom"));
    insertTeacher.addBindValue(QStringLiteral("Wide teacher notes"));
    QVERIFY2(insertTeacher.exec(), qPrintable(insertTeacher.lastError().text()));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Wide teacher selected details")
        );
    QVERIFY(createdClass.has_value());
    const int selectedClass = *createdClass;
    QVERIFY(selectedClass > 0);
    QVERIFY(selectedClass <= std::numeric_limits<int>::max());

    QSqlQuery insertAssignment(database);
    QVERIFY(insertAssignment.prepare(QStringLiteral(R"(
        INSERT INTO class_info (
            class_id, teacher_id, class_grade, class_level,
            class_color, font_color, notes
        ) VALUES (?, ?, ?, ?, ?, ?, ?)
    )")));
    insertAssignment.addBindValue(selectedClass);
    insertAssignment.addBindValue(QVariant::fromValue<qlonglong>(wideTeacherId));
    insertAssignment.addBindValue(QStringLiteral("E4"));
    insertAssignment.addBindValue(QStringLiteral("Wide details"));
    insertAssignment.addBindValue(QStringLiteral("#112233"));
    insertAssignment.addBindValue(QStringLiteral("#AABBCC"));
    insertAssignment.addBindValue(QStringLiteral("Wide teacher class notes"));
    QVERIFY2(
        insertAssignment.exec(),
        qPrintable(insertAssignment.lastError().text())
        );

    QSqlQuery verifyStoredId(database);
    QVERIFY(verifyStoredId.prepare(QStringLiteral(R"(
        SELECT typeof(teachers.id), teachers.id,
               typeof(class_info.teacher_id), class_info.teacher_id
        FROM teachers
        INNER JOIN class_info ON class_info.teacher_id = teachers.id
        WHERE class_info.class_id = ?
    )")));
    verifyStoredId.addBindValue(selectedClass);
    QVERIFY2(verifyStoredId.exec(), qPrintable(verifyStoredId.lastError().text()));
    QVERIFY(verifyStoredId.next());
    QCOMPARE(verifyStoredId.value(0).toString(), QStringLiteral("integer"));
    QCOMPARE(verifyStoredId.value(1).toLongLong(), wideTeacherId);
    QCOMPARE(verifyStoredId.value(2).toString(), QStringLiteral("integer"));
    QCOMPARE(verifyStoredId.value(3).toLongLong(), wideTeacherId);
    QVERIFY(!verifyStoredId.next());

    ApplicationServicesSubPrepClassDetailsPort port(services);
    const auto result = port.loadDetails(classId(selectedClass));
    QVERIFY(result);

    const auto& details = result.value();
    QVERIFY(details.classId == classId(selectedClass));
    QVERIFY(details.teacherId.has_value());
    QVERIFY(*details.teacherId == *expectedTeacherId);
    QCOMPARE(details.teacherId->value(), std::string("2147483648"));
    QCOMPARE(details.classNotes, std::string("Wide teacher class notes"));
    QCOMPARE(details.teacherDisplayName, std::string("Wide Preferred Name"));
    QCOMPARE(details.teacherFacilities.room, std::string("Wide Room 214"));
    QCOMPARE(
        details.teacherFacilities.wifiName,
        std::string("Wide Teacher Network")
        );
    QCOMPARE(
        details.teacherFacilities.wifiPassword,
        std::string("wide-wifi-password")
        );
    QCOMPARE(details.teacherFacilities.internetType, std::string("LAN"));
    QCOMPARE(details.teacherFacilities.zoomId, std::string("wide-zoom-id"));
    QCOMPARE(
        details.teacherFacilities.zoomPassword,
        std::string("wide-zoom-password")
        );
    QCOMPARE(details.teacherFacilities.projectionType, std::string("Zoom"));
    QCOMPARE(details.teacherNotes, std::string("Wide teacher notes"));
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
selectedClassDetailsUnavailableSessionsAreTechnical()
{
    ApplicationServices unopenedServices;
    QVERIFY(unopenedServices.dataService());
    QVERIFY(!unopenedServices.dataService()->isOpen());

    ApplicationServicesSubPrepClassDetailsPort unopenedPort(unopenedServices);
    auto result = unopenedPort.loadDetails(classId(42));
    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Technical);

    ApplicationServices closedServices;
    const QString path = databasePath(m_directory);
    QVERIFY(closedServices.openDatabase(path));
    const int selectedClass = createClass(
        closedServices,
        QStringLiteral("Closed selected details"),
        -1,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Persisted class notes"),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {}
        );
    QVERIFY(selectedClass > 0);
    closedServices.closeDatabase();
    QVERIFY(!closedServices.hasOpenDatabase());
    QVERIFY(closedServices.dataService());
    QVERIFY(!closedServices.dataService()->isOpen());

    ApplicationServicesSubPrepClassDetailsPort closedPort(closedServices);
    result = closedPort.loadDetails(classId(selectedClass));
    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Technical);

    QVERIFY(closedServices.openDatabase(path));
    WorkspaceStore* const store = closedServices.workspaceStore();
    QVERIFY(store);
    QVERIFY(store->isOpen());
    ClassInfoRepository* const repository = store->classInfoRepository();
    QVERIFY(repository);
    const auto persisted = repository->loadSubPrepClassDetails(selectedClass);
    QVERIFY(persisted);
    QCOMPARE(persisted->classNotes, QStringLiteral("Persisted class notes"));
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
selectedClassDetailsSqlRepositoryFailureIsTechnical()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    QVERIFY(store->isOpen());
    const int selectedClass = createClass(
        services,
        QStringLiteral("Unreadable selected details"),
        -1,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Class notes"),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {}
        );
    QVERIFY(selectedClass > 0);

    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE class_info")));
    QSqlQuery confirmAbsent(store->database());
    QVERIFY(confirmAbsent.exec(QStringLiteral(
        "SELECT name FROM sqlite_master "
        "WHERE type = 'table' AND name = 'class_info'"
        )));
    QVERIFY(!confirmAbsent.next());

    ClassInfoRepository* const repository = store->classInfoRepository();
    QVERIFY(repository);
    const auto repositoryFailure =
        repository->loadSubPrepClassDetailsDetailed(selectedClass);
    QVERIFY(!repositoryFailure);
    QCOMPARE(
        repositoryFailure.error().kind,
        SubPrepClassDetailsReadFailure::Kind::Technical
        );

    ApplicationServicesSubPrepClassDetailsPort port(services);
    const auto result = port.loadDetails(classId(selectedClass));
    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Technical);
    QVERIFY(!result.error().message.empty());
    QVERIFY(result.error().message.find("Loading selected Sub Prep class details")
        != std::string::npos);
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
selectedClassDetailsUsesMissingTeacherFallbackAndBoundsFields()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    QVERIFY(store->isOpen());

    const int unassignedClass = createClass(
        services,
        QStringLiteral("Unassigned details"),
        -1,
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral("Unassigned class notes"),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {}
        );
    QVERIFY(unassignedClass > 0);

    const int missingTeacherClass = createClass(
        services,
        QStringLiteral("Stale teacher details"),
        -1,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Stale teacher notes"),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {}
        );
    QVERIFY(missingTeacherClass > 0);

    const auto classWithoutInfo = services.classService()->create(
        QStringLiteral("Missing class info")
        );
    QVERIFY(classWithoutInfo);

    QVERIFY(executeSql(services, QStringLiteral("PRAGMA foreign_keys = OFF")));
    QSqlQuery orphanAssignment(store->database());
    QVERIFY(orphanAssignment.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id = ? WHERE class_id = ?"
        )));
    orphanAssignment.addBindValue(999999);
    orphanAssignment.addBindValue(missingTeacherClass);
    QVERIFY2(
        orphanAssignment.exec(),
        qPrintable(orphanAssignment.lastError().text())
        );
    QVERIFY(executeSql(services, QStringLiteral("PRAGMA foreign_keys = ON")));

    ApplicationServicesSubPrepClassDetailsPort port(services);
    for (const int id : {unassignedClass, missingTeacherClass})
    {
        const auto result = port.loadDetails(classId(id));
        QVERIFY(result);
        QVERIFY(!result.value().teacherId.has_value());
        QVERIFY(result.value().teacherDisplayName.empty());
        QVERIFY(
            result.value().teacherFacilities
            == SelectedClassTeacherFacilities{}
            );
        QVERIFY(result.value().teacherNotes.empty());
    }

    const auto noInfo = port.loadDetails(classId(*classWithoutInfo));
    QVERIFY(noInfo);
    QCOMPARE(noInfo.value().classNotes, std::string());
    QVERIFY(!noInfo.value().teacherId.has_value());

    ClassInfoRepository* const repository = store->classInfoRepository();
    QVERIFY(repository);
    const auto missingClassOutcome =
        repository->loadSubPrepClassDetailsDetailed(999999);
    QVERIFY(missingClassOutcome);
    QVERIFY(missingClassOutcome->parentMissing);

    const auto missingClass = port.loadDetails(classId(999999));
    QVERIFY(!missingClass);
    QCOMPARE(missingClass.error().code, ErrorCode::NotFound);

    const auto alias = ClassId::fromString("01");
    QVERIFY(alias.has_value());
    const auto invalidId = port.loadDetails(*alias);
    QVERIFY(!invalidId);
    QCOMPARE(invalidId.error().code, ErrorCode::InvalidInput);

    const int oversizedTeacher = createTeacher(
        services,
        QStringLiteral("Oversized Room"),
        QStringLiteral("Oversized Room"),
        QStringLiteral("oversized")
        );
    QVERIFY(oversizedTeacher > 0);
    QSqlQuery oversizedRoom(store->database());
    QVERIFY(oversizedRoom.prepare(QStringLiteral(
        "UPDATE teachers SET room_number = ? WHERE id = ?"
        )));
    oversizedRoom.addBindValue(QString(
        static_cast<qsizetype>(kSelectedClassDetailsMaxTeacherRoomLength + 1),
        QLatin1Char('R')
        ));
    oversizedRoom.addBindValue(oversizedTeacher);
    QVERIFY2(oversizedRoom.exec(), qPrintable(oversizedRoom.lastError().text()));
    const int oversizedDetailsClass = createClass(
        services,
        QStringLiteral("Oversized detail field"),
        oversizedTeacher,
        QStringLiteral("E4"),
        QStringLiteral("Odysseus"),
        QStringLiteral("Valid class notes"),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {}
        );
    QVERIFY(oversizedDetailsClass > 0);
    const auto oversized = port.loadDetails(classId(oversizedDetailsClass));
    QVERIFY(!oversized);
    QCOMPARE(oversized.error().code, ErrorCode::Validation);
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
projectsSelectedClassesInRequestOrderAndCopiesFilteredSource()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    const int teacherOne = createTeacher(
        services,
        QStringLiteral("Jin Park"),
        QStringLiteral("Jin Park"),
        QStringLiteral("one")
        );
    const int teacherTwo = createTeacher(
        services,
        QStringLiteral("Alex Morgan"),
        QStringLiteral("Alex"),
        QStringLiteral("two")
        );
    QVERIFY(teacherOne > 0);
    QVERIFY(teacherTwo > 0);

    const int classZ = createClass(
        services,
        QStringLiteral("Z Class"),
        teacherOne,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Z class notes"),
        QStringLiteral("#112233"),
        QStringLiteral("#AABBCC"),
        {
            {QStringLiteral("Tuesday"), QStringLiteral("10:00 AM"), QStringLiteral("10:45 AM")},
            {QStringLiteral("Thursday"), QStringLiteral("1:00 PM"), QStringLiteral("1:45 PM")},
            {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")}
        },
        {
            {QStringLiteral("Wednesday"), QStringLiteral("2:00 PM"), QStringLiteral("2:45 PM")}
        }
        );
    const int classA = createClass(
        services,
        QStringLiteral("A Class"),
        teacherTwo,
        QStringLiteral("E5"),
        QStringLiteral("Apollo"),
        QStringLiteral("A class notes"),
        QStringLiteral("#223344"),
        QStringLiteral("#DDEEFF"),
        {
            {QStringLiteral("Friday"), QStringLiteral("8:00 AM"), QStringLiteral("8:45 AM")},
            {QStringLiteral("Monday"), QStringLiteral("11:00 AM"), QStringLiteral("11:45 AM")}
        },
        {
            {QStringLiteral("Monday"), QStringLiteral("3:00 PM"), QStringLiteral("3:45 PM")}
        }
        );
    const int classY = createClass(
        services,
        QStringLiteral("Y Class"),
        teacherOne,
        QStringLiteral("E6"),
        QStringLiteral("Helios"),
        QStringLiteral("Y class notes"),
        QStringLiteral("#445566"),
        QStringLiteral("#778899"),
        {
            {QStringLiteral("Tuesday"), QStringLiteral("2:00 PM"), QStringLiteral("2:45 PM")}
        }
        );
    const int unselectedClass = createClass(
        services,
        QStringLiteral("B Unselected"),
        teacherOne,
        QStringLiteral("E4"),
        QStringLiteral("Hercules"),
        QStringLiteral("outside selected scope"),
        QStringLiteral("#334455"),
        QStringLiteral("#FFFFFF"),
        {
            {QStringLiteral("Monday"), QStringLiteral("12:00 PM"), QStringLiteral("12:45 PM")}
        }
        );
    QVERIFY(classZ > 0);
    QVERIFY(classA > 0);
    QVERIFY(classY > 0);
    QVERIFY(unselectedClass > 0);

    QVERIFY(services.rosterService()->saveRoster(classZ, rosterWithTwoStudents()));
    QVERIFY(executeSql(
        services,
        QStringLiteral("DROP TABLE class_intensive_times")
        ));

    ApplicationServicesSubPrepPrintSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {classId(classZ), classId(classY), classId(classA)},
        {SubPrepWeekday::Monday, SubPrepWeekday::Tuesday},
        ScheduleViewMode::Regular
        ));

    QVERIFY(result);
    const SubPrepPrintSourceInput& source = result.value();
    QCOMPARE(source.classes.size(), std::size_t(3));
    QCOMPARE(source.classes[0].id, classId(classZ));
    QCOMPARE(source.classes[1].id, classId(classY));
    QCOMPARE(source.classes[2].id, classId(classA));
    QVERIFY(source.classes[0].teacherId == teacherId(teacherOne));
    QVERIFY(source.classes[1].teacherId == teacherId(teacherOne));
    QVERIFY(source.classes[2].teacherId == teacherId(teacherTwo));

    QCOMPARE(source.classes[0].grade, std::string("E4"));
    QCOMPARE(source.classes[0].level, std::string("Perseus"));
    QCOMPARE(source.classes[0].classNotes, std::string("Z class notes"));
    QCOMPARE(source.classes[0].classColor, std::string("#112233"));
    QCOMPARE(source.classes[0].fontColor, std::string("#AABBCC"));
    QCOMPARE(source.classes[0].studentCount.rawValue(), std::size_t(2));
    QCOMPARE(source.classes[0].meetings.size(), std::size_t(2));
    QCOMPARE(source.classes[0].meetings[0].weekday, SubPrepWeekday::Tuesday);
    QCOMPARE(source.classes[0].meetings[0].startTime, std::string("10:00 AM"));
    QCOMPARE(source.classes[0].meetings[0].endTime, std::string("10:45 AM"));
    QCOMPARE(source.classes[0].meetings[1].weekday, SubPrepWeekday::Monday);
    QCOMPARE(source.classes[0].meetings[1].startTime, std::string("9:00 AM"));
    QCOMPARE(source.classes[1].meetings.size(), std::size_t(1));
    QCOMPARE(source.classes[1].meetings[0].weekday, SubPrepWeekday::Tuesday);
    QCOMPARE(source.classes[1].meetings[0].startTime, std::string("2:00 PM"));
    QCOMPARE(source.classes[2].meetings.size(), std::size_t(1));
    QCOMPARE(source.classes[2].meetings[0].weekday, SubPrepWeekday::Monday);
    QCOMPARE(source.classes[2].meetings[0].startTime, std::string("11:00 AM"));

    QCOMPARE(source.teachers.size(), std::size_t(2));
    QCOMPARE(source.teachers[0].id, teacherId(teacherOne));
    QCOMPARE(source.teachers[0].englishName, std::string("Jin Park"));
    QCOMPARE(source.teachers[0].koreanName, utf8(QString::fromUtf8(
        "\xEA\xB0\x80\xEB\x82\x98"
        )));
    QCOMPARE(source.teachers[0].preferredName, std::string("Jin Park"));
    QCOMPARE(source.teachers[0].preferredRomanization, std::string("Jin Park"));
    QCOMPARE(source.teachers[0].room, std::string("Room one"));
    QCOMPARE(source.teachers[0].wifiName, std::string("Network one"));
    QCOMPARE(source.teachers[0].wifiPassword, std::string("wifi-password-one"));
    QCOMPARE(source.teachers[0].internetType, std::string("LAN"));
    QCOMPARE(source.teachers[0].zoomId, std::string("zoom-one"));
    QCOMPARE(source.teachers[0].zoomPassword, std::string("zoom-password-one"));
    QCOMPARE(source.teachers[0].projectionType, std::string("Zoom"));
    QCOMPARE(source.teachers[0].teacherNotes, utf8(QString::fromUtf8(
        "\xEA\xB5\x90\xEC\x82\xAC \xEB\x85\xB8\xED\x8A\xB8 one"
        )));
    QCOMPARE(source.teachers[1].id, teacherId(teacherTwo));
    QCOMPARE(source.teachers[1].englishName, std::string("Alex Morgan"));

    const SubPrepPrintSourceInput previous = source;
    services.closeDatabase();
    QVERIFY(source == previous);
    QCOMPARE(source.teachers[0].room, std::string("Room one"));
    QCOMPARE(source.classes[0].meetings[0].startTime, std::string("10:00 AM"));
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
batchesTeacherProfilesInFirstReferenceOrderWithoutUnassignedReads()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    const int firstTeacher = createTeacher(
        services,
        QStringLiteral("First teacher"),
        QStringLiteral("First teacher"),
        QStringLiteral("first")
        );
    const int secondTeacher = createTeacher(
        services,
        QStringLiteral("Second teacher"),
        QStringLiteral("Second teacher"),
        QStringLiteral("second")
        );
    QVERIFY(firstTeacher > 0);
    QVERIFY(secondTeacher > 0);

    const auto createScheduledClass = [&](
        const QString& name,
        const QString& level,
        const int assignedTeacherId,
        const QString& day,
        const QString& startTime,
        const QString& endTime
        )
    {
        return createClass(
            services,
            name,
            assignedTeacherId,
            QStringLiteral("E4"),
            level,
            QString(),
            QStringLiteral("#FFFFFF"),
            QStringLiteral("#000000"),
            {
                {day, startTime, endTime}
            }
            );
    };

    const int unassignedClass = createScheduledClass(
        QStringLiteral("Unassigned"),
        QStringLiteral("Theseus"),
        -1,
        QStringLiteral("Monday"),
        QStringLiteral("8:00 AM"),
        QStringLiteral("8:45 AM")
        );
    const int secondTeacherFirstClass = createScheduledClass(
        QStringLiteral("Second teacher first"),
        QStringLiteral("Perseus"),
        secondTeacher,
        QStringLiteral("Tuesday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:45 AM")
        );
    const int firstTeacherClass = createScheduledClass(
        QStringLiteral("First teacher second"),
        QStringLiteral("Odysseus"),
        firstTeacher,
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:45 AM")
        );
    const int secondTeacherRepeatedClass = createScheduledClass(
        QStringLiteral("Second teacher repeated"),
        QStringLiteral("Hercules"),
        secondTeacher,
        QStringLiteral("Friday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:45 AM")
        );
    QVERIFY(unassignedClass > 0);
    QVERIFY(secondTeacherFirstClass > 0);
    QVERIFY(firstTeacherClass > 0);
    QVERIFY(secondTeacherRepeatedClass > 0);

    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    QVERIFY(store->isOpen());
    TeacherRepository* const teacherRepository = store->teacherRepository();
    QVERIFY(teacherRepository);
    ApplicationServicesSubPrepPrintSourcePort port(services);

    const auto unassignedResult = port.loadSource(requestFor(
        {classId(unassignedClass)},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));
    QVERIFY(unassignedResult);
    QVERIFY(unassignedResult.value().classes.empty());
    QVERIFY(unassignedResult.value().teachers.empty());
    const auto& emptyMetrics =
        teacherRepository->subPrepPrintSourceTeacherProfileBatchReadMetrics();
    QCOMPARE(emptyMetrics.callCount, 0);
    QCOMPARE(emptyMetrics.requestedTeacherCount, 0);
    QCOMPARE(emptyMetrics.statementCount, 0);
    QCOMPARE(emptyMetrics.fallbackSingleReadCount, 0);

    const auto result = port.loadSource(requestFor(
        {
            classId(secondTeacherFirstClass),
            classId(firstTeacherClass),
            classId(secondTeacherRepeatedClass)
        },
        {
            SubPrepWeekday::Monday,
            SubPrepWeekday::Tuesday,
            SubPrepWeekday::Friday
        },
        ScheduleViewMode::Regular
        ));

    QVERIFY(result);
    const SubPrepPrintSourceInput& source = result.value();
    QCOMPARE(source.classes.size(), std::size_t(3));
    QCOMPARE(source.classes[0].id, classId(secondTeacherFirstClass));
    QCOMPARE(source.classes[1].id, classId(firstTeacherClass));
    QCOMPARE(source.classes[2].id, classId(secondTeacherRepeatedClass));
    QCOMPARE(source.teachers.size(), std::size_t(2));
    QCOMPARE(source.teachers[0].id, teacherId(secondTeacher));
    QCOMPARE(source.teachers[1].id, teacherId(firstTeacher));

    const auto& metrics =
        teacherRepository->subPrepPrintSourceTeacherProfileBatchReadMetrics();
    QCOMPARE(metrics.callCount, 1);
    QCOMPARE(metrics.requestedTeacherCount, 2);
    QCOMPARE(metrics.statementCount, 1);
    QCOMPARE(metrics.fallbackSingleReadCount, 0);
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
printSourcePreservesWideTeacherIdAndCompleteProfile()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    const int temporaryTeacher = createTeacher(
        services,
        QStringLiteral("Temp print"),
        QStringLiteral("Temp print"),
        QStringLiteral("temporary-print")
        );
    QVERIFY(temporaryTeacher > 0);

    const int selectedClass = createClass(
        services,
        QStringLiteral("Wide teacher print source"),
        temporaryTeacher,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Wide print class notes"),
        QStringLiteral("#112233"),
        QStringLiteral("#AABBCC"),
        {
            {
                QStringLiteral("Monday"),
                QStringLiteral("9:00 AM"),
                QStringLiteral("9:45 AM")
            }
        }
        );
    QVERIFY(selectedClass > 0);
    QVERIFY(selectedClass <= std::numeric_limits<int>::max());

    constexpr qlonglong wideTeacherIdValue = 2'147'483'648LL;
    const auto expectedTeacherId = TeacherId::fromString("2147483648");
    QVERIFY(expectedTeacherId.has_value());

    DataService* const dataService = services.dataService();
    QVERIFY(dataService);
    QVERIFY(dataService->databaseSession());
    const QSqlDatabase database = dataService->databaseSession()->database();

    QSqlQuery insertTeacher(database);
    QVERIFY(insertTeacher.prepare(QStringLiteral(R"(
        INSERT INTO teachers (
            id, teacher_kr, teacher_en, preferred_romanization,
            preferred_name, room_number, birthday, phone_number,
            wifi_name, wifi_password, internet_type, zoom_id,
            zoom_password, projection_type, notes
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )")));
    insertTeacher.addBindValue(
        QVariant::fromValue<qlonglong>(wideTeacherIdValue)
        );
    insertTeacher.addBindValue(QStringLiteral("Wide Print Teacher Korean"));
    insertTeacher.addBindValue(QStringLiteral("Wide Print Teacher English"));
    insertTeacher.addBindValue(QStringLiteral("Wide Print Romanization"));
    insertTeacher.addBindValue(QStringLiteral("Wide Print Preferred Name"));
    insertTeacher.addBindValue(QStringLiteral("Wide Print Room 214"));
    insertTeacher.addBindValue(QStringLiteral("2000-01-01"));
    insertTeacher.addBindValue(QStringLiteral("555-214-7483"));
    insertTeacher.addBindValue(QStringLiteral("Wide Print Network"));
    insertTeacher.addBindValue(QStringLiteral("wide-print-wifi-password"));
    insertTeacher.addBindValue(QStringLiteral("LAN"));
    insertTeacher.addBindValue(QStringLiteral("wide-print-zoom-id"));
    insertTeacher.addBindValue(QStringLiteral("wide-print-zoom-password"));
    insertTeacher.addBindValue(QStringLiteral("Zoom"));
    insertTeacher.addBindValue(QStringLiteral("Wide print teacher notes"));
    QVERIFY2(insertTeacher.exec(), qPrintable(insertTeacher.lastError().text()));

    QSqlQuery assignTeacher(database);
    QVERIFY(assignTeacher.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id = ? WHERE class_id = ?"
        )));
    assignTeacher.addBindValue(
        QVariant::fromValue<qlonglong>(wideTeacherIdValue)
        );
    assignTeacher.addBindValue(selectedClass);
    QVERIFY2(assignTeacher.exec(), qPrintable(assignTeacher.lastError().text()));
    QCOMPARE(assignTeacher.numRowsAffected(), 1);

    QSqlQuery verifyStoredIds(database);
    QVERIFY(verifyStoredIds.prepare(QStringLiteral(R"(
        SELECT typeof(teachers.id), teachers.id,
               typeof(class_info.teacher_id), class_info.teacher_id
        FROM teachers
        INNER JOIN class_info ON class_info.teacher_id = teachers.id
        WHERE class_info.class_id = ?
    )")));
    verifyStoredIds.addBindValue(selectedClass);
    QVERIFY2(
        verifyStoredIds.exec(),
        qPrintable(verifyStoredIds.lastError().text())
        );
    QVERIFY(verifyStoredIds.next());
    QCOMPARE(verifyStoredIds.value(0).toString(), QStringLiteral("integer"));
    QCOMPARE(verifyStoredIds.value(1).toLongLong(), wideTeacherIdValue);
    QCOMPARE(verifyStoredIds.value(2).toString(), QStringLiteral("integer"));
    QCOMPARE(verifyStoredIds.value(3).toLongLong(), wideTeacherIdValue);
    QVERIFY(!verifyStoredIds.next());

    ApplicationServicesSubPrepPrintSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {classId(selectedClass)},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));

    QVERIFY(result);
    const SubPrepPrintSourceInput& source = result.value();
    QCOMPARE(source.classes.size(), std::size_t(1));
    QCOMPARE(source.classes[0].id, classId(selectedClass));
    QVERIFY(source.classes[0].teacherId.has_value());
    QVERIFY(*source.classes[0].teacherId == *expectedTeacherId);
    QCOMPARE(source.classes[0].teacherId->value(), std::string("2147483648"));
    QCOMPARE(source.classes[0].grade, std::string("E4"));
    QCOMPARE(source.classes[0].level, std::string("Perseus"));
    QCOMPARE(source.classes[0].classNotes, std::string("Wide print class notes"));
    QCOMPARE(source.teachers.size(), std::size_t(1));

    const SubPrepPrintTeacher& teacher = source.teachers[0];
    QVERIFY(teacher.id == *expectedTeacherId);
    QCOMPARE(teacher.id.value(), std::string("2147483648"));
    QCOMPARE(teacher.englishName, std::string("Wide Print Teacher English"));
    QCOMPARE(teacher.koreanName, std::string("Wide Print Teacher Korean"));
    QCOMPARE(teacher.preferredName, std::string("Wide Print Preferred Name"));
    QCOMPARE(
        teacher.preferredRomanization,
        std::string("Wide Print Romanization")
        );
    QCOMPARE(teacher.room, std::string("Wide Print Room 214"));
    QCOMPARE(teacher.wifiName, std::string("Wide Print Network"));
    QCOMPARE(teacher.wifiPassword, std::string("wide-print-wifi-password"));
    QCOMPARE(teacher.internetType, std::string("LAN"));
    QCOMPARE(teacher.zoomId, std::string("wide-print-zoom-id"));
    QCOMPARE(teacher.zoomPassword, std::string("wide-print-zoom-password"));
    QCOMPARE(teacher.projectionType, std::string("Zoom"));
    QCOMPARE(teacher.teacherNotes, std::string("Wide print teacher notes"));

    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    QVERIFY(store->isOpen());
    TeacherRepository* const teacherRepository = store->teacherRepository();
    QVERIFY(teacherRepository);
    const auto& metrics =
        teacherRepository->subPrepPrintSourceTeacherProfileBatchReadMetrics();
    QCOMPARE(metrics.callCount, 1);
    QCOMPARE(metrics.requestedTeacherCount, 1);
    QCOMPARE(metrics.statementCount, 1);
    QCOMPARE(metrics.fallbackSingleReadCount, 0);
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
selectsIntensiveTimesAndOmitsClassesOutsideSelectedDays()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(
        services,
        QStringLiteral("Taylor"),
        QStringLiteral("Taylor"),
        QStringLiteral("mode")
        );
    QVERIFY(teacher > 0);

    const int classOne = createClass(
        services,
        QStringLiteral("First"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")}
        },
        {
            {QStringLiteral("Wednesday"), QStringLiteral("1:00 PM"), QStringLiteral("1:45 PM")},
            {QStringLiteral("Monday"), QStringLiteral("2:00 PM"), QStringLiteral("2:45 PM")}
        }
        );
    const int classTwo = createClass(
        services,
        QStringLiteral("Second"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Tuesday"), QStringLiteral("10:00 AM"), QStringLiteral("10:45 AM")}
        },
        {
            {QStringLiteral("Friday"), QStringLiteral("3:00 PM"), QStringLiteral("3:45 PM")}
        }
        );
    QVERIFY(classOne > 0);
    QVERIFY(classTwo > 0);
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE class_times")));

    ApplicationServicesSubPrepPrintSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {classId(classOne), classId(classTwo)},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Intensive
        ));

    QVERIFY(result);
    QCOMPARE(result.value().classes.size(), std::size_t(1));
    QCOMPARE(result.value().classes[0].id, classId(classOne));
    QCOMPARE(result.value().classes[0].meetings.size(), std::size_t(1));
    QCOMPARE(result.value().classes[0].meetings[0].weekday, SubPrepWeekday::Monday);
    QCOMPARE(result.value().classes[0].meetings[0].startTime, std::string("2:00 PM"));
    QCOMPARE(result.value().teachers.size(), std::size_t(1));
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
batchesRosterCountsAfterPrintScopeAndPreservesLegacyMetrics()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(
        services,
        QStringLiteral("Roster batch"),
        QStringLiteral("Roster batch"),
        QStringLiteral("roster-batch")
        );
    QVERIFY(teacher > 0);

    const int noNameColumnsClass = createClass(
        services,
        QStringLiteral("No name columns"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("8:00 AM"), QStringLiteral("8:45 AM")}
        }
        );
    const int populatedClass = createClass(
        services,
        QStringLiteral("Sparse populated"),
        teacher,
        QStringLiteral("E5"),
        QStringLiteral("Apollo"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")}
        }
        );
    const int outOfScopeClass = createClass(
        services,
        QStringLiteral("Out of scope"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Odysseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Tuesday"), QStringLiteral("11:00 AM"), QStringLiteral("11:45 AM")}
        }
        );
    QVERIFY(noNameColumnsClass > 0);
    QVERIFY(populatedClass > 0);
    QVERIFY(outOfScopeClass > 0);

    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    QVERIFY(store->isOpen());
    RosterRepository* const rosterRepository = store->rosterRepository();
    QVERIFY(rosterRepository);

    Roster noNameColumnsRoster;
    noNameColumnsRoster.columns = {QStringLiteral("Notes")};
    noNameColumnsRoster.rows = {
        QStringList{QString()},
        QStringList{QStringLiteral("No name data")}
    };
    QVERIFY(rosterRepository->saveRoster(
        noNameColumnsClass,
        noNameColumnsRoster
        ));

    Roster sparseRoster;
    sparseRoster.columns = {
        QStringLiteral("English"),
        QStringLiteral("English"),
        QStringLiteral("Korean"),
        QStringLiteral("english"),
        QStringLiteral("Notes")
    };
    for (int rowIndex = 0; rowIndex < 28; ++rowIndex)
    {
        sparseRoster.rows.append(QStringList{
            QString(), QString(), QString(), QString(), QString()
        });
    }
    sparseRoster.rows[0][0] = QString::fromUtf8("\xC2\xA0");
    sparseRoster.rows[1][1] = QStringLiteral("Duplicate English ignored");
    sparseRoster.rows[1][2] = QString::fromUtf8("\xE2\x80\x83");
    sparseRoster.rows[2][2] = QString::fromUtf8("\xED\x95\x99\xEC\x83\x9D");
    sparseRoster.rows[27][0] = QStringLiteral("Last sparse row");
    QVERIFY(rosterRepository->saveRoster(
        populatedClass,
        sparseRoster
        ));
    StartupProfiler profiler;
    StartupProfiler::activate(&profiler);
    StartupProfiler::setSubPrepDiagnosticsActive(true);

    ApplicationServicesSubPrepPrintSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {
            classId(noNameColumnsClass),
            classId(populatedClass),
            classId(outOfScopeClass)
        },
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));

    const QList<StartupProfilingEvent> events = profiler.events();
    StartupProfiler::setSubPrepDiagnosticsActive(false);
    StartupProfiler::activate(nullptr);

    QVERIFY(result);
    QCOMPARE(result.value().classes.size(), std::size_t(2));
    QCOMPARE(result.value().classes[0].id, classId(noNameColumnsClass));
    QCOMPARE(result.value().classes[1].id, classId(populatedClass));
    QCOMPARE(
        result.value().classes[0].studentCount.rawValue(),
        std::size_t(0)
        );
    QCOMPARE(
        result.value().classes[1].studentCount.rawValue(),
        std::size_t(2)
        );
    QCOMPARE(result.value().teachers.size(), std::size_t(1));

    const auto& metrics =
        rosterRepository->subPrepStudentCountBatchReadMetrics();
    QCOMPARE(metrics.callCount, 1);
    QCOMPARE(metrics.requestedClassCount, 2);
    QCOMPARE(metrics.columnStatementCount, 1);
    QCOMPARE(metrics.dataStatementCount, 1);
    QCOMPARE(metrics.fallbackClassReadCount, 0);

    QCOMPARE(events.size(), 2);
    QCOMPARE(events[0].name, QStringLiteral("sub-prep-roster-query"));
    QCOMPARE(
        events[0].detail,
        QStringLiteral(
            "classId=%1; columns=1; rows=2; cells=2; returnedStudents=0"
            ).arg(noNameColumnsClass)
        );
    QCOMPARE(events[1].name, QStringLiteral("sub-prep-roster-query"));
    QCOMPARE(
        events[1].detail,
        QStringLiteral(
            "classId=%1; columns=5; rows=28; cells=140; returnedStudents=2"
            ).arg(populatedClass)
        );
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
subPrepCountDoesNotAliasFractionalRosterColumns()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(
        services,
        QStringLiteral("Fractional roster"),
        QStringLiteral("Fractional roster"),
        QStringLiteral("fractional-roster")
        );
    QVERIFY(teacher > 0);

    const int fractionalClass = createClass(
        services,
        QStringLiteral("Fractional only name"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QString(),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("8:00 AM"), QStringLiteral("8:45 AM")}
        }
        );
    const int ordinaryClass = createClass(
        services,
        QStringLiteral("Ordinary names"),
        teacher,
        QStringLiteral("E5"),
        QStringLiteral("Apollo"),
        QString(),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")}
        }
        );
    const int emptyRosterClass = createClass(
        services,
        QStringLiteral("Empty roster"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Odysseus"),
        QString(),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("10:00 AM"), QStringLiteral("10:45 AM")}
        }
        );
    QVERIFY(fractionalClass > 0);
    QVERIFY(ordinaryClass > 0);
    QVERIFY(emptyRosterClass > 0);

    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    QVERIFY(store->isOpen());
    RosterRepository* const rosterRepository = store->rosterRepository();
    QVERIFY(rosterRepository);
    Roster fractionalRoster;
    fractionalRoster.columns = {
        QStringLiteral("English"),
        QStringLiteral("Korean")
    };
    fractionalRoster.rows = {{QString(), QString()}};
    QVERIFY(rosterRepository->saveRoster(fractionalClass, fractionalRoster));
    QVERIFY(rosterRepository->saveRoster(
        ordinaryClass,
        rosterWithTwoStudents()
        ));
    QVERIFY(rosterRepository->saveRoster(emptyRosterClass, Roster{}));

    DataService* const dataService = services.dataService();
    QVERIFY(dataService);
    QVERIFY(dataService->databaseSession());
    QSqlDatabase database = dataService->databaseSession()->database();
    QSqlQuery insertCell(database);
    QVERIFY(insertCell.prepare(QStringLiteral(
        "INSERT INTO roster_data (class_id, row_index, col_index, value) "
        "VALUES (?, ?, ?, ?)"
        )));
    insertCell.addBindValue(fractionalClass);
    insertCell.addBindValue(0);
    insertCell.addBindValue(0);
    insertCell.addBindValue(QString());
    QVERIFY2(insertCell.exec(), qPrintable(insertCell.lastError().text()));
    insertCell.finish();
    QVERIFY(insertCell.prepare(QStringLiteral(
        "INSERT INTO roster_data (class_id, row_index, col_index, value) "
        "VALUES (?, ?, ?, ?)"
        )));
    insertCell.addBindValue(fractionalClass);
    insertCell.addBindValue(0);
    insertCell.addBindValue(QVariant::fromValue(0.25));
    insertCell.addBindValue(QStringLiteral("Fractional false student"));
    QVERIFY2(insertCell.exec(), qPrintable(insertCell.lastError().text()));
    insertCell.finish();

    QSqlQuery storedCells(database);
    QVERIFY(storedCells.prepare(QStringLiteral(
        "SELECT col_index, typeof(col_index) AS storage_type, value "
        "FROM roster_data WHERE class_id=? ORDER BY col_index"
        )));
    storedCells.addBindValue(fractionalClass);
    QVERIFY2(storedCells.exec(), qPrintable(storedCells.lastError().text()));
    QVERIFY(storedCells.next());
    QCOMPARE(storedCells.value("col_index").toInt(), 0);
    QCOMPARE(storedCells.value("storage_type").toString(), QStringLiteral("integer"));
    QVERIFY(storedCells.value("value").toString().isEmpty());
    QVERIFY(storedCells.next());
    QCOMPARE(storedCells.value("col_index").toDouble(), 0.25);
    QCOMPARE(storedCells.value("storage_type").toString(), QStringLiteral("real"));
    QCOMPARE(
        storedCells.value("value").toString(),
        QStringLiteral("Fractional false student")
        );
    QVERIFY(!storedCells.next());
    storedCells.finish();

    ApplicationServicesSubPrepPrintSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {
            classId(fractionalClass),
            classId(ordinaryClass),
            classId(emptyRosterClass)
        },
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));

    QVERIFY(result);
    QCOMPARE(result.value().classes.size(), std::size_t(3));
    std::optional<std::size_t> fractionalStudentCount;
    std::optional<std::size_t> ordinaryStudentCount;
    std::optional<std::size_t> emptyRosterStudentCount;
    for (const auto& classRecord : result.value().classes)
    {
        if (classRecord.id == classId(fractionalClass))
        {
            fractionalStudentCount = classRecord.studentCount.rawValue();
        }
        else if (classRecord.id == classId(ordinaryClass))
        {
            ordinaryStudentCount = classRecord.studentCount.rawValue();
        }
        else if (classRecord.id == classId(emptyRosterClass))
        {
            emptyRosterStudentCount = classRecord.studentCount.rawValue();
        }
    }
    QVERIFY(fractionalStudentCount.has_value());
    QVERIFY(ordinaryStudentCount.has_value());
    QVERIFY(emptyRosterStudentCount.has_value());
    QCOMPARE(*fractionalStudentCount, std::size_t(0));
    QCOMPARE(*ordinaryStudentCount, std::size_t(2));
    QCOMPARE(*emptyRosterStudentCount, std::size_t(0));

    const auto& metrics =
        rosterRepository->subPrepStudentCountBatchReadMetrics();
    QCOMPARE(metrics.callCount, 1);
    QCOMPARE(metrics.requestedClassCount, 3);
    QCOMPARE(metrics.columnStatementCount, 1);
    QCOMPARE(metrics.dataStatementCount, 1);
    QCOMPARE(metrics.fallbackClassReadCount, 3);
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
subPrepCountDoesNotAliasFractionalRosterRows()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(
        services,
        QStringLiteral("Fractional row"),
        QStringLiteral("Fractional row"),
        QStringLiteral("fractional-row")
        );
    QVERIFY(teacher > 0);

    const auto createScheduledClass = [&](
        const QString& name,
        const QString& grade,
        const QString& level,
        const QString& startTime,
        const QString& endTime
        )
    {
        return createClass(
            services,
            name,
            teacher,
            grade,
            level,
            QString(),
            QStringLiteral("#FFFFFF"),
            QStringLiteral("#000000"),
            {
                {
                    QStringLiteral("Monday"),
                    startTime,
                    endTime
                }
            }
            );
    };
    const int fractionalClass = createScheduledClass(
        QStringLiteral("Fractional row index"),
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("8:00 AM"),
        QStringLiteral("8:45 AM")
        );
    const int sparseClass = createScheduledClass(
        QStringLiteral("Sparse integer rows"),
        QStringLiteral("E5"),
        QStringLiteral("Apollo"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:45 AM")
        );
    const int emptyRosterClass = createScheduledClass(
        QStringLiteral("Empty roster"),
        QStringLiteral("E4"),
        QStringLiteral("Odysseus"),
        QStringLiteral("10:00 AM"),
        QStringLiteral("10:45 AM")
        );
    QVERIFY(fractionalClass > 0);
    QVERIFY(sparseClass > 0);
    QVERIFY(emptyRosterClass > 0);

    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    QVERIFY(store->isOpen());
    RosterRepository* const rosterRepository = store->rosterRepository();
    QVERIFY(rosterRepository);
    Roster fractionalRoster;
    fractionalRoster.columns = {
        QStringLiteral("English"),
        QStringLiteral("Korean")
    };
    fractionalRoster.rows = {{QString(), QString()}};
    QVERIFY(rosterRepository->saveRoster(fractionalClass, fractionalRoster));

    Roster sparseRoster;
    sparseRoster.columns = {
        QStringLiteral("English"),
        QStringLiteral("Korean")
    };
    for (int rowIndex = 0; rowIndex < 6; ++rowIndex)
    {
        sparseRoster.rows.append(QStringList{
            QString(),
            QString()
        });
    }
    sparseRoster.rows[2][0] = QStringLiteral("Sparse student one");
    sparseRoster.rows[5][1] = QStringLiteral("Sparse student two");
    QVERIFY(rosterRepository->saveRoster(sparseClass, sparseRoster));
    QVERIFY(rosterRepository->saveRoster(emptyRosterClass, Roster{}));

    DataService* const dataService = services.dataService();
    QVERIFY(dataService);
    QVERIFY(dataService->databaseSession());
    QSqlDatabase database = dataService->databaseSession()->database();
    QSqlQuery insertCell(database);
    QVERIFY(insertCell.prepare(QStringLiteral(
        "INSERT INTO roster_data (class_id, row_index, col_index, value) "
        "VALUES (?, ?, ?, ?)"
        )));
    insertCell.addBindValue(fractionalClass);
    insertCell.addBindValue(QVariant::fromValue(0.25));
    insertCell.addBindValue(0);
    insertCell.addBindValue(QStringLiteral("Fractional false student"));
    QVERIFY2(insertCell.exec(), qPrintable(insertCell.lastError().text()));
    insertCell.finish();

    QSqlQuery storedRow(database);
    QVERIFY(storedRow.prepare(QStringLiteral(
        "SELECT row_index, typeof(row_index) AS storage_type, value "
        "FROM roster_data WHERE class_id=? AND row_index=0.25"
        )));
    storedRow.addBindValue(fractionalClass);
    QVERIFY2(storedRow.exec(), qPrintable(storedRow.lastError().text()));
    QVERIFY(storedRow.next());
    QCOMPARE(storedRow.value("row_index").toDouble(), 0.25);
    QCOMPARE(storedRow.value("storage_type").toString(), QStringLiteral("real"));
    QCOMPARE(
        storedRow.value("value").toString(),
        QStringLiteral("Fractional false student")
        );
    QVERIFY(!storedRow.next());
    storedRow.finish();

    ApplicationServicesSubPrepPrintSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {
            classId(fractionalClass),
            classId(sparseClass),
            classId(emptyRosterClass)
        },
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));

    QVERIFY(result);
    QCOMPARE(result.value().classes.size(), std::size_t(3));
    std::optional<std::size_t> fractionalStudentCount;
    std::optional<std::size_t> sparseStudentCount;
    std::optional<std::size_t> emptyRosterStudentCount;
    for (const auto& classRecord : result.value().classes)
    {
        if (classRecord.id == classId(fractionalClass))
        {
            fractionalStudentCount = classRecord.studentCount.rawValue();
        }
        else if (classRecord.id == classId(sparseClass))
        {
            sparseStudentCount = classRecord.studentCount.rawValue();
        }
        else if (classRecord.id == classId(emptyRosterClass))
        {
            emptyRosterStudentCount = classRecord.studentCount.rawValue();
        }
    }
    QVERIFY(fractionalStudentCount.has_value());
    QVERIFY(sparseStudentCount.has_value());
    QVERIFY(emptyRosterStudentCount.has_value());
    QCOMPARE(*fractionalStudentCount, std::size_t(0));
    QCOMPARE(*sparseStudentCount, std::size_t(2));
    QCOMPARE(*emptyRosterStudentCount, std::size_t(0));

    const auto& metrics =
        rosterRepository->subPrepStudentCountBatchReadMetrics();
    QCOMPARE(metrics.callCount, 1);
    QCOMPARE(metrics.requestedClassCount, 3);
    QCOMPARE(metrics.columnStatementCount, 1);
    QCOMPARE(metrics.dataStatementCount, 1);
    QCOMPARE(metrics.fallbackClassReadCount, 3);
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
batchesRosterCountsWideRowIndicesAndSaturatesMetrics()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(
        services,
        QStringLiteral("Wide roster"),
        QStringLiteral("Wide roster"),
        QStringLiteral("wide-roster")
        );
    QVERIFY(teacher > 0);
    const int populatedClass = createClass(
        services,
        QStringLiteral("Wide row identities"),
        teacher,
        QStringLiteral("E5"),
        QStringLiteral("Apollo"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")}
        }
        );
    QVERIFY(populatedClass > 0);

    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    QVERIFY(store->isOpen());
    RosterRepository* const rosterRepository = store->rosterRepository();
    QVERIFY(rosterRepository);
    Roster roster;
    roster.columns = {QStringLiteral("English"), QStringLiteral("Korean")};
    QVERIFY(rosterRepository->saveRoster(populatedClass, roster));

    const qlonglong intMax = std::numeric_limits<int>::max();
    const std::vector<qlonglong> rowIndices{
        intMax,
        intMax + 1,
        intMax + 2,
        std::numeric_limits<qlonglong>::max()
    };
    const std::vector<QStringList> names{
        {QStringLiteral("At INT_MAX"), QString::fromUtf8("\xED\x95\x9C\xEA\xB8\x80")},
        {QStringLiteral("Above INT_MAX"), QString()},
        {QString(), QString::fromUtf8("\xED\x95\x99\xEC\x83\x9D")},
        {QStringLiteral("At qlonglong max"), QString()}
    };
    DataService* const dataService = services.dataService();
    QVERIFY(dataService);
    QVERIFY(dataService->databaseSession());
    QSqlDatabase database = dataService->databaseSession()->database();
    QSqlQuery insertCell(database);
    QVERIFY(insertCell.prepare(QStringLiteral(
        "INSERT INTO roster_data (class_id, row_index, col_index, value) "
        "VALUES (?, ?, ?, ?)"
        )));
    for (std::size_t row = 0; row < rowIndices.size(); ++row)
    {
        for (int column = 0; column < 2; ++column)
        {
            insertCell.bindValue(0, populatedClass);
            insertCell.bindValue(1, QVariant::fromValue(rowIndices[row]));
            insertCell.bindValue(2, column);
            insertCell.bindValue(
                3,
                names[row].at(column)
                );
            QVERIFY(insertCell.exec());
        }
    }
    insertCell.finish();

    QSqlQuery storedRows(database);
    QVERIFY(storedRows.exec(QStringLiteral(
        "SELECT DISTINCT row_index, typeof(row_index) AS storage_type "
        "FROM roster_data WHERE class_id=%1 ORDER BY row_index"
        ).arg(populatedClass)));
    for (const qlonglong expectedIndex : rowIndices)
    {
        QVERIFY(storedRows.next());
        bool rowIndexValid = false;
        QCOMPARE(storedRows.value("row_index").toLongLong(&rowIndexValid), expectedIndex);
        QVERIFY(rowIndexValid);
        QCOMPARE(storedRows.value("storage_type").toString(), QStringLiteral("integer"));
    }
    QVERIFY(!storedRows.next());
    storedRows.finish();

    StartupProfiler profiler;
    StartupProfiler::activate(&profiler);
    StartupProfiler::setSubPrepDiagnosticsActive(true);

    ApplicationServicesSubPrepPrintSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {classId(populatedClass)},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));

    const QList<StartupProfilingEvent> events = profiler.events();
    StartupProfiler::setSubPrepDiagnosticsActive(false);
    StartupProfiler::activate(nullptr);

    QVERIFY(result);
    QCOMPARE(result.value().classes.size(), std::size_t(1));
    QCOMPARE(result.value().classes[0].id, classId(populatedClass));
    QCOMPARE(result.value().classes[0].studentCount.rawValue(), std::size_t(4));

    const auto& metrics =
        rosterRepository->subPrepStudentCountBatchReadMetrics();
    QCOMPARE(metrics.callCount, 1);
    QCOMPARE(metrics.requestedClassCount, 1);
    QCOMPARE(metrics.columnStatementCount, 1);
    QCOMPARE(metrics.dataStatementCount, 1);
    QCOMPARE(metrics.fallbackClassReadCount, 0);

    QCOMPARE(events.size(), 1);
    QCOMPARE(events[0].name, QStringLiteral("sub-prep-roster-query"));
    QCOMPARE(
        events[0].detail,
        QStringLiteral(
            "classId=%1; columns=2; rows=%2; cells=%2; returnedStudents=4"
            ).arg(populatedClass)
             .arg(std::numeric_limits<int>::max())
        );
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
rosterBatchFailureFallsBackPerClassAndKeepsSuccessfulSibling()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(
        services,
        QStringLiteral("Roster fallback"),
        QStringLiteral("Roster fallback"),
        QStringLiteral("roster-fallback-batch")
        );
    QVERIFY(teacher > 0);
    const int classWithData = createClass(
        services,
        QStringLiteral("Data read fails"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("8:00 AM"), QStringLiteral("8:45 AM")}
        }
        );
    const int classWithoutColumns = createClass(
        services,
        QStringLiteral("Empty roster"),
        teacher,
        QStringLiteral("E5"),
        QStringLiteral("Apollo"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")}
        }
        );
    QVERIFY(classWithData > 0);
    QVERIFY(classWithoutColumns > 0);
    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    QVERIFY(store->isOpen());
    RosterRepository* const rosterRepository = store->rosterRepository();
    QVERIFY(rosterRepository);
    QVERIFY(rosterRepository->saveRoster(
        classWithData,
        rosterWithTwoStudents()
        ));
    QVERIFY(rosterRepository->saveRoster(
        classWithoutColumns,
        Roster{}
        ));
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE roster_data")));

    StartupProfiler profiler;
    StartupProfiler::activate(&profiler);
    StartupProfiler::setSubPrepDiagnosticsActive(true);

    ApplicationServicesSubPrepPrintSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {classId(classWithData), classId(classWithoutColumns)},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));

    const QList<StartupProfilingEvent> events = profiler.events();
    StartupProfiler::setSubPrepDiagnosticsActive(false);
    StartupProfiler::activate(nullptr);

    QVERIFY(result);
    QCOMPARE(result.value().classes.size(), std::size_t(2));
    QCOMPARE(result.value().classes[0].id, classId(classWithData));
    QCOMPARE(result.value().classes[1].id, classId(classWithoutColumns));
    QCOMPARE(
        result.value().classes[0].studentCount.rawValue(),
        std::size_t(0)
        );
    QCOMPARE(
        result.value().classes[1].studentCount.rawValue(),
        std::size_t(0)
        );
    QCOMPARE(result.value().teachers.size(), std::size_t(1));

    const auto& metrics =
        rosterRepository->subPrepStudentCountBatchReadMetrics();
    QCOMPARE(metrics.callCount, 1);
    QCOMPARE(metrics.requestedClassCount, 2);
    QCOMPARE(metrics.columnStatementCount, 1);
    QCOMPARE(metrics.dataStatementCount, 1);
    QCOMPARE(metrics.fallbackClassReadCount, 2);

    QCOMPARE(events.size(), 1);
    QCOMPARE(events[0].name, QStringLiteral("sub-prep-roster-query"));
    QCOMPARE(
        events[0].detail,
        QStringLiteral(
            "classId=%1; columns=0; rows=0; cells=0; returnedStudents=0"
            ).arg(classWithoutColumns)
        );
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
omittedPrintClassesDoNotReadRosters()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(
        services,
        QStringLiteral("Omitted roster"),
        QStringLiteral("Omitted roster"),
        QStringLiteral("omitted-roster")
        );
    QVERIFY(teacher > 0);
    const int outOfScopeClass = createClass(
        services,
        QStringLiteral("Out of scope"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Tuesday"), QStringLiteral("8:00 AM"), QStringLiteral("8:45 AM")}
        }
        );
    const int missingTeacherClass = createClass(
        services,
        QStringLiteral("Missing teacher"),
        teacher,
        QStringLiteral("E5"),
        QStringLiteral("Apollo"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")}
        }
        );
    QVERIFY(outOfScopeClass > 0);
    QVERIFY(missingTeacherClass > 0);
    QVERIFY(executeSql(services, QStringLiteral("PRAGMA foreign_keys = OFF")));
    QSqlQuery missingTeacherAssignment(
        services.databaseSession()->database()
        );
    QVERIFY(missingTeacherAssignment.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id = ? WHERE class_id = ?"
        )));
    missingTeacherAssignment.addBindValue(999999);
    missingTeacherAssignment.addBindValue(missingTeacherClass);
    QVERIFY2(
        missingTeacherAssignment.exec(),
        qPrintable(missingTeacherAssignment.lastError().text())
        );
    QVERIFY(executeSql(services, QStringLiteral("PRAGMA foreign_keys = ON")));
    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    QVERIFY(store->isOpen());
    RosterRepository* const rosterRepository = store->rosterRepository();
    QVERIFY(rosterRepository);

    ApplicationServicesSubPrepPrintSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {classId(outOfScopeClass), classId(missingTeacherClass)},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));

    QVERIFY(result);
    QVERIFY(result.value().classes.empty());
    const auto& metrics =
        rosterRepository->subPrepStudentCountBatchReadMetrics();
    QCOMPARE(metrics.callCount, 0);
    QCOMPARE(metrics.columnStatementCount, 0);
    QCOMPARE(metrics.dataStatementCount, 0);
    QCOMPARE(metrics.fallbackClassReadCount, 0);
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
supportsUnassignedAndMissingTeachersAndEmptyRosterFallback()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int assignedTeacher = createTeacher(
        services,
        QStringLiteral("Roster Teacher"),
        QStringLiteral("Roster Teacher"),
        QStringLiteral("roster")
        );
    QVERIFY(assignedTeacher > 0);
    const int classIdWithoutAssignment = createClass(
        services,
        QStringLiteral("No teacher"),
        -1,
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("8:00 AM"), QStringLiteral("8:45 AM")}
        }
        );
    QVERIFY(classIdWithoutAssignment > 0);
    const int classWithMissingTeacher = createClass(
        services,
        QStringLiteral("Missing teacher"),
        -1,
        QStringLiteral("E4"),
        QStringLiteral("Odysseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("11:00 AM"), QStringLiteral("11:45 AM")}
        }
        );
    QVERIFY(classWithMissingTeacher > 0);
    QVERIFY(executeSql(services, QStringLiteral("PRAGMA foreign_keys = OFF")));
    DataService* dataService = services.dataService();
    QVERIFY(dataService);
    QVERIFY(dataService->databaseSession());
    QSqlQuery orphanAssignment(
        dataService->databaseSession()->database()
        );
    QVERIFY(orphanAssignment.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id = ? WHERE class_id = ?"
        )));
    orphanAssignment.addBindValue(999999);
    orphanAssignment.addBindValue(classWithMissingTeacher);
    QVERIFY2(
        orphanAssignment.exec(),
        qPrintable(orphanAssignment.lastError().text())
        );
    QVERIFY(executeSql(services, QStringLiteral("PRAGMA foreign_keys = ON")));
    const int classWithTeacherButNoRoster = createClass(
        services,
        QStringLiteral("No roster"),
        assignedTeacher,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")}
        }
        );
    QVERIFY(classWithTeacherButNoRoster > 0);
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE roster_columns")));

    ApplicationServicesSubPrepPrintSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {
            classId(classIdWithoutAssignment),
            classId(classWithMissingTeacher),
            classId(classWithTeacherButNoRoster)
        },
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));

    QVERIFY(result);
    QCOMPARE(result.value().classes.size(), std::size_t(1));
    QCOMPARE(result.value().classes[0].id, classId(classWithTeacherButNoRoster));
    QVERIFY(result.value().classes[0].teacherId == teacherId(assignedTeacher));
    QCOMPARE(result.value().classes[0].studentCount.rawValue(), std::size_t(0));
    QCOMPARE(result.value().teachers.size(), std::size_t(1));
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
unavailableSessionsDoNotFallBackToDataService()
{
    ApplicationServices services;
    QVERIFY(services.dataService());
    ApplicationServicesSubPrepPrintSourcePort port(services);

    const SubPrepPrintSourceRequest request = requestFor(
        {classId(1)},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        );
    const auto unopened = port.loadSource(request);
    QVERIFY(!unopened);
    QCOMPARE(unopened.error().code, ErrorCode::NotFound);

    QVERIFY(openDatabase(services, m_directory));
    services.closeDatabase();
    QVERIFY(!services.hasOpenDatabase());

    const auto closed = port.loadSource(request);
    QVERIFY(!closed);
    QCOMPARE(closed.error().code, ErrorCode::NotFound);
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
queryEmptyScopeDoesNotReadAndNonemptyReadFailurePropagates()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    ApplicationServicesSubPrepPrintSourcePort port(services);
    const SubPrepPrintSourceQuery query(port);

    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE classes")));
    const auto emptyResult = query.execute(requestFor(
        {},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));
    QVERIFY(emptyResult);
    QVERIFY(emptyResult.value().empty());
    const auto emptyDaysResult = query.execute(requestFor(
        {classId(1)},
        {},
        ScheduleViewMode::Regular
        ));
    QVERIFY(emptyDaysResult);
    QVERIFY(emptyDaysResult.value().empty());

    ApplicationServices failureServices;
    QVERIFY(openDatabase(failureServices, m_directory));
    const int classIdWithSchedule = createClass(
        failureServices,
        QStringLiteral("Unreadable schedule"),
        -1,
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")}
        }
        );
    QVERIFY(classIdWithSchedule > 0);
    QVERIFY(executeSql(failureServices, QStringLiteral("DROP TABLE class_times")));

    ApplicationServicesSubPrepPrintSourcePort failingPort(failureServices);
    const auto failed = failingPort.loadSource(requestFor(
        {classId(classIdWithSchedule)},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));

    QVERIFY(!failed);
    QCOMPARE(failed.error().code, ErrorCode::Technical);
    QVERIFY(!failed.error().message.empty());
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
rejectsNoncanonicalClassIdAliasesBeforeReading()
{
    ApplicationServices services;
    ApplicationServicesSubPrepPrintSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {classId(1), *ClassId::fromString("01")},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
maximumClassScopeFitsSqliteBindLimits()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    std::vector<ClassId> selectedClassIds;
    selectedClassIds.reserve(kSubPrepPrintSourceMaxClassIds);
    for (std::size_t index = 1;
         index <= kSubPrepPrintSourceMaxClassIds;
         ++index)
    {
        selectedClassIds.push_back(classId(static_cast<int>(index)));
    }

    ApplicationServicesSubPrepPrintSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        selectedClassIds,
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));

    QVERIFY(result);
    QVERIFY(result.value().classes.empty());
    QVERIFY(result.value().teachers.empty());
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
ordersAllCanonicalWeekdaysInSummariesAndNavigation()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    const int teacher = createTeacher(
        services,
        QStringLiteral("English Name"),
        QStringLiteral("Preferred Name"),
        QStringLiteral("summary")
        );
    QVERIFY(teacher > 0);
    const int classWithAllWeekdays = createClass(
        services,
        QStringLiteral("All weekdays"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {}
        );
    const int classWithSecondPrecisionStart = createClass(
        services,
        QStringLiteral("Second precision start"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {}
        );
    QVERIFY(classWithAllWeekdays > 0);
    QVERIFY(classWithSecondPrecisionStart > 0);

    const QString canonicalWeekdays[] = {
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday"),
        QStringLiteral("Saturday"),
        QStringLiteral("Sunday")
    };
    for (int index = 6; index >= 0; --index)
    {
        QVERIFY(insertScheduleMeeting(
            services,
            QStringLiteral("class_times"),
            classWithAllWeekdays,
            canonicalWeekdays[index],
            QStringLiteral("09:15")
            ));
    }
    QVERIFY(insertScheduleMeeting(
        services,
        QStringLiteral("class_times"),
        classWithSecondPrecisionStart,
        QStringLiteral("Monday"),
        QStringLiteral("09:30:59")
        ));

    ApplicationServicesSubPrepScheduleSummaryPort port(services);
    const auto result = port.loadSummaries(summaryRequestFor(
        {
            classId(classWithAllWeekdays),
            classId(classWithSecondPrecisionStart)
        },
        {
            SubPrepWeekday::Monday,
            SubPrepWeekday::Tuesday,
            SubPrepWeekday::Wednesday,
            SubPrepWeekday::Thursday,
            SubPrepWeekday::Friday,
            SubPrepWeekday::Saturday,
            SubPrepWeekday::Sunday
        },
        ScheduleViewMode::Regular
        ));

    QVERIFY(result);
    QCOMPARE(result.value().classes.size(), std::size_t(2));
    const auto& summary = result.value().classes.front();
    QCOMPARE(summary.id, classId(classWithAllWeekdays));
    QCOMPARE(
        summary.meetingText,
        std::string("MonTuesWedThursFriSatSun 9:15am")
        );
    QCOMPARE(
        summary.navigationLabel,
        utf8(QStringLiteral("Perseus \u2022 M/T/W/Th/F/Sat/Sun 9:15"))
        );
    QCOMPARE(summary.navigationFirstDayOrder.rawValue(), 0);
    QCOMPARE(summary.navigationFirstTimeOrder.rawValue(), 9 * 60 + 15);

    const auto& secondPrecisionSummary = result.value().classes.back();
    QCOMPARE(secondPrecisionSummary.id, classId(classWithSecondPrecisionStart));
    QCOMPARE(secondPrecisionSummary.meetingText, std::string("Mon 9:30am"));
    QCOMPARE(
        secondPrecisionSummary.navigationLabel,
        utf8(QStringLiteral("Perseus \u2022 M 9:30"))
        );
    QVERIFY2(
        secondPrecisionSummary.navigationFirstTimeOrder.rawValue() == 570,
        "Raw 09:30:59 is mapped to minute 570 before Application order reduction."
        );
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
regularAllWeekNavigationMetadataWinsOverIntensiveSummary()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    const int teacher = createTeacher(
        services,
        QStringLiteral("English Name"),
        QStringLiteral("Preferred Name"),
        QStringLiteral("regular-navigation-precedence")
        );
    QVERIFY(teacher > 0);
    const int selectedClass = createClass(
        services,
        QStringLiteral("Intensive selected-day summary"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")}
        },
        {
            {QStringLiteral("Wednesday"), QStringLiteral("2:00 PM"), QStringLiteral("2:45 PM")}
        }
        );
    QVERIFY(selectedClass > 0);

    ApplicationServicesSubPrepScheduleSummaryPort port(services);
    const auto result = port.loadSummaries(summaryRequestFor(
        {classId(selectedClass)},
        {SubPrepWeekday::Wednesday},
        ScheduleViewMode::Intensive
        ));

    QVERIFY(result);
    QCOMPARE(result.value().classes.size(), std::size_t(1));
    const auto& summary = result.value().classes.front();
    QCOMPARE(summary.meetingText, std::string("Wed 2pm"));
    QCOMPARE(
        summary.navigationLabel,
        utf8(QStringLiteral("Perseus \u2022 M 9:00"))
        );
    QCOMPARE(summary.navigationTeacherLabel, std::string("English Name"));
    QCOMPARE(summary.navigationFirstDayOrder.rawValue(), 0);
    QCOMPARE(summary.navigationFirstTimeOrder.rawValue(), 9 * 60);
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
parsesRawScheduleTimesAndKeepsMalformedNavigationFallback()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    const int teacher = createTeacher(
        services,
        QStringLiteral("English Name"),
        QStringLiteral("Preferred Name"),
        QStringLiteral("schedule-time-parsing")
        );
    QVERIFY(teacher > 0);
    const int mixedSchedule = createClass(
        services,
        QStringLiteral("Mixed schedule times"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {}
        );
    const int malformedOnlySchedule = createClass(
        services,
        QStringLiteral("Malformed-only schedule"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {}
        );
    QVERIFY(mixedSchedule > 0);
    QVERIFY(malformedOnlySchedule > 0);

    // Insert raw legacy values through SQL so this exercises the public port's
    // conversion boundary, including its distinct summary and navigation
    // fallbacks for malformed input.
    QVERIFY(insertScheduleMeeting(
        services,
        QStringLiteral("class_times"),
        mixedSchedule,
        QStringLiteral("Monday"),
        QStringLiteral("09:15")
        ));
    QVERIFY(insertScheduleMeeting(
        services,
        QStringLiteral("class_times"),
        mixedSchedule,
        QStringLiteral("Tuesday"),
        QStringLiteral("2:30 PM")
        ));
    QVERIFY(insertScheduleMeeting(
        services,
        QStringLiteral("class_times"),
        mixedSchedule,
        QStringLiteral("Wednesday"),
        QStringLiteral(" 11:00 AM ")
        ));
    QVERIFY(insertScheduleMeeting(
        services,
        QStringLiteral("class_times"),
        mixedSchedule,
        QStringLiteral("Thursday"),
        QStringLiteral(" 7:xx pM ")
        ));
    QVERIFY(insertScheduleMeeting(
        services,
        QStringLiteral("class_times"),
        malformedOnlySchedule,
        QStringLiteral("Monday"),
        QStringLiteral(" 8:bad aM ")
        ));

    ApplicationServicesSubPrepScheduleSummaryPort port(services);
    const auto result = port.loadSummaries(summaryRequestFor(
        {classId(mixedSchedule), classId(malformedOnlySchedule)},
        {
            SubPrepWeekday::Monday,
            SubPrepWeekday::Tuesday,
            SubPrepWeekday::Wednesday,
            SubPrepWeekday::Thursday
        },
        ScheduleViewMode::Regular
        ));

    QVERIFY(result);
    QCOMPARE(result.value().classes.size(), std::size_t(2));
    const auto& mixed = result.value().classes[0];
    QCOMPARE(mixed.id, classId(mixedSchedule));
    QCOMPARE(
        mixed.meetingText,
        std::string("Mon 9:15am & Tues 2:30pm & Wed 11am")
        );
    QCOMPARE(
        mixed.navigationLabel,
        utf8(QStringLiteral("Perseus • M 9:15; T 2:30; W 11:00; Th 7:xx"))
        );
    QCOMPARE(mixed.navigationFirstDayOrder.rawValue(), 0);
    QCOMPARE(mixed.navigationFirstTimeOrder.rawValue(), 9 * 60 + 15);

    // The malformed-only meeting has no summary label, while navigation keeps
    // the trimmed legacy fallback after removing its literal AM/PM suffix.
    const auto& malformedOnly = result.value().classes[1];
    QCOMPARE(malformedOnly.id, classId(malformedOnlySchedule));
    QCOMPARE(malformedOnly.meetingText, std::string("N/A"));
    QCOMPARE(
        malformedOnly.navigationLabel,
        utf8(QStringLiteral("Perseus • M 8:bad"))
        );
    QCOMPARE(malformedOnly.navigationFirstDayOrder.rawValue(), 0);
    QCOMPARE(
        malformedOnly.navigationFirstTimeOrder.rawValue(),
        kClassSummaryUnknownNavigationOrder
        );
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
unknownAndNoncanonicalRawDaysDoNotChangeNavigationOrPresentation()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    const int teacher = createTeacher(
        services,
        QStringLiteral("English Name"),
        QStringLiteral("Preferred Name"),
        QStringLiteral("unknown-navigation-days")
        );
    QVERIFY(teacher > 0);
    const auto createScheduleClass = [&services, teacher](const QString& name)
    {
        return createClass(
            services,
            name,
            teacher,
            QStringLiteral("E4"),
            QStringLiteral("Perseus"),
            QStringLiteral(""),
            QStringLiteral("#FFFFFF"),
            QStringLiteral("#000000"),
            {}
            );
    };
    const int canonicalOnlyClass = createScheduleClass(
        QStringLiteral("Canonical only")
        );
    const int classWithUnrecognizedDays = createScheduleClass(
        QStringLiteral("Unknown and noncanonical days")
        );
    QVERIFY(canonicalOnlyClass > 0);
    QVERIFY(classWithUnrecognizedDays > 0);

    for (const int scheduleClass : {
             canonicalOnlyClass,
             classWithUnrecognizedDays
         })
    {
        QVERIFY(insertScheduleMeeting(
            services,
            QStringLiteral("class_times"),
            scheduleClass,
            QStringLiteral("Monday"),
            QStringLiteral("09:00")
            ));
    }
    QVERIFY(insertScheduleMeeting(
        services,
        QStringLiteral("class_times"),
        classWithUnrecognizedDays,
        QStringLiteral("monday"),
        QStringLiteral("12:01 AM")
        ));
    QVERIFY(insertScheduleMeeting(
        services,
        QStringLiteral("class_times"),
        classWithUnrecognizedDays,
        QStringLiteral("Funday"),
        QStringLiteral("12:00 AM")
        ));

    ApplicationServicesSubPrepScheduleSummaryPort port(services);
    const auto result = port.loadSummaries(summaryRequestFor(
        {classId(canonicalOnlyClass), classId(classWithUnrecognizedDays)},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));

    QVERIFY(result);
    QCOMPARE(result.value().classes.size(), std::size_t(2));
    const auto& canonicalOnly = result.value().classes[0];
    const auto& withUnrecognizedDays = result.value().classes[1];
    QCOMPARE(canonicalOnly.meetingText, std::string("Mon 9am"));
    QCOMPARE(withUnrecognizedDays.meetingText, canonicalOnly.meetingText);
    QCOMPARE(
        canonicalOnly.navigationLabel,
        utf8(QStringLiteral("Perseus • M 9:00"))
        );
    QCOMPARE(
        withUnrecognizedDays.navigationLabel,
        canonicalOnly.navigationLabel
        );
    QCOMPARE(canonicalOnly.navigationFirstDayOrder.rawValue(), 0);
    QCOMPARE(canonicalOnly.navigationFirstTimeOrder.rawValue(), 9 * 60);
    QCOMPARE(
        withUnrecognizedDays.navigationFirstDayOrder.rawValue(),
        canonicalOnly.navigationFirstDayOrder.rawValue()
        );
    QCOMPARE(
        withUnrecognizedDays.navigationFirstTimeOrder.rawValue(),
        canonicalOnly.navigationFirstTimeOrder.rawValue()
        );
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
perClassMeetingOverflowSurfacesValidation()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(
        services,
        QStringLiteral("Overflow Teacher"),
        QStringLiteral("Overflow Teacher"),
        QStringLiteral("overflow")
        );
    QVERIFY(teacher > 0);
    const int classWithManyMeetings = createClass(
        services,
        QStringLiteral("Many meetings"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {}
        );
    QVERIFY(classWithManyMeetings > 0);

    DataService* dataService = services.dataService();
    QVERIFY(dataService);
    QVERIFY(dataService->databaseSession());
    QSqlQuery insert(dataService->databaseSession()->database());
    QVERIFY(insert.prepare(QStringLiteral(
        "INSERT INTO class_times (class_id, day, start_time, end_time) "
        "VALUES (?, ?, ?, ?)"
        )));
    for (std::size_t index = 0;
         index <= kSubPrepPrintSourceMaxMeetingsPerClass;
         ++index)
    {
        insert.bindValue(0, classWithManyMeetings);
        insert.bindValue(1, QStringLiteral("Monday"));
        insert.bindValue(2, QStringLiteral("9:00 AM"));
        insert.bindValue(3, QStringLiteral("9:45 AM"));
        QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
    }

    ApplicationServicesSubPrepPrintSourcePort port(services);
    const SubPrepPrintSourceQuery query(port);
    const auto result = query.execute(requestFor(
        {classId(classWithManyMeetings)},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Validation);
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
aggregateMeetingOverflowSurfacesValidation()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(
        services,
        QStringLiteral("Overflow"),
        QStringLiteral("Overflow"),
        QStringLiteral("agg")
        );
    QVERIFY(teacher > 0);

    DataService* dataService = services.dataService();
    QVERIFY(dataService);
    QVERIFY(dataService->databaseSession());
    const QSqlDatabase database = dataService->databaseSession()->database();

    std::vector<ClassId> selectedClassIds;
    selectedClassIds.reserve(253);
    QSqlQuery classInsert(database);
    QVERIFY(classInsert.prepare(QStringLiteral(
        "INSERT INTO classes (name) VALUES (?)"
        )));
    QSqlQuery classInfoInsert(database);
    QVERIFY(classInfoInsert.prepare(QStringLiteral(
        "INSERT INTO class_info "
        "(class_id, teacher_id, class_grade, class_level, class_color, font_color) "
        "VALUES (?, ?, 'E4', 'Aggregate overflow', '#FFFFFF', '#000000')"
        )));
    for (int index = 0; index < 253; ++index)
    {
        classInsert.bindValue(0, QStringLiteral("Aggregate overflow %1")
                                     .arg(index));
        QVERIFY2(classInsert.exec(), qPrintable(classInsert.lastError().text()));
        const int classId = classInsert.lastInsertId().toInt();
        QVERIFY(classId > 0);
        selectedClassIds.push_back(ClassId::fromString(
            std::to_string(classId)
            ).value());

        classInfoInsert.bindValue(0, classId);
        classInfoInsert.bindValue(1, teacher);
        QVERIFY2(
            classInfoInsert.exec(),
            qPrintable(classInfoInsert.lastError().text())
            );
    }

    QSqlQuery insertMeetings(database);
    QVERIFY(insertMeetings.exec(QStringLiteral(R"(
        WITH RECURSIVE meeting_number(n) AS (
            SELECT 1
            UNION ALL SELECT n + 1 FROM meeting_number WHERE n < 65
        )
        INSERT INTO class_times (class_id, day, start_time, end_time)
        SELECT
            class_info.class_id,
            'Monday',
            '9:00 AM',
            '9:45 AM'
        FROM class_info
        CROSS JOIN meeting_number
        WHERE class_info.class_grade = 'E4'
          AND class_info.class_level = 'Aggregate overflow'
    )")));

    ApplicationServicesSubPrepPrintSourcePort port(services);
    const SubPrepPrintSourceQuery query(port);
    const auto result = query.execute(requestFor(
        selectedClassIds,
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Validation);
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
boundaryReturnsOwningTypedValues()
{
    using Port = ApplicationServicesSubPrepPrintSourcePort;
    static_assert(std::is_same_v<
        decltype(std::declval<Port&>().loadSource(
            std::declval<const SubPrepPrintSourceRequest&>()
            )),
        SubPrepPrintSourceReadResult
        >);
    static_assert(!std::is_copy_constructible_v<Port>);
    static_assert(!std::is_move_constructible_v<Port>);
    QVERIFY(true);
}

QTEST_MAIN(NextPlatformApplicationServicesSubPrepPrintSourcePortTests)

#include "next_platform_application_services_sub_prep_print_source_port_tests.moc"
