#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "features/classes/ui/class_co_teacher_page.h"
#include "next/application/class_co_teacher_assignment_use_case.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/autosave_coordinator.h"
#include "ui/shared/pages/page_header.h"
#include "ui/shared/widgets/sections/teacher_info_section.h"

#include <QComboBox>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>
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
    ClassCoTeacherPage page(&services, false, nullptr, &assignmentPort);
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(Classroom(QStringLiteral("Co-Teacher Page Test"), classId));
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

QTEST_MAIN(NextFeatureClassCoTeacherPageTests)

#include "next_feature_class_co_teacher_page_tests.moc"
