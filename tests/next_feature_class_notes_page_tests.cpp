#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "features/classes/ui/class_notes_page.h"
#include "next/application/class_notes_save_port.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/autosave_coordinator.h"

#include <QTemporaryDir>
#include <QTextEdit>
#include <QUuid>
#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("class-notes-page-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

int createSeededClass(ApplicationServices& services)
{
    const auto created = services.classService()->create(
        QStringLiteral("Class Notes Page Test")
        );
    if (!created)
    {
        return -1;
    }

    const Status saved = services.classService()->saveClassNotes(
        *created,
        QStringLiteral("Saved notes"),
        QStringLiteral("Saved activities")
        );
    return saved ? *created : -1;
}

class RecordingClassNotesSavePort final
    : public Application::ClassNotesSavePort
{
public:
    [[nodiscard]] Application::ClassNotesSaveResult saveClassNotes(
        const Application::ClassNotesSaveRequest& request
        ) const override
    {
        ++callCount;
        lastRequest = request;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<Application::ClassNotesSaveRequest> lastRequest;
    Application::ClassNotesSaveResult result =
        Application::ClassNotesSaveResult::success();
};

class RecordingPromptService final : public IUserPromptService
{
public:
    void showMessage(const PromptRequest& request) override
    {
        requests.push_back(request);
    }

    void showMessageAsync(const PromptRequest& request) override
    {
        requests.push_back(request);
    }

    PromptChoice confirm(const PromptRequest&) override
    {
        return PromptChoice::Canceled;
    }

    UnsavedChangesChoice confirmUnsavedChanges(
        const UnsavedChangesRequest&
        ) override
    {
        return UnsavedChangesChoice::Cancel;
    }

    QString chooseAction(const ActionPromptRequest&) override
    {
        return {};
    }

    std::vector<PromptRequest> requests;
};

}

class NextFeatureClassNotesPageTests final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void defaultPortSavesBothFieldsToPersistence();
    void successfulManualSaveTrimsFieldsAndCancelsAutosave();
    void manualFailureWarnsAndRetainsDirtyState();
    void autosaveFailureRetainsDirtyStateWithoutWarning();
    void discardReloadsSavedValuesAndCancelsAutosave();

private:
    RecordingPromptService m_promptService;
};

void NextFeatureClassNotesPageTests::init()
{
    m_promptService.requests.clear();
    DialogServices::setUserPromptServiceForTesting(&m_promptService);
}

void NextFeatureClassNotesPageTests::cleanup()
{
    DialogServices::setUserPromptServiceForTesting(nullptr);
}

void NextFeatureClassNotesPageTests::
defaultPortSavesBothFieldsToPersistence()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createSeededClass(services);
    QVERIFY(classId > 0);

    ClassNotesPage page(&services);
    page.loadClass(Classroom(QStringLiteral("Class Notes Page Test"), classId));

    const QList<QTextEdit*> editors = page.findChildren<QTextEdit*>();
    QCOMPARE(editors.size(), 2);
    editors[0]->setPlainText(QStringLiteral("  Persisted notes  "));
    editors[1]->setPlainText(QStringLiteral("  Persisted activities  "));
    QVERIFY(page.hasUnsavedChanges());

    QVERIFY(page.saveChanges());
    QVERIFY(!page.hasUnsavedChanges());

    const auto loaded = services.classService()->classInfo(classId);
    QVERIFY(loaded);
    QCOMPARE(loaded->notes, QStringLiteral("Persisted notes"));
    QCOMPARE(loaded->timeFillerActivities, QStringLiteral("Persisted activities"));
    QVERIFY(m_promptService.requests.empty());
}

void NextFeatureClassNotesPageTests::
successfulManualSaveTrimsFieldsAndCancelsAutosave()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createSeededClass(services);
    QVERIFY(classId > 0);

    RecordingClassNotesSavePort savePort;
    ClassNotesPage page(&services, false, nullptr, &savePort);
    page.loadClass(Classroom(QStringLiteral("Class Notes Page Test"), classId));

    const QList<QTextEdit*> editors = page.findChildren<QTextEdit*>();
    QCOMPARE(editors.size(), 2);
    QCOMPARE(editors[0]->toPlainText(), QStringLiteral("Saved notes"));
    QCOMPARE(editors[1]->toPlainText(), QStringLiteral("Saved activities"));

    editors[0]->setPlainText(QStringLiteral("  Updated notes  "));
    editors[1]->setPlainText(QStringLiteral("  Updated activities  "));
    QVERIFY(page.hasUnsavedChanges());

    QVERIFY(page.saveChanges());
    QCOMPARE(savePort.callCount, 1);
    QVERIFY(savePort.lastRequest.has_value());
    QCOMPARE(savePort.lastRequest->classId.value(), std::to_string(classId));
    QVERIFY(savePort.lastRequest->notes == u"Updated notes");
    QVERIFY(savePort.lastRequest->timeFillerActivities == u"Updated activities");
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(m_promptService.requests.empty());

    QTest::qWait(AutosaveCoordinator::DefaultDebounceIntervalMs + 100);
    QCOMPARE(savePort.callCount, 1);
}

void NextFeatureClassNotesPageTests::manualFailureWarnsAndRetainsDirtyState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createSeededClass(services);
    QVERIFY(classId > 0);

    RecordingClassNotesSavePort savePort;
    savePort.result = Application::ClassNotesSaveResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "write rejected",
        .recoverable = false
    });
    ClassNotesPage page(&services, false, nullptr, &savePort);
    page.loadClass(Classroom(QStringLiteral("Class Notes Page Test"), classId));

    const QList<QTextEdit*> editors = page.findChildren<QTextEdit*>();
    QCOMPARE(editors.size(), 2);
    editors[0]->setPlainText(QStringLiteral("Changed notes"));

    QVERIFY(!page.saveChanges());
    QCOMPARE(savePort.callCount, 1);
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(m_promptService.requests.size(), std::size_t(1));
    QCOMPARE(m_promptService.requests.front().severity, PromptSeverity::Warning);
    QCOMPARE(
        m_promptService.requests.front().title,
        QStringLiteral("Save Class Notes")
        );
    QCOMPARE(m_promptService.requests.front().message, QStringLiteral("write rejected"));
}

void NextFeatureClassNotesPageTests::
autosaveFailureRetainsDirtyStateWithoutWarning()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createSeededClass(services);
    QVERIFY(classId > 0);

    RecordingClassNotesSavePort savePort;
    savePort.result = Application::ClassNotesSaveResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "autosave rejected",
        .recoverable = false
    });
    ClassNotesPage page(&services, false, nullptr, &savePort);
    page.loadClass(Classroom(QStringLiteral("Class Notes Page Test"), classId));

    const QList<QTextEdit*> editors = page.findChildren<QTextEdit*>();
    QCOMPARE(editors.size(), 2);
    editors[0]->setPlainText(QStringLiteral("Changed notes"));

    QTRY_COMPARE_WITH_TIMEOUT(savePort.callCount, 1, 2'500);
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(m_promptService.requests.empty());
}

void NextFeatureClassNotesPageTests::discardReloadsSavedValuesAndCancelsAutosave()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createSeededClass(services);
    QVERIFY(classId > 0);

    RecordingClassNotesSavePort savePort;
    ClassNotesPage page(&services, false, nullptr, &savePort);
    page.loadClass(Classroom(QStringLiteral("Class Notes Page Test"), classId));

    const QList<QTextEdit*> editors = page.findChildren<QTextEdit*>();
    QCOMPARE(editors.size(), 2);
    editors[0]->setPlainText(QStringLiteral("Discard me"));
    editors[1]->setPlainText(QStringLiteral("Discard this too"));
    QVERIFY(page.hasUnsavedChanges());

    page.discardChanges();
    QCOMPARE(editors[0]->toPlainText(), QStringLiteral("Saved notes"));
    QCOMPARE(editors[1]->toPlainText(), QStringLiteral("Saved activities"));
    QVERIFY(!page.hasUnsavedChanges());

    QTest::qWait(AutosaveCoordinator::DefaultDebounceIntervalMs + 100);
    QCOMPARE(savePort.callCount, 0);
}

QTEST_MAIN(NextFeatureClassNotesPageTests)

#include "next_feature_class_notes_page_tests.moc"
