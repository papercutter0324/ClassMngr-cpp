#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
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

    const QSqlDatabase database = services.databaseSession()->database();
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
    QVERIFY(result.value().classes[0].grade.empty());
    QVERIFY(result.value().classes[0].level.empty());
    QVERIFY(result.value().classes[0].regularSchedule.empty());
    QVERIFY(result.value().classes[0].intensiveSchedule.empty());
    QVERIFY(result.value().classes[0].teacherEnglishName.empty());
    QVERIFY(result.value().classes[0].teacherKoreanName.empty());

    const auto& valid = result.value().classes[1];
    QVERIFY(valid.classId == classId(*validTeacherClass));
    QCOMPARE(valid.className, std::u16string(u"Zeta 안녕"));
    QCOMPARE(valid.grade, std::u16string(u"E4"));
    QCOMPARE(valid.level, std::u16string(u"Perseus"));
    QCOMPARE(valid.regularSchedule.size(), std::size_t(2));
    QCOMPARE(valid.regularSchedule[0].day, std::u16string(u"Legacy Friday"));
    QCOMPARE(valid.regularSchedule[0].startTime, std::u16string(u" 4:00 PM "));
    QCOMPARE(valid.regularSchedule[1].day, std::u16string(u"Monday"));
    QCOMPARE(valid.intensiveSchedule.size(), std::size_t(1));
    QCOMPARE(valid.intensiveSchedule[0].day, std::u16string(u"Not a weekday"));
    QCOMPARE(valid.intensiveSchedule[0].endTime, std::u16string(u"end text"));
    QCOMPARE(valid.teacherEnglishName,
             std::u16string(u"Teacher \U0001F9ED"));
    QCOMPARE(valid.teacherKoreanName, std::u16string(u"김민지"));

    const auto& missingTeacher = result.value().classes[2];
    QVERIFY(missingTeacher.classId == classId(*missingTeacherClass));
    QCOMPARE(missingTeacher.regularSchedule.size(), std::size_t(1));
    QCOMPARE(missingTeacher.regularSchedule[0].day,
             std::u16string(u"Raw Wednesday"));
    QVERIFY(missingTeacher.teacherEnglishName.empty());
    QVERIFY(missingTeacher.teacherKoreanName.empty());

    const auto& invalidTeacher = result.value().classes[3];
    QVERIFY(invalidTeacher.classId == classId(*invalidTeacherClass));
    QCOMPARE(invalidTeacher.grade, std::u16string(u"M1"));
    QVERIFY(invalidTeacher.teacherEnglishName.empty());
    QVERIFY(invalidTeacher.teacherKoreanName.empty());

    const ClassesNavigationReadMetrics& metrics =
        services.databaseSession()->classInfoRepository()
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
