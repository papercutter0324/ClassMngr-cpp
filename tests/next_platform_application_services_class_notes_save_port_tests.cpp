#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "next/application/class_notes_save_use_case.h"
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
    WorkspaceStore* const store = services.workspaceStore();
    return store && store->isOpen()
        ? store->classInfoRepository()
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
    void unknownPositiveClassIdReturnsNotFoundAndPreservesLegacyDiagnostic();
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

    const RepositoryResult<ClassInfo> loaded = repository->loadClassInfo(classId);
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

    QSqlQuery query(services.workspaceStore()->database());
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

    const RepositoryResult<ClassInfo> loaded = repository->loadClassInfo(classId);
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
    QVERIFY(unavailable.error().recoverable);
    QVERIFY(fromUtf8(unavailable.error().message).contains(
        QStringLiteral("unavailable")
        ));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices closedServices;
    QVERIFY(closedServices.openDatabase(databasePath(directory)));
    QVERIFY(createClass(closedServices) > 0);
    closedServices.closeDatabase();
    QVERIFY(!closedServices.workspaceStore()->isOpen());

    Platform::ApplicationServicesClassNotesSavePort closedPort(closedServices);
    const Application::ClassNotesSaveResult closed =
        closedPort.saveClassNotes(
            request("1", u"Notes", u"Activities")
            );
    QVERIFY(!closed);
    QCOMPARE(closed.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(closed.error().recoverable);
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

    QSqlQuery query(services.workspaceStore()->database());
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

    const RepositoryResult<ClassInfo> loaded = repository->loadClassInfo(classId);
    QVERIFY(loaded);
    QCOMPARE(loaded->notes, QStringLiteral("Original notes"));
    QCOMPARE(
        loaded->timeFillerActivities,
        QStringLiteral("Original activities")
        );
}

void NextPlatformApplicationServicesClassNotesSavePortTests::
unknownPositiveClassIdReturnsNotFoundAndPreservesLegacyDiagnostic()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int existingClassId = createClass(services);
    QVERIFY(existingClassId > 0);
    const int unknownClassId = existingClassId + 100000;
    ClassInfoRepository* const repository = classInfoRepository(services);
    QVERIFY(repository);

    QSqlDatabase database = services.workspaceStore()->database();
    QSqlQuery classBeforeQuery(database);
    classBeforeQuery.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM classes WHERE id=?"
        ));
    classBeforeQuery.addBindValue(unknownClassId);
    QVERIFY(classBeforeQuery.exec());
    QVERIFY(classBeforeQuery.next());
    QCOMPARE(classBeforeQuery.value(0).toInt(), 0);

    QSqlQuery classInfoBeforeQuery(database);
    classInfoBeforeQuery.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM class_info WHERE class_id=?"
        ));
    classInfoBeforeQuery.addBindValue(unknownClassId);
    QVERIFY(classInfoBeforeQuery.exec());
    QVERIFY(classInfoBeforeQuery.next());
    QCOMPARE(classInfoBeforeQuery.value(0).toInt(), 0);

    const RepositoryStatus legacySave = repository->saveClassNotes(
        unknownClassId,
        QStringLiteral("Legacy notes"),
        QStringLiteral("Legacy activities")
        );
    QVERIFY(!legacySave);
    QCOMPARE(
        toLegacyDiagnostic(legacySave.error()),
        QStringLiteral(
            "Saving class notes failed for class id %1: FOREIGN KEY constraint "
            "failed Unable to fetch row (database error 787)"
            ).arg(unknownClassId)
        );

    QSqlQuery classInfoAfterLegacyQuery(database);
    classInfoAfterLegacyQuery.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM class_info WHERE class_id=?"
        ));
    classInfoAfterLegacyQuery.addBindValue(unknownClassId);
    QVERIFY(classInfoAfterLegacyQuery.exec());
    QVERIFY(classInfoAfterLegacyQuery.next());
    QCOMPARE(classInfoAfterLegacyQuery.value(0).toInt(), 0);

    Platform::ApplicationServicesClassNotesSavePort port(services);
    const Application::ClassNotesSaveResult typedSave =
        Application::ClassNotesSaveUseCase::execute(
            request(
                std::to_string(unknownClassId),
                u"Typed notes",
                u"Typed activities"
                ),
            port
            );
    QVERIFY(!typedSave);
    QCOMPARE(typedSave.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(!typedSave.error().recoverable);
    QCOMPARE(
        fromUtf8(typedSave.error().message),
        QStringLiteral(
            "Saving class notes failed: parent class id %1 was not found."
            ).arg(unknownClassId)
        );

    QSqlQuery classAfterQuery(database);
    classAfterQuery.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM classes WHERE id=?"
        ));
    classAfterQuery.addBindValue(unknownClassId);
    QVERIFY(classAfterQuery.exec());
    QVERIFY(classAfterQuery.next());
    QCOMPARE(classAfterQuery.value(0).toInt(), 0);

    QSqlQuery classInfoAfterTypedQuery(database);
    classInfoAfterTypedQuery.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM class_info WHERE class_id=?"
        ));
    classInfoAfterTypedQuery.addBindValue(unknownClassId);
    QVERIFY(classInfoAfterTypedQuery.exec());
    QVERIFY(classInfoAfterTypedQuery.next());
    QCOMPARE(classInfoAfterTypedQuery.value(0).toInt(), 0);
}

QTEST_MAIN(NextPlatformApplicationServicesClassNotesSavePortTests)

#include "next_platform_application_services_class_notes_save_port_tests.moc"
