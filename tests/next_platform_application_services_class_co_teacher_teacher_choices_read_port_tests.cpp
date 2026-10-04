#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "next/platform/application_services_class_co_teacher_teacher_choices_read_port.h"

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
        QStringLiteral("co-teacher-teacher-choices-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

int createTeacher(
    ApplicationServices& services,
    const QString& koreanName,
    const QString& englishName,
    const QString& room
    )
{
    Teacher teacher;
    teacher.teacherKr = koreanName;
    teacher.teacherEn = englishName;
    teacher.roomNumber = room;
    teacher.internetType = QStringLiteral("LAN");
    teacher.wifiName = QStringLiteral("  Wi-Fi Network  ");
    teacher.wifiPassword = QStringLiteral("  network password  ");
    teacher.projectionType = QStringLiteral("Zoom");
    teacher.zoomId = QStringLiteral("  meeting-id  ");
    teacher.zoomPassword = QStringLiteral("  meeting password  ");
    const auto result =
        services.databaseSession()->teacherRepository()->createTeacher(
            teacher
            );
    return result.value_or(-1);
}

}

class NextPlatformApplicationServicesClassCoTeacherTeacherChoicesReadPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void mapsTeacherChoicesFromActiveSessionInRepositoryOrder();
    void preservesSuccessfulEmptyTeacherChoiceResults();
    void unavailableSessionDoesNotUseDataServiceFallback();
    void repositoryFailureReturnsStructuredTechnicalError();
    void rejectsNonpositiveTeacherIdsAsNonrecoverableValidation();
};

void NextPlatformApplicationServicesClassCoTeacherTeacherChoicesReadPortTests::
mapsTeacherChoicesFromActiveSessionInRepositoryOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int zuluId = createTeacher(
        services,
        QStringLiteral("  \uAE40\uBBFC\uC900  "),
        QStringLiteral("  Zulu Teacher  "),
        QStringLiteral("  Room Z  ")
        );
    const int alphaId = createTeacher(
        services,
        QStringLiteral("  \uBC15\uBBFC\uC900  "),
        QStringLiteral("  Alpha Teacher  "),
        QStringLiteral("  Room A  ")
        );
    QVERIFY(zuluId > 0);
    QVERIFY(alphaId > 0);

    QSqlQuery rawValues(services.databaseSession()->database());
    QVERIFY(rawValues.exec(QStringLiteral(
        "PRAGMA ignore_check_constraints=ON"
        )));
    QVERIFY(rawValues.exec(QStringLiteral(
        "UPDATE teachers "
        "SET internet_type='  LAN  ', projection_type='  Zoom  '"
        )));

    Platform::ApplicationServicesClassCoTeacherTeacherChoicesReadPort port(
        &services
        );
    const auto result = port.readClassCoTeacherTeacherChoices();

    QVERIFY(result);
    QCOMPARE(result.value().teachers.size(), std::size_t(2));
    const auto& alpha = result.value().teachers[0];
    QCOMPARE(alpha.teacherId.value(), std::to_string(alphaId));
    QVERIFY(alpha.teacherKr == u"  \uBC15\uBBFC\uC900  ");
    QVERIFY(alpha.teacherEn == u"  Alpha Teacher  ");
    QVERIFY(alpha.roomNumber == u"  Room A  ");
    QVERIFY(alpha.internetType == u"  LAN  ");
    QVERIFY(alpha.wifiName == u"  Wi-Fi Network  ");
    QVERIFY(alpha.wifiPassword == u"  network password  ");
    QVERIFY(alpha.projectionType == u"  Zoom  ");
    QVERIFY(alpha.zoomId == u"  meeting-id  ");
    QVERIFY(alpha.zoomPassword == u"  meeting password  ");

    const auto& zulu = result.value().teachers[1];
    QCOMPARE(zulu.teacherId.value(), std::to_string(zuluId));
    QVERIFY(zulu.teacherKr == u"  \uAE40\uBBFC\uC900  ");
    QVERIFY(zulu.teacherEn == u"  Zulu Teacher  ");
    QVERIFY(zulu.roomNumber == u"  Room Z  ");
    QVERIFY(zulu.internetType == u"  LAN  ");
    QVERIFY(zulu.wifiName == u"  Wi-Fi Network  ");
    QVERIFY(zulu.wifiPassword == u"  network password  ");
    QVERIFY(zulu.projectionType == u"  Zoom  ");
    QVERIFY(zulu.zoomId == u"  meeting-id  ");
    QVERIFY(zulu.zoomPassword == u"  meeting password  ");
}

void NextPlatformApplicationServicesClassCoTeacherTeacherChoicesReadPortTests::
preservesSuccessfulEmptyTeacherChoiceResults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    Platform::ApplicationServicesClassCoTeacherTeacherChoicesReadPort port(
        &services
        );

    const auto result = port.readClassCoTeacherTeacherChoices();

    QVERIFY(result);
    QVERIFY(result.value().teachers.empty());
}

void NextPlatformApplicationServicesClassCoTeacherTeacherChoicesReadPortTests::
unavailableSessionDoesNotUseDataServiceFallback()
{
    ApplicationServices services;
    QVERIFY(services.dataService());
    QVERIFY(!services.hasOpenDatabase());
    Platform::ApplicationServicesClassCoTeacherTeacherChoicesReadPort port(
        &services
        );

    const auto result = port.readClassCoTeacherTeacherChoices();

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(result.error().recoverable);
}

void NextPlatformApplicationServicesClassCoTeacherTeacherChoicesReadPortTests::
repositoryFailureReturnsStructuredTechnicalError()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlQuery query(services.databaseSession()->database());
    QVERIFY2(query.exec(QStringLiteral("DROP TABLE teachers")),
             qPrintable(query.lastError().text()));

    Platform::ApplicationServicesClassCoTeacherTeacherChoicesReadPort port(
        &services
        );
    const auto result = port.readClassCoTeacherTeacherChoices();

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().message.empty());
    QVERIFY(!result.error().recoverable);
}

void NextPlatformApplicationServicesClassCoTeacherTeacherChoicesReadPortTests::
rejectsNonpositiveTeacherIdsAsNonrecoverableValidation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlQuery insert(services.databaseSession()->database());
    QVERIFY2(insert.exec(QStringLiteral(
        "INSERT INTO teachers (id, teacher_kr, teacher_en) "
        "VALUES (0, 'Invalid', 'Invalid')"
        )), qPrintable(insert.lastError().text()));

    Platform::ApplicationServicesClassCoTeacherTeacherChoicesReadPort port(
        &services
        );
    const auto result = port.readClassCoTeacherTeacherChoices();

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QVERIFY(!result.error().recoverable);
}

QTEST_MAIN(
    NextPlatformApplicationServicesClassCoTeacherTeacherChoicesReadPortTests
    )

#include "next_platform_application_services_class_co_teacher_teacher_choices_read_port_tests.moc"
