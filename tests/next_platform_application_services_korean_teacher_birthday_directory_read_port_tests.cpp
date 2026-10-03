#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/teacher.h"
#include "next/platform/application_services_korean_teacher_birthday_directory_read_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("korean-teacher-birthday-read-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Teacher teacher(
    const QString& koreanName,
    const QString& englishName,
    const QString& romanization,
    const QString& preferredName,
    const QString& birthday
    )
{
    Teacher value;
    value.teacherKr = koreanName;
    value.teacherEn = englishName;
    value.preferredRomanization = romanization;
    value.preferredName = preferredName;
    value.birthday = birthday;
    return value;
}

bool seedTeachers(ApplicationServices& services)
{
    Teacher zulu = teacher(
        QStringLiteral("  \uAE40\uC120\uC0DD  "),
        QStringLiteral(" Zulu "),
        QStringLiteral(" Z Romanization "),
        QStringLiteral(" Preferred Zulu "),
        QStringLiteral(" 12-31 ")
        );
    Teacher alpha = teacher(
        QStringLiteral("Teacher Alpha Korean"),
        QStringLiteral("Alpha"),
        QStringLiteral("A Romanization"),
        QString(),
        QStringLiteral("02-03")
        );
    const auto first = services.databaseSession()->teacherRepository()
        ->createTeacher(zulu);
    const auto second = services.databaseSession()->teacherRepository()
        ->createTeacher(alpha);
    return first && second;
}

}

class NextPlatformApplicationServicesKoreanTeacherBirthdayDirectoryReadPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void readsOnlyBirthdayAndDisplayNameFieldsInRepositoryOrder();
    void distinguishesUnavailableSessionFromRepositoryFailure();
};

void NextPlatformApplicationServicesKoreanTeacherBirthdayDirectoryReadPortTests::
readsOnlyBirthdayAndDisplayNameFieldsInRepositoryOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedTeachers(services));

    Platform::ApplicationServicesKoreanTeacherBirthdayDirectoryReadPort port(
        &services);
    const Application::KoreanTeacherBirthdayDirectoryReadResult result =
        port.readKoreanTeacherBirthdayDirectory();

    QVERIFY(result);
    QCOMPARE(result.value().size(), std::size_t(2));
    QCOMPARE(result.value()[0].birthday, std::u16string(u" 12-31 "));
    QCOMPARE(result.value()[0].teacherKr,
        std::u16string(u"  \uAE40\uC120\uC0DD  "));
    QCOMPARE(result.value()[0].teacherEn, std::u16string(u" Zulu "));
    QCOMPARE(result.value()[0].preferredRomanization,
        std::u16string(u" Z Romanization "));
    QCOMPARE(result.value()[0].preferredName,
        std::u16string(u" Preferred Zulu "));
    QCOMPARE(result.value()[1].birthday, std::u16string(u"02-03"));
    QCOMPARE(result.value()[1].teacherKr,
        std::u16string(u"Teacher Alpha Korean"));
    QCOMPARE(result.value()[1].teacherEn, std::u16string(u"Alpha"));
    QCOMPARE(result.value()[1].preferredRomanization,
        std::u16string(u"A Romanization"));
    QCOMPARE(result.value()[1].preferredName, std::u16string());
}

void NextPlatformApplicationServicesKoreanTeacherBirthdayDirectoryReadPortTests::
distinguishesUnavailableSessionFromRepositoryFailure()
{
    ApplicationServices services;
    QVERIFY(services.dataService());
    QVERIFY(!services.hasOpenDatabase());

    Platform::ApplicationServicesKoreanTeacherBirthdayDirectoryReadPort port(
        &services);
    auto unavailable = port.readKoreanTeacherBirthdayDirectory();
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(unavailable.error().recoverable);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedTeachers(services));

    QSqlQuery query(services.databaseSession()->database());
    QVERIFY2(query.exec(QStringLiteral("DROP TABLE teachers")),
        qPrintable(query.lastError().text()));
    const auto repositoryFailure = port.readKoreanTeacherBirthdayDirectory();
    QVERIFY(!repositoryFailure);
    QCOMPARE(repositoryFailure.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!repositoryFailure.error().message.empty());
    QVERIFY(!repositoryFailure.error().recoverable);

    services.closeDatabase();
    unavailable = port.readKoreanTeacherBirthdayDirectory();
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, Domain::ErrorCode::NotFound);
}

QTEST_MAIN(
    NextPlatformApplicationServicesKoreanTeacherBirthdayDirectoryReadPortTests)

#include "next_platform_application_services_korean_teacher_birthday_directory_read_port_tests.moc"
