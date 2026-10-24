#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "next/application/classes_navigation_snapshot.h"
#include "next/platform/application_services_classes_navigation_read_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <utility>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("classes-navigation-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

QString teacherEnglishName()
{
    return QString::fromUcs4(U"Teacher \U0001F9ED");
}

QString missingInfoClassName()
{
    return QString::fromUcs4(U"  Missing Ω \U0001F9ED  ");
}

Domain::ClassId classId(const int value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

Application::ClassesNavigationSnapshotQuery query(
    const QList<QPair<int, QString>>& classes
    )
{
    Application::ClassesNavigationSnapshotQuery result;
    for (const auto& [id, name] : classes)
    {
        result.classes.push_back({
            .classId = classId(id),
            .className = name.toStdU16String()
        });
    }
    return result;
}

bool insertClassInfo(
    QSqlDatabase database,
    const int classIdValue,
    const std::optional<int> teacherId,
    const QString& grade,
    const QString& level
    )
{
    QSqlQuery insert(database);
    insert.prepare(QStringLiteral(
        "INSERT INTO class_info "
        "(class_id, teacher_id, class_grade, class_level) "
        "VALUES (?, %1, ?, ?)"
        ).arg(teacherId ? QStringLiteral("?") : QStringLiteral("NULL")));
    insert.addBindValue(classIdValue);
    if (teacherId)
    {
        insert.addBindValue(*teacherId);
    }
    insert.addBindValue(grade);
    insert.addBindValue(level);
    return insert.exec();
}

bool insertSchedule(
    QSqlDatabase database,
    const QString& table,
    const int classIdValue,
    const QString& day,
    const QString& start,
    const QString& end
    )
{
    QSqlQuery insert(database);
    insert.prepare(QStringLiteral(
        "INSERT INTO %1 (class_id, day, start_time, end_time) "
        "VALUES (?, ?, ?, ?)"
        ).arg(table));
    insert.addBindValue(classIdValue);
    insert.addBindValue(day);
    insert.addBindValue(start);
    insert.addBindValue(end);
    return insert.exec();
}

}

class NextPlatformApplicationServicesClassesNavigationReadPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void readsBatchedCompactSnapshotAndPreservesMissingRowsAndOrder();
    void deletedParentsWithOrphansReturnPerEntryNotFound();
    void closedOrUnavailableSessionFailsWithoutLegacyFallback();
};

void NextPlatformApplicationServicesClassesNavigationReadPortTests::
readsBatchedCompactSnapshotAndPreservesMissingRowsAndOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto missingInfo = services.classService()->create(
        missingInfoClassName()
        );
    const auto validTeacherClass = services.classService()->create(
        QStringLiteral("Zeta 안녕")
        );
    const auto missingTeacherClass = services.classService()->create(
        QStringLiteral("Beta")
        );
    const auto invalidTeacherClass = services.classService()->create(
        QStringLiteral("Alpha")
        );
    QVERIFY(missingInfo);
    QVERIFY(validTeacherClass);
    QVERIFY(missingTeacherClass);
    QVERIFY(invalidTeacherClass);

    const QSqlDatabase database = services.workspaceStore()->database();
    QSqlQuery insertTeacher(database);
    insertTeacher.prepare(QStringLiteral(
        "INSERT INTO teachers (teacher_en, teacher_kr) VALUES (?, ?)"
        ));
    insertTeacher.addBindValue(teacherEnglishName());
    insertTeacher.addBindValue(QStringLiteral("김민지"));
    QVERIFY2(
        insertTeacher.exec(),
        qPrintable(insertTeacher.lastError().text())
        );
    const int teacherId = insertTeacher.lastInsertId().toInt();
    QVERIFY(teacherId > 0);

    QVERIFY2(
        insertClassInfo(
            database,
            *validTeacherClass,
            teacherId,
            QStringLiteral("E4"),
            QStringLiteral("Perseus")
            ),
        "Could not insert valid class metadata."
        );
    QVERIFY2(
        insertClassInfo(
            database,
            *missingTeacherClass,
            std::nullopt,
            QStringLiteral("E5"),
            QStringLiteral("Athena")
            ),
        "Could not insert class metadata with a missing teacher."
        );

    QSqlQuery foreignKeys(database);
    QVERIFY2(foreignKeys.exec(QStringLiteral("PRAGMA foreign_keys = OFF")),
        qPrintable(foreignKeys.lastError().text()));
    QVERIFY2(
        insertClassInfo(
            database,
            *invalidTeacherClass,
            987654321,
            QStringLiteral("M1"),
            QStringLiteral("Solis")
            ),
        qPrintable(foreignKeys.lastError().text())
        );
    QVERIFY2(foreignKeys.exec(QStringLiteral("PRAGMA foreign_keys = ON")),
        qPrintable(foreignKeys.lastError().text()));

    const QString regularTable = QStringLiteral("class_times");
    QVERIFY(insertSchedule(
        database,
        regularTable,
        *validTeacherClass,
        QStringLiteral("Legacy Friday"),
        QStringLiteral(" 4:00 PM "),
        QStringLiteral("4:55 PM")
        ));
    QVERIFY(insertSchedule(
        database,
        regularTable,
        *validTeacherClass,
        QStringLiteral("Monday"),
        QStringLiteral("4:00 PM"),
        QStringLiteral("4:50 PM")
        ));
    QVERIFY(insertSchedule(
        database,
        QStringLiteral("class_intensive_times"),
        *validTeacherClass,
        QStringLiteral("Not a weekday"),
        QStringLiteral("??"),
        QStringLiteral("end text")
        ));
    QVERIFY(insertSchedule(
        database,
        regularTable,
        *missingTeacherClass,
        QStringLiteral("Raw Wednesday"),
        QStringLiteral("10:01"),
        QStringLiteral("10:37")
        ));
    QVERIFY(insertSchedule(
        database,
        regularTable,
        *missingInfo,
        QStringLiteral("Orphan schedule"),
        QStringLiteral("start"),
        QStringLiteral("end")
        ));

    const Application::ClassesNavigationSnapshotQuery requested = query({
        {*missingInfo, missingInfoClassName()},
        {*validTeacherClass, QStringLiteral("Zeta 안녕")},
        {*missingTeacherClass, QStringLiteral("Beta")},
        {*invalidTeacherClass, QStringLiteral("Alpha")}
    });
    Platform::ApplicationServicesClassesNavigationReadPort port(services);
    const auto result =
        Application::ClassesNavigationSnapshotQueryHandler::execute(
            requested,
            port
            );

    QVERIFY2(
        result,
        qPrintable(result ? QString() : QString::fromStdString(result.error().message))
        );
    QCOMPARE(result.value().classes.size(), std::size_t(4));
    QVERIFY(result.value().classes[0].classId == classId(*missingInfo));
    QCOMPARE(result.value().classes[0].className,
             std::u16string(u"  Missing Ω \U0001F9ED  "));
    QVERIFY(result.value().classes[0].details);
    const auto& missingInfoDetails = result.value().classes[0].details.value();
    QVERIFY(missingInfoDetails.grade.empty());
    QVERIFY(missingInfoDetails.level.empty());
    QVERIFY(missingInfoDetails.regularSchedule.empty());
    QVERIFY(missingInfoDetails.intensiveSchedule.empty());
    QVERIFY(missingInfoDetails.teacherEnglishName.empty());
    QVERIFY(missingInfoDetails.teacherKoreanName.empty());

    const auto& valid = result.value().classes[1];
    QVERIFY(valid.classId == classId(*validTeacherClass));
    QCOMPARE(valid.className, std::u16string(u"Zeta 안녕"));
    QVERIFY(valid.details);
    QCOMPARE(valid.details.value().grade, std::u16string(u"E4"));
    QCOMPARE(valid.details.value().level, std::u16string(u"Perseus"));
    QCOMPARE(valid.details.value().regularSchedule.size(), std::size_t(2));
    QCOMPARE(
        valid.details.value().regularSchedule[0].day,
        std::u16string(u"Legacy Friday")
        );
    QCOMPARE(
        valid.details.value().regularSchedule[0].startTime,
        std::u16string(u" 4:00 PM ")
        );
    QCOMPARE(
        valid.details.value().regularSchedule[1].day,
        std::u16string(u"Monday")
        );
    QCOMPARE(valid.details.value().intensiveSchedule.size(), std::size_t(1));
    QCOMPARE(
        valid.details.value().intensiveSchedule[0].day,
        std::u16string(u"Not a weekday")
        );
    QCOMPARE(
        valid.details.value().intensiveSchedule[0].endTime,
        std::u16string(u"end text")
        );
    QCOMPARE(valid.details.value().teacherEnglishName,
             std::u16string(u"Teacher \U0001F9ED"));
    QCOMPARE(valid.details.value().teacherKoreanName, std::u16string(u"김민지"));

    const auto& missingTeacher = result.value().classes[2];
    QVERIFY(missingTeacher.classId == classId(*missingTeacherClass));
    QVERIFY(missingTeacher.details);
    QCOMPARE(
        missingTeacher.details.value().regularSchedule.size(),
        std::size_t(1)
        );
    QCOMPARE(missingTeacher.details.value().regularSchedule[0].day,
             std::u16string(u"Raw Wednesday"));
    QVERIFY(missingTeacher.details.value().teacherEnglishName.empty());
    QVERIFY(missingTeacher.details.value().teacherKoreanName.empty());

    const auto& invalidTeacher = result.value().classes[3];
    QVERIFY(invalidTeacher.classId == classId(*invalidTeacherClass));
    QVERIFY(invalidTeacher.details);
    QCOMPARE(
        invalidTeacher.details.value().grade,
        std::u16string(u"M1")
        );
    QVERIFY(invalidTeacher.details.value().teacherEnglishName.empty());
    QVERIFY(invalidTeacher.details.value().teacherKoreanName.empty());

    const ClassesNavigationReadMetrics& metrics =
        services.workspaceStore()->classInfoRepository()
            ->classesNavigationReadMetrics();
    QCOMPARE(metrics.metadataStatementCount, 1);
    QCOMPARE(metrics.regularScheduleStatementCount, 1);
    QCOMPARE(metrics.intensiveScheduleStatementCount, 1);
}

void NextPlatformApplicationServicesClassesNavigationReadPortTests::
deletedParentsWithOrphansReturnPerEntryNotFound()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto deletedParent = services.classService()->create(
        QStringLiteral("Deleted with orphans")
        );
    const auto populatedParent = services.classService()->create(
        QStringLiteral("Existing populated")
        );
    const auto noInfoParent = services.classService()->create(
        QStringLiteral("Existing without info")
        );
    QVERIFY(deletedParent);
    QVERIFY(populatedParent);
    QVERIFY(noInfoParent);

    const QSqlDatabase database = services.workspaceStore()->database();
    QVERIFY2(
        insertClassInfo(
            database,
            *deletedParent,
            std::nullopt,
            QStringLiteral("G8"),
            QStringLiteral("Orphan metadata")
            ),
        "Could not seed orphan metadata."
        );
    QVERIFY2(
        insertSchedule(
            database,
            QStringLiteral("class_times"),
            *deletedParent,
            QStringLiteral("Orphan Friday"),
            QStringLiteral("orphan start"),
            QStringLiteral("orphan end")
            ),
        "Could not seed an orphan regular schedule."
        );
    QVERIFY2(
        insertSchedule(
            database,
            QStringLiteral("class_intensive_times"),
            *deletedParent,
            QStringLiteral("Orphan intensive"),
            QStringLiteral("orphan intensive start"),
            QStringLiteral("orphan intensive end")
            ),
        "Could not seed an orphan intensive schedule."
        );
    QVERIFY2(
        insertClassInfo(
            database,
            *populatedParent,
            std::nullopt,
            QStringLiteral("G5"),
            QStringLiteral("Existing metadata")
            ),
        "Could not seed populated class metadata."
        );
    QVERIFY(insertSchedule(
        database,
        QStringLiteral("class_times"),
        *populatedParent,
        QStringLiteral("Raw Wednesday"),
        QStringLiteral(" 4:03 PM "),
        QStringLiteral("4:57 PM")
        ));
    QVERIFY(insertSchedule(
        database,
        QStringLiteral("class_intensive_times"),
        *populatedParent,
        QStringLiteral("Raw holiday"),
        QStringLiteral("start text"),
        QStringLiteral("end text")
        ));
    QVERIFY(insertSchedule(
        database,
        QStringLiteral("class_times"),
        *noInfoParent,
        QStringLiteral("Must be suppressed"),
        QStringLiteral("start"),
        QStringLiteral("end")
        ));

    QSqlQuery foreignKeys(database);
    QVERIFY2(
        foreignKeys.exec(QStringLiteral("PRAGMA foreign_keys = OFF")),
        qPrintable(foreignKeys.lastError().text())
        );
    QSqlQuery deleteParent(database);
    deleteParent.prepare(QStringLiteral("DELETE FROM classes WHERE id=?"));
    deleteParent.addBindValue(*deletedParent);
    QVERIFY2(deleteParent.exec(), qPrintable(deleteParent.lastError().text()));
    QCOMPARE(deleteParent.numRowsAffected(), 1);
    QVERIFY2(
        foreignKeys.exec(QStringLiteral("PRAGMA foreign_keys = ON")),
        qPrintable(foreignKeys.lastError().text())
        );

    QSqlQuery orphanCounts(database);
    orphanCounts.prepare(QStringLiteral(R"(
        SELECT
            (SELECT COUNT(*) FROM class_info WHERE class_id=?),
            (SELECT COUNT(*) FROM class_times WHERE class_id=?),
            (SELECT COUNT(*) FROM class_intensive_times WHERE class_id=?)
    )"));
    orphanCounts.addBindValue(*deletedParent);
    orphanCounts.addBindValue(*deletedParent);
    orphanCounts.addBindValue(*deletedParent);
    QVERIFY2(orphanCounts.exec(), qPrintable(orphanCounts.lastError().text()));
    QVERIFY(orphanCounts.next());
    QCOMPARE(orphanCounts.value(0).toInt(), 1);
    QCOMPARE(orphanCounts.value(1).toInt(), 1);
    QCOMPARE(orphanCounts.value(2).toInt(), 1);

    constexpr int neverPresentClassId = 987654321;
    const Application::ClassesNavigationSnapshotQuery requested = query({
        {*deletedParent, QStringLiteral("Deleted with orphans")},
        {neverPresentClassId, QStringLiteral("Never present")},
        {*populatedParent, QStringLiteral("Existing populated")},
        {*noInfoParent, QStringLiteral("Existing without info")}
    });
    Platform::ApplicationServicesClassesNavigationReadPort port(services);
    const auto result =
        Application::ClassesNavigationSnapshotQueryHandler::execute(
            requested,
            port
            );

    QVERIFY2(
        result,
        qPrintable(result ? QString() : QString::fromStdString(result.error().message))
        );
    QCOMPARE(result.value().classes.size(), std::size_t(4));
    for (std::size_t index = 0; index < 2; ++index)
    {
        const auto& entry = result.value().classes[index];
        QVERIFY(entry.classId == requested.classes[index].classId);
        QCOMPARE(entry.className, requested.classes[index].className);
        QVERIFY(!entry.details);
        QCOMPARE(entry.details.error().code, Domain::ErrorCode::NotFound);
        QVERIFY(entry.details.error().recoverable);
    }

    const auto& populated = result.value().classes[2];
    QVERIFY(populated.classId == classId(*populatedParent));
    const auto& populatedDetails = populated.details.value();
    QCOMPARE(populatedDetails.grade, std::u16string(u"G5"));
    QCOMPARE(populatedDetails.level, std::u16string(u"Existing metadata"));
    QCOMPARE(populatedDetails.regularSchedule.size(), std::size_t(1));
    QCOMPARE(populatedDetails.regularSchedule[0].day,
             std::u16string(u"Raw Wednesday"));
    QCOMPARE(populatedDetails.regularSchedule[0].startTime,
             std::u16string(u" 4:03 PM "));
    QCOMPARE(populatedDetails.intensiveSchedule.size(), std::size_t(1));
    QCOMPARE(populatedDetails.intensiveSchedule[0].day,
             std::u16string(u"Raw holiday"));

    const auto& noInfo = result.value().classes[3];
    QVERIFY(noInfo.classId == classId(*noInfoParent));
    QVERIFY(noInfo.details);
    QVERIFY(noInfo.details.value().grade.empty());
    QVERIFY(noInfo.details.value().level.empty());
    QVERIFY(noInfo.details.value().regularSchedule.empty());
    QVERIFY(noInfo.details.value().intensiveSchedule.empty());

    const ClassesNavigationReadMetrics& metrics =
        services.workspaceStore()->classInfoRepository()
            ->classesNavigationReadMetrics();
    QCOMPARE(metrics.metadataStatementCount, 1);
    QCOMPARE(metrics.regularScheduleStatementCount, 1);
    QCOMPARE(metrics.intensiveScheduleStatementCount, 1);
}

void NextPlatformApplicationServicesClassesNavigationReadPortTests::
closedOrUnavailableSessionFailsWithoutLegacyFallback()
{
    const Application::ClassesNavigationSnapshotQuery requested = query({
        {42, QStringLiteral("Class 42")},
        {43, QStringLiteral("Class 43")}
    });

    ApplicationServices unavailableServices;
    Platform::ApplicationServicesClassesNavigationReadPort unavailablePort(
        unavailableServices
        );
    auto result = unavailablePort.readClasses(requested);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices closedServices;
    QVERIFY(closedServices.openDatabase(databasePath(directory)));
    closedServices.closeDatabase();
    Platform::ApplicationServicesClassesNavigationReadPort closedPort(
        closedServices
        );
    result = closedPort.readClasses(requested);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
}

QTEST_GUILESS_MAIN(NextPlatformApplicationServicesClassesNavigationReadPortTests)

#include "next_platform_application_services_classes_navigation_read_port_tests.moc"
