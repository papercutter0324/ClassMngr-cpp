#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "next/platform/application_services_class_notes_save_port.h"

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

}

class NextPlatformApplicationServicesClassNotesSavePortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void savesBothFieldsAndPreservesUtf16Limit();
    void mapsInvalidInputBeforeServiceAccess();
    void mapsUnavailableService();
    void mapsLegacyWriteFailureAndPreservesStoredNotes();
};

void NextPlatformApplicationServicesClassNotesSavePortTests::
savesBothFieldsAndPreservesUtf16Limit()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(services);
    QVERIFY(classId > 0);

    std::u16string emojiNotes;
    emojiNotes.reserve(Application::kClassNotesSaveMaxTextCodeUnits);
    for (std::size_t index = 0;
         index < Application::kClassNotesSaveMaxTextCodeUnits / 2;
         ++index)
    {
        emojiNotes.push_back(u'\xD83E');
        emojiNotes.push_back(u'\xDDED');
    }

    Platform::ApplicationServicesClassNotesSavePort port(services);
    const Application::ClassNotesSaveResult saved = port.saveClassNotes(
        request(
            std::to_string(classId),
            std::move(emojiNotes),
            u"  Filler activity  "
            )
        );
    QVERIFY(saved);

    const auto loaded = services.classService()->classInfo(classId);
    QVERIFY(loaded);
    QCOMPARE(loaded->notes.size(), 10'000);
    QCOMPARE(loaded->notes.toUtf8().size(), 20'000);
    QCOMPARE(loaded->timeFillerActivities, QStringLiteral("Filler activity"));
}

void NextPlatformApplicationServicesClassNotesSavePortTests::
mapsInvalidInputBeforeServiceAccess()
{
    ApplicationServices services;
    Platform::ApplicationServicesClassNotesSavePort port(&services);

    const Application::ClassNotesSaveResult invalidClass =
        port.saveClassNotes(request("not-an-integer", u"Notes", u"Activities"));
    QVERIFY(!invalidClass);
    QCOMPARE(invalidClass.error().code, Domain::ErrorCode::InvalidInput);

    const Application::ClassNotesSaveResult oversizedText =
        port.saveClassNotes(request(
            "1",
            std::u16string(
                Application::kClassNotesSaveMaxTextCodeUnits + 1,
                u'x'
                ),
            u""
            ));
    QVERIFY(!oversizedText);
    QCOMPARE(oversizedText.error().code, Domain::ErrorCode::Validation);
}

void NextPlatformApplicationServicesClassNotesSavePortTests::
mapsUnavailableService()
{
    ApplicationServices services;
    Platform::ApplicationServicesClassNotesSavePort port(services);

    const Application::ClassNotesSaveResult result = port.saveClassNotes(
        request("1", u"Notes", u"Activities")
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(fromUtf8(result.error().message).contains(
        QStringLiteral("unavailable")
        ));
}

void NextPlatformApplicationServicesClassNotesSavePortTests::
mapsLegacyWriteFailureAndPreservesStoredNotes()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(services);
    QVERIFY(classId > 0);
    QVERIFY(services.classService()->saveClassNotes(
        classId,
        QStringLiteral("Original notes"),
        QStringLiteral("Original activities")
        ));

    QSqlQuery query(services.dataService()->databaseSession()->database());
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

    const auto loaded = services.classService()->classInfo(classId);
    QVERIFY(loaded);
    QCOMPARE(loaded->notes, QStringLiteral("Original notes"));
    QCOMPARE(loaded->timeFillerActivities, QStringLiteral("Original activities"));
}

QTEST_MAIN(NextPlatformApplicationServicesClassNotesSavePortTests)

#include "next_platform_application_services_class_notes_save_port_tests.moc"
