#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "domain/models/class_info.h"
#include "next/platform/application_services_sub_prep_class_details_port.h"

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
        QStringLiteral("sub-prep-class-details-port-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Domain::ClassId classId(const int value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

int createClass(ApplicationServices& services, const QString& name)
{
    const auto created = services.workspaceStore()->classRepository()
        ->createClass(name);
    return created ? *created : -1;
}

}

class NextPlatformApplicationServicesSubPrepClassDetailsPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void readsOwnedClassDetails();
    void mapsMissingParentAndRepositoryFailure();
};

void NextPlatformApplicationServicesSubPrepClassDetailsPortTests::
readsOwnedClassDetails()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const int id = createClass(services, QStringLiteral("Sub Prep details"));
    QVERIFY(id > 0);
    ClassInfo info;
    info.classId = id;
    info.notes = QStringLiteral("Class notes copied into the result");
    QVERIFY(services.workspaceStore()->classInfoRepository()->saveClassInfo(info));

    Platform::ApplicationServicesSubPrepClassDetailsPort port(services);
    const auto result = port.loadDetails(classId(id));

    QVERIFY(result);
    QVERIFY(result.value().classId == classId(id));
    QCOMPARE(
        result.value().classNotes,
        std::string("Class notes copied into the result")
        );
    QVERIFY(!result.value().teacherId);
    const auto owned = result.value();
    services.closeDatabase();
    QVERIFY(owned == result.value());
}

void NextPlatformApplicationServicesSubPrepClassDetailsPortTests::
mapsMissingParentAndRepositoryFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int id = createClass(services, QStringLiteral("Broken details"));
    QVERIFY(id > 0);
    QVERIFY(services.workspaceStore()->classInfoRepository()
        ->saveClassInfo(ClassInfo{.classId = id}));

    Platform::ApplicationServicesSubPrepClassDetailsPort port(services);
    const auto missing = port.loadDetails(classId(id + 100));
    QVERIFY(!missing);
    QCOMPARE(missing.error().code, Domain::ErrorCode::NotFound);

    QSqlQuery dropInfo(services.workspaceStore()->database());
    QVERIFY(dropInfo.exec(QStringLiteral("DROP TABLE class_info")));
    const auto failed = port.loadDetails(classId(id));
    QVERIFY(!failed);
    QCOMPARE(failed.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!failed.error().message.empty());
}

QTEST_GUILESS_MAIN(NextPlatformApplicationServicesSubPrepClassDetailsPortTests)

#include "next_platform_application_services_sub_prep_class_details_port_tests.moc"
