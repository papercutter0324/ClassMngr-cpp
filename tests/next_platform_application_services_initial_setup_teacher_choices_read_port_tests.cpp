#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/initial_setup_teacher_choices_read_query.h"
#include "next/platform/application_services_initial_setup_teacher_choices_read_port.h"

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
        QStringLiteral("initial-setup-teacher-choices-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Teacher teacher(
    const QString& teacherKr,
    const QString& teacherEn,
    const QString& preferredRomanization,
    const QString& preferredName
    )
{
    Teacher value;
    value.teacherKr = teacherKr;
    value.teacherEn = teacherEn;
    value.preferredRomanization = preferredRomanization;
    value.preferredName = preferredName;
    return value;
}

Domain::TeacherId teacherId(const int value)
{
    const auto parsed = Domain::TeacherId::fromString(std::to_string(value));
    if (!parsed)
    {
        qFatal("Database teacher ID must have a typed representation.");
    }
    return *parsed;
}

}

class NextPlatformApplicationServicesInitialSetupTeacherChoicesReadPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void mapsAllTeacherFieldsInRepositoryOrderFromTheActiveSession();
    void preservesSuccessfulEmptyRepositoryResult();
    void reportsUnavailableSessionAsRecoverableNotFound();
    void reportsTeacherRepositoryReadFailure();
};

void NextPlatformApplicationServicesInitialSetupTeacherChoicesReadPortTests::
mapsAllTeacherFieldsInRepositoryOrderFromTheActiveSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    TeacherRepository* const repository =
        services.databaseSession()->teacherRepository();
    QVERIFY(repository);

    const auto zuluId = repository->createTeacher(teacher(
        QStringLiteral("Korean Zulu"),
        QStringLiteral("Zulu"),
        QStringLiteral("Roman Zulu"),
        QStringLiteral("Preferred Zulu")
        ));
    QVERIFY(zuluId);
    const auto alphaId = repository->createTeacher(teacher(
        QStringLiteral("  \uAE40\uC120\uC0DD  "),
        QStringLiteral("  Alpha  "),
        QStringLiteral("  Roman Alpha  "),
        QStringLiteral("  Preferred Alpha  ")
        ));
    QVERIFY(alphaId);

    const auto repositoryRecords =
        repository->loadInitialSetupTeacherChoiceRecords();
    QVERIFY(repositoryRecords);
    QCOMPARE(repositoryRecords->size(), 2);
    QCOMPARE(repositoryRecords->at(0).teacherId, alphaId.value());
    QCOMPARE(repositoryRecords->at(0).teacherKr,
             QStringLiteral("  \uAE40\uC120\uC0DD  "));
    QCOMPARE(repositoryRecords->at(0).teacherEn, QStringLiteral("  Alpha  "));
    QCOMPARE(repositoryRecords->at(0).preferredRomanization,
             QStringLiteral("  Roman Alpha  "));
    QCOMPARE(repositoryRecords->at(0).preferredName,
             QStringLiteral("  Preferred Alpha  "));
    QCOMPARE(repositoryRecords->at(1).teacherId, zuluId.value());

    Platform::ApplicationServicesInitialSetupTeacherChoicesReadPort port(
        &services
        );
    const Application::InitialSetupTeacherChoicesReadQuery query(port);
    const auto result = query.execute();

    QVERIFY(result);
    QCOMPARE(result.value().teachers.size(), std::size_t(2));
    const auto& alpha = result.value().teachers[0];
    QCOMPARE(alpha.teacherId, teacherId(alphaId.value()));
    QCOMPARE(alpha.teacherKr, std::u16string(u"  \uAE40\uC120\uC0DD  "));
    QCOMPARE(alpha.teacherEn, std::u16string(u"  Alpha  "));
    QCOMPARE(alpha.preferredRomanization, std::u16string(u"  Roman Alpha  "));
    QCOMPARE(alpha.preferredName, std::u16string(u"  Preferred Alpha  "));

    const auto& zulu = result.value().teachers[1];
    QCOMPARE(zulu.teacherId, teacherId(zuluId.value()));
    QCOMPARE(zulu.teacherKr, std::u16string(u"Korean Zulu"));
    QCOMPARE(zulu.teacherEn, std::u16string(u"Zulu"));
    QCOMPARE(zulu.preferredRomanization, std::u16string(u"Roman Zulu"));
    QCOMPARE(zulu.preferredName, std::u16string(u"Preferred Zulu"));
}

void NextPlatformApplicationServicesInitialSetupTeacherChoicesReadPortTests::
preservesSuccessfulEmptyRepositoryResult()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    Platform::ApplicationServicesInitialSetupTeacherChoicesReadPort port(
        &services
        );

    const auto result = port.readInitialSetupTeacherChoices();

    QVERIFY(result);
    QVERIFY(result.value().teachers.empty());
}

void NextPlatformApplicationServicesInitialSetupTeacherChoicesReadPortTests::
reportsUnavailableSessionAsRecoverableNotFound()
{
    ApplicationServices services;
    Platform::ApplicationServicesInitialSetupTeacherChoicesReadPort port(
        &services
        );

    const auto result = port.readInitialSetupTeacherChoices();

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(result.error().recoverable);
    QVERIFY(!result.error().message.empty());
}

void NextPlatformApplicationServicesInitialSetupTeacherChoicesReadPortTests::
reportsTeacherRepositoryReadFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlQuery dropTeachers(services.databaseSession()->database());
    QVERIFY(dropTeachers.exec(QStringLiteral("DROP TABLE teachers")));

    Platform::ApplicationServicesInitialSetupTeacherChoicesReadPort port(
        &services
        );
    const auto result = port.readInitialSetupTeacherChoices();

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().message.empty());
    QVERIFY(result.error().message.find("Loading teachers")
            != std::string::npos);
    QVERIFY(!result.error().recoverable);
}

QTEST_GUILESS_MAIN(
    NextPlatformApplicationServicesInitialSetupTeacherChoicesReadPortTests
    )

#include "next_platform_application_services_initial_setup_teacher_choices_read_port_tests.moc"
