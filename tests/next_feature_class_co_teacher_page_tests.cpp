#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "features/classes/ui/class_co_teacher_page.h"
#include "next/application/class_co_teacher_assignment_use_case.h"
#include "next/application/class_co_teacher_page_read_query.h"
#include "next/application/class_co_teacher_teacher_choices_read_query.h"
#include "next/platform/application_services_class_co_teacher_page_read_port.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/autosave_coordinator.h"
#include "ui/shared/pages/page_header.h"
#include "ui/shared/widgets/sections/teacher_info_section.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSqlQuery>
#include <QSqlError>
#include <QSignalSpy>
#include <QStringList>
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

qint64 insertWideTeacher(ApplicationServices& services)
{
    constexpr qint64 teacherId = 2147483648LL;
    QSqlQuery query(services.databaseSession()->database());
    query.prepare(QStringLiteral(
        "INSERT INTO teachers (id, teacher_kr, teacher_en, preferred_name, "
        "room_number, internet_type, wifi_name, wifi_password, "
        "projection_type, zoom_id, zoom_password) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
        ));
    query.addBindValue(teacherId);
    query.addBindValue(QStringLiteral("Wide Korean"));
    query.addBindValue(QStringLiteral("Wide English"));
    query.addBindValue(QStringLiteral("Wide Display"));
    query.addBindValue(QStringLiteral("Room Wide"));
    query.addBindValue(QStringLiteral("LAN"));
    query.addBindValue(QStringLiteral("Wide Network"));
    query.addBindValue(QStringLiteral("Wide Password"));
    query.addBindValue(QStringLiteral("HDMI"));
    query.addBindValue(QStringLiteral("Wide Zoom"));
    query.addBindValue(QStringLiteral("Wide Zoom Password"));
    if (!query.exec())
    {
        qFatal("Wide teacher must be seeded: %s",
            qPrintable(query.lastError().text()));
    }
    return teacherId;
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

QStringList visibleTeacherDetails(const TeacherInfoSection* section)
{
    QStringList details;
    if (!section)
    {
        return details;
    }

    for (const QLineEdit* field : section->findChildren<QLineEdit*>())
    {
        details.append(field->text());
    }
    return details;
}

QLabel* assignmentReadStatus(ClassCoTeacherPage& page)
{
    return page.findChild<QLabel*>(
        QStringLiteral("classCoTeacherAssignmentReadStatus")
        );
}

QPushButton* coTeacherSaveButton(ClassCoTeacherPage& page)
{
    return page.findChild<QPushButton*>(
        QStringLiteral("classCoTeacherSaveButton")
        );
}

void verifyUnknownAssignmentStatus(
    const QLabel* status,
    const QString& code,
    const QString& message,
    const bool recoverable = false
    )
{
    QVERIFY(status);
    QCOMPARE(status->accessibleName(),
        QStringLiteral("Co-Teacher assignment status"));
    QVERIFY(!status->isHidden());
    QVERIFY(status->text().contains(
        QStringLiteral("could not be confirmed")
        ));
    QVERIFY(status->text().contains(QStringLiteral("Save Changes")));
    QVERIFY(status->text().contains(QStringLiteral("Read error")));
    QVERIFY(status->text().contains(QStringLiteral("code %1").arg(code)));
    QVERIFY(status->text().contains(
        QStringLiteral("recoverable: %1").arg(
            recoverable ? QStringLiteral("true") : QStringLiteral("false")
            )
        ));
    QVERIFY(status->text().contains(QStringLiteral("details: %1").arg(message)));
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
    fields.regularSchedule = {
        Application::ClassCoTeacherPageScheduleSlot(u"Monday", u"4:00 PM")
    };
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
        if (events)
        {
            events->push_back("classInfo");
        }
        requests.push_back(id);
        if (callCount == failOnCall)
        {
            return result;
        }
        if (delegate)
        {
            return delegate->readClassCoTeacherPage(id);
        }
        return result;
    }

    mutable int callCount = 0;
    mutable std::vector<Domain::ClassId> requests;
    mutable std::vector<std::string>* events = nullptr;
    const Application::ClassCoTeacherPageReadPort* delegate = nullptr;
    int failOnCall = 0;
    Application::ClassCoTeacherPageReadResult result =
        Application::ClassCoTeacherPageReadResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unconfigured page read",
            .recoverable = false
        });
};

class RecordingTeacherChoicesReadPort final
    : public Application::ClassCoTeacherTeacherChoicesReadPort
{
public:
    [[nodiscard]] Application::ClassCoTeacherTeacherChoicesResult
    readClassCoTeacherTeacherChoices() const override
    {
        ++callCount;
        if (events)
        {
            events->push_back("teacherChoices");
        }
        return result;
    }

    mutable int callCount = 0;
    mutable std::vector<std::string>* events = nullptr;
    Application::ClassCoTeacherTeacherChoicesResult result =
        Application::ClassCoTeacherTeacherChoicesResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unconfigured teacher choices read",
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
    void successfulAssignmentSurvivesFailedFollowupRead();
    void manualFailureWarnsAndRetainsDirtyState();
    void loadDiscardAndSuccessfulSaveUseFreshSnapshotsAtExpectedTimes();
    void readFailuresKeepDefaultSelectionAndIndependentTitleSources();
    void unknownAssignmentAllowsOnlyExplicitButtonSaveAndUnassignment();
    void failedExplicitWriteRetainsUnknownStatusAndDraft();
    void unknownExplicitWriteReadbackFailurePreservesCommittedUi();
    void nonpositivePersistedTeacherIdRetainsClassTitleAndClearsSelection();
    void teacherChoicesFailureWarnsAndClearsDatabaseState();
    void wideTeacherIdentitySurvivesSelectionSaveAndReadback();

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

void NextFeatureClassCoTeacherPageTests::
successfulAssignmentSurvivesFailedFollowupRead()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(services);

    Teacher assignedTeacher;
    assignedTeacher.teacherEn = QStringLiteral("Committed Teacher");
    assignedTeacher.preferredName = assignedTeacher.teacherEn;
    assignedTeacher.roomNumber = QStringLiteral("Room 18");
    assignedTeacher.internetType = QStringLiteral("LAN");
    assignedTeacher.wifiName = QStringLiteral("Network 18");
    assignedTeacher.wifiPassword = QStringLiteral("Password 18");
    assignedTeacher.projectionType = QStringLiteral("HDMI");
    assignedTeacher.zoomId = QStringLiteral("Zoom 18");
    assignedTeacher.zoomPassword = QStringLiteral("Zoom Password 18");
    const auto assignedTeacherResult =
        services.teacherService()->save(assignedTeacher);
    const int assignedTeacherId = assignedTeacherResult
        ? *assignedTeacherResult
        : -1;

    Teacher refreshedTeacher;
    refreshedTeacher.teacherEn = QStringLiteral("Refreshed Teacher");
    refreshedTeacher.preferredName = refreshedTeacher.teacherEn;
    refreshedTeacher.roomNumber = QStringLiteral("Room 27");
    refreshedTeacher.internetType = QStringLiteral("WiFi");
    refreshedTeacher.wifiName = QStringLiteral("Network 27");
    refreshedTeacher.wifiPassword = QStringLiteral("Password 27");
    refreshedTeacher.projectionType = QStringLiteral("Zoom");
    refreshedTeacher.zoomId = QStringLiteral("Zoom 27");
    refreshedTeacher.zoomPassword = QStringLiteral("Zoom Password 27");
    const auto refreshedTeacherResult =
        services.teacherService()->save(refreshedTeacher);
    const int refreshedTeacherId = refreshedTeacherResult
        ? *refreshedTeacherResult
        : -1;
    QVERIFY(classId > 0);
    QVERIFY(assignedTeacherId > 0);
    QVERIFY(refreshedTeacherId > 0);

    Platform::ApplicationServicesClassCoTeacherPageReadPort actualReadPort(
        &services
        );
    RecordingPageReadPort readPort;
    readPort.delegate = &actualReadPort;
    readPort.failOnCall = 2;
    readPort.result = Application::ClassCoTeacherPageReadResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "follow-up page read failed",
        .recoverable = false
    });
    ClassCoTeacherPage page(
        &services,
        false,
        nullptr,
        nullptr,
        &readPort
        );
    page.setSaveMode(SaveMode::Manual);
    const Classroom classroom(QStringLiteral("Co-Teacher Page Test"), classId);
    page.loadClass(classroom);

    auto* section = page.findChild<TeacherInfoSection*>();
    auto* header = page.findChild<PageHeader*>();
    QVERIFY(section);
    QVERIFY(header);
    QCOMPARE(readPort.callCount, 1);
    QComboBox* selector = section->teacherSelector();
    QVERIFY(selector);
    const int assignedTeacherIndex = selector->findData(assignedTeacherId);
    QVERIFY(assignedTeacherIndex > 0);
    selector->setCurrentIndex(assignedTeacherIndex);
    QVERIFY(page.hasUnsavedChanges());

    QSignalSpy savedSpy(&page, &ClassCoTeacherPage::classInfoSaved);
    QVERIFY(savedSpy.isValid());
    QVERIFY(page.saveChanges());
    QCOMPARE(readPort.callCount, 2);
    QCOMPARE(savedSpy.size(), 1);
    QCOMPARE(savedSpy.front().front().toInt(), classId);
    QVERIFY(!page.hasUnsavedChanges());

    const auto persisted = services.classService()->classInfo(classId);
    QVERIFY(persisted);
    QCOMPARE(persisted->teacherId, assignedTeacherId);
    QCOMPARE(section->teacherId(), assignedTeacherId);
    QCOMPARE(selector->currentData().toInt(), assignedTeacherId);
    const QStringList assignedTeacherDetails{
        QStringLiteral("Room 18"),
        QStringLiteral("LAN"),
        QStringLiteral("Network 18"),
        QStringLiteral("Password 18"),
        QStringLiteral("HDMI"),
        QStringLiteral("Zoom 18"),
        QStringLiteral("Zoom Password 18")
    };
    QCOMPARE(visibleTeacherDetails(section), assignedTeacherDetails);
    QVERIFY(header->subtitle().contains(QStringLiteral("Committed Teacher")));
    QVERIFY(!header->subtitle().contains(QStringLiteral("No Teacher")));
    QVERIFY(m_promptService.requests.empty());

    QVERIFY(updatePersistedTeacherId(services, classId, refreshedTeacherId));
    page.loadClass(classroom);
    QCOMPARE(readPort.callCount, 3);
    QCOMPARE(savedSpy.size(), 1);
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(section->teacherId(), refreshedTeacherId);
    QCOMPARE(selector->currentData().toInt(), refreshedTeacherId);
    const QStringList refreshedTeacherDetails{
        QStringLiteral("Room 27"),
        QStringLiteral("WiFi"),
        QStringLiteral("Network 27"),
        QStringLiteral("Password 27"),
        QStringLiteral("Zoom"),
        QStringLiteral("Zoom 27"),
        QStringLiteral("Zoom Password 27")
    };
    QCOMPARE(visibleTeacherDetails(section), refreshedTeacherDetails);
    QVERIFY(header->subtitle().contains(QStringLiteral("Refreshed Teacher")));
    QVERIFY(!header->subtitle().contains(QStringLiteral("Committed Teacher")));

    const int committedTeacherIndex = selector->findData(assignedTeacherId);
    QVERIFY(committedTeacherIndex > 0);
    selector->setCurrentIndex(committedTeacherIndex);
    QVERIFY(page.hasUnsavedChanges());
    readPort.failOnCall = 4;
    readPort.result = pageReadResult(
        classId,
        Domain::Result<Application::ClassCoTeacherPageFields>::success(
            readFields(std::nullopt, u"E5", u"NoIdentityClass")
            ),
        Domain::Result<std::u16string>::success(u"Unmatched Teacher")
        );

    QVERIFY(page.saveChanges());
    QCOMPARE(readPort.callCount, 4);
    QCOMPARE(savedSpy.size(), 2);
    QVERIFY(!page.hasUnsavedChanges());
    const auto persistedAgain = services.classService()->classInfo(classId);
    QVERIFY(persistedAgain);
    QCOMPARE(persistedAgain->teacherId, assignedTeacherId);
    QCOMPARE(section->teacherId(), assignedTeacherId);
    QCOMPARE(selector->currentData().toInt(), assignedTeacherId);
    QCOMPARE(visibleTeacherDetails(section), assignedTeacherDetails);
    QVERIFY(header->subtitle().contains(QStringLiteral("NoIdentityClass")));
    QVERIFY(header->subtitle().contains(QStringLiteral("Committed Teacher")));
    QVERIFY(!header->subtitle().contains(QStringLiteral("Unmatched Teacher")));

    readPort.failOnCall = 0;
    page.loadClass(classroom);
    QCOMPARE(readPort.callCount, 5);
    QCOMPARE(savedSpy.size(), 2);
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(section->teacherId(), assignedTeacherId);
    QCOMPARE(selector->currentData().toInt(), assignedTeacherId);
    QCOMPARE(visibleTeacherDetails(section), assignedTeacherDetails);
    QVERIFY(header->subtitle().contains(QStringLiteral("Committed Teacher")));
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
    teacherTwoRecord.teacherEn = QStringLiteral("Alpha Teacher");
    teacherTwoRecord.preferredName = teacherTwoRecord.teacherEn;
    const auto savedTeacherTwo = services.teacherService()->save(teacherTwoRecord);
    const int teacherTwo = savedTeacherTwo ? *savedTeacherTwo : -1;
    QVERIFY(classId > 0);
    QVERIFY(teacherOne > 0);
    QVERIFY(teacherTwo > 0);

    RecordingAssignmentPort assignmentPort;
    RecordingPageReadPort readPort;
    RecordingTeacherChoicesReadPort teacherChoicesPort;
    std::vector<std::string> readEvents;
    readPort.events = &readEvents;
    teacherChoicesPort.events = &readEvents;
    readPort.result = pageReadResult(
        classId,
        Domain::Result<Application::ClassCoTeacherPageFields>::success(
            readFields(teacherOne)
            ),
        Domain::Result<std::u16string>::success(u"Zulu Teacher")
        );
    Application::ClassCoTeacherTeacherChoicesSnapshot choices;
    choices.teachers = {
        {
            .teacherId = typedTeacherId(teacherTwo),
            .teacherKr = u"\uBC15\uBBFC\uC900",
            .teacherEn = u"Alpha Teacher",
            .roomNumber = u"Room Two",
            .internetType = u"Ethernet",
            .wifiName = u"Network Two",
            .wifiPassword = u"Password Two",
            .projectionType = u"Zoom",
            .zoomId = u"Zoom Two",
            .zoomPassword = u"Zoom Password Two"
        },
        {
            .teacherId = typedTeacherId(teacherOne),
            .teacherKr = u"\uAE40\uBBFC\uC900",
            .teacherEn = u"Zulu Teacher",
            .roomNumber = u"Room One",
            .internetType = u"WiFi",
            .wifiName = u"Network One",
            .wifiPassword = u"Password One",
            .projectionType = u"HDMI",
            .zoomId = u"Zoom One",
            .zoomPassword = u"Zoom Password One"
        }
    };
    teacherChoicesPort.result =
        Application::ClassCoTeacherTeacherChoicesResult::success(
            std::move(choices)
            );
    ClassCoTeacherPage page(
        &services,
        false,
        nullptr,
        &assignmentPort,
        &readPort,
        &teacherChoicesPort
        );
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(Classroom(QStringLiteral("Co-Teacher Page Test"), classId));

    auto* section = page.findChild<TeacherInfoSection*>();
    auto* header = page.findChild<PageHeader*>();
    QVERIFY(section);
    QVERIFY(header);
    QCOMPARE(readPort.callCount, 1);
    QCOMPARE(teacherChoicesPort.callCount, 1);
    QVERIFY((readEvents == std::vector<std::string>{
        "teacherChoices", "classInfo"
    }));
    QVERIFY(readPort.requests.front() == typedClassId(classId));
    QCOMPARE(section->teacherId(), teacherOne);
    QVERIFY(header->subtitle().contains(QStringLiteral("Zulu Teacher")));
    QCOMPARE(section->teacherSelector()->currentData().toInt(), teacherOne);
    QCOMPARE(section->teacherSelector()->currentText(),
             QStringLiteral("\uAE40\uBBFC\uC900"));
    QCOMPARE(section->teacherSelector()->itemData(0).toInt(), -1);
    QCOMPARE(section->teacherSelector()->itemData(1).toInt(), teacherOne);
    QCOMPARE(section->teacherSelector()->itemData(2).toInt(), teacherTwo);
    auto* englishSelector =
        section->findChild<QComboBox*>(QStringLiteral("teacherEnCombo"));
    QVERIFY(englishSelector);
    QCOMPARE(englishSelector->currentData().toInt(), teacherOne);
    QCOMPARE(englishSelector->currentText(), QStringLiteral("Zulu Teacher"));
    QCOMPARE(englishSelector->itemData(0).toInt(), -1);
    QCOMPARE(englishSelector->itemData(1).toInt(), teacherTwo);
    QCOMPARE(englishSelector->itemData(2).toInt(), teacherOne);
    const QStringList teacherOneDetails{
        QStringLiteral("Room One"),
        QStringLiteral("WiFi"),
        QStringLiteral("Network One"),
        QStringLiteral("Password One"),
        QStringLiteral("HDMI"),
        QStringLiteral("Zoom One"),
        QStringLiteral("Zoom Password One")
    };
    QVERIFY(visibleTeacherDetails(section) == teacherOneDetails);

    page.refresh();
    QCOMPARE(readPort.callCount, 1);
    QCOMPARE(teacherChoicesPort.callCount, 1);
    page.retranslateUi();
    QCOMPARE(readPort.callCount, 1);
    QCOMPARE(teacherChoicesPort.callCount, 1);

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
        Domain::Result<std::u16string>::success(u"Alpha Teacher")
        );

    QVERIFY(page.saveChanges());
    QCOMPARE(assignmentPort.callCount, 1);
    QVERIFY(assignmentPort.requests.front().teacherId.has_value());
    QCOMPARE(assignmentPort.requests.front().teacherId->value(),
        std::to_string(teacherTwo));
    QCOMPARE(readPort.callCount, 2);
    QCOMPARE(teacherChoicesPort.callCount, 1);
    QVERIFY((readEvents == std::vector<std::string>{
        "teacherChoices", "classInfo", "classInfo"
    }));
    QCOMPARE(section->teacherId(), teacherTwo);
    QCOMPARE(section->teacherSelector()->currentData().toInt(), teacherTwo);
    const QStringList teacherTwoDetails{
        QStringLiteral("Room Two"),
        QStringLiteral("Ethernet"),
        QStringLiteral("Network Two"),
        QStringLiteral("Password Two"),
        QStringLiteral("Zoom"),
        QStringLiteral("Zoom Two"),
        QStringLiteral("Zoom Password Two")
    };
    QVERIFY(visibleTeacherDetails(section) == teacherTwoDetails);
    QVERIFY(header->subtitle().contains(QStringLiteral("Alpha Teacher")));
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
        Domain::Result<std::u16string>::success(u"Zulu Teacher")
        );

    page.discardChanges();
    QCOMPARE(readPort.callCount, 3);
    QCOMPARE(teacherChoicesPort.callCount, 2);
    QVERIFY((readEvents == std::vector<std::string>{
        "teacherChoices", "classInfo", "classInfo",
        "teacherChoices", "classInfo"
    }));
    QCOMPARE(section->teacherId(), teacherOne);
    QCOMPARE(section->teacherSelector()->currentData().toInt(), teacherOne);
    QCOMPARE(section->teacherSelector()->currentText(),
             QStringLiteral("\uAE40\uBBFC\uC900"));
    QCOMPARE(section->teacherSelector()->itemData(0).toInt(), -1);
    QCOMPARE(section->teacherSelector()->itemData(1).toInt(), teacherOne);
    QCOMPARE(section->teacherSelector()->itemData(2).toInt(), teacherTwo);
    QCOMPARE(englishSelector->currentData().toInt(), teacherOne);
    QCOMPARE(englishSelector->currentText(), QStringLiteral("Zulu Teacher"));
    QCOMPARE(englishSelector->itemData(0).toInt(), -1);
    QCOMPARE(englishSelector->itemData(1).toInt(), teacherTwo);
    QCOMPARE(englishSelector->itemData(2).toInt(), teacherOne);
    QVERIFY(visibleTeacherDetails(section) == teacherOneDetails);
    QVERIFY(header->subtitle().contains(QStringLiteral("Discard Reload")));
    QVERIFY(header->subtitle().contains(QStringLiteral("Zulu Teacher")));
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(assignmentPort.callCount, 1);
    page.clearDatabaseState();
    QCOMPARE(readPort.callCount, 3);
    QCOMPARE(teacherChoicesPort.callCount, 2);
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

    RecordingAssignmentPort assignmentPort;
    RecordingPageReadPort readPort;
    ClassCoTeacherPage page(
        &services, false, nullptr, &assignmentPort, &readPort);
    const Classroom classroom(QStringLiteral("Fallback Class"), classId);
    auto* section = page.findChild<TeacherInfoSection*>();
    auto* header = page.findChild<PageHeader*>();
    auto* status = assignmentReadStatus(page);
    QVERIFY(section);
    QVERIFY(header);
    QVERIFY(status);

    readPort.result = Application::ClassCoTeacherPageReadResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "snapshot read failed",
        .recoverable = false
    });
    page.loadClass(classroom);
    QCOMPARE(section->teacherId(), -1);
    QVERIFY(header->subtitle().contains(QStringLiteral("Unknown Class")));
    QVERIFY(header->subtitle().contains(QStringLiteral("No Teacher")));
    verifyUnknownAssignmentStatus(
        status,
        QStringLiteral("Technical"),
        QStringLiteral("snapshot read failed")
        );

    const int teacherIndex = section->teacherSelector()->findData(teacherId);
    QVERIFY(teacherIndex > 0);
    section->teacherSelector()->setCurrentIndex(teacherIndex);
    QVERIFY(page.hasUnsavedChanges());
    verifyUnknownAssignmentStatus(
        status,
        QStringLiteral("Technical"),
        QStringLiteral("snapshot read failed")
        );
    QVERIFY(!page.saveChanges());
    page.saveData();
    QTest::qWait(900);
    QCOMPARE(assignmentPort.callCount, 0);
    QVERIFY(page.hasUnsavedChanges());

    readPort.result = pageReadResult(
        classId,
        Domain::Result<Application::ClassCoTeacherPageFields>::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "class info failed",
            .recoverable = true
        }),
        Domain::Result<std::u16string>::success(u"Ignored Without Class")
        );
    page.loadClass(classroom);
    QCOMPARE(section->teacherId(), -1);
    QVERIFY(header->subtitle().contains(QStringLiteral("Unknown Class")));
    QVERIFY(header->subtitle().contains(QStringLiteral("No Teacher")));
    verifyUnknownAssignmentStatus(
        status,
        QStringLiteral("NotFound"),
        QStringLiteral("class info failed"),
        true
        );
    const int nextTeacherIndex = section->teacherSelector()->findData(teacherId);
    QVERIFY(nextTeacherIndex > 0);
    section->teacherSelector()->setCurrentIndex(nextTeacherIndex);
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(!page.saveChanges());
    page.saveData();
    QTest::qWait(900);
    QCOMPARE(assignmentPort.callCount, 0);
    QVERIFY(page.hasUnsavedChanges());

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
    QVERIFY(status->isHidden());
    QVERIFY(header->subtitle().contains(QStringLiteral("E4")));
    QVERIFY(header->subtitle().contains(QStringLiteral("Theseus")));
    QVERIFY(header->subtitle().contains(QStringLiteral("No Teacher")));
    QVERIFY(!header->subtitle().contains(QStringLiteral("Page Teacher")));
    QCOMPARE(readPort.callCount, 3);
    QCOMPARE(assignmentPort.callCount, 0);

    section->teacherSelector()->setCurrentIndex(0);
    QVERIFY(page.hasUnsavedChanges());
    QTRY_COMPARE_WITH_TIMEOUT(assignmentPort.callCount, 1, 3000);
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(status->isHidden());
    QVERIFY(!assignmentPort.requests.front().teacherId.has_value());
}

void NextFeatureClassCoTeacherPageTests::
unknownAssignmentAllowsOnlyExplicitButtonSaveAndUnassignment()
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
    RecordingPageReadPort readPort;
    readPort.failOnCall = 1;
    readPort.result = Application::ClassCoTeacherPageReadResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "initial assignment read failed",
        .recoverable = false
    });
    ClassCoTeacherPage page(
        &services, false, nullptr, &assignmentPort, &readPort);
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(Classroom(QStringLiteral("Fallback Class"), classId));

    auto* section = page.findChild<TeacherInfoSection*>();
    auto* status = assignmentReadStatus(page);
    auto* saveButton = coTeacherSaveButton(page);
    QVERIFY(section);
    QVERIFY(status);
    QVERIFY(saveButton);
    verifyUnknownAssignmentStatus(
        status,
        QStringLiteral("Technical"),
        QStringLiteral("initial assignment read failed")
        );

    const int teacherIndex = section->teacherSelector()->findData(teacherId);
    QVERIFY(teacherIndex > 0);
    section->teacherSelector()->setCurrentIndex(teacherIndex);
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(!saveButton->isHidden());
    QVERIFY(saveButton->isEnabled());
    QVERIFY(!page.saveChanges());
    page.saveData();
    QCOMPARE(assignmentPort.callCount, 0);
    QVERIFY(page.hasUnsavedChanges());

    readPort.result = pageReadResult(
        classId,
        Domain::Result<Application::ClassCoTeacherPageFields>::success(
            readFields(teacherId)
            ),
        Domain::Result<std::u16string>::success(u"Page Teacher")
        );
    QTest::mouseClick(saveButton, Qt::LeftButton);
    QCOMPARE(assignmentPort.callCount, 1);
    QVERIFY(assignmentPort.requests.front().teacherId.has_value());
    QCOMPARE(assignmentPort.requests.front().teacherId->value(),
        std::to_string(teacherId));
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(status->isHidden());
    QCOMPARE(section->teacherId(), teacherId);

    readPort.failOnCall = 3;
    readPort.result = Application::ClassCoTeacherPageReadResult::failure({
        .code = Domain::ErrorCode::NotFound,
        .message = "reload assignment read failed",
        .recoverable = true
    });
    page.loadClass(Classroom(QStringLiteral("Fallback Class"), classId));
    verifyUnknownAssignmentStatus(
        status,
        QStringLiteral("NotFound"),
        QStringLiteral("reload assignment read failed"),
        true
        );
    QCOMPARE(section->teacherId(), -1);

    section->teacherSelector()->setCurrentIndex(
        section->teacherSelector()->findData(teacherId)
        );
    QVERIFY(page.hasUnsavedChanges());
    section->teacherSelector()->setCurrentIndex(0);
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(saveButton->isEnabled());

    readPort.result = pageReadResult(
        classId,
        Domain::Result<Application::ClassCoTeacherPageFields>::success(
            readFields(std::nullopt)
            ),
        Domain::Result<std::u16string>::success({})
        );
    QTest::mouseClick(saveButton, Qt::LeftButton);
    QCOMPARE(assignmentPort.callCount, 2);
    QVERIFY(!assignmentPort.requests.back().teacherId.has_value());
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(status->isHidden());
    QCOMPARE(section->teacherId(), -1);

    page.loadClass(Classroom(QStringLiteral("Fallback Class"), classId));
    QVERIFY(status->isHidden());

    readPort.failOnCall = 6;
    readPort.result = Application::ClassCoTeacherPageReadResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "clear-state assignment read failed",
        .recoverable = false
    });
    page.loadClass(Classroom(QStringLiteral("Fallback Class"), classId));
    verifyUnknownAssignmentStatus(
        status,
        QStringLiteral("Technical"),
        QStringLiteral("clear-state assignment read failed")
        );
    section->teacherSelector()->setCurrentIndex(
        section->teacherSelector()->findData(teacherId)
        );
    QVERIFY(page.hasUnsavedChanges());
    page.clearDatabaseState();
    QVERIFY(status->isHidden());
    QVERIFY(!page.hasUnsavedChanges());
    const int writesBeforeClearSave = assignmentPort.callCount;
    page.saveData();
    QCOMPARE(assignmentPort.callCount, writesBeforeClearSave);
}

void NextFeatureClassCoTeacherPageTests::
failedExplicitWriteRetainsUnknownStatusAndDraft()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(services);
    Teacher assignedTeacher;
    assignedTeacher.teacherEn = QStringLiteral("Committed Teacher");
    assignedTeacher.preferredName = assignedTeacher.teacherEn;
    assignedTeacher.roomNumber = QStringLiteral("Room 18");
    assignedTeacher.internetType = QStringLiteral("LAN");
    assignedTeacher.wifiName = QStringLiteral("Network 18");
    assignedTeacher.wifiPassword = QStringLiteral("Password 18");
    assignedTeacher.projectionType = QStringLiteral("HDMI");
    assignedTeacher.zoomId = QStringLiteral("Zoom 18");
    assignedTeacher.zoomPassword = QStringLiteral("Zoom Password 18");
    const auto assignedTeacherResult =
        services.teacherService()->save(assignedTeacher);
    const int teacherId = assignedTeacherResult
        ? *assignedTeacherResult
        : -1;
    QVERIFY(classId > 0);
    QVERIFY(teacherId > 0);

    RecordingAssignmentPort assignmentPort;
    assignmentPort.result = Domain::Result<void>::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "assignment write failed",
        .recoverable = false
    });
    RecordingPageReadPort readPort;
    readPort.result = Application::ClassCoTeacherPageReadResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "read state remains unknown",
        .recoverable = false
    });
    ClassCoTeacherPage page(
        &services, false, nullptr, &assignmentPort, &readPort);
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(Classroom(QStringLiteral("Fallback Class"), classId));
    auto* section = page.findChild<TeacherInfoSection*>();
    auto* status = assignmentReadStatus(page);
    auto* saveButton = coTeacherSaveButton(page);
    QVERIFY(section);
    QVERIFY(status);
    QVERIFY(saveButton);

    const int teacherIndex = section->teacherSelector()->findData(teacherId);
    QVERIFY(teacherIndex > 0);
    section->teacherSelector()->setCurrentIndex(teacherIndex);
    QVERIFY(page.hasUnsavedChanges());
    const QString originalStatus = status->text();
    QTest::mouseClick(saveButton, Qt::LeftButton);

    QCOMPARE(assignmentPort.callCount, 1);
    QCOMPARE(readPort.callCount, 1);
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(section->teacherId(), teacherId);
    QVERIFY(!status->isHidden());
    QCOMPARE(status->text(), originalStatus);
    QCOMPARE(m_promptService.requests.size(), std::size_t(1));
    QCOMPARE(m_promptService.requests.front().message,
        QStringLiteral("assignment write failed"));
}

void NextFeatureClassCoTeacherPageTests::
unknownExplicitWriteReadbackFailurePreservesCommittedUi()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(services);
    Teacher assignedTeacher;
    assignedTeacher.teacherEn = QStringLiteral("Committed Teacher");
    assignedTeacher.preferredName = assignedTeacher.teacherEn;
    assignedTeacher.roomNumber = QStringLiteral("Room 18");
    assignedTeacher.internetType = QStringLiteral("LAN");
    assignedTeacher.wifiName = QStringLiteral("Network 18");
    assignedTeacher.wifiPassword = QStringLiteral("Password 18");
    assignedTeacher.projectionType = QStringLiteral("HDMI");
    assignedTeacher.zoomId = QStringLiteral("Zoom 18");
    assignedTeacher.zoomPassword = QStringLiteral("Zoom Password 18");
    const auto assignedTeacherResult =
        services.teacherService()->save(assignedTeacher);
    const int teacherId = assignedTeacherResult
        ? *assignedTeacherResult
        : -1;
    QVERIFY(classId > 0);
    QVERIFY(teacherId > 0);

    Platform::ApplicationServicesClassCoTeacherPageReadPort actualReadPort(
        &services
        );
    RecordingAssignmentPort assignmentPort;
    RecordingPageReadPort readPort;
    readPort.delegate = &actualReadPort;
    readPort.failOnCall = 1;
    readPort.result = Application::ClassCoTeacherPageReadResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "initial assignment read failed",
        .recoverable = false
    });
    ClassCoTeacherPage page(
        &services, false, nullptr, &assignmentPort, &readPort);
    const Classroom classroom(QStringLiteral("Fallback Class"), classId);
    page.loadClass(classroom);

    auto* section = page.findChild<TeacherInfoSection*>();
    auto* header = page.findChild<PageHeader*>();
    auto* status = assignmentReadStatus(page);
    auto* saveButton = coTeacherSaveButton(page);
    QVERIFY(section);
    QVERIFY(header);
    QVERIFY(status);
    QVERIFY(saveButton);

    const int teacherIndex = section->teacherSelector()->findData(teacherId);
    QVERIFY(teacherIndex > 0);
    section->teacherSelector()->setCurrentIndex(teacherIndex);
    QVERIFY(page.hasUnsavedChanges());
    const QStringList committedDetails{
        QStringLiteral("Room 18"),
        QStringLiteral("LAN"),
        QStringLiteral("Network 18"),
        QStringLiteral("Password 18"),
        QStringLiteral("HDMI"),
        QStringLiteral("Zoom 18"),
        QStringLiteral("Zoom Password 18")
    };

    readPort.failOnCall = 2;
    readPort.result = Application::ClassCoTeacherPageReadResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "post-save readback failed",
        .recoverable = false
    });
    QTest::mouseClick(saveButton, Qt::LeftButton);

    QCOMPARE(assignmentPort.callCount, 1);
    QCOMPARE(readPort.callCount, 2);
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(section->teacherId(), teacherId);
    QCOMPARE(section->teacherSelector()->currentData().toInt(), teacherId);
    QCOMPARE(visibleTeacherDetails(section), committedDetails);
    QVERIFY(header->subtitle().contains(QStringLiteral("Fallback Class")));
    QVERIFY(header->subtitle().contains(QStringLiteral("Committed Teacher")));
    QVERIFY(!header->subtitle().contains(QStringLiteral("No Teacher")));
    QVERIFY(status->isHidden());

    // A later ordinary autosave proves the successful explicit write released
    // the unknown-state save barrier, even though its readback failed.
    section->teacherSelector()->setCurrentIndex(0);
    QVERIFY(page.hasUnsavedChanges());
    QTRY_COMPARE_WITH_TIMEOUT(assignmentPort.callCount, 2, 3000);
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(!assignmentPort.requests.back().teacherId.has_value());
    QVERIFY(status->isHidden());
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

void NextFeatureClassCoTeacherPageTests::
teacherChoicesFailureWarnsAndClearsDatabaseState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(services);
    QVERIFY(classId > 0);

    RecordingPageReadPort classReadPort;
    RecordingTeacherChoicesReadPort teacherChoicesPort;
    teacherChoicesPort.result =
        Application::ClassCoTeacherTeacherChoicesResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "teacher catalogue unavailable",
            .recoverable = false
        });
    ClassCoTeacherPage page(
        &services,
        false,
        nullptr,
        nullptr,
        &classReadPort,
        &teacherChoicesPort
        );

    page.loadClass(Classroom(QStringLiteral("Co-Teacher Page Test"), classId));

    const auto* section = page.findChild<TeacherInfoSection*>();
    const auto* header = page.findChild<PageHeader*>();
    QVERIFY(section);
    QVERIFY(header);
    QCOMPARE(teacherChoicesPort.callCount, 1);
    QCOMPARE(classReadPort.callCount, 0);
    QCOMPARE(section->teacherId(), -1);
    QCOMPARE(section->teacherSelector()->count(), 1);
    QCOMPARE(section->teacherSelector()->currentData().toInt(), -1);
    QCOMPARE(header->subtitle(), QStringLiteral("No class selected"));
    QVERIFY(!page.hasUnsavedChanges());

    QCOMPARE(m_promptService.requests.size(), std::size_t(1));
    QCOMPARE(m_promptService.requests.front().severity, PromptSeverity::Warning);
    QCOMPARE(m_promptService.requests.front().title,
             QStringLiteral("Load Co-Teacher"));
    QCOMPARE(m_promptService.requests.front().message,
             QStringLiteral("Teachers could not be loaded."));
    QCOMPARE(m_promptService.requests.front().details,
             QStringLiteral("teacher catalogue unavailable"));
}

void NextFeatureClassCoTeacherPageTests::
wideTeacherIdentitySurvivesSelectionSaveAndReadback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(services);
    const int regularTeacherId = createTeacher(services);
    constexpr qint64 wideTeacherId = 2147483648LL;
    QCOMPARE(insertWideTeacher(services), wideTeacherId);
    QVERIFY(classId > 0);
    QVERIFY(regularTeacherId > 0);

    ClassCoTeacherPage page(&services);
    page.loadClass(Classroom(QStringLiteral("Co-Teacher Page Test"), classId));
    auto* section = page.findChild<TeacherInfoSection*>();
    QVERIFY(section);
    QComboBox* selector = section->teacherSelector();
    QVERIFY(selector);
    const int wideIndex = selector->findText(QStringLiteral("Wide Korean"));
    QVERIFY(wideIndex > 0);
    const int wideUiId = selector->itemData(wideIndex).toInt();
    QVERIFY(wideUiId > 0);
    QVERIFY(wideUiId != regularTeacherId);

    selector->setCurrentIndex(wideIndex);
    QCOMPARE(section->teacherId(), wideUiId);
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(page.saveChanges());
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(section->teacherId(), wideUiId);
    QCOMPARE(selector->currentText(), QStringLiteral("Wide Korean"));

    QSqlQuery persisted(services.databaseSession()->database());
    persisted.prepare(QStringLiteral(
        "SELECT typeof(teacher_id), teacher_id "
        "FROM class_info WHERE class_id=?"
        ));
    persisted.addBindValue(classId);
    QVERIFY2(persisted.exec(), qPrintable(persisted.lastError().text()));
    QVERIFY(persisted.next());
    QCOMPARE(persisted.value(0).toString(), QStringLiteral("integer"));
    bool persistedIdValid = false;
    QCOMPARE(persisted.value(1).toLongLong(&persistedIdValid), wideTeacherId);
    QVERIFY(persistedIdValid);

    Platform::ApplicationServicesClassCoTeacherPageReadPort readPort(&services);
    const Application::ClassCoTeacherPageReadQuery readQuery(readPort);
    const auto snapshot = readQuery.execute(typedClassId(classId));
    QVERIFY(snapshot);
    QVERIFY(snapshot.value().classFields);
    QVERIFY(snapshot.value().classFields.value().selectedTeacherId.has_value());
    QCOMPARE(
        snapshot.value().classFields.value().selectedTeacherId->value(),
        std::string("2147483648")
        );
    QVERIFY(snapshot.value().teacherDisplayName);
    QCOMPARE(
        QString::fromStdU16String(snapshot.value().teacherDisplayName.value()),
        QStringLiteral("Wide Display")
        );
}

QTEST_MAIN(NextFeatureClassCoTeacherPageTests)

#include "next_feature_class_co_teacher_page_tests.moc"
