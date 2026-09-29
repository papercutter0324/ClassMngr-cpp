#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "features/classes/ui/class_notes_page.h"
#include "next/application/class_notes_page_read_query.h"
#include "next/application/class_notes_save_port.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/autosave_coordinator.h"
#include "ui/shared/pages/page_header.h"
#include "core/utils/sidebar_node_naming.h"

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

Domain::ClassId typedClassId(const int value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

Application::ClassNotesPageReadResult readSnapshot(
    const int classId,
    Domain::Result<Application::ClassNotesPageFields> classFields,
    Domain::Result<std::u16string> teacherName
    )
{
    return Application::ClassNotesPageReadResult::success({
        typedClassId(classId),
        std::move(classFields),
        std::move(teacherName)
    });
}

Application::ClassNotesPageFields notesPageFields(
    std::u16string grade,
    std::u16string level,
    std::u16string notes,
    std::u16string activities
    )
{
    Application::ClassNotesPageFields fields;
    fields.classGrade = std::move(grade);
    fields.classLevel = std::move(level);
    fields.regularSchedule = {{u"Monday", u"4:00 PM"}};
    fields.notes = std::move(notes);
    fields.timeFillerActivities = std::move(activities);
    return fields;
}

class RecordingClassNotesPageReadPort final
    : public Application::ClassNotesPageReadPort
{
public:
    [[nodiscard]] Application::ClassNotesPageReadResult readClassNotesPage(
        const Domain::ClassId& id
        ) const override
    {
        ++callCount;
        requests.push_back(id);
        return result;
    }

    mutable int callCount = 0;
    mutable std::vector<Domain::ClassId> requests;
    Application::ClassNotesPageReadResult result =
        Application::ClassNotesPageReadResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unconfigured page read",
            .recoverable = false
        });
};

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
    void oversizedManualSaveWarnsWithoutCallingPort();
    void manualFailureWarnsAndRetainsDirtyState();
    void autosaveFailureRetainsDirtyStateWithoutWarning();
    void discardReloadsSavedValuesAndCancelsAutosave();
    void loadsTypedSnapshotAndReadsOnlyForLoadOrDiscard();
    void failedSourceResultsKeepDefaultAndIndependentDisplayBehavior();

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

void NextFeatureClassNotesPageTests::
oversizedManualSaveWarnsWithoutCallingPort()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createSeededClass(services);
    QVERIFY(classId > 0);

    RecordingClassNotesSavePort savePort;
    ClassNotesPage page(&services, false, nullptr, &savePort);
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(Classroom(QStringLiteral("Class Notes Page Test"), classId));

    const QList<QTextEdit*> editors = page.findChildren<QTextEdit*>();
    QCOMPARE(editors.size(), 2);
    editors[0]->setPlainText(QString(
        static_cast<qsizetype>(
            Application::kClassNotesSaveMaxTextCodeUnits + 1
            ),
        QChar(u'n')
        ));
    QVERIFY(page.hasUnsavedChanges());

    QVERIFY(!page.saveChanges());
    QCOMPARE(savePort.callCount, 0);
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(m_promptService.requests.size(), std::size_t(1));
    QCOMPARE(m_promptService.requests.front().severity, PromptSeverity::Warning);
    QCOMPARE(m_promptService.requests.front().title,
        QStringLiteral("Save Class Notes"));
    QVERIFY(m_promptService.requests.front().message.contains(
        QStringLiteral("10,000")
        ));
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

void NextFeatureClassNotesPageTests::
loadsTypedSnapshotAndReadsOnlyForLoadOrDiscard()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createSeededClass(services);
    QVERIFY(classId > 0);

    RecordingClassNotesSavePort savePort;
    RecordingClassNotesPageReadPort readPort;
    readPort.result = readSnapshot(
        classId,
        Domain::Result<Application::ClassNotesPageFields>::success(
            notesPageFields(
                u"E4", u"Theseus", u"  Loaded notes  ",
                u"  Loaded activities  "
                )
            ),
        Domain::Result<std::u16string>::success(u"Display Teacher")
        );
    ClassNotesPage page(&services, false, nullptr, &savePort, &readPort);
    page.setSaveMode(SaveMode::Manual);
    const Classroom classroom(QStringLiteral("Fallback class name"), classId);
    page.loadClass(classroom);

    QCOMPARE(readPort.callCount, 1);
    QVERIFY(readPort.requests.front() == typedClassId(classId));
    const QList<QTextEdit*> editors = page.findChildren<QTextEdit*>();
    QCOMPARE(editors.size(), 2);
    QCOMPARE(editors[0]->toPlainText(), QStringLiteral("Loaded notes"));
    QCOMPARE(editors[1]->toPlainText(), QStringLiteral("Loaded activities"));

    ClassInfo expectedInfo;
    expectedInfo.classGrade = QStringLiteral("E4");
    expectedInfo.classLevel = QStringLiteral("Theseus");
    expectedInfo.classTimes = {
        {QStringLiteral("Monday"), QStringLiteral("4:00 PM"),
         QStringLiteral("4:50 PM")}
    };
    Teacher expectedTeacher;
    expectedTeacher.preferredName = QStringLiteral("Display Teacher");
    auto* header = page.findChild<PageHeader*>();
    QVERIFY(header);
    QCOMPARE(
        header->subtitle(),
        SidebarNodeNaming::formatClassDisplayName(expectedInfo, expectedTeacher)
        );

    page.refresh();
    QCOMPARE(readPort.callCount, 1);
    editors[0]->setPlainText(QStringLiteral("Changed before save"));
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(page.saveChanges());
    QCOMPARE(savePort.callCount, 1);
    QCOMPARE(readPort.callCount, 1);
    QVERIFY(!page.hasUnsavedChanges());

    readPort.result = readSnapshot(
        classId,
        Domain::Result<Application::ClassNotesPageFields>::success(
            notesPageFields(
                u"G2", u"After Load", u"  Reloaded notes  ",
                u"  Reloaded activities  "
                )
            ),
        Domain::Result<std::u16string>::success(u"Second Teacher")
        );
    editors[1]->setPlainText(QStringLiteral("Discard this edit"));
    QVERIFY(page.hasUnsavedChanges());
    page.discardChanges();

    QCOMPARE(readPort.callCount, 2);
    QVERIFY(readPort.requests.back() == typedClassId(classId));
    QCOMPARE(editors[0]->toPlainText(), QStringLiteral("Reloaded notes"));
    QCOMPARE(editors[1]->toPlainText(), QStringLiteral("Reloaded activities"));
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(savePort.callCount, 1);
    ClassInfo reloadedInfo;
    reloadedInfo.classId = classId;
    reloadedInfo.classGrade = QStringLiteral("G2");
    reloadedInfo.classLevel = QStringLiteral("After Load");
    reloadedInfo.classTimes = {
        {QStringLiteral("Monday"), QStringLiteral("4:00 PM"),
         QStringLiteral("4:50 PM")}
    };
    Teacher reloadedTeacher;
    reloadedTeacher.preferredName = QStringLiteral("Second Teacher");
    QCOMPARE(
        header->subtitle(),
        SidebarNodeNaming::formatClassDisplayName(
            reloadedInfo,
            reloadedTeacher
            )
        );
}

void NextFeatureClassNotesPageTests::
failedSourceResultsKeepDefaultAndIndependentDisplayBehavior()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createSeededClass(services);
    QVERIFY(classId > 0);

    RecordingClassNotesPageReadPort readPort;
    ClassNotesPage page(&services, false, nullptr, nullptr, &readPort);
    const Classroom classroom(QStringLiteral("Fallback class name"), classId);
    auto* header = page.findChild<PageHeader*>();
    QVERIFY(header);
    const QList<QTextEdit*> editors = page.findChildren<QTextEdit*>();
    QCOMPARE(editors.size(), 2);

    readPort.result = Application::ClassNotesPageReadResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "page snapshot unavailable",
        .recoverable = false
    });
    page.loadClass(classroom);
    QCOMPARE(header->subtitle(), SidebarNodeNaming::formatClassDisplayName(
        ClassInfo{}, Teacher{}));
    QVERIFY(editors[0]->toPlainText().isEmpty());
    QVERIFY(editors[1]->toPlainText().isEmpty());
    QVERIFY(!page.hasUnsavedChanges());

    readPort.result = readSnapshot(
        classId,
        Domain::Result<Application::ClassNotesPageFields>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "class source failed",
            .recoverable = true
        }),
        Domain::Result<std::u16string>::success(u"Teacher still available")
        );
    page.loadClass(classroom);
    Teacher availableTeacher;
    availableTeacher.preferredName = QStringLiteral("Teacher still available");
    QCOMPARE(header->subtitle(), SidebarNodeNaming::formatClassDisplayName(
        ClassInfo{}, availableTeacher));
    QVERIFY(editors[0]->toPlainText().isEmpty());
    QVERIFY(editors[1]->toPlainText().isEmpty());

    readPort.result = readSnapshot(
        classId,
        Domain::Result<Application::ClassNotesPageFields>::success(
            notesPageFields(
                u"E5", u"Class Text Survives", u"  Retained notes  ",
                u"  Retained activities  "
                )
            ),
        Domain::Result<std::u16string>::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "teacher source failed",
            .recoverable = true
        })
        );
    page.loadClass(classroom);
    QCOMPARE(editors[0]->toPlainText(), QStringLiteral("Retained notes"));
    QCOMPARE(editors[1]->toPlainText(), QStringLiteral("Retained activities"));
    ClassInfo classFields;
    classFields.classGrade = QStringLiteral("E5");
    classFields.classLevel = QStringLiteral("Class Text Survives");
    classFields.classTimes = {
        {QStringLiteral("Monday"), QStringLiteral("4:00 PM"),
         QStringLiteral("4:50 PM")}
    };
    QCOMPARE(header->subtitle(), SidebarNodeNaming::formatClassDisplayName(
        classFields, Teacher{}));
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(readPort.callCount, 3);
}

QTEST_MAIN(NextFeatureClassNotesPageTests)

#include "next_feature_class_notes_page_tests.moc"
