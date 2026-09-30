#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "next/platform/application_services_class_notes_save_port.h"

#include <QByteArray>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>
#include <utility>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("class-notes-save-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Application::ClassNotesSaveRequest request(
    std::string classId,
    std::u16string notes,
    std::u16string activities
    )
{
    return {
        .classId = *Domain::ClassId::fromString(classId),
        .notes = std::move(notes),
        .timeFillerActivities = std::move(activities)
    };
}

QString fromUtf8(const std::string& value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

int createClass(ApplicationServices& services)
{
    const auto created = services.classService()->create(
        QStringLiteral("Class Notes Port Test")
        );
    return created ? *created : -1;
}

ClassInfoRepository* classInfoRepository(ApplicationServices& services)
{
    DatabaseSession* const session = services.databaseSession();
    return session && session->isOpen()
        ? session->classInfoRepository()
        : nullptr;
}

}

class NextPlatformApplicationServicesClassNotesSavePortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void savesTrimmedFieldsAtExactUtf16Limit();
    void rejectsInvalidAndOverLimitInputBeforeAnyWrite();
    void mapsUnavailableAndClosedSession();
    void mapsRepositoryWriteFailureAndPreservesStoredNotes();
};

void NextPlatformApplicationServicesClassNotesSavePortTests::
savesTrimmedFieldsAtExactUtf16Limit()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(services);
    QVERIFY(classId > 0);
    ClassInfoRepository* const repository = classInfoRepository(services);
    QVERIFY(repository);

    std::u16string notes = u"  ";
    notes.reserve(Application::kClassNotesSaveMaxTextCodeUnits);
    for (std::size_t index = 0;
         index < (Application::kClassNotesSaveMaxTextCodeUnits - 4) / 2;
         ++index)
    {
        notes.push_back(u'\xD83E');
        notes.push_back(u'\xDDED');
    }
    notes += u"  ";
    QCOMPARE(
        notes.size(),
        Application::kClassNotesSaveMaxTextCodeUnits
        );

    std::u16string activities = u"  ";
    activities.append(
        Application::kClassNotesSaveMaxTextCodeUnits - 4,
        u'a'
        );
    activities += u"  ";
    QCOMPARE(
        activities.size(),
        Application::kClassNotesSaveMaxTextCodeUnits
        );

    Platform::ApplicationServicesClassNotesSavePort port(services);
    const Application::ClassNotesSaveResult saved = port.saveClassNotes(
        request(
            std::to_string(classId),
            std::move(notes),
            std::move(activities)
            )
        );
    QVERIFY(saved);

    const Result<ClassInfo> loaded = repository->loadClassInfo(classId);
    QVERIFY(loaded);
    QCOMPARE(loaded->notes.size(), 9'996);
    QCOMPARE(loaded->notes.toUtf8().size(), 19'992);
    QCOMPARE(loaded->timeFillerActivities, QString(9'996, u'a'));
}

void NextPlatformApplicationServicesClassNotesSavePortTests::
rejectsInvalidAndOverLimitInputBeforeAnyWrite()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(services);
    QVERIFY(classId > 0);
    ClassInfoRepository* const repository = classInfoRepository(services);
    QVERIFY(repository);
    QVERIFY(repository->saveClassNotes(
        classId,
        QStringLiteral("Original notes"),
        QStringLiteral("Original activities")
        ));

    QSqlQuery query(services.databaseSession()->database());
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TRIGGER reject_class_notes_update "
        "BEFORE UPDATE ON class_info "
        "BEGIN "
        "SELECT RAISE(ABORT, 'input validation must run before writing'); "
        "END"
        )));

    Platform::ApplicationServicesClassNotesSavePort port(services);
    const Application::ClassNotesSaveResult invalidClass =
        port.saveClassNotes(request(
            "not-an-integer",
            u"Notes",
            u"Activities"
            ));
    QVERIFY(!invalidClass);
    QCOMPARE(invalidClass.error().code, Domain::ErrorCode::InvalidInput);

    const Application::ClassNotesSaveResult nonPositiveClass =
        port.saveClassNotes(request("0", u"Notes", u"Activities"));
    QVERIFY(!nonPositiveClass);
    QCOMPARE(nonPositiveClass.error().code, Domain::ErrorCode::InvalidInput);

    const Application::ClassNotesSaveResult oversizedNotes =
        port.saveClassNotes(request(
            std::to_string(classId),
            std::u16string(
                Application::kClassNotesSaveMaxTextCodeUnits + 1,
                u'x'
                ),
            u"Activities"
            ));
    QVERIFY(!oversizedNotes);
    QCOMPARE(oversizedNotes.error().code, Domain::ErrorCode::Validation);

    const Application::ClassNotesSaveResult oversizedActivities =
        port.saveClassNotes(request(
            std::to_string(classId),
            u"Notes",
            std::u16string(
                Application::kClassNotesSaveMaxTextCodeUnits + 1,
                u'x'
                )
            ));
    QVERIFY(!oversizedActivities);
    QCOMPARE(oversizedActivities.error().code, Domain::ErrorCode::Validation);

    const Result<ClassInfo> loaded = repository->loadClassInfo(classId);
    QVERIFY(loaded);
    QCOMPARE(loaded->notes, QStringLiteral("Original notes"));
    QCOMPARE(
        loaded->timeFillerActivities,
        QStringLiteral("Original activities")
        );
}

void NextPlatformApplicationServicesClassNotesSavePortTests::
mapsUnavailableAndClosedSession()
{
    ApplicationServices unavailableServices;
    QVERIFY(unavailableServices.dataService());
    Platform::ApplicationServicesClassNotesSavePort unavailablePort(
        unavailableServices
        );
    const Application::ClassNotesSaveResult unavailable =
        unavailablePort.saveClassNotes(
            request("1", u"Notes", u"Activities")
            );
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(fromUtf8(unavailable.error().message).contains(
        QStringLiteral("unavailable")
        ));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices closedServices;
    QVERIFY(closedServices.openDatabase(databasePath(directory)));
    QVERIFY(createClass(closedServices) > 0);
    closedServices.closeDatabase();
    QVERIFY(!closedServices.databaseSession()->isOpen());

    Platform::ApplicationServicesClassNotesSavePort closedPort(closedServices);
    const Application::ClassNotesSaveResult closed =
        closedPort.saveClassNotes(
            request("1", u"Notes", u"Activities")
            );
    QVERIFY(!closed);
    QCOMPARE(closed.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(fromUtf8(closed.error().message).contains(
        QStringLiteral("unavailable")
        ));
}

void NextPlatformApplicationServicesClassNotesSavePortTests::
mapsRepositoryWriteFailureAndPreservesStoredNotes()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(services);
    QVERIFY(classId > 0);
    ClassInfoRepository* const repository = classInfoRepository(services);
    QVERIFY(repository);
    QVERIFY(repository->saveClassNotes(
        classId,
        QStringLiteral("Original notes"),
        QStringLiteral("Original activities")
        ));

    QSqlQuery query(services.databaseSession()->database());
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TRIGGER reject_class_notes_update "
        "BEFORE UPDATE OF notes ON class_info "
        "WHEN NEW.notes = 'Reject notes' "
        "BEGIN "
        "SELECT RAISE(ABORT, 'injected class notes failure'); "
        "END"
        )));

    Platform::ApplicationServicesClassNotesSavePort port(services);
    const Application::ClassNotesSaveResult result = port.saveClassNotes(
        request(
            std::to_string(classId),
            u"Reject notes",
            u"Changed activities"
            )
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(fromUtf8(result.error().message).contains(
        QStringLiteral("Saving class notes")
        ));

    const Result<ClassInfo> loaded = repository->loadClassInfo(classId);
    QVERIFY(loaded);
    QCOMPARE(loaded->notes, QStringLiteral("Original notes"));
    QCOMPARE(
        loaded->timeFillerActivities,
        QStringLiteral("Original activities")
        );
}

QTEST_MAIN(NextPlatformApplicationServicesClassNotesSavePortTests)

#include "next_platform_application_services_class_notes_save_port_tests.moc"
