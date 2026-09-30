#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "features/classes/ui/class_co_teacher_page.h"
#include "next/application/class_co_teacher_assignment_use_case.h"
#include "next/application/class_co_teacher_page_read_query.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/autosave_coordinator.h"
#include "ui/shared/pages/page_header.h"
#include "ui/shared/widgets/sections/teacher_info_section.h"

#include <QComboBox>
#include <QSqlQuery>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>
#include <optional>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("co-teacher-page-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

int createTeacher(ApplicationServices& services)
{
    Teacher teacher;
    teacher.teacherEn = QStringLiteral("Page Teacher");
    teacher.preferredName = teacher.teacherEn;
    const auto saved = services.teacherService()->save(teacher);
    return saved ? *saved : -1;
}

int createClass(ApplicationServices& services)
{
    const auto created = services.classService()->create(
        QStringLiteral("Co-Teacher Page Test")
        );
    return created ? *created : -1;
}

Domain::ClassId typedClassId(const int value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

Domain::TeacherId typedTeacherId(const int value)
{
    return *Domain::TeacherId::fromString(std::to_string(value));
}

bool updatePersistedTeacherId(
    ApplicationServices& services,
    const int classId,
    const int teacherId
    )
{
    QSqlQuery query(services.databaseSession()->database());
    if (!query.exec(QStringLiteral("PRAGMA foreign_keys=OFF")))
    {
        return false;
    }
    query.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id=? WHERE class_id=?"
        ));
    query.addBindValue(teacherId);
    query.addBindValue(classId);
    return query.exec() && query.numRowsAffected() == 1;
}

Application::ClassCoTeacherPageFields readFields(
    const std::optional<int> teacherId,
    std::u16string grade = u"E4",
    std::u16string level = u"Theseus"
    )
{
    Application::ClassCoTeacherPageFields fields;
    if (teacherId)
    {
        fields.selectedTeacherId = typedTeacherId(*teacherId);
    }
    fields.classGrade = std::move(grade);
    fields.classLevel = std::move(level);
    fields.regularSchedule = {{u"Monday", u"4:00 PM"}};
    return fields;
}

Application::ClassCoTeacherPageReadResult pageReadResult(
    const int classId,
    Domain::Result<Application::ClassCoTeacherPageFields> classFields,
    Domain::Result<std::u16string> teacherName
    )
{
    return Application::ClassCoTeacherPageReadResult::success({
        typedClassId(classId),
        std::move(classFields),
        std::move(teacherName)
    });
}

class RecordingAssignmentPort final
    : public Application::ClassCoTeacherAssignmentPort
{
public:
    [[nodiscard]] Domain::Result<void> assignClassCoTeacher(
        const Application::ClassCoTeacherAssignmentRequest& request
        ) const override
    {
        ++callCount;
        requests.push_back(request);
        return result;
    }

    mutable int callCount = 0;
    mutable std::vector<Application::ClassCoTeacherAssignmentRequest> requests;
    Domain::Result<void> result = Domain::Result<void>::success();
};

class RecordingPageReadPort final
    : public Application::ClassCoTeacherPageReadPort
{
public:
    [[nodiscard]] Application::ClassCoTeacherPageReadResult
    readClassCoTeacherPage(
        const Domain::ClassId& id
        ) const override
    {
        ++callCount;
        requests.push_back(id);
        return result;
    }

    mutable int callCount = 0;
    mutable std::vector<Domain::ClassId> requests;
    Application::ClassCoTeacherPageReadResult result =
        Application::ClassCoTeacherPageReadResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unconfigured page read",
            .recoverable = false
        });
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

class NextFeatureClassCoTeacherPageTests final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void defaultPortSavesAssignmentAndRefreshesTitle();
    void manualFailureWarnsAndRetainsDirtyState();
    void loadDiscardAndSuccessfulSaveUseFreshSnapshotsAtExpectedTimes();
    void readFailuresKeepDefaultSelectionAndIndependentTitleSources();
    void nonpositivePersistedTeacherIdRetainsClassTitleAndClearsSelection();

private:
    RecordingPromptService m_promptService;
};

void NextFeatureClassCoTeacherPageTests::init()
{
    m_promptService.requests.clear();
    DialogServices::setUserPromptServiceForTesting(&m_promptService);
}

void NextFeatureClassCoTeacherPageTests::cleanup()
{
    DialogServices::setUserPromptServiceForTesting(nullptr);
}

void NextFeatureClassCoTeacherPageTests::
defaultPortSavesAssignmentAndRefreshesTitle()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(services);
    const int teacherId = createTeacher(services);
    QVERIFY(classId > 0);
    QVERIFY(teacherId > 0);

    ClassCoTeacherPage page(&services);
    page.loadClass(Classroom(QStringLiteral("Co-Teacher Page Test"), classId));
    auto* section = page.findChild<TeacherInfoSection*>();
    auto* header = page.findChild<PageHeader*>();
    QVERIFY(section);
    QVERIFY(header);
    const QString initialSubtitle = header->subtitle();

    QComboBox* teacherSelector = section->teacherSelector();
    QVERIFY(teacherSelector);
    const int teacherIndex = teacherSelector->findData(teacherId);
    QVERIFY(teacherIndex > 0);
    teacherSelector->setCurrentIndex(teacherIndex);
    QVERIFY(page.hasUnsavedChanges());

    QSignalSpy savedSpy(&page, &ClassCoTeacherPage::classInfoSaved);
    QVERIFY(savedSpy.isValid());
    QVERIFY(page.saveChanges());
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(savedSpy.size(), 1);
    QCOMPARE(savedSpy.front().front().toInt(), classId);

    const auto stored = services.classService()->classInfo(classId);
    QVERIFY(stored);
    QCOMPARE(stored->teacherId, teacherId);
    QCOMPARE(section->teacherId(), teacherId);
    QVERIFY(header->subtitle() != initialSubtitle);
    QVERIFY(header->subtitle().contains(QStringLiteral("Page Teacher")));
    QVERIFY(m_promptService.requests.empty());
}

void NextFeatureClassCoTeacherPageTests::manualFailureWarnsAndRetainsDirtyState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(services);
    const int teacherId = createTeacher(services);
    QVERIFY(classId > 0);
    QVERIFY(teacherId > 0);

    RecordingAssignmentPort assignmentPort;
    assignmentPort.result = Domain::Result<void>::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "assignment rejected",
        .recoverable = false
    });
    RecordingPageReadPort readPort;
    readPort.result = pageReadResult(
        classId,
        Domain::Result<Application::ClassCoTeacherPageFields>::success(
            readFields(std::nullopt)
            ),
        Domain::Result<std::u16string>::success({})
        );
    ClassCoTeacherPage page(
        &services, false, nullptr, &assignmentPort, &readPort);
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(Classroom(QStringLiteral("Co-Teacher Page Test"), classId));
    QCOMPARE(readPort.callCount, 1);
    auto* section = page.findChild<TeacherInfoSection*>();
    QVERIFY(section);
    QComboBox* teacherSelector = section->teacherSelector();
    QVERIFY(teacherSelector);
    const int teacherIndex = teacherSelector->findData(teacherId);
    QVERIFY(teacherIndex > 0);
    teacherSelector->setCurrentIndex(teacherIndex);
    QVERIFY(page.hasUnsavedChanges());

    QSignalSpy savedSpy(&page, &ClassCoTeacherPage::classInfoSaved);
    QVERIFY(savedSpy.isValid());
    QVERIFY(!page.saveChanges());

    QCOMPARE(assignmentPort.callCount, 1);
    QCOMPARE(readPort.callCount, 1);
    QCOMPARE(assignmentPort.requests.size(), std::size_t(1));
    QCOMPARE(assignmentPort.requests.front().classId.value(),
        std::to_string(classId));
    QVERIFY(assignmentPort.requests.front().teacherId.has_value());
    QCOMPARE(assignmentPort.requests.front().teacherId->value(),
        std::to_string(teacherId));
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(savedSpy.isEmpty());
    QCOMPARE(m_promptService.requests.size(), std::size_t(1));
    QCOMPARE(m_promptService.requests.front().severity, PromptSeverity::Warning);
    QCOMPARE(m_promptService.requests.front().title,
        QStringLiteral("Save Co-Teacher"));
    QCOMPARE(m_promptService.requests.front().message,
        QStringLiteral("assignment rejected"));
}

void NextFeatureClassCoTeacherPageTests::
loadDiscardAndSuccessfulSaveUseFreshSnapshotsAtExpectedTimes()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(services);
    const int teacherOne = createTeacher(services);
    Teacher teacherTwoRecord;
    teacherTwoRecord.teacherEn = QStringLiteral("Second Page Teacher");
    teacherTwoRecord.preferredName = teacherTwoRecord.teacherEn;
    const auto savedTeacherTwo = services.teacherService()->save(teacherTwoRecord);
    const int teacherTwo = savedTeacherTwo ? *savedTeacherTwo : -1;
    QVERIFY(classId > 0);
    QVERIFY(teacherOne > 0);
    QVERIFY(teacherTwo > 0);

    RecordingAssignmentPort assignmentPort;
    RecordingPageReadPort readPort;
    readPort.result = pageReadResult(
        classId,
        Domain::Result<Application::ClassCoTeacherPageFields>::success(
            readFields(teacherOne)
            ),
        Domain::Result<std::u16string>::success(u"Page Teacher")
        );
    ClassCoTeacherPage page(
        &services, false, nullptr, &assignmentPort, &readPort);
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(Classroom(QStringLiteral("Co-Teacher Page Test"), classId));

    auto* section = page.findChild<TeacherInfoSection*>();
    auto* header = page.findChild<PageHeader*>();
    QVERIFY(section);
    QVERIFY(header);
    QCOMPARE(readPort.callCount, 1);
    QVERIFY(readPort.requests.front() == typedClassId(classId));
    QCOMPARE(section->teacherId(), teacherOne);
    QVERIFY(header->subtitle().contains(QStringLiteral("Page Teacher")));

    page.refresh();
    QCOMPARE(readPort.callCount, 1);
    page.retranslateUi();
    QCOMPARE(readPort.callCount, 1);

    QComboBox* selector = section->teacherSelector();
    QVERIFY(selector);
    const int teacherTwoIndex = selector->findData(teacherTwo);
    QVERIFY(teacherTwoIndex > 0);
    selector->setCurrentIndex(teacherTwoIndex);
    QVERIFY(page.hasUnsavedChanges());
    readPort.result = pageReadResult(
        classId,
        Domain::Result<Application::ClassCoTeacherPageFields>::success(
            readFields(teacherTwo, u"G2", u"After Assignment")
            ),
        Domain::Result<std::u16string>::success(u"Second Page Teacher")
        );

    QVERIFY(page.saveChanges());
    QCOMPARE(assignmentPort.callCount, 1);
    QVERIFY(assignmentPort.requests.front().teacherId.has_value());
    QCOMPARE(assignmentPort.requests.front().teacherId->value(),
        std::to_string(teacherTwo));
    QCOMPARE(readPort.callCount, 2);
    QCOMPARE(section->teacherId(), teacherTwo);
    QVERIFY(header->subtitle().contains(QStringLiteral("Second Page Teacher")));
    QVERIFY(header->subtitle().contains(QStringLiteral("G2")));
    QVERIFY(!page.hasUnsavedChanges());

    const int teacherOneIndex = selector->findData(teacherOne);
    QVERIFY(teacherOneIndex > 0);
    selector->setCurrentIndex(teacherOneIndex);
    QVERIFY(page.hasUnsavedChanges());
    readPort.result = pageReadResult(
        classId,
        Domain::Result<Application::ClassCoTeacherPageFields>::success(
            readFields(teacherOne, u"E4", u"Discard Reload")
            ),
        Domain::Result<std::u16string>::success(u"Page Teacher")
        );

    page.discardChanges();
    QCOMPARE(readPort.callCount, 3);
    QCOMPARE(section->teacherId(), teacherOne);
    QVERIFY(header->subtitle().contains(QStringLiteral("Discard Reload")));
    QVERIFY(header->subtitle().contains(QStringLiteral("Page Teacher")));
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(assignmentPort.callCount, 1);
    page.clearDatabaseState();
    QCOMPARE(readPort.callCount, 3);
    QCOMPARE(header->subtitle(), QStringLiteral("No class selected"));
}

void NextFeatureClassCoTeacherPageTests::
readFailuresKeepDefaultSelectionAndIndependentTitleSources()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(services);
    const int teacherId = createTeacher(services);
    QVERIFY(classId > 0);
    QVERIFY(teacherId > 0);

    RecordingPageReadPort readPort;
    ClassCoTeacherPage page(&services, false, nullptr, nullptr, &readPort);
    const Classroom classroom(QStringLiteral("Fallback Class"), classId);
    auto* section = page.findChild<TeacherInfoSection*>();
    auto* header = page.findChild<PageHeader*>();
    QVERIFY(section);
    QVERIFY(header);

    readPort.result = Application::ClassCoTeacherPageReadResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "snapshot read failed",
        .recoverable = false
    });
    page.loadClass(classroom);
    QCOMPARE(section->teacherId(), -1);
    QVERIFY(header->subtitle().contains(QStringLiteral("Unknown Class")));
    QVERIFY(header->subtitle().contains(QStringLiteral("No Teacher")));

    readPort.result = pageReadResult(
        classId,
        Domain::Result<Application::ClassCoTeacherPageFields>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "class info failed",
            .recoverable = false
        }),
        Domain::Result<std::u16string>::success(u"Ignored Without Class")
        );
    page.loadClass(classroom);
    QCOMPARE(section->teacherId(), -1);
    QVERIFY(header->subtitle().contains(QStringLiteral("Unknown Class")));
    QVERIFY(header->subtitle().contains(QStringLiteral("No Teacher")));

    readPort.result = pageReadResult(
        classId,
        Domain::Result<Application::ClassCoTeacherPageFields>::success(
            readFields(teacherId)
            ),
        Domain::Result<std::u16string>::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "teacher details missing",
            .recoverable = true
        })
        );
    page.loadClass(classroom);
    QCOMPARE(section->teacherId(), teacherId);
    QVERIFY(header->subtitle().contains(QStringLiteral("E4")));
    QVERIFY(header->subtitle().contains(QStringLiteral("Theseus")));
    QVERIFY(header->subtitle().contains(QStringLiteral("No Teacher")));
    QVERIFY(!header->subtitle().contains(QStringLiteral("Page Teacher")));
    QCOMPARE(readPort.callCount, 3);
}

void NextFeatureClassCoTeacherPageTests::
nonpositivePersistedTeacherIdRetainsClassTitleAndClearsSelection()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(services);
    QVERIFY(classId > 0);

    ClassInfo persisted;
    persisted.classId = classId;
    persisted.teacherId = -1;
    persisted.classGrade = QStringLiteral("E5");
    persisted.classLevel = QStringLiteral("Persisted Level");
    persisted.classTimes = {
        {QStringLiteral("Monday"), QStringLiteral("4:15 PM"),
         QStringLiteral("5:00 PM")}
    };
    QVERIFY(services.databaseSession()->classInfoRepository()->saveClassInfo(
        persisted
        ));
    QVERIFY(updatePersistedTeacherId(services, classId, 0));

    ClassCoTeacherPage page(&services);
    page.loadClass(Classroom(QStringLiteral("Co-Teacher Page Test"), classId));

    auto* section = page.findChild<TeacherInfoSection*>();
    auto* header = page.findChild<PageHeader*>();
    QVERIFY(section);
    QVERIFY(header);
    QCOMPARE(section->teacherId(), -1);
    QCOMPARE(section->teacherSelector()->currentData().toInt(), -1);
    QVERIFY(header->subtitle().contains(QStringLiteral("E5 Persisted Level")));
    QVERIFY(header->subtitle().contains(QStringLiteral("No Teacher")));
    QVERIFY(header->subtitle().contains(QStringLiteral("Mon")));
    QVERIFY(header->subtitle().contains(QStringLiteral("4:15")));
}

QTEST_MAIN(NextFeatureClassCoTeacherPageTests)

#include "next_feature_class_co_teacher_page_tests.moc"
