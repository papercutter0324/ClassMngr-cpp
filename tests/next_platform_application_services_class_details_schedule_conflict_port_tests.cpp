#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "next/application/class_details_schedule_conflict_query.h"
#include "next/platform/application_services_class_details_schedule_conflict_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("class-details-conflicts-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Domain::ClassId classId(const int value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

Domain::ScheduleTime domainTime(
    const Domain::Weekday weekday,
    const int startMinute,
    const int endMinute
    )
{
    return *Domain::ScheduleTime::fromMinutes(
        weekday,
        startMinute,
        endMinute
        );
}

ClassTime legacyTime(
    const QString& day,
    const QString& start,
    const QString& end
    )
{
    return {day, start, end};
}

int createClass(ApplicationServices& services, const QString& name)
{
    return services.classService()->create(name).value_or(-1);
}

bool saveClassTimes(
    ApplicationServices& services,
    const int classIdValue,
    const QList<ClassTime>& regular,
    const QList<ClassTime>& intensive = {}
    )
{
    auto info = services.classService()->classInfo(classIdValue);
    if (!info)
    {
        return false;
    }
    info->classTimes = regular;
    info->intensiveTimes = intensive;
    return services.classService()->saveClassInfo(*info).has_value();
}

Application::ClassDetailsScheduleConflictRequest request(
    const int id,
    const Application::ClassDetailsScheduleMode mode,
    std::vector<Domain::ScheduleTime> times
    )
{
    return {
        .classId = classId(id),
        .mode = mode,
        .candidateTimes = std::move(times)
    };
}

void compareDisplayConflict(
    const Application::ClassDetailsScheduleConflict& actual,
    const ClassConflict& expected
    )
{
    QCOMPARE(actual.className, expected.className.toStdU16String());
    QCOMPARE(actual.day, expected.day.toStdU16String());
    QCOMPARE(actual.startTime, expected.startTime.toStdU16String());
    QCOMPARE(actual.endTime, expected.endTime.toStdU16String());
    QCOMPARE(
        actual.conflictingClassName,
        expected.conflictingClassName.toStdU16String()
        );
}

void verifyUnavailable(
    const Application::ClassDetailsScheduleConflictResult& result
    )
{
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(!result.error().message.empty());
}

}

class NextPlatformApplicationServicesClassDetailsScheduleConflictPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void readsRegularAndIntensiveConflictsAndPreservesRepositoryOrder();
    void blankNameWidePersistedConflictIdPreservesFullFallback();
    void repositoryOwnsSelfOverlapAndBoundaryBehavior();
    void unavailableSessionsAreNotFoundAndDoNotUseFallback();
    void terminalConflictCursorFailureDiscardsPartialConflicts();
    void missingClassAndRepositoryFailureAreTechnical();
};

void NextPlatformApplicationServicesClassDetailsScheduleConflictPortTests::
readsRegularAndIntensiveConflictsAndPreservesRepositoryOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int target = createClass(
        services,
        QStringLiteral("Selected Conflict Class")
        );
    const int source = createClass(
        services,
        QStringLiteral("Regular Conflict Source")
        );
    const int intensiveSource = createClass(
        services,
        QStringLiteral("Intensive Conflict Source")
        );
    QVERIFY(target > 0);
    QVERIFY(source > 0);
    QVERIFY(intensiveSource > 0);
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session && session->isOpen());
    ClassInfoRepository* const repository = session->classInfoRepository();
    QVERIFY(repository);

    auto sourceInfo = services.classService()->classInfo(source);
    QVERIFY(sourceInfo);
    sourceInfo->classTimes = {
        legacyTime(
            QStringLiteral("Monday"),
            QStringLiteral("9:0 AM"),
            QStringLiteral("9:55 AM")
            ),
        legacyTime(
            QStringLiteral("Tuesday"),
            QStringLiteral("10:00   AM"),
            QStringLiteral("10:55 AM")
            ),
        // This would overlap the Monday candidate if the malformed end
        // time were accepted.
        legacyTime(
            QStringLiteral("Monday"),
            QStringLiteral("9:05 AM"),
            QStringLiteral("9:35 A M")
            ),
        legacyTime(
            QStringLiteral(" Monday "),
            QStringLiteral("9:05 AM"),
            QStringLiteral("9:35 AM")
            ),
        legacyTime(
            QStringLiteral("monday"),
            QStringLiteral("9:05 AM"),
            QStringLiteral("9:35 AM")
            )
    };
    const auto savedSourceInfo = repository->saveClassInfo(*sourceInfo);
    QVERIFY(savedSourceInfo);
    const auto persistedSourceInfo = repository->loadClassInfo(source);
    QVERIFY(persistedSourceInfo);
    QCOMPARE(persistedSourceInfo->classTimes.size(), 5);
    QCOMPARE(
        persistedSourceInfo->classTimes.at(0).startTime,
        QStringLiteral("9:0 AM")
        );
    QCOMPARE(
        persistedSourceInfo->classTimes.at(1).startTime,
        QStringLiteral("10:00   AM")
        );
    QCOMPARE(
        persistedSourceInfo->classTimes.at(2).endTime,
        QStringLiteral("9:35 A M")
        );
    QCOMPARE(
        persistedSourceInfo->classTimes.at(3).day,
        QStringLiteral(" Monday ")
        );
    QCOMPARE(
        persistedSourceInfo->classTimes.at(4).day,
        QStringLiteral("monday")
        );
    QVERIFY(saveClassTimes(
        services,
        intensiveSource,
        {},
        {
            legacyTime(
                QStringLiteral("Wednesday"),
                QStringLiteral("12:00 PM"),
                QStringLiteral("12:55 PM")
                )
        }
        ));

    const std::vector<Domain::ScheduleTime> regularCandidates{
        domainTime(Domain::Weekday::Tuesday, 10 * 60 + 15, 11 * 60),
        domainTime(Domain::Weekday::Monday, 9 * 60 + 10, 9 * 60 + 40)
    };
    const auto rawRegular = repository->getClassTimeConflicts(
        target,
        {
            legacyTime(
                QStringLiteral("Tuesday"),
                QStringLiteral("10:15 AM"),
                QStringLiteral("11:00 AM")
                ),
            legacyTime(
                QStringLiteral("Monday"),
                QStringLiteral("9:10 AM"),
                QStringLiteral("9:40 AM")
                )
        },
        ScheduleType::Regular
        );
    QVERIFY(rawRegular);

    Platform::ApplicationServicesClassDetailsScheduleConflictPort port(
        &services
        );
    const auto regular = Application::ClassDetailsScheduleConflictQuery::execute(
        request(
            target,
            Application::ClassDetailsScheduleMode::Regular,
            regularCandidates
            ),
        port
        );
    QVERIFY(regular);
    QCOMPARE(regular.value().size(), static_cast<std::size_t>(rawRegular->size()));
    for (std::size_t index = 0; index < regular.value().size(); ++index)
    {
        compareDisplayConflict(
            regular.value()[index],
            rawRegular->at(static_cast<qsizetype>(index))
            );
    }
    QCOMPARE(regular.value().size(), std::size_t(2));
    QCOMPARE(regular.value().at(0).day, std::u16string(u"Monday"));
    QCOMPARE(
        regular.value().at(0).conflictingClassName,
        std::u16string(u"Regular Conflict Source")
        );
    QCOMPARE(regular.value().at(1).day, std::u16string(u"Tuesday"));
    QCOMPARE(
        regular.value().at(1).conflictingClassName,
        std::u16string(u"Regular Conflict Source")
        );

    const auto regularWrongMode =
        Application::ClassDetailsScheduleConflictQuery::execute(
            request(
                target,
                Application::ClassDetailsScheduleMode::Intensive,
                regularCandidates
                ),
            port
            );
    QVERIFY(regularWrongMode);
    QVERIFY(regularWrongMode.value().empty());

    const std::vector<Domain::ScheduleTime> intensiveCandidates{
        domainTime(Domain::Weekday::Wednesday, 12 * 60 + 5, 12 * 60 + 30)
    };
    const auto intensive =
        Application::ClassDetailsScheduleConflictQuery::execute(
            request(
                target,
                Application::ClassDetailsScheduleMode::Intensive,
                intensiveCandidates
                ),
            port
            );
    QVERIFY(intensive);
    QCOMPARE(intensive.value().size(), std::size_t(1));
    QCOMPARE(intensive.value().front().className,
        std::u16string(u"Selected Conflict Class"));
    QCOMPARE(intensive.value().front().conflictingClassName,
        std::u16string(u"Intensive Conflict Source"));

    const auto intensiveWrongMode =
        Application::ClassDetailsScheduleConflictQuery::execute(
            request(
                target,
                Application::ClassDetailsScheduleMode::Regular,
                intensiveCandidates
                ),
            port
            );
    QVERIFY(intensiveWrongMode);
    QVERIFY(intensiveWrongMode.value().empty());
}

void NextPlatformApplicationServicesClassDetailsScheduleConflictPortTests::
blankNameWidePersistedConflictIdPreservesFullFallback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int target = createClass(
        services,
        QStringLiteral("Requested Class")
        );
    QVERIFY(target > 0);

    constexpr qlonglong ordinaryConflictingClassId = 42;
    constexpr qlonglong wideConflictingClassId = 4294967297LL;
    QSqlQuery insertOrdinaryClass(services.databaseSession()->database());
    insertOrdinaryClass.prepare(
        QStringLiteral("INSERT INTO classes (id, name) VALUES (?, ?)")
        );
    insertOrdinaryClass.addBindValue(
        QVariant::fromValue<qlonglong>(ordinaryConflictingClassId)
        );
    insertOrdinaryClass.addBindValue(QString{});
    QVERIFY2(
        insertOrdinaryClass.exec(),
        qPrintable(insertOrdinaryClass.lastError().text())
        );

    QSqlQuery insertOrdinaryTime(services.databaseSession()->database());
    insertOrdinaryTime.prepare(
        QStringLiteral(
            "INSERT INTO class_times "
            "(class_id, day, start_time, end_time) VALUES (?, ?, ?, ?)"
            )
        );
    insertOrdinaryTime.addBindValue(
        QVariant::fromValue<qlonglong>(ordinaryConflictingClassId)
        );
    insertOrdinaryTime.addBindValue(QStringLiteral("Monday"));
    insertOrdinaryTime.addBindValue(QStringLiteral("9:00 AM"));
    insertOrdinaryTime.addBindValue(QStringLiteral("9:55 AM"));
    QVERIFY2(
        insertOrdinaryTime.exec(),
        qPrintable(insertOrdinaryTime.lastError().text())
        );

    QSqlQuery insertClass(services.databaseSession()->database());
    insertClass.prepare(
        QStringLiteral("INSERT INTO classes (id, name) VALUES (?, ?)")
        );
    insertClass.addBindValue(
        QVariant::fromValue<qlonglong>(wideConflictingClassId)
        );
    insertClass.addBindValue(QString{});
    QVERIFY2(insertClass.exec(), qPrintable(insertClass.lastError().text()));

    QSqlQuery insertTime(services.databaseSession()->database());
    insertTime.prepare(
        QStringLiteral(
            "INSERT INTO class_times "
            "(class_id, day, start_time, end_time) VALUES (?, ?, ?, ?)"
            )
        );
    insertTime.addBindValue(
        QVariant::fromValue<qlonglong>(wideConflictingClassId)
        );
    insertTime.addBindValue(QStringLiteral("Monday"));
    insertTime.addBindValue(QStringLiteral("9:00 AM"));
    insertTime.addBindValue(QStringLiteral("9:55 AM"));
    QVERIFY2(insertTime.exec(), qPrintable(insertTime.lastError().text()));

    QSqlQuery persisted(services.databaseSession()->database());
    persisted.prepare(
        QStringLiteral(
            "SELECT classes.id, typeof(classes.id), classes.name, "
            "class_times.class_id, typeof(class_times.class_id) "
            "FROM classes INNER JOIN class_times "
            "ON class_times.class_id = classes.id "
            "WHERE classes.id = ?"
            )
        );
    persisted.addBindValue(
        QVariant::fromValue<qlonglong>(wideConflictingClassId)
        );
    QVERIFY2(persisted.exec(), qPrintable(persisted.lastError().text()));
    QVERIFY(persisted.next());
    QCOMPARE(persisted.value(0).toLongLong(), wideConflictingClassId);
    QCOMPARE(persisted.value(1).toString(), QStringLiteral("integer"));
    QVERIFY(persisted.value(2).toString().isEmpty());
    QCOMPARE(persisted.value(3).toLongLong(), wideConflictingClassId);
    QCOMPARE(persisted.value(4).toString(), QStringLiteral("integer"));
    QVERIFY(!persisted.next());

    Platform::ApplicationServicesClassDetailsScheduleConflictPort port(
        &services
        );
    const auto result = Application::ClassDetailsScheduleConflictQuery::execute(
        request(
            target,
            Application::ClassDetailsScheduleMode::Regular,
            {domainTime(Domain::Weekday::Monday, 9 * 60 + 10, 9 * 60 + 40)}
            ),
        port
        );
    QVERIFY(result);
    QCOMPARE(result.value().size(), std::size_t(2));
    bool sawOrdinaryFallback = false;
    bool sawWideFallback = false;
    for (const auto& conflict : result.value())
    {
        if (conflict.conflictingClassName == std::u16string(u"Class 42"))
        {
            sawOrdinaryFallback = true;
        }
        if (
            conflict.conflictingClassName
            == std::u16string(u"Class 4294967297")
            )
        {
            sawWideFallback = true;
        }
        QCOMPARE(conflict.className, std::u16string(u"Requested Class"));
        QCOMPARE(conflict.day, std::u16string(u"Monday"));
    }
    QVERIFY(sawOrdinaryFallback);
    QVERIFY(sawWideFallback);

    persisted.bindValue(
        0,
        QVariant::fromValue<qlonglong>(wideConflictingClassId)
        );
    QVERIFY2(persisted.exec(), qPrintable(persisted.lastError().text()));
    QVERIFY(persisted.next());
    QCOMPARE(persisted.value(0).toLongLong(), wideConflictingClassId);
    QCOMPARE(persisted.value(1).toString(), QStringLiteral("integer"));
    QVERIFY(persisted.value(2).toString().isEmpty());
    QCOMPARE(persisted.value(3).toLongLong(), wideConflictingClassId);
    QCOMPARE(persisted.value(4).toString(), QStringLiteral("integer"));
    QVERIFY(!persisted.next());
}

void NextPlatformApplicationServicesClassDetailsScheduleConflictPortTests::
repositoryOwnsSelfOverlapAndBoundaryBehavior()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int target = createClass(
        services,
        QStringLiteral("Self Overlap Class")
        );
    const int source = createClass(
        services,
        QStringLiteral("Boundary Source")
        );
    QVERIFY(target > 0);
    QVERIFY(source > 0);
    QVERIFY(saveClassTimes(
        services,
        source,
        {
            legacyTime(
                QStringLiteral("Monday"),
                QStringLiteral("9:00 AM"),
                QStringLiteral("9:55 AM")
                )
        }
        ));

    Platform::ApplicationServicesClassDetailsScheduleConflictPort port(
        &services
        );
    const auto selfOverlap =
        Application::ClassDetailsScheduleConflictQuery::execute(
            request(
                target,
                Application::ClassDetailsScheduleMode::Regular,
                {
                    domainTime(Domain::Weekday::Monday, 10 * 60, 10 * 60 + 35),
                    domainTime(Domain::Weekday::Monday, 10 * 60 + 20, 10 * 60 + 50)
                }
                ),
            port
            );
    QVERIFY(selfOverlap);
    QCOMPARE(selfOverlap.value().size(), std::size_t(1));
    QCOMPARE(selfOverlap.value().front().className,
        std::u16string(u"Self Overlap Class"));
    QCOMPARE(selfOverlap.value().front().conflictingClassName,
        std::u16string(u"Self Overlap Class"));

    const auto adjacent =
        Application::ClassDetailsScheduleConflictQuery::execute(
            request(
                target,
                Application::ClassDetailsScheduleMode::Regular,
                {domainTime(Domain::Weekday::Monday, 9 * 60 + 55, 10 * 60 + 5)}
                ),
            port
            );
    QVERIFY(adjacent);
    QVERIFY(adjacent.value().empty());

    const auto oneMinuteOverlap =
        Application::ClassDetailsScheduleConflictQuery::execute(
            request(
                target,
                Application::ClassDetailsScheduleMode::Regular,
                {domainTime(Domain::Weekday::Monday, 9 * 60 + 54, 10 * 60)}
                ),
            port
            );
    QVERIFY(oneMinuteOverlap);
    QCOMPARE(oneMinuteOverlap.value().size(), std::size_t(1));
    QCOMPARE(oneMinuteOverlap.value().front().conflictingClassName,
        std::u16string(u"Boundary Source"));
}

void NextPlatformApplicationServicesClassDetailsScheduleConflictPortTests::
unavailableSessionsAreNotFoundAndDoNotUseFallback()
{
    const auto query = [](ApplicationServices* services) {
        Platform::ApplicationServicesClassDetailsScheduleConflictPort port(
            services
            );
        return Application::ClassDetailsScheduleConflictQuery::execute(
            request(
                42,
                Application::ClassDetailsScheduleMode::Regular,
                {}
                ),
            port
            );
    };

    verifyUnavailable(query(nullptr));

    ApplicationServices unopenedServices;
    QVERIFY(unopenedServices.dataService());
    verifyUnavailable(query(&unopenedServices));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices closedServices;
    QVERIFY(closedServices.openDatabase(databasePath(directory)));
    DatabaseSession* const session = closedServices.databaseSession();
    QVERIFY(session && session->isOpen());
    closedServices.closeDatabase();
    QVERIFY(!session->isOpen());
    verifyUnavailable(query(&closedServices));
}

void NextPlatformApplicationServicesClassDetailsScheduleConflictPortTests::
terminalConflictCursorFailureDiscardsPartialConflicts()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int target = createClass(
        services,
        QStringLiteral("Requested Class")
        );
    const int source = createClass(
        services,
        QStringLiteral("Cursor Source")
        );
    QVERIFY(target > 0);
    QVERIFY(source > 0);
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session && session->isOpen());
    ClassInfoRepository* const repository = session->classInfoRepository();
    QVERIFY(repository);

    const QSqlDatabase database = session->database();
    QSqlQuery schemaQuery(database);
    QVERIFY2(
        schemaQuery.exec(QStringLiteral("DROP TABLE class_times")),
        qPrintable(schemaQuery.lastError().text())
        );
    QVERIFY2(
        schemaQuery.exec(QStringLiteral(R"(
            CREATE TABLE class_time_cursor_rows (
                id INTEGER PRIMARY KEY,
                class_id INTEGER NOT NULL,
                payload TEXT NOT NULL
            )
        )")),
        qPrintable(schemaQuery.lastError().text())
        );
    QVERIFY2(
        schemaQuery.exec(QStringLiteral(R"(
            CREATE VIEW class_times AS
            SELECT
                id,
                class_id,
                json_extract(payload, '$.day') AS day,
                json_extract(payload, '$.start_time') AS start_time,
                json_extract(payload, '$.end_time') AS end_time
            FROM class_time_cursor_rows
        )")),
        qPrintable(schemaQuery.lastError().text())
        );

    QSqlQuery validRow(database);
    validRow.prepare(QStringLiteral(
        "INSERT INTO class_time_cursor_rows (id, class_id, payload) "
        "VALUES (?, ?, ?)"
        ));
    validRow.addBindValue(1);
    validRow.addBindValue(source);
    validRow.addBindValue(QStringLiteral(
        R"({"day":"Monday","start_time":"9:00 AM","end_time":"9:55 AM"})"
        ));
    QVERIFY2(validRow.exec(), qPrintable(validRow.lastError().text()));

    QSqlQuery malformedRow(database);
    malformedRow.prepare(QStringLiteral(
        "INSERT INTO class_time_cursor_rows (id, class_id, payload) "
        "VALUES (?, ?, ?)"
        ));
    malformedRow.addBindValue(2);
    malformedRow.addBindValue(source);
    malformedRow.addBindValue(QStringLiteral("{malformed json"));
    QVERIFY2(
        malformedRow.exec(),
        qPrintable(malformedRow.lastError().text())
        );

    // Keep this SELECT identical to the repository query. The assertions below
    // verify the actual cursor order before exercising the repository and port.
    QSqlQuery rawQuery(database);
    rawQuery.prepare(QStringLiteral(R"(
        SELECT
            times.class_id,
            classes.name AS class_name,
            times.day,
            times.start_time,
            times.end_time
        FROM class_times times
        LEFT JOIN classes
        ON classes.id = times.class_id
        WHERE times.class_id != ?
    )"));
    rawQuery.addBindValue(target);
    QVERIFY2(rawQuery.exec(), qPrintable(rawQuery.lastError().text()));
    QVERIFY2(
        rawQuery.next(),
        "the first production-query row must be the valid overlapping conflict"
        );
    QCOMPARE(rawQuery.value("class_id").toInt(), source);
    QCOMPARE(rawQuery.value("class_name").toString(),
        QStringLiteral("Cursor Source"));
    QCOMPARE(rawQuery.value("day").toString(), QStringLiteral("Monday"));
    QCOMPARE(rawQuery.value("start_time").toString(), QStringLiteral("9:00 AM"));
    QCOMPARE(rawQuery.value("end_time").toString(), QStringLiteral("9:55 AM"));
    QVERIFY2(
        !rawQuery.next(),
        "the malformed JSON row must terminate the cursor after one valid row"
        );
    const QSqlError rawCursorError = rawQuery.lastError();
    QVERIFY2(rawCursorError.isValid(), "the terminal cursor error must be valid");
    QVERIFY2(
        rawCursorError.text().contains(
            QStringLiteral("malformed JSON"),
            Qt::CaseInsensitive
            ),
        qPrintable(rawCursorError.text())
        );
    rawQuery.finish();

    const QList<ClassTime> candidateTimes{
        legacyTime(
            QStringLiteral("Monday"),
            QStringLiteral("9:10 AM"),
            QStringLiteral("9:40 AM")
            )
    };
    const auto repositoryResult = repository->getClassTimeConflicts(
        target,
        candidateTimes,
        ScheduleType::Regular
        );
    QVERIFY(!repositoryResult);
    QVERIFY(repositoryResult.error().contains(
        QStringLiteral("Loading class time conflicts")
        ));
    QVERIFY(repositoryResult.error().contains(
        QStringLiteral("class id %1").arg(target)
        ));

    Platform::ApplicationServicesClassDetailsScheduleConflictPort port(
        &services
        );
    const auto typedResult =
        Application::ClassDetailsScheduleConflictQuery::execute(
            request(
                target,
                Application::ClassDetailsScheduleMode::Regular,
                {domainTime(Domain::Weekday::Monday, 9 * 60 + 10, 9 * 60 + 40)}
                ),
            port
            );
    QVERIFY(!typedResult);
    QCOMPARE(typedResult.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!typedResult.error().message.empty());
}

void NextPlatformApplicationServicesClassDetailsScheduleConflictPortTests::
missingClassAndRepositoryFailureAreTechnical()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classIdValue = createClass(
        services,
        QStringLiteral("Repository Error Class")
        );
    QVERIFY(classIdValue > 0);

    Platform::ApplicationServicesClassDetailsScheduleConflictPort port(
        &services
        );
    const auto missing = Application::ClassDetailsScheduleConflictQuery::execute(
        request(
            999999,
            Application::ClassDetailsScheduleMode::Regular,
            {}
            ),
        port
        );
    QVERIFY(!missing);
    QCOMPARE(missing.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!missing.error().message.empty());

    QSqlQuery query(services.databaseSession()->database());
    QVERIFY2(query.exec(QStringLiteral("DROP TABLE class_times")),
        qPrintable(query.lastError().text()));
    const auto repositoryFailure =
        Application::ClassDetailsScheduleConflictQuery::execute(
            request(
                classIdValue,
                Application::ClassDetailsScheduleMode::Regular,
                {domainTime(Domain::Weekday::Monday, 9 * 60, 9 * 60 + 55)}
                ),
            port
            );
    QVERIFY(!repositoryFailure);
    QCOMPARE(repositoryFailure.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!repositoryFailure.error().message.empty());
}

QTEST_MAIN(
    NextPlatformApplicationServicesClassDetailsScheduleConflictPortTests
    )

#include "next_platform_application_services_class_details_schedule_conflict_port_tests.moc"
