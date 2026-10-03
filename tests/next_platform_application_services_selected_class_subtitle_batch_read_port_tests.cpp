#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/selected_class_subtitle_batch_read_query.h"
#include "next/platform/application_services_selected_class_subtitle_batch_read_port.h"

#include <QSqlQuery>
#include <QSqlError>
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
        QStringLiteral("selected-class-subtitle-batch-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Domain::ClassId classId(const int value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

Teacher teacher(const QString& englishName)
{
    Teacher value;
    value.teacherKr = QStringLiteral("Korean ") + englishName;
    value.teacherEn = englishName;
    value.preferredRomanization = QStringLiteral("Romanized ") + englishName;
    value.preferredName = QStringLiteral("Preferred ") + englishName;
    value.roomNumber = QStringLiteral("Not part of the subtitle projection");
    return value;
}

ClassInfo classInfo(
    const int id,
    const int teacherId,
    const QString& grade,
    const QString& level,
    QList<ClassTime> times
    )
{
    ClassInfo info;
    info.classId = id;
    info.teacherId = teacherId;
    info.classGrade = grade;
    info.classLevel = level;
    info.readingBook = QStringLiteral("Not part of the subtitle projection");
    info.classTimes = std::move(times);
    info.intensiveTimes.clear();
    return info;
}

bool seedClassSubtitle(
    QSqlDatabase database,
    const ClassInfo& info,
    QString* error
    )
{
    QSqlQuery metadataQuery(database);
    metadataQuery.prepare(QStringLiteral(
        "INSERT INTO class_info(class_id, teacher_id, class_grade, class_level) "
        "VALUES(?, ?, ?, ?)"
        ));
    metadataQuery.addBindValue(info.classId);
    metadataQuery.addBindValue(
        info.teacherId > 0 ? QVariant(info.teacherId) : QVariant()
        );
    metadataQuery.addBindValue(info.classGrade);
    metadataQuery.addBindValue(info.classLevel);
    if (!metadataQuery.exec())
    {
        if (error)
        {
            *error = metadataQuery.lastError().text();
        }
        return false;
    }

    QSqlQuery scheduleQuery(database);
    scheduleQuery.prepare(QStringLiteral(
        "INSERT INTO class_times(class_id, day, start_time, end_time) "
        "VALUES(?, ?, ?, ?)"
        ));
    for (const ClassTime& row : info.classTimes)
    {
        scheduleQuery.bindValue(0, info.classId);
        scheduleQuery.bindValue(1, row.day);
        scheduleQuery.bindValue(2, row.startTime);
        scheduleQuery.bindValue(3, row.endTime);
        if (!scheduleQuery.exec())
        {
            if (error)
            {
                *error = scheduleQuery.lastError().text();
            }
            return false;
        }
    }

    return true;
}

std::vector<Domain::ClassId> ids(const QList<int>& values)
{
    std::vector<Domain::ClassId> result;
    result.reserve(static_cast<std::size_t>(values.size()));
    for (const int value : values)
    {
        result.push_back(classId(value));
    }
    return result;
}

}

class NextPlatformApplicationServicesSelectedClassSubtitleBatchReadPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void readsSubtitleProjectionWithFixedStatementCounts();
    void missingAssignmentsAndReadFailuresRemainIndependent();
    void missingSessionAndInvalidIdsReturnStructuredErrors();
};

void NextPlatformApplicationServicesSelectedClassSubtitleBatchReadPortTests::
readsSubtitleProjectionWithFixedStatementCounts()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);

    TeacherRepository* const teacherRepository = session->teacherRepository();
    ClassInfoRepository* const classRepository = session->classInfoRepository();
    QVERIFY(teacherRepository);
    QVERIFY(classRepository);

    const auto teacherA = teacherRepository->saveTeacher(
        teacher(QStringLiteral("English A"))
        );
    const auto teacherB = teacherRepository->saveTeacher(
        teacher(QStringLiteral("English B"))
        );
    QVERIFY(teacherA);
    QVERIFY(teacherB);

    const auto classA = services.classService()->create(QStringLiteral("A"));
    const auto classB = services.classService()->create(QStringLiteral("B"));
    const auto classC = services.classService()->create(QStringLiteral("C"));
    QVERIFY(classA);
    QVERIFY(classB);
    QVERIFY(classC);
    QString seedError;
    QVERIFY2(seedClassSubtitle(session->database(), classInfo(
        *classA,
        *teacherA,
        QStringLiteral(" E4 "),
        QStringLiteral("Theseus"),
        {
            {QStringLiteral("Friday"), QStringLiteral("9:00 AM"), QStringLiteral("9:55 AM")},
            {QStringLiteral("Monday"), QStringLiteral("10:00 AM"), QStringLiteral("10:55 AM")}
        }
        ), &seedError), qPrintable(seedError));
    QVERIFY2(seedClassSubtitle(session->database(), classInfo(
        *classB,
        *teacherB,
        QStringLiteral("M2"),
        QStringLiteral("Hydra"),
        {{QStringLiteral("Wednesday"), QStringLiteral("2:00 PM"), QStringLiteral("2:55 PM")}}
        ), &seedError), qPrintable(seedError));
    QVERIFY2(seedClassSubtitle(session->database(), classInfo(
        *classC,
        *teacherA,
        QStringLiteral("E5"),
        QStringLiteral("Orion"),
        {{QStringLiteral("Tuesday"), QStringLiteral("3:00 PM"), QStringLiteral("3:55 PM")}}
        ), &seedError), qPrintable(seedError));

    Platform::ApplicationServicesSelectedClassSubtitleBatchReadPort port(
        &services
        );
    const Application::SelectedClassSubtitleBatchReadQuery query(port);

    const auto single = query.execute(ids({*classA}));
    QVERIFY(single);
    QCOMPARE(single.value().size(), std::size_t(1));
    QVERIFY(single.value().front().classFields);
    QCOMPARE(single.value().front().classFields.value().classGrade,
             std::u16string(u" E4 "));
    QCOMPARE(single.value().front().classFields.value().regularSchedule.size(),
             std::size_t(2));
    QVERIFY((single.value().front().classFields.value().regularSchedule[0] ==
        Application::SelectedClassSubtitleScheduleRow{u"Friday", u"9:00 AM"}));
    QVERIFY((single.value().front().classFields.value().regularSchedule[1] ==
        Application::SelectedClassSubtitleScheduleRow{u"Monday", u"10:00 AM"}));
    QVERIFY(single.value().front().assignedTeacher);
    QVERIFY(single.value().front().assignedTeacher.value().has_value());
    QCOMPARE(single.value().front().assignedTeacher.value()->teacherKr,
             std::u16string(u"Korean English A"));
    QCOMPARE(single.value().front().assignedTeacher.value()->teacherEn,
             std::u16string(u"English A"));
    QCOMPARE(single.value().front().assignedTeacher.value()->preferredRomanization,
             std::u16string(u"Romanized English A"));
    QCOMPARE(single.value().front().assignedTeacher.value()->preferredName,
             std::u16string(u"Preferred English A"));

    const auto classMetricsAfterSingle =
        classRepository->classSubtitleBatchReadMetrics();
    const auto teacherMetricsAfterSingle =
        teacherRepository->teacherDisplayNameBatchReadMetrics();
    QCOMPARE(classMetricsAfterSingle.callCount, 1);
    QCOMPARE(classMetricsAfterSingle.metadataStatementCount, 1);
    QCOMPARE(classMetricsAfterSingle.regularScheduleStatementCount, 1);
    QCOMPARE(teacherMetricsAfterSingle.callCount, 1);
    QCOMPARE(teacherMetricsAfterSingle.statementCount, 1);

    const auto several = query.execute(ids({*classC, *classB, *classA}));
    QVERIFY(several);
    QCOMPARE(several.value().size(), std::size_t(3));
    QVERIFY(several.value()[0].classId == classId(*classC));
    QVERIFY(several.value()[1].classId == classId(*classB));
    QVERIFY(several.value()[2].classId == classId(*classA));
    QCOMPARE(several.value()[0].classFields.value().classLevel,
             std::u16string(u"Orion"));
    QCOMPARE(several.value()[1].classFields.value().classLevel,
             std::u16string(u"Hydra"));
    QCOMPARE(several.value()[1].classFields.value().regularSchedule.size(),
             std::size_t(1));
    QVERIFY(several.value()[1].assignedTeacher.value().has_value());
    QCOMPARE(several.value()[1].assignedTeacher.value()->teacherEn,
             std::u16string(u"English B"));

    const auto classMetricsAfterSeveral =
        classRepository->classSubtitleBatchReadMetrics();
    const auto teacherMetricsAfterSeveral =
        teacherRepository->teacherDisplayNameBatchReadMetrics();
    QCOMPARE(classMetricsAfterSeveral.callCount, 2);
    QCOMPARE(classMetricsAfterSeveral.metadataStatementCount, 2);
    QCOMPARE(classMetricsAfterSeveral.regularScheduleStatementCount, 2);
    QCOMPARE(teacherMetricsAfterSeveral.callCount, 2);
    QCOMPARE(teacherMetricsAfterSeveral.statementCount, 2);

    const auto noInfoClass = services.classService()->create(
        QStringLiteral("No class subtitle row")
        );
    QVERIFY(noInfoClass);
    const auto missingMetadata = query.execute(ids({*noInfoClass}));
    QVERIFY(missingMetadata);
    QVERIFY(missingMetadata.value().front().classFields);
    QVERIFY(missingMetadata.value().front().classFields.value().classGrade.empty());
    QVERIFY(missingMetadata.value().front().classFields.value().classLevel.empty());
    QVERIFY(missingMetadata.value().front().assignedTeacher);
    QVERIFY(!missingMetadata.value().front().assignedTeacher.value().has_value());
}

void NextPlatformApplicationServicesSelectedClassSubtitleBatchReadPortTests::
missingAssignmentsAndReadFailuresRemainIndependent()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    TeacherRepository* const teacherRepository = session->teacherRepository();
    ClassInfoRepository* const classRepository = session->classInfoRepository();
    QVERIFY(teacherRepository);
    QVERIFY(classRepository);

    const auto teacherId = teacherRepository->saveTeacher(
        teacher(QStringLiteral("Assigned"))
        );
    QVERIFY(teacherId);
    const auto assignedClass = services.classService()->create(
        QStringLiteral("Assigned class")
        );
    const auto unassignedClass = services.classService()->create(
        QStringLiteral("Unassigned class")
        );
    QVERIFY(assignedClass);
    QVERIFY(unassignedClass);
    QString seedError;
    QVERIFY2(seedClassSubtitle(session->database(), classInfo(
        *assignedClass,
        *teacherId,
        QStringLiteral("E4"),
        QStringLiteral("Orion"),
        {{QStringLiteral("Monday"), QStringLiteral("4:00 PM"), QStringLiteral("4:50 PM")}}
        ), &seedError), qPrintable(seedError));
    QVERIFY2(seedClassSubtitle(session->database(), classInfo(
        *unassignedClass,
        -1,
        QStringLiteral("E5"),
        QStringLiteral("Hydra"),
        {{QStringLiteral("Tuesday"), QStringLiteral("5:00 PM"), QStringLiteral("5:50 PM")}}
        ), &seedError), qPrintable(seedError));

    Platform::ApplicationServicesSelectedClassSubtitleBatchReadPort port(
        &services
        );
    const Application::SelectedClassSubtitleBatchReadQuery query(port);

    QSqlQuery disableForeignKeys(session->database());
    QVERIFY(disableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys=OFF")));
    QSqlQuery deleteTeacher(session->database());
    deleteTeacher.prepare(QStringLiteral("DELETE FROM teachers WHERE id=?"));
    deleteTeacher.addBindValue(*teacherId);
    QVERIFY(deleteTeacher.exec());

    const auto missingTeacher = query.execute(ids({*assignedClass, *unassignedClass}));
    QVERIFY(missingTeacher);
    QVERIFY(missingTeacher.value()[0].classFields);
    QCOMPARE(missingTeacher.value()[0].classFields.value().classLevel,
             std::u16string(u"Orion"));
    QVERIFY(!missingTeacher.value()[0].assignedTeacher);
    QCOMPARE(missingTeacher.value()[0].assignedTeacher.error().code,
             Domain::ErrorCode::NotFound);
    QVERIFY(missingTeacher.value()[1].classFields);
    QVERIFY(missingTeacher.value()[1].assignedTeacher);
    QVERIFY(!missingTeacher.value()[1].assignedTeacher.value().has_value());

    QSqlQuery dropTeachers(session->database());
    QVERIFY(dropTeachers.exec(QStringLiteral("DROP TABLE teachers")));
    const auto teacherReadFailure = query.execute(ids({*assignedClass}));
    QVERIFY(teacherReadFailure);
    QVERIFY(teacherReadFailure.value().front().classFields);
    QCOMPARE(teacherReadFailure.value().front().classFields.value().classGrade,
             std::u16string(u"E4"));
    QCOMPARE(teacherReadFailure.value().front().classFields.value().classLevel,
             std::u16string(u"Orion"));
    QCOMPARE(teacherReadFailure.value().front().classFields.value().regularSchedule.size(),
             std::size_t(1));
    QVERIFY((teacherReadFailure.value().front().classFields.value().regularSchedule[0] ==
        Application::SelectedClassSubtitleScheduleRow{u"Monday", u"4:00 PM"}));
    QVERIFY(!teacherReadFailure.value().front().assignedTeacher);
    QCOMPARE(teacherReadFailure.value().front().assignedTeacher.error().code,
             Domain::ErrorCode::Technical);

    QSqlQuery dropTimes(session->database());
    QVERIFY(dropTimes.exec(QStringLiteral("DROP TABLE class_times")));
    const auto classReadFailure = query.execute(ids({*assignedClass}));
    QVERIFY(classReadFailure);
    QVERIFY(!classReadFailure.value().front().classFields);
    QVERIFY(classReadFailure.value().front().assignedTeacher);
    QVERIFY(!classReadFailure.value().front().assignedTeacher.value().has_value());
}

void NextPlatformApplicationServicesSelectedClassSubtitleBatchReadPortTests::
missingSessionAndInvalidIdsReturnStructuredErrors()
{
    ApplicationServices services;
    Platform::ApplicationServicesSelectedClassSubtitleBatchReadPort port(
        &services
        );
    const auto unavailable = port.readSelectedClassSubtitles({classId(42)});
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, Domain::ErrorCode::NotFound);

    const auto nonCanonical = Domain::ClassId::fromString("042");
    QVERIFY(nonCanonical);
    const auto invalid = port.readSelectedClassSubtitles({*nonCanonical});
    QVERIFY(!invalid);
    QCOMPARE(invalid.error().code, Domain::ErrorCode::InvalidInput);

    Platform::ApplicationServicesSelectedClassSubtitleBatchReadPort nullPort(
        nullptr
        );
    const auto noServices = nullPort.readSelectedClassSubtitles({classId(42)});
    QVERIFY(!noServices);
    QCOMPARE(noServices.error().code, Domain::ErrorCode::NotFound);
}

QTEST_MAIN(NextPlatformApplicationServicesSelectedClassSubtitleBatchReadPortTests)

#include "next_platform_application_services_selected_class_subtitle_batch_read_port_tests.moc"
