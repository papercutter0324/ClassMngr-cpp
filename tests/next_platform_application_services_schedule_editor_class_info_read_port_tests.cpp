#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "next/application/schedule_editor_class_info_query.h"
#include "next/platform/application_services_schedule_editor_class_info_read_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("schedule-editor-class-info-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Domain::ClassId classId(const int value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

int createClassWithCompleteDetails(
    ApplicationServices& services,
    QString& failureStep
    )
{
    const auto createdClass = services.classService()->create(
        QStringLiteral("Schedule Editor Read Test")
        );
    if (!createdClass)
    {
        failureStep = QStringLiteral("create class");
        return -1;
    }

    Teacher teacher;
    teacher.teacherKr =
        QString::fromUtf8("\xEA\xB9\x80\xEC\x84\xA0\xEC\x83\x9D");
    teacher.teacherEn = QStringLiteral("English Name");
    teacher.preferredRomanization = QStringLiteral("Preferred Teacher");
    teacher.preferredName = QStringLiteral("Preferred Teacher");
    teacher.roomNumber = QStringLiteral("308");
    const auto createdTeacher = services.teacherService()->save(teacher);
    if (!createdTeacher)
    {
        failureStep = QStringLiteral("save teacher");
        return -1;
    }

    auto info = services.classService()->classInfo(*createdClass);
    if (!info)
    {
        failureStep = QStringLiteral("read class info");
        return -1;
    }
    info->teacherId = *createdTeacher;
    info->classGrade = QStringLiteral("E4");
    info->classLevel = QStringLiteral("Theseus");
    info->readingBook = QStringLiteral("Reading Explorer 1");
    info->essayBook = QStringLiteral("4A");
    info->classColor = QStringLiteral("#123456");
    info->fontColor = QStringLiteral("#654321");
    if (!services.classService()->saveClassInfo(*info))
    {
        failureStep = QStringLiteral("save class info");
        return -1;
    }

    return *createdClass;
}

void verifyUnavailable(
    const Application::ScheduleEditorClassInfoReadResult& result
    )
{
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(!result.error().message.empty());
}

}

class NextPlatformApplicationServicesScheduleEditorClassInfoReadPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void readsEveryFieldFromTheActiveSessionRepository();
    void readsDefaultsWithoutMetadataAndBlanksForDanglingTeacher();
    void nullMissingAndClosedSessionsDoNotUseServiceFallback();
    void forwardsRepositoryReadFailure();
};

void NextPlatformApplicationServicesScheduleEditorClassInfoReadPortTests::
readsEveryFieldFromTheActiveSessionRepository()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QString failureStep;
    const int id = createClassWithCompleteDetails(services, failureStep);
    QVERIFY2(id > 0, qPrintable(failureStep));

    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    Platform::ApplicationServicesScheduleEditorClassInfoReadPort port(
        &services
        );
    const auto loaded = port.readScheduleEditorClassInfo(classId(id));

    QVERIFY(loaded);
    const Application::ScheduleEditorClassInfoSnapshot expected{
        .classId = classId(id),
        .classGrade = u"E4",
        .classLevel = u"Theseus",
        .readingBook = u"Reading Explorer 1",
        .essayBook = u"4A",
        .classColor = u"#123456",
        .fontColor = u"#654321",
        .teacherKoreanName = u"\uAE40\uC120\uC0DD",
        .roomNumber = u"308"
    };
    QVERIFY(loaded.value() == expected);
    const auto& metrics = session->classInfoRepository()
        ->scheduleEditorClassInfoReadMetrics();
    QCOMPARE(metrics.callCount, 1);
    QCOMPARE(metrics.statementCount, 1);
}

void NextPlatformApplicationServicesScheduleEditorClassInfoReadPortTests::
readsDefaultsWithoutMetadataAndBlanksForDanglingTeacher()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const auto createdClass = services.classService()->create(
        QStringLiteral("Schedule Editor Missing Metadata Test")
        );
    QVERIFY(createdClass);

    QSqlDatabase database = services.databaseSession()->database();
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "DELETE FROM class_info WHERE class_id = ?"
        ));
    query.addBindValue(*createdClass);
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));

    Platform::ApplicationServicesScheduleEditorClassInfoReadPort port(
        &services
        );
    const Domain::ClassId expectedClassId = classId(*createdClass);
    const auto missingMetadata =
        port.readScheduleEditorClassInfo(expectedClassId);

    QVERIFY(missingMetadata);
    const Application::ScheduleEditorClassInfoSnapshot defaults{
        .classId = expectedClassId,
        .classGrade = u"",
        .classLevel = u"",
        .readingBook = u"",
        .essayBook = u"",
        .classColor = u"#FFFFFF",
        .fontColor = u"#000000",
        .teacherKoreanName = u"",
        .roomNumber = u""
    };
    QVERIFY(missingMetadata.value() == defaults);

    QVERIFY2(query.exec(QStringLiteral("PRAGMA foreign_keys = OFF")),
        qPrintable(query.lastError().text()));
    query.prepare(QStringLiteral(R"(
        INSERT INTO class_info (
            class_id, teacher_id, class_grade, class_level,
            reading_book, essay_book, class_color, font_color
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )"));
    query.addBindValue(*createdClass);
    query.addBindValue(987654);
    query.addBindValue(QStringLiteral("E5"));
    query.addBindValue(QStringLiteral("Aeneid"));
    query.addBindValue(QStringLiteral("Reading Book"));
    query.addBindValue(QStringLiteral("Essay Book"));
    query.addBindValue(QStringLiteral("#AABBCC"));
    query.addBindValue(QStringLiteral("#112233"));
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));

    const auto danglingTeacher =
        port.readScheduleEditorClassInfo(expectedClassId);

    QVERIFY(danglingTeacher);
    const Application::ScheduleEditorClassInfoSnapshot expectedDangling{
        .classId = expectedClassId,
        .classGrade = u"E5",
        .classLevel = u"Aeneid",
        .readingBook = u"Reading Book",
        .essayBook = u"Essay Book",
        .classColor = u"#AABBCC",
        .fontColor = u"#112233",
        .teacherKoreanName = u"",
        .roomNumber = u""
    };
    QVERIFY(danglingTeacher.value() == expectedDangling);

    const auto& metrics = services.databaseSession()->classInfoRepository()
        ->scheduleEditorClassInfoReadMetrics();
    QCOMPARE(metrics.callCount, 2);
    QCOMPARE(metrics.statementCount, 2);
}

void NextPlatformApplicationServicesScheduleEditorClassInfoReadPortTests::
nullMissingAndClosedSessionsDoNotUseServiceFallback()
{
    const Domain::ClassId id = classId(42);

    Platform::ApplicationServicesScheduleEditorClassInfoReadPort nullPort(
        nullptr
        );
    verifyUnavailable(nullPort.readScheduleEditorClassInfo(id));

    ApplicationServices unopenedServices;
    QVERIFY(unopenedServices.dataService());
    DatabaseSession* const unopenedSession =
        unopenedServices.databaseSession();
    QVERIFY(!unopenedSession || !unopenedSession->isOpen());
    Platform::ApplicationServicesScheduleEditorClassInfoReadPort unopenedPort(
        &unopenedServices
        );
    verifyUnavailable(unopenedPort.readScheduleEditorClassInfo(id));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices closedServices;
    QVERIFY(closedServices.openDatabase(databasePath(directory)));
    DatabaseSession* const closedSession = closedServices.databaseSession();
    QVERIFY(closedSession);
    QVERIFY(closedSession->isOpen());
    closedServices.closeDatabase();
    QVERIFY(!closedSession->isOpen());

    Platform::ApplicationServicesScheduleEditorClassInfoReadPort closedPort(
        &closedServices
        );
    verifyUnavailable(closedPort.readScheduleEditorClassInfo(id));
}

void NextPlatformApplicationServicesScheduleEditorClassInfoReadPortTests::
forwardsRepositoryReadFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QString failureStep;
    const int id = createClassWithCompleteDetails(services, failureStep);
    QVERIFY2(id > 0, qPrintable(failureStep));

    QSqlQuery query(services.databaseSession()->database());
    QVERIFY2(query.exec(QStringLiteral("DROP TABLE class_info")),
        qPrintable(query.lastError().text()));

    Platform::ApplicationServicesScheduleEditorClassInfoReadPort port(
        &services
        );
    const auto result = port.readScheduleEditorClassInfo(classId(id));

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().message.empty());
}

QTEST_MAIN(
    NextPlatformApplicationServicesScheduleEditorClassInfoReadPortTests
    )

#include "next_platform_application_services_schedule_editor_class_info_read_port_tests.moc"
