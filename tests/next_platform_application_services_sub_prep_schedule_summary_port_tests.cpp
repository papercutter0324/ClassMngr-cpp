#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/class_info.h"
#include "domain/models/teacher.h"
#include "next/application/sub_prep_schedule_summary_query.h"
#include "next/platform/application_services_sub_prep_schedule_summary_port.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("sub-prep-summary-port-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Domain::ClassId classId(const int value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

}

class NextPlatformApplicationServicesSubPrepScheduleSummaryPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void readsVisibleClassSummary();
    void mapsTypedRepositoryFailureAndUnavailableStore();
};

void NextPlatformApplicationServicesSubPrepScheduleSummaryPortTests::
readsVisibleClassSummary()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Teacher teacher;
    teacher.teacherEn = QStringLiteral("Substitute teacher");
    const auto teacherId = services.workspaceStore()->teacherRepository()
        ->createTeacher(teacher);
    QVERIFY(teacherId);

    const auto classResult = services.workspaceStore()->classRepository()
        ->createClass(QStringLiteral("Summary class"));
    QVERIFY(classResult);
    const int id = *classResult;
    ClassInfo info;
    info.classId = id;
    info.teacherId = *teacherId;
    info.classGrade = QStringLiteral("E4");
    info.classLevel = QStringLiteral("Theseus");
    info.classTimes = {
        {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")}
    };
    QVERIFY(services.workspaceStore()->classInfoRepository()->saveClassInfo(info));

    Platform::ApplicationServicesSubPrepScheduleSummaryPort port(services);
    const auto result = port.loadSummaries({
        .visibleClassIds = {classId(id)},
        .selectedDays = {Application::SubPrepWeekday::Monday},
        .mode = Application::ScheduleViewMode::Regular
    });

    QVERIFY(result);
    QCOMPARE(result.value().classes.size(), std::size_t{1});
    QVERIFY(result.value().classes.front().id == classId(id));
    QCOMPARE(result.value().classes.front().grade, std::string("E4"));
    QCOMPARE(result.value().classes.front().level, std::string("Theseus"));
    QCOMPARE(result.value().classes.front().meetingText, std::string("Mon 9am"));
}

void NextPlatformApplicationServicesSubPrepScheduleSummaryPortTests::
mapsTypedRepositoryFailureAndUnavailableStore()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const auto classResult = services.workspaceStore()->classRepository()
        ->createClass(QStringLiteral("Failure class"));
    QVERIFY(classResult);

    Platform::ApplicationServicesSubPrepScheduleSummaryPort port(services);
    QSqlQuery dropTimes(services.workspaceStore()->database());
    QVERIFY(dropTimes.exec(QStringLiteral("DROP TABLE class_times")));
    const auto failed = port.loadSummaries({
        .visibleClassIds = {classId(*classResult)},
        .selectedDays = {Application::SubPrepWeekday::Monday},
        .mode = Application::ScheduleViewMode::Regular
    });
    QVERIFY(!failed);
    QCOMPARE(failed.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!failed.error().message.empty());

    services.closeDatabase();
    const auto unavailable = port.loadSummaries({
        .visibleClassIds = {classId(*classResult)},
        .selectedDays = {Application::SubPrepWeekday::Monday},
        .mode = Application::ScheduleViewMode::Regular
    });
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, Domain::ErrorCode::NotFound);
}

QTEST_GUILESS_MAIN(
    NextPlatformApplicationServicesSubPrepScheduleSummaryPortTests
    )

#include "next_platform_application_services_sub_prep_schedule_summary_port_tests.moc"
