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
    const int weekday,
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
    void repositoryOwnsSelfOverlapAndBoundaryBehavior();
    void unavailableSessionsAreNotFoundAndDoNotUseFallback();
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
    QVERIFY(saveClassTimes(
        services,
        source,
        {
            legacyTime(
                QStringLiteral("Monday"),
                QStringLiteral("9:00 AM"),
                QStringLiteral("9:55 AM")
                ),
            legacyTime(
                QStringLiteral("Tuesday"),
                QStringLiteral("10:00 AM"),
                QStringLiteral("10:55 AM")
                )
        }
        ));
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

    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session && session->isOpen());
    ClassInfoRepository* const repository = session->classInfoRepository();
    QVERIFY(repository);

    const std::vector<Domain::ScheduleTime> regularCandidates{
        domainTime(1, 10 * 60 + 15, 11 * 60),
        domainTime(0, 9 * 60 + 10, 9 * 60 + 40)
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
        domainTime(2, 12 * 60 + 5, 12 * 60 + 30)
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
                    domainTime(0, 10 * 60, 10 * 60 + 35),
                    domainTime(0, 10 * 60 + 20, 10 * 60 + 50)
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
                {domainTime(0, 9 * 60 + 55, 10 * 60 + 5)}
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
                {domainTime(0, 9 * 60 + 54, 10 * 60)}
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
                {domainTime(0, 9 * 60, 9 * 60 + 55)}
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
