#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/class_info_repository.h"
#include "next/platform/application_services_selected_class_grade_read_port.h"

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
        QStringLiteral("selected-class-grade-read-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Domain::ClassId typedClassId(const int value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

int createClass(ApplicationServices& services, const QString& name)
{
    return services.classService()->create(name).value_or(-1);
}

}

class NextPlatformApplicationServicesSelectedClassGradeReadPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void readsSelectedClassGradeFromActiveRepository();
    void preservesMissingOrEmptyGrade();
    void missingParentHasTypedProvenanceAndLegacyDefault();
    void rejectsNonCanonicalClassId();
    void unavailableOrClosedSessionDoesNotFallBack();
    void repositoryFailureIsReturned();
};

void NextPlatformApplicationServicesSelectedClassGradeReadPortTests::
readsSelectedClassGradeFromActiveRepository()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int id = createClass(services, QStringLiteral("Grade boundary class"));
    QVERIFY(id > 0);

    ClassInfo info;
    info.classId = id;
    info.classGrade = QStringLiteral(" \u3042 \U0001F30C M2 \t");
    QVERIFY(services.databaseSession()->classInfoRepository()
        ->saveClassInfo(info));

    Platform::ApplicationServicesSelectedClassGradeReadPort port(services);
    const auto result = port.readSelectedClassGrade(typedClassId(id));

    QVERIFY(result);
    QVERIFY(result.value().classId == typedClassId(id));
    const QByteArray expectedGradeBytes = info.classGrade.toUtf8();
    QCOMPARE(
        result.value().classGrade.value(),
        std::string(
            expectedGradeBytes.constData(),
            static_cast<std::size_t>(expectedGradeBytes.size())
            )
        );
    const auto& metrics = services.databaseSession()
        ->classInfoRepository()->selectedClassGradeReadMetrics();
    QCOMPARE(metrics.callCount, 1);
    QCOMPARE(metrics.statementCount, 1);
}

void NextPlatformApplicationServicesSelectedClassGradeReadPortTests::
preservesMissingOrEmptyGrade()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int missingId = createClass(
        services,
        QStringLiteral("Missing grade class")
        );
    const int emptyId = createClass(
        services,
        QStringLiteral("Empty grade class")
        );
    QVERIFY(missingId > 0);
    QVERIFY(emptyId > 0);

    ClassInfo emptyInfo;
    emptyInfo.classId = emptyId;
    QVERIFY(services.databaseSession()->classInfoRepository()
        ->saveClassInfo(emptyInfo));

    QSqlQuery query(services.databaseSession()->database());
    query.prepare(QStringLiteral(
        "DELETE FROM class_info WHERE class_id = ?"
        ));
    query.addBindValue(missingId);
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));

    Platform::ApplicationServicesSelectedClassGradeReadPort port(services);
    const auto missing = port.readSelectedClassGrade(typedClassId(missingId));
    const auto empty = port.readSelectedClassGrade(typedClassId(emptyId));

    QVERIFY(missing);
    QVERIFY(empty);
    QVERIFY(missing.value().classId == typedClassId(missingId));
    QVERIFY(empty.value().classId == typedClassId(emptyId));
    QVERIFY(missing.value().classGrade.value().empty());
    QVERIFY(empty.value().classGrade.value().empty());
    const auto& metrics = services.databaseSession()
        ->classInfoRepository()->selectedClassGradeReadMetrics();
    QCOMPARE(metrics.callCount, 2);
    QCOMPARE(metrics.statementCount, 2);
}

void NextPlatformApplicationServicesSelectedClassGradeReadPortTests::
missingParentHasTypedProvenanceAndLegacyDefault()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int deletedId = createClass(
        services,
        QStringLiteral("Deleted grade class")
        );
    QVERIFY(deletedId > 0);

    ClassInfo info;
    info.classId = deletedId;
    info.classGrade = QStringLiteral("M3");
    QVERIFY(services.databaseSession()->classInfoRepository()
        ->saveClassInfo(info));
    QVERIFY(services.databaseSession()->classRepository()
        ->deleteClass(deletedId));

    constexpr int unknownId = 2000000000;
    Platform::ApplicationServicesSelectedClassGradeReadPort port(services);
    const auto unknown = port.readSelectedClassGrade(typedClassId(unknownId));
    const auto deleted = port.readSelectedClassGrade(typedClassId(deletedId));

    QVERIFY(!unknown);
    QCOMPARE(unknown.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(unknown.error().recoverable);
    QVERIFY(!deleted);
    QCOMPARE(deleted.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(deleted.error().recoverable);

    const auto legacyUnknown = services.databaseSession()
        ->classInfoRepository()->loadSelectedClassGradeRecord(unknownId);
    const auto legacyDeleted = services.databaseSession()
        ->classInfoRepository()->loadSelectedClassGradeRecord(deletedId);
    QVERIFY(legacyUnknown);
    QVERIFY(legacyDeleted);
    QCOMPARE(legacyUnknown->classId, unknownId);
    QCOMPARE(legacyDeleted->classId, deletedId);
    QVERIFY(legacyUnknown->classGrade.isEmpty());
    QVERIFY(legacyDeleted->classGrade.isEmpty());

    const auto& metrics = services.databaseSession()
        ->classInfoRepository()->selectedClassGradeReadMetrics();
    QCOMPARE(metrics.callCount, 4);
    QCOMPARE(metrics.statementCount, 4);
}

void NextPlatformApplicationServicesSelectedClassGradeReadPortTests::
rejectsNonCanonicalClassId()
{
    ApplicationServices services;
    Platform::ApplicationServicesSelectedClassGradeReadPort port(services);
    const auto nonCanonicalId = Domain::ClassId::fromString("01");
    QVERIFY(nonCanonicalId.has_value());

    const auto result = port.readSelectedClassGrade(*nonCanonicalId);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(!result.error().recoverable);
}

void NextPlatformApplicationServicesSelectedClassGradeReadPortTests::
unavailableOrClosedSessionDoesNotFallBack()
{
    ApplicationServices unavailableServices;
    QVERIFY(unavailableServices.dataService());
    QVERIFY(!unavailableServices.hasOpenDatabase());
    Platform::ApplicationServicesSelectedClassGradeReadPort unavailablePort(
        unavailableServices
        );

    const auto unavailable =
        unavailablePort.readSelectedClassGrade(typedClassId(42));

    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, Domain::ErrorCode::NotFound);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int id = createClass(services, QStringLiteral("Closed session class"));
    QVERIFY(id > 0);
    ClassInfo info;
    info.classId = id;
    info.classGrade = QStringLiteral("M1");
    QVERIFY(services.databaseSession()->classInfoRepository()
        ->saveClassInfo(info));
    services.closeDatabase();

    Platform::ApplicationServicesSelectedClassGradeReadPort closedPort(services);
    const auto closed = closedPort.readSelectedClassGrade(typedClassId(id));

    QVERIFY(!closed);
    QCOMPARE(closed.error().code, Domain::ErrorCode::NotFound);
}

void NextPlatformApplicationServicesSelectedClassGradeReadPortTests::
repositoryFailureIsReturned()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int id = createClass(services, QStringLiteral("Broken grade class"));
    QVERIFY(id > 0);

    QSqlQuery query(services.databaseSession()->database());
    QVERIFY2(query.exec(QStringLiteral("DROP TABLE class_info")),
             qPrintable(query.lastError().text()));

    Platform::ApplicationServicesSelectedClassGradeReadPort port(services);
    const auto result = port.readSelectedClassGrade(typedClassId(id));

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(result.error().message.find("Loading class information")
        != std::string::npos);
}

QTEST_MAIN(NextPlatformApplicationServicesSelectedClassGradeReadPortTests)

#include "next_platform_application_services_selected_class_grade_read_port_tests.moc"
