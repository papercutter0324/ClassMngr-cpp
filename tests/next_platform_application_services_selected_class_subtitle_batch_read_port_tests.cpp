#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/selected_class_subtitle_batch_read_query.h"
#include "next/platform/application_services_selected_class_subtitle_batch_read_port.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryDir>
#include <QVariant>
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

bool seedClassSchedule(
    QSqlDatabase database,
    const int classId,
    const QList<ClassTime>& times,
    QString* error
    )
{
    QSqlQuery scheduleQuery(database);
    scheduleQuery.prepare(QStringLiteral(
        "INSERT INTO class_times(class_id, day, start_time, end_time) "
        "VALUES(?, ?, ?, ?)"
        ));
    for (const ClassTime& row : times)
    {
        scheduleQuery.bindValue(0, classId);
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

    return seedClassSchedule(database, info.classId, info.classTimes, error);
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
    void missingParentsDoNotExposeOrphanSubtitleData();
    void readsDisplayNamesForWideAssignedTeacherId();
    void missingAssignmentsAndReadFailuresRemainIndependent();
    void malformedPersistedTeacherIdIsValidationForOrderedBatch();
    void missingSessionAndInvalidIdsReturnStructuredErrors();
};

void NextPlatformApplicationServicesSelectedClassSubtitleBatchReadPortTests::
readsSubtitleProjectionWithFixedStatementCounts()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);

    TeacherRepository* const teacherRepository = store->teacherRepository();
    ClassInfoRepository* const classRepository = store->classInfoRepository();
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
    QVERIFY2(seedClassSubtitle(store->database(), classInfo(
        *classA,
        *teacherA,
        QStringLiteral(" E4 "),
        QStringLiteral("Theseus"),
        {
            {QStringLiteral("Friday"), QStringLiteral("9:00 AM"), QStringLiteral("9:55 AM")},
            {QStringLiteral("Monday"), QStringLiteral("10:00 AM"), QStringLiteral("10:55 AM")}
        }
        ), &seedError), qPrintable(seedError));
    QVERIFY2(seedClassSubtitle(store->database(), classInfo(
        *classB,
        *teacherB,
        QStringLiteral("M2"),
        QStringLiteral("Hydra"),
        {{QStringLiteral("Wednesday"), QStringLiteral("2:00 PM"), QStringLiteral("2:55 PM")}}
        ), &seedError), qPrintable(seedError));
    QVERIFY2(seedClassSubtitle(store->database(), classInfo(
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
missingParentsDoNotExposeOrphanSubtitleData()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    TeacherRepository* const teacherRepository = store->teacherRepository();
    ClassInfoRepository* const classRepository = store->classInfoRepository();
    QVERIFY(teacherRepository);
    QVERIFY(classRepository);

    const auto populatedTeacher = teacherRepository->saveTeacher(
        teacher(QStringLiteral("Populated sibling"))
        );
    const auto orphanTeacher = teacherRepository->saveTeacher(
        teacher(QStringLiteral("Deleted orphan"))
        );
    QVERIFY(populatedTeacher);
    QVERIFY(orphanTeacher);

    const auto populatedClass = services.classService()->create(
        QStringLiteral("Populated sibling")
        );
    const auto deletedClass = services.classService()->create(
        QStringLiteral("Deleted orphan")
        );
    const auto noInfoClass = services.classService()->create(
        QStringLiteral("Existing class without subtitle metadata")
        );
    QVERIFY(populatedClass);
    QVERIFY(deletedClass);
    QVERIFY(noInfoClass);
    QCOMPARE(*populatedClass, 1);
    QCOMPARE(*deletedClass, 2);
    QCOMPARE(*noInfoClass, 3);

    QString seedError;
    QVERIFY2(seedClassSubtitle(store->database(), classInfo(
        *populatedClass,
        *populatedTeacher,
        QStringLiteral("Sibling grade"),
        QStringLiteral("Sibling level"),
        {{QStringLiteral("Monday"), QStringLiteral("8:00 AM"), QStringLiteral("8:55 AM")}}
        ), &seedError), qPrintable(seedError));
    QVERIFY2(seedClassSubtitle(store->database(), classInfo(
        *deletedClass,
        *orphanTeacher,
        QStringLiteral("Orphan grade"),
        QStringLiteral("Orphan level"),
        {{QStringLiteral("Thursday"), QStringLiteral("6:00 PM"), QStringLiteral("6:55 PM")}}
        ), &seedError), qPrintable(seedError));
    QVERIFY2(seedClassSchedule(
        store->database(),
        *noInfoClass,
        {
            {QStringLiteral("Friday"), QStringLiteral("9:00 AM"), QStringLiteral("9:55 AM")},
            {QStringLiteral("Monday"), QStringLiteral("10:00 AM"), QStringLiteral("10:55 AM")}
        },
        &seedError
        ), qPrintable(seedError));

    QSqlQuery disableForeignKeys(store->database());
    QVERIFY(disableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys=OFF")));
    QSqlQuery deleteParent(store->database());
    QVERIFY(deleteParent.prepare(QStringLiteral("DELETE FROM classes WHERE id=?")));
    deleteParent.addBindValue(*deletedClass);
    QVERIFY2(deleteParent.exec(), qPrintable(deleteParent.lastError().text()));
    QCOMPARE(deleteParent.numRowsAffected(), 1);
    QSqlQuery enableForeignKeys(store->database());
    QVERIFY(enableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys=ON")));

    QSqlQuery verifyOrphanRows(store->database());
    QVERIFY(verifyOrphanRows.prepare(QStringLiteral(R"(
        SELECT
            (SELECT COUNT(*) FROM classes WHERE id=?),
            (SELECT COUNT(*) FROM class_info WHERE class_id=?),
            (SELECT COUNT(*) FROM class_times WHERE class_id=?),
            (SELECT COUNT(*) FROM class_info WHERE class_id=? AND teacher_id=?)
    )")));
    verifyOrphanRows.addBindValue(*deletedClass);
    verifyOrphanRows.addBindValue(*deletedClass);
    verifyOrphanRows.addBindValue(*deletedClass);
    verifyOrphanRows.addBindValue(*deletedClass);
    verifyOrphanRows.addBindValue(*orphanTeacher);
    QVERIFY2(verifyOrphanRows.exec(), qPrintable(verifyOrphanRows.lastError().text()));
    QVERIFY(verifyOrphanRows.next());
    QCOMPARE(verifyOrphanRows.value(0).toInt(), 0);
    QCOMPARE(verifyOrphanRows.value(1).toInt(), 1);
    QCOMPARE(verifyOrphanRows.value(2).toInt(), 1);
    QCOMPARE(verifyOrphanRows.value(3).toInt(), 1);
    QVERIFY(!verifyOrphanRows.next());

    Platform::ApplicationServicesSelectedClassSubtitleBatchReadPort port(
        &services
        );
    const Application::SelectedClassSubtitleBatchReadQuery query(port);
    constexpr int neverPresentClassId = 987654321;
    const auto result = query.execute(ids({
        *deletedClass,
        neverPresentClassId,
        *populatedClass,
        *noInfoClass
    }));
    QVERIFY(result);
    QCOMPARE(result.value().size(), std::size_t(4));
    QVERIFY(result.value()[0].classId == classId(*deletedClass));
    QVERIFY(result.value()[1].classId == classId(neverPresentClassId));
    QVERIFY(result.value()[2].classId == classId(*populatedClass));
    QVERIFY(result.value()[3].classId == classId(*noInfoClass));

    for (const std::size_t index : {std::size_t(0), std::size_t(1)})
    {
        QVERIFY(!result.value()[index].classFields);
        QCOMPARE(result.value()[index].classFields.error().code,
                 Domain::ErrorCode::NotFound);
        QVERIFY(!result.value()[index].assignedTeacher);
        QCOMPARE(result.value()[index].assignedTeacher.error().code,
                 Domain::ErrorCode::NotFound);
    }

    const auto& populated = result.value()[2];
    QVERIFY(populated.classFields);
    QCOMPARE(populated.classFields.value().classGrade,
             std::u16string(u"Sibling grade"));
    QCOMPARE(populated.classFields.value().classLevel,
             std::u16string(u"Sibling level"));
    QCOMPARE(populated.classFields.value().regularSchedule.size(), std::size_t(1));
    QVERIFY((populated.classFields.value().regularSchedule[0] ==
        Application::SelectedClassSubtitleScheduleRow{u"Monday", u"8:00 AM"}));
    QVERIFY(populated.assignedTeacher);
    QVERIFY(populated.assignedTeacher.value().has_value());
    QCOMPARE(populated.assignedTeacher.value()->teacherEn,
             std::u16string(u"Populated sibling"));

    const auto& noInfo = result.value()[3];
    QVERIFY(noInfo.classFields);
    QVERIFY(noInfo.classFields.value().classGrade.empty());
    QVERIFY(noInfo.classFields.value().classLevel.empty());
    QCOMPARE(noInfo.classFields.value().regularSchedule.size(), std::size_t(2));
    QVERIFY((noInfo.classFields.value().regularSchedule[0] ==
        Application::SelectedClassSubtitleScheduleRow{u"Friday", u"9:00 AM"}));
    QVERIFY((noInfo.classFields.value().regularSchedule[1] ==
        Application::SelectedClassSubtitleScheduleRow{u"Monday", u"10:00 AM"}));
    QVERIFY(noInfo.assignedTeacher);
    QVERIFY(!noInfo.assignedTeacher.value().has_value());

    const auto classMetrics = classRepository->classSubtitleBatchReadMetrics();
    QCOMPARE(classMetrics.callCount, 1);
    QCOMPARE(classMetrics.requestedClassCount, 4);
    QCOMPARE(classMetrics.metadataStatementCount, 1);
    QCOMPARE(classMetrics.regularScheduleStatementCount, 1);
    const auto teacherMetrics = teacherRepository->teacherDisplayNameBatchReadMetrics();
    QCOMPARE(teacherMetrics.callCount, 1);
    QCOMPARE(teacherMetrics.requestedTeacherCount, 1);
    QCOMPARE(teacherMetrics.statementCount, 1);
}

void NextPlatformApplicationServicesSelectedClassSubtitleBatchReadPortTests::
readsDisplayNamesForWideAssignedTeacherId()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);

    constexpr qlonglong wideTeacherId = 2'147'483'648LL;
    QSqlQuery insertTeacher(store->database());
    QVERIFY(insertTeacher.prepare(QStringLiteral(R"(
        INSERT INTO teachers (
            id, teacher_kr, teacher_en, preferred_romanization, preferred_name
        ) VALUES (?, ?, ?, ?, ?)
    )")));
    insertTeacher.addBindValue(QVariant::fromValue<qlonglong>(wideTeacherId));
    insertTeacher.addBindValue(QStringLiteral("Wide Teacher Korean"));
    insertTeacher.addBindValue(QStringLiteral("Wide Teacher English"));
    insertTeacher.addBindValue(QStringLiteral("Wide Romanized Name"));
    insertTeacher.addBindValue(QStringLiteral("Wide Preferred Name"));
    QVERIFY2(insertTeacher.exec(), qPrintable(insertTeacher.lastError().text()));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Wide Teacher Subtitle Batch")
        );
    QVERIFY(createdClass);
    QString seedError;
    QVERIFY2(seedClassSubtitle(store->database(), classInfo(
        *createdClass,
        -1,
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        {{QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:55 AM")}}
        ), &seedError), qPrintable(seedError));

    QSqlQuery assignTeacher(store->database());
    QVERIFY(assignTeacher.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id = ? WHERE class_id = ?")));
    assignTeacher.addBindValue(QVariant::fromValue<qlonglong>(wideTeacherId));
    assignTeacher.addBindValue(*createdClass);
    QVERIFY2(assignTeacher.exec(), qPrintable(assignTeacher.lastError().text()));
    QCOMPARE(assignTeacher.numRowsAffected(), 1);

    QSqlQuery verifyStoredId(store->database());
    QVERIFY(verifyStoredId.prepare(QStringLiteral(R"(
        SELECT typeof(teachers.id), teachers.id,
               typeof(class_info.teacher_id), class_info.teacher_id
        FROM teachers
        INNER JOIN class_info ON class_info.teacher_id = teachers.id
        WHERE class_info.class_id = ?
    )")));
    verifyStoredId.addBindValue(*createdClass);
    QVERIFY2(verifyStoredId.exec(), qPrintable(verifyStoredId.lastError().text()));
    QVERIFY(verifyStoredId.next());
    QCOMPARE(verifyStoredId.value(0).toString(), QStringLiteral("integer"));
    QCOMPARE(verifyStoredId.value(1).toLongLong(), wideTeacherId);
    QCOMPARE(verifyStoredId.value(2).toString(), QStringLiteral("integer"));
    QCOMPARE(verifyStoredId.value(3).toLongLong(), wideTeacherId);
    QVERIFY(!verifyStoredId.next());

    Platform::ApplicationServicesSelectedClassSubtitleBatchReadPort port(
        &services
        );
    const Application::SelectedClassSubtitleBatchReadQuery query(port);
    const auto result = query.execute(ids({*createdClass}));
    QVERIFY(result);
    QCOMPARE(result.value().size(), std::size_t(1));
    const auto& snapshot = result.value().front();
    QVERIFY(snapshot.classId == classId(*createdClass));
    QVERIFY(snapshot.classFields);
    QCOMPARE(snapshot.classFields.value().classGrade, std::u16string(u"E4"));
    QVERIFY(snapshot.assignedTeacher);
    QVERIFY(snapshot.assignedTeacher.value().has_value());
    const auto& fields = snapshot.assignedTeacher.value().value();
    QCOMPARE(fields.teacherKr, std::u16string(u"Wide Teacher Korean"));
    QCOMPARE(fields.teacherEn, std::u16string(u"Wide Teacher English"));
    QCOMPARE(fields.preferredRomanization, std::u16string(u"Wide Romanized Name"));
    QCOMPARE(fields.preferredName, std::u16string(u"Wide Preferred Name"));
}

void NextPlatformApplicationServicesSelectedClassSubtitleBatchReadPortTests::
missingAssignmentsAndReadFailuresRemainIndependent()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    TeacherRepository* const teacherRepository = store->teacherRepository();
    ClassInfoRepository* const classRepository = store->classInfoRepository();
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
    QVERIFY2(seedClassSubtitle(store->database(), classInfo(
        *assignedClass,
        *teacherId,
        QStringLiteral("E4"),
        QStringLiteral("Orion"),
        {{QStringLiteral("Monday"), QStringLiteral("4:00 PM"), QStringLiteral("4:50 PM")}}
        ), &seedError), qPrintable(seedError));
    QVERIFY2(seedClassSubtitle(store->database(), classInfo(
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

    QSqlQuery disableForeignKeys(store->database());
    QVERIFY(disableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys=OFF")));
    QSqlQuery deleteTeacher(store->database());
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

    QSqlQuery dropTeachers(store->database());
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

    QSqlQuery dropTimes(store->database());
    QVERIFY(dropTimes.exec(QStringLiteral("DROP TABLE class_times")));
    const auto classReadFailure = query.execute(ids({*assignedClass}));
    QVERIFY(classReadFailure);
    QVERIFY(!classReadFailure.value().front().classFields);
    QCOMPARE(classReadFailure.value().front().classFields.error().code,
             Domain::ErrorCode::Technical);
    QVERIFY(classReadFailure.value().front().assignedTeacher);
    QVERIFY(!classReadFailure.value().front().assignedTeacher.value().has_value());
}

void NextPlatformApplicationServicesSelectedClassSubtitleBatchReadPortTests::
malformedPersistedTeacherIdIsValidationForOrderedBatch()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);

    const auto assignedTeacher = store->teacherRepository()->saveTeacher(
        teacher(QStringLiteral("Must not be looked up"))
        );
    QVERIFY(assignedTeacher);
    const auto assignedClass = services.classService()->create(
        QStringLiteral("Assigned sibling")
        );
    const auto malformedClass = services.classService()->create(
        QStringLiteral("Malformed persisted teacher ID")
        );
    const auto unassignedClass = services.classService()->create(
        QStringLiteral("Unassigned sibling")
        );
    QVERIFY(assignedClass);
    QVERIFY(malformedClass);
    QVERIFY(unassignedClass);

    QString seedError;
    const QList<ClassTime> times{
        {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:55 AM")}
    };
    QVERIFY2(seedClassSubtitle(store->database(), classInfo(
        *assignedClass,
        *assignedTeacher,
        QStringLiteral("E4"),
        QStringLiteral("Orion"),
        times
        ), &seedError), qPrintable(seedError));
    QVERIFY2(seedClassSubtitle(store->database(), classInfo(
        *malformedClass,
        -1,
        QStringLiteral("E5"),
        QStringLiteral("Hydra"),
        times
        ), &seedError), qPrintable(seedError));
    QVERIFY2(seedClassSubtitle(store->database(), classInfo(
        *unassignedClass,
        -1,
        QStringLiteral("M1"),
        QStringLiteral("Theseus"),
        times
        ), &seedError), qPrintable(seedError));

    QSqlQuery disableForeignKeys(store->database());
    QVERIFY2(disableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys=OFF")),
             qPrintable(disableForeignKeys.lastError().text()));
    QSqlQuery corruptTeacherId(store->database());
    QVERIFY(corruptTeacherId.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id = ? WHERE class_id = ?")));
    corruptTeacherId.addBindValue(QStringLiteral("not-a-number"));
    corruptTeacherId.addBindValue(*malformedClass);
    QVERIFY2(corruptTeacherId.exec(),
             qPrintable(corruptTeacherId.lastError().text()));
    QCOMPARE(corruptTeacherId.numRowsAffected(), 1);

    QSqlQuery verifyStoredId(store->database());
    QVERIFY(verifyStoredId.prepare(QStringLiteral(
        "SELECT typeof(teacher_id), teacher_id FROM class_info WHERE class_id = ?")));
    verifyStoredId.addBindValue(*malformedClass);
    QVERIFY2(verifyStoredId.exec(), qPrintable(verifyStoredId.lastError().text()));
    QVERIFY(verifyStoredId.next());
    QCOMPARE(verifyStoredId.value(0).toString(), QStringLiteral("text"));
    QCOMPARE(verifyStoredId.value(1).toString(), QStringLiteral("not-a-number"));
    QVERIFY(!verifyStoredId.next());

    Platform::ApplicationServicesSelectedClassSubtitleBatchReadPort port(
        &services
        );
    const Application::SelectedClassSubtitleBatchReadQuery query(port);
    const auto requestedIds = ids({
        *unassignedClass,
        *malformedClass,
        *assignedClass
    });
    const auto result = query.execute(requestedIds);

    QVERIFY(result);
    QCOMPARE(result.value().size(), std::size_t(3));
    const std::vector<Domain::ClassId> expectedIds{
        classId(*unassignedClass),
        classId(*malformedClass),
        classId(*assignedClass)
    };
    const std::string expectedDiagnostic =
        "Loading class subtitles failed: the database returned an invalid teacher identifier.";
    for (std::size_t index = 0; index < expectedIds.size(); ++index)
    {
        QVERIFY(result.value()[index].classId == expectedIds[index]);
        QVERIFY(!result.value()[index].classFields);
        QCOMPARE(result.value()[index].classFields.error().code,
                 Domain::ErrorCode::Validation);
        QVERIFY(!result.value()[index].classFields.error().recoverable);
        QCOMPARE(result.value()[index].classFields.error().message,
                 expectedDiagnostic);
        QVERIFY(result.value()[index].assignedTeacher);
        QVERIFY(!result.value()[index].assignedTeacher.value().has_value());
    }

    const auto classMetrics =
        store->classInfoRepository()->classSubtitleBatchReadMetrics();
    QCOMPARE(classMetrics.callCount, 1);
    QCOMPARE(classMetrics.requestedClassCount, 3);
    QCOMPARE(classMetrics.metadataStatementCount, 1);
    QCOMPARE(classMetrics.regularScheduleStatementCount, 0);
    const auto teacherMetrics =
        store->teacherRepository()->teacherDisplayNameBatchReadMetrics();
    QCOMPARE(teacherMetrics.callCount, 0);
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
