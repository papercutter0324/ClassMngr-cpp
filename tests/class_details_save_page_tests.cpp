#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "features/classes/ui/class_details_page.h"
#include "next/application/class_details_validation_context_query.h"
#include "next/application/class_details_save_use_case.h"
#include "next/application/class_details_schedule_conflict_query.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/autosave_coordinator.h"
#include "ui/shared/pages/page_header.h"
#include "ui/shared/widgets/sectioncards/class_time_row.h"
#include "ui/shared/widgets/sections/class_schedule_section.h"
#include "ui/shared/validation/form_validation_binder.h"

#include <QComboBox>
#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("class-details-page-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

int createClass(ApplicationServices& services, const QString& name)
{
    const auto result = services.classService()->create(name);
    return result ? *result : -1;
}

class RecordingClassDetailsSavePort final
    : public Application::ClassDetailsSavePort
{
public:
    [[nodiscard]] Domain::Result<void> saveClassDetails(
        const Application::ClassDetailsSaveRequest& value
        ) const override
    {
        ++callCount;
        lastRequest = value;
        if (events)
        {
            events->push_back("save");
        }
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<Application::ClassDetailsSaveRequest> lastRequest;
    mutable std::vector<std::string>* events = nullptr;
    Domain::Result<void> result = Domain::Result<void>::success();
};

class RecordingClassDetailsScheduleConflictPort final
    : public Application::ClassDetailsScheduleConflictPort
{
public:
    [[nodiscard]] Application::ClassDetailsScheduleConflictResult
    classDetailsScheduleConflicts(
        const Application::ClassDetailsScheduleConflictRequest& request
        ) const override
    {
        requests.push_back(request);
        if (events)
        {
            events->push_back(
                request.mode == Application::ClassDetailsScheduleMode::Regular
                    ? "regular"
                    : "intensive"
                );
        }
        return request.mode == Application::ClassDetailsScheduleMode::Regular
            ? regularResult
            : intensiveResult;
    }

    mutable std::vector<
        Application::ClassDetailsScheduleConflictRequest
        > requests;
    mutable std::vector<std::string>* events = nullptr;
    Application::ClassDetailsScheduleConflictResult regularResult =
        Application::ClassDetailsScheduleConflictResult::success({});
    Application::ClassDetailsScheduleConflictResult intensiveResult =
        Application::ClassDetailsScheduleConflictResult::success({});
};

class RecordingClassDetailsValidationContextPort final
    : public Application::ClassDetailsValidationContextPort
{
public:
    [[nodiscard]] Application::ClassDetailsValidationContextPortResult
    loadClassDetailsValidationContext(
        const Domain::ClassId& classId
        ) const override
    {
        ++callCount;
        requestedClassIds.push_back(classId);
        if (events)
        {
            events->push_back("context");
        }
        return result;
    }

    mutable int callCount = 0;
    mutable std::vector<Domain::ClassId> requestedClassIds;
    mutable std::vector<std::string>* events = nullptr;
    Application::ClassDetailsValidationContextPortResult result =
        Application::ClassDetailsValidationContextPortResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unconfigured validation context port.",
            .recoverable = false
        });
};

Application::ClassDetailsScheduleConflict conflict(
    const std::u16string& className,
    const std::u16string& day,
    const std::u16string& startTime,
    const std::u16string& endTime,
    const std::u16string& conflictingClassName
    )
{
    return {
        .className = className,
        .day = day,
        .startTime = startTime,
        .endTime = endTime,
        .conflictingClassName = conflictingClassName
    };
}

void chooseDetails(ClassDetailsPage& page)
{
    auto* grade = page.findChild<QComboBox*>(QStringLiteral("classGradeCombo"));
    auto* level = page.findChild<QComboBox*>(QStringLiteral("classLevelCombo"));
    auto* reading = page.findChild<QComboBox*>(
        QStringLiteral("classReadingBookCombo")
        );
    auto* essay = page.findChild<QComboBox*>(
        QStringLiteral("classEssayBookCombo")
        );
    Q_ASSERT(grade && level && reading && essay);

    grade->setCurrentText(QStringLiteral("E4"));
    level->setCurrentText(QStringLiteral("Theseus"));
    reading->setCurrentText(QStringLiteral("Reading Explorer 1"));
    essay->setCurrentText(QStringLiteral("4A"));
}

void setSchedule(
    ClassTimeRow* row,
    const QString& day,
    const QString& start,
    const QString& end
    )
{
    Q_ASSERT(row);
    row->setDay(day);
    row->setStartTime(start);
    row->setEndTime(end);
}

ClassTimeRow* addScheduleRow(
    ClassDetailsPage& page,
    const ScheduleType type
    )
{
    auto* schedule = page.findChild<ClassScheduleSection*>();
    if (!schedule)
    {
        return nullptr;
    }

    const QString addButtonText = type == ScheduleType::Regular
        ? QStringLiteral("+ Add Time")
        : QStringLiteral("+ Add Intensive Time");
    for (QPushButton* button : schedule->findChildren<QPushButton*>())
    {
        if (button->text() == addButtonText)
        {
            button->click();
            break;
        }
    }

    const QList<ClassTimeRow*>& rows = type == ScheduleType::Regular
        ? schedule->regularRows()
        : schedule->intensiveRows();
    return rows.isEmpty() ? nullptr : rows.last();
}

bool saveAndAcceptWarning(
    ClassDetailsPage& page,
    QString* warningTitle,
    QString* warningMessage
    )
{
    QTimer::singleShot(0, [&] {
        const auto prompt = DialogServices::promptTestDriver().activePrompt();
        if (!prompt)
        {
            return;
        }
        if (warningTitle)
        {
            *warningTitle = prompt->title;
        }
        if (warningMessage)
        {
            *warningMessage = prompt->text;
        }
        DialogServices::promptTestDriver().accept(prompt->id);
    });
    return page.saveChanges();
}

}

class ClassDetailsSavePageTests final : public QObject
{
    Q_OBJECT

private slots:
    void normalizedSchedulesReachUseCaseAsMinuteValues();
    void portFailureShowsItsErrorAndLeavesPageDirty();
    void invalidFieldsBlockTheSavePort();
    void malformedScheduleBlocksConflictQueriesAndFocusesItsField();
    void freshInvalidContextBlocksBeforeConflictAndSave();
    void contextReadFailureFallsBackAndContinuesSaveOrder();
    void regularAndIntensiveConflictsBlockTheSavePort();
    void regularConflictShortCircuitsIntensiveAndKeepsSameNameWording();
    void intensiveConflictFollowsAnEmptyRegularQuery();
    void regularQueryFailureBlocksBeforeIntensive();
    void intensiveQueryFailureBlocksAfterRegularSuccess();
    void noninteractiveConflictBlocksSilently();
};

void ClassDetailsSavePageTests::normalizedSchedulesReachUseCaseAsMinuteValues()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(
        services,
        QStringLiteral("Class Details Save Page")
        );
    QVERIFY(classId > 0);

    RecordingClassDetailsSavePort port;
    RecordingClassDetailsScheduleConflictPort conflictPort;
    ClassDetailsPage page(
        &services,
        false,
        nullptr,
        &port,
        nullptr,
        &conflictPort
        );
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(Classroom(QStringLiteral("Class Details Save Page"), classId));

    auto* header = page.findChild<PageHeader*>();
    QVERIFY(header);
    const QString originalSubtitle = header->subtitle();
    chooseDetails(page);

    ClassTimeRow* morningRow = addScheduleRow(page, ScheduleType::Regular);
    QVERIFY(morningRow);
    setSchedule(
        morningRow,
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:55 AM")
        );
    ClassTimeRow* afternoonRow = addScheduleRow(page, ScheduleType::Regular);
    QVERIFY(afternoonRow);
    setSchedule(
        afternoonRow,
        QStringLiteral("Tuesday"),
        QStringLiteral("3:00 PM"),
        QStringLiteral("3:55 PM")
        );
    ClassTimeRow* noonRow = addScheduleRow(page, ScheduleType::Intensive);
    QVERIFY(noonRow);
    setSchedule(
        noonRow,
        QStringLiteral("Tuesday"),
        QStringLiteral("12:00 PM"),
        QStringLiteral("12:55 PM")
        );
    ClassTimeRow* midnightRow = addScheduleRow(page, ScheduleType::Intensive);
    QVERIFY(midnightRow);
    setSchedule(
        midnightRow,
        QStringLiteral("Wednesday"),
        QStringLiteral("12:00 AM"),
        QStringLiteral("12:55 AM")
        );

    QVERIFY(page.hasUnsavedChanges());
    QSignalSpy savedSpy(&page, &ClassDetailsPage::classInfoSaved);
    QVERIFY(savedSpy.isValid());
    QVERIFY(page.saveChanges());

    QCOMPARE(port.callCount, 1);
    QCOMPARE(conflictPort.requests.size(), std::size_t(2));
    QCOMPARE(
        conflictPort.requests[0].mode,
        Application::ClassDetailsScheduleMode::Regular
        );
    QCOMPARE(
        conflictPort.requests[1].mode,
        Application::ClassDetailsScheduleMode::Intensive
        );
    QCOMPARE(conflictPort.requests[0].candidateTimes.size(), std::size_t(2));
    QCOMPARE(conflictPort.requests[1].candidateTimes.size(), std::size_t(2));
    QCOMPARE(conflictPort.requests[0].candidateTimes[0].weekdayIndex(), 0);
    QCOMPARE(
        conflictPort.requests[0].candidateTimes[0].startMinute(),
        9 * 60
        );
    QCOMPARE(conflictPort.requests[0].candidateTimes[1].weekdayIndex(), 1);
    QCOMPARE(
        conflictPort.requests[0].candidateTimes[1].startMinute(),
        15 * 60
        );
    QCOMPARE(conflictPort.requests[1].candidateTimes[0].weekdayIndex(), 1);
    QCOMPARE(
        conflictPort.requests[1].candidateTimes[0].startMinute(),
        12 * 60
        );
    QCOMPARE(conflictPort.requests[1].candidateTimes[1].weekdayIndex(), 2);
    QCOMPARE(
        conflictPort.requests[1].candidateTimes[1].startMinute(),
        0
        );
    QVERIFY(port.lastRequest.has_value());
    const Application::ClassDetailsSaveRequest& request = *port.lastRequest;
    QCOMPARE(request.classId.value(), std::to_string(classId));
    QCOMPARE(request.classGrade, std::u16string(u"E4"));
    QCOMPARE(request.classLevel, std::u16string(u"Theseus"));
    QCOMPARE(request.readingBook, std::u16string(u"Reading Explorer 1"));
    QCOMPARE(request.essayBook, std::u16string(u"4A"));
    QVERIFY(request.regularTimes.has_value());
    QCOMPARE(request.regularTimes->size(), std::size_t(2));
    QCOMPARE((*request.regularTimes)[0].weekdayIndex(), 0);
    QCOMPARE((*request.regularTimes)[0].startMinute(), 9 * 60);
    QCOMPARE((*request.regularTimes)[0].endMinute(), 9 * 60 + 55);
    QCOMPARE((*request.regularTimes)[1].weekdayIndex(), 1);
    QCOMPARE((*request.regularTimes)[1].startMinute(), 15 * 60);
    QCOMPARE((*request.regularTimes)[1].endMinute(), 15 * 60 + 55);
    QVERIFY(request.intensiveTimes.has_value());
    QCOMPARE(request.intensiveTimes->size(), std::size_t(2));
    QCOMPARE((*request.intensiveTimes)[0].weekdayIndex(), 1);
    QCOMPARE((*request.intensiveTimes)[0].startMinute(), 12 * 60);
    QCOMPARE((*request.intensiveTimes)[0].endMinute(), 12 * 60 + 55);
    QCOMPARE((*request.intensiveTimes)[1].weekdayIndex(), 2);
    QCOMPARE((*request.intensiveTimes)[1].startMinute(), 0);
    QCOMPARE((*request.intensiveTimes)[1].endMinute(), 55);
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(savedSpy.size(), 1);
    QCOMPARE(savedSpy.at(0).at(0).toInt(), classId);

    QVERIFY(header->subtitle() != originalSubtitle);
}

void ClassDetailsSavePageTests::portFailureShowsItsErrorAndLeavesPageDirty()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(
        services,
        QStringLiteral("Class Details Failed Save")
        );
    QVERIFY(classId > 0);

    RecordingClassDetailsSavePort port;
    port.result = Domain::Result<void>::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "storage write rejected",
        .recoverable = true
    });
    ClassDetailsPage page(&services, false, nullptr, &port);
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(Classroom(QStringLiteral("Class Details Failed Save"), classId));
    ClassTimeRow* row = addScheduleRow(page, ScheduleType::Regular);
    QVERIFY(row);
    setSchedule(
        row,
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:55 AM")
        );

    QString warningTitle;
    QString warningMessage;
    QVERIFY(!saveAndAcceptWarning(page, &warningTitle, &warningMessage));
    QCOMPARE(warningTitle, QStringLiteral("Save Class Information"));
    QCOMPARE(warningMessage, QStringLiteral("storage write rejected"));
    QCOMPARE(port.callCount, 1);
    QVERIFY(page.hasUnsavedChanges());
}

void ClassDetailsSavePageTests::invalidFieldsBlockTheSavePort()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(
        services,
        QStringLiteral("Class Details Invalid Save")
        );
    QVERIFY(classId > 0);

    RecordingClassDetailsSavePort port;
    RecordingClassDetailsScheduleConflictPort conflictPort;
    ClassDetailsPage page(
        &services,
        false,
        nullptr,
        &port,
        nullptr,
        &conflictPort
        );
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(Classroom(QStringLiteral("Class Details Invalid Save"), classId));
    auto* grade = page.findChild<QComboBox*>(QStringLiteral("classGradeCombo"));
    auto* level = page.findChild<QComboBox*>(QStringLiteral("classLevelCombo"));
    QVERIFY(grade && level);
    page.show();
    page.activateWindow();
    QApplication::processEvents();
    grade->setCurrentText(QStringLiteral("E4"));
    level->setCurrentIndex(0);

    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(!page.saveChanges());
    QCOMPARE(port.callCount, 0);
    QVERIFY(conflictPort.requests.empty());

    auto* validationMessage = page.findChild<QLabel*>(
        QStringLiteral("classLevelValidationMessage")
        );
    QVERIFY(validationMessage);
    QCOMPARE(validationMessage->text(), QStringLiteral("This field is required."));
    QCOMPARE(level->property("formValidationState").toString(),
        QStringLiteral("error"));
    QTRY_VERIFY(level->hasFocus());
    QVERIFY(page.hasUnsavedChanges());
}

void ClassDetailsSavePageTests::
malformedScheduleBlocksConflictQueriesAndFocusesItsField()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(
        services,
        QStringLiteral("Class Details Malformed Schedule")
        );
    QVERIFY(classId > 0);

    RecordingClassDetailsSavePort port;
    RecordingClassDetailsScheduleConflictPort conflictPort;
    ClassDetailsPage page(
        &services,
        false,
        nullptr,
        &port,
        nullptr,
        &conflictPort
        );
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(
        Classroom(QStringLiteral("Class Details Malformed Schedule"), classId)
        );
    page.show();
    page.activateWindow();
    QApplication::processEvents();

    ClassTimeRow* row = addScheduleRow(page, ScheduleType::Regular);
    QVERIFY(row);
    setSchedule(
        row,
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:55 AM")
        );
    row->endCombo()->addItem(QStringLiteral("not-a-time"));
    row->endCombo()->setCurrentText(QStringLiteral("not-a-time"));

    ClassTimeRow* intensiveRow = addScheduleRow(page, ScheduleType::Intensive);
    QVERIFY(intensiveRow);
    setSchedule(
        intensiveRow,
        QStringLiteral("Tuesday"),
        QStringLiteral("12:00 PM"),
        QStringLiteral("12:55 PM")
        );
    intensiveRow->endCombo()->addItem(QStringLiteral("also-not-a-time"));
    intensiveRow->endCombo()->setCurrentText(QStringLiteral("also-not-a-time"));

    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(!page.saveChanges());
    QCOMPARE(port.callCount, 0);
    QVERIFY(conflictPort.requests.empty());
    QVERIFY(page.hasUnsavedChanges());

    auto* validationMessage = page.findChild<QLabel*>(
        QStringLiteral("classRegularScheduleValidationMessage")
        );
    auto* intensiveValidationMessage = page.findChild<QLabel*>(
        QStringLiteral("classIntensiveScheduleValidationMessage")
        );
    QVERIFY(validationMessage && intensiveValidationMessage);
    QCOMPARE(validationMessage->text(), QStringLiteral("Enter a valid value."));
    QCOMPARE(
        intensiveValidationMessage->text(),
        QStringLiteral("Enter a valid value.")
        );
    QCOMPARE(row->endCombo()->property("formValidationState").toString(),
        QStringLiteral("error"));
    QTRY_VERIFY(row->endCombo()->hasFocus());
}

void ClassDetailsSavePageTests::freshInvalidContextBlocksBeforeConflictAndSave()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(
        services,
        QStringLiteral("Class Details Fresh Validation Context")
        );
    QVERIFY(classId > 0);

    std::vector<std::string> events;
    RecordingClassDetailsSavePort savePort;
    savePort.events = &events;
    RecordingClassDetailsScheduleConflictPort conflictPort;
    conflictPort.events = &events;
    RecordingClassDetailsValidationContextPort contextPort;
    contextPort.events = &events;
    const Domain::ClassId typedClassId = *Domain::ClassId::fromString(
        std::to_string(classId)
        );
    contextPort.result =
        Application::ClassDetailsValidationContextPortResult::success({
            .matchedClassId = typedClassId,
            .teacherId = -1,
            .notes = {},
            .timeFillerActivities = {}
        });

    ClassDetailsPage page(
        &services,
        false,
        nullptr,
        &savePort,
        nullptr,
        &conflictPort,
        &contextPort
        );
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(
        Classroom(QStringLiteral("Class Details Fresh Validation Context"), classId)
        );
    QCOMPARE(contextPort.callCount, 0);

    // Simulate hidden data changing after load. Save should query its current
    // validation context at the point it validates the form.
    contextPort.result =
        Application::ClassDetailsValidationContextPortResult::success({
            .matchedClassId = typedClassId,
            .teacherId = 0,
            .notes = {},
            .timeFillerActivities = {}
        });
    chooseDetails(page);
    QSignalSpy savedSpy(&page, &ClassDetailsPage::classInfoSaved);
    QVERIFY(savedSpy.isValid());
    QVERIFY(page.hasUnsavedChanges());

    QVERIFY(!page.saveChanges());
    QCOMPARE(contextPort.callCount, 1);
    QCOMPARE(contextPort.requestedClassIds.size(), std::size_t(1));
    QCOMPARE(contextPort.requestedClassIds.front().value(),
        std::to_string(classId));
    QCOMPARE(events.size(), std::size_t(1));
    QCOMPARE(events.front(), std::string("context"));
    QVERIFY(conflictPort.requests.empty());
    QCOMPARE(savePort.callCount, 0);
    QCOMPARE(savedSpy.size(), 0);
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(!DialogServices::promptTestDriver().activePrompt());

    FormValidationBinder* const binder = page.findChild<FormValidationBinder*>();
    QVERIFY(binder);
    QVERIFY(binder->hasErrors());
    bool foundTeacherIssue = false;
    for (const ValidationIssue& issue : binder->validation().issues())
    {
        if (issue.code == QStringLiteral("class_info.teacher_id.invalid"))
        {
            foundTeacherIssue = true;
            QCOMPARE(issue.field, QStringLiteral("teacherId"));
        }
    }
    QVERIFY(foundTeacherIssue);
}

void ClassDetailsSavePageTests::contextReadFailureFallsBackAndContinuesSaveOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(
        services,
        QStringLiteral("Class Details Validation Context Fallback")
        );
    QVERIFY(classId > 0);

    std::vector<std::string> events;
    RecordingClassDetailsSavePort savePort;
    savePort.events = &events;
    RecordingClassDetailsScheduleConflictPort conflictPort;
    conflictPort.events = &events;
    RecordingClassDetailsValidationContextPort contextPort;
    contextPort.events = &events;
    contextPort.result =
        Application::ClassDetailsValidationContextPortResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "validation context repository read failed",
            .recoverable = true
        });

    ClassDetailsPage page(
        &services,
        false,
        nullptr,
        &savePort,
        nullptr,
        &conflictPort,
        &contextPort
        );
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(
        Classroom(
            QStringLiteral("Class Details Validation Context Fallback"),
            classId
            )
        );
    chooseDetails(page);
    QSignalSpy savedSpy(&page, &ClassDetailsPage::classInfoSaved);
    QVERIFY(savedSpy.isValid());

    QVERIFY(page.saveChanges());
    QCOMPARE(contextPort.callCount, 1);
    QCOMPARE(conflictPort.requests.size(), std::size_t(2));
    QCOMPARE(savePort.callCount, 1);
    QCOMPARE(events.size(), std::size_t(4));
    QCOMPARE(events[0], std::string("context"));
    QCOMPARE(events[1], std::string("regular"));
    QCOMPARE(events[2], std::string("intensive"));
    QCOMPARE(events[3], std::string("save"));
    QCOMPARE(savedSpy.size(), 1);
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(!DialogServices::promptTestDriver().activePrompt());
}

void ClassDetailsSavePageTests::regularAndIntensiveConflictsBlockTheSavePort()
{
    const struct ConflictCase
    {
        ScheduleType type;
        QString day;
        QString start;
        QString end;
        QString warningTitle;
    } cases[] = {
        {
            ScheduleType::Regular,
            QStringLiteral("Monday"),
            QStringLiteral("9:00 AM"),
            QStringLiteral("9:55 AM"),
            QStringLiteral("Regular Schedule Conflicts")
        },
        {
            ScheduleType::Intensive,
            QStringLiteral("Tuesday"),
            QStringLiteral("12:00 PM"),
            QStringLiteral("12:55 PM"),
            QStringLiteral("Intensive Schedule Conflicts")
        }
    };

    for (const ConflictCase& conflictCase : cases)
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ApplicationServices services;
        QVERIFY(services.openDatabase(databasePath(directory)));
        const int classId = createClass(
            services,
            QStringLiteral("Class Details Conflict Target")
            );
        const int conflictingClassId = createClass(
            services,
            QStringLiteral("Class Details Conflict Source")
            );
        QVERIFY(classId > 0);
        QVERIFY(conflictingClassId > 0);
        auto conflictingInfo = services.classService()
            ->classInfo(conflictingClassId);
        QVERIFY(conflictingInfo);
        if (conflictCase.type == ScheduleType::Regular)
        {
            conflictingInfo->classTimes.append({
                conflictCase.day,
                conflictCase.start,
                conflictCase.end
            });
        }
        else
        {
            conflictingInfo->intensiveTimes.append({
                conflictCase.day,
                conflictCase.start,
                conflictCase.end
            });
        }
        QVERIFY(services.classService()->saveClassInfo(*conflictingInfo));

        RecordingClassDetailsSavePort port;
        ClassDetailsPage page(&services, false, nullptr, &port);
        page.setSaveMode(SaveMode::Manual);
        page.loadClass(
            Classroom(QStringLiteral("Class Details Conflict Target"), classId)
            );
        ClassTimeRow* candidate = addScheduleRow(page, conflictCase.type);
        QVERIFY(candidate);
        setSchedule(
            candidate,
            conflictCase.day,
            conflictCase.start,
            conflictCase.end
            );

        QString warningTitle;
        QString warningMessage;
        QVERIFY(!saveAndAcceptWarning(page, &warningTitle, &warningMessage));
        QCOMPARE(warningTitle, conflictCase.warningTitle);
        QVERIFY(warningMessage.contains(QStringLiteral("conflicts")));
        QCOMPARE(port.callCount, 0);
        QVERIFY(page.hasUnsavedChanges());
    }
}

void ClassDetailsSavePageTests::
regularConflictShortCircuitsIntensiveAndKeepsSameNameWording()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(
        services,
        QStringLiteral("Class Details Same Name Conflict")
        );
    QVERIFY(classId > 0);

    RecordingClassDetailsSavePort savePort;
    RecordingClassDetailsScheduleConflictPort conflictPort;
    conflictPort.regularResult =
        Application::ClassDetailsScheduleConflictResult::success({
            conflict(
                u"Same Display Name",
                u"Monday",
                u"9:00 AM",
                u"9:55 AM",
                u"Same Display Name"
                ),
            conflict(
                u"Same Display Name",
                u"Wednesday",
                u"11:00 AM",
                u"11:55 AM",
                u"Other Display Name"
                )
        });
    ClassDetailsPage page(
        &services,
        false,
        nullptr,
        &savePort,
        nullptr,
        &conflictPort
        );
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(
        Classroom(QStringLiteral("Class Details Same Name Conflict"), classId)
        );

    ClassTimeRow* regular = addScheduleRow(page, ScheduleType::Regular);
    QVERIFY(regular);
    setSchedule(
        regular,
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:55 AM")
        );
    ClassTimeRow* intensive = addScheduleRow(page, ScheduleType::Intensive);
    QVERIFY(intensive);
    setSchedule(
        intensive,
        QStringLiteral("Tuesday"),
        QStringLiteral("12:00 PM"),
        QStringLiteral("12:55 PM")
        );

    QString warningTitle;
    QString warningMessage;
    QVERIFY(!saveAndAcceptWarning(page, &warningTitle, &warningMessage));

    QCOMPARE(warningTitle, QStringLiteral("Regular Schedule Conflicts"));
    QCOMPARE(
        warningMessage,
        QStringLiteral(
            "Please resolve these schedule conflicts before saving:\n\n"
            "Monday 9:00 AM-9:55 AM conflicts with another time in this class.\n"
            "Wednesday 11:00 AM-11:55 AM conflicts with Other Display Name."
            )
        );
    QCOMPARE(conflictPort.requests.size(), std::size_t(1));
    QCOMPARE(
        conflictPort.requests.front().mode,
        Application::ClassDetailsScheduleMode::Regular
        );
    QCOMPARE(conflictPort.requests.front().classId.value(),
        std::to_string(classId));
    QCOMPARE(conflictPort.requests.front().candidateTimes.size(),
        std::size_t(1));
    QCOMPARE(savePort.callCount, 0);
    QVERIFY(page.hasUnsavedChanges());
}

void ClassDetailsSavePageTests::intensiveConflictFollowsAnEmptyRegularQuery()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(
        services,
        QStringLiteral("Class Details Intensive Conflict")
        );
    QVERIFY(classId > 0);

    RecordingClassDetailsSavePort savePort;
    RecordingClassDetailsScheduleConflictPort conflictPort;
    conflictPort.intensiveResult =
        Application::ClassDetailsScheduleConflictResult::success({
            conflict(
                u"Class Details Intensive Conflict",
                u"Tuesday",
                u"12:00 PM",
                u"12:55 PM",
                u"Intensive Source"
                )
        });
    ClassDetailsPage page(
        &services,
        false,
        nullptr,
        &savePort,
        nullptr,
        &conflictPort
        );
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(
        Classroom(QStringLiteral("Class Details Intensive Conflict"), classId)
        );
    ClassTimeRow* intensive = addScheduleRow(page, ScheduleType::Intensive);
    QVERIFY(intensive);
    setSchedule(
        intensive,
        QStringLiteral("Tuesday"),
        QStringLiteral("12:00 PM"),
        QStringLiteral("12:55 PM")
        );

    QString warningTitle;
    QString warningMessage;
    QVERIFY(!saveAndAcceptWarning(page, &warningTitle, &warningMessage));

    QCOMPARE(warningTitle, QStringLiteral("Intensive Schedule Conflicts"));
    QCOMPARE(
        warningMessage,
        QStringLiteral(
            "Please resolve these schedule conflicts before saving:\n\n"
            "Tuesday 12:00 PM-12:55 PM conflicts with Intensive Source."
            )
        );
    QCOMPARE(conflictPort.requests.size(), std::size_t(2));
    QCOMPARE(
        conflictPort.requests[0].mode,
        Application::ClassDetailsScheduleMode::Regular
        );
    QVERIFY(conflictPort.requests[0].candidateTimes.empty());
    QCOMPARE(
        conflictPort.requests[1].mode,
        Application::ClassDetailsScheduleMode::Intensive
        );
    QCOMPARE(conflictPort.requests[1].candidateTimes.size(), std::size_t(1));
    QCOMPARE(savePort.callCount, 0);
    QVERIFY(page.hasUnsavedChanges());
}

void ClassDetailsSavePageTests::regularQueryFailureBlocksBeforeIntensive()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(
        services,
        QStringLiteral("Class Details Conflict Query Error")
        );
    QVERIFY(classId > 0);

    RecordingClassDetailsSavePort savePort;
    RecordingClassDetailsScheduleConflictPort conflictPort;
    conflictPort.regularResult =
        Application::ClassDetailsScheduleConflictResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "regular conflict query failed",
            .recoverable = true
        });
    ClassDetailsPage page(
        &services,
        false,
        nullptr,
        &savePort,
        nullptr,
        &conflictPort
        );
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(
        Classroom(QStringLiteral("Class Details Conflict Query Error"), classId)
        );
    ClassTimeRow* regular = addScheduleRow(page, ScheduleType::Regular);
    QVERIFY(regular);
    setSchedule(
        regular,
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:55 AM")
        );
    ClassTimeRow* intensive = addScheduleRow(page, ScheduleType::Intensive);
    QVERIFY(intensive);
    setSchedule(
        intensive,
        QStringLiteral("Tuesday"),
        QStringLiteral("12:00 PM"),
        QStringLiteral("12:55 PM")
        );

    QString warningTitle;
    QString warningMessage;
    QVERIFY(!saveAndAcceptWarning(page, &warningTitle, &warningMessage));

    QCOMPARE(warningTitle, QStringLiteral("Regular Schedule Conflicts"));
    QCOMPARE(warningMessage, QStringLiteral("regular conflict query failed"));
    QCOMPARE(conflictPort.requests.size(), std::size_t(1));
    QCOMPARE(savePort.callCount, 0);
    QVERIFY(page.hasUnsavedChanges());
}

void ClassDetailsSavePageTests::
intensiveQueryFailureBlocksAfterRegularSuccess()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(
        services,
        QStringLiteral("Class Details Intensive Query Error")
        );
    QVERIFY(classId > 0);

    RecordingClassDetailsSavePort savePort;
    RecordingClassDetailsScheduleConflictPort conflictPort;
    conflictPort.intensiveResult =
        Application::ClassDetailsScheduleConflictResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "intensive conflict query failed",
            .recoverable = true
        });
    ClassDetailsPage page(
        &services,
        false,
        nullptr,
        &savePort,
        nullptr,
        &conflictPort
        );
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(
        Classroom(QStringLiteral("Class Details Intensive Query Error"), classId)
        );
    ClassTimeRow* regular = addScheduleRow(page, ScheduleType::Regular);
    QVERIFY(regular);
    setSchedule(
        regular,
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:55 AM")
        );
    ClassTimeRow* intensive = addScheduleRow(page, ScheduleType::Intensive);
    QVERIFY(intensive);
    setSchedule(
        intensive,
        QStringLiteral("Tuesday"),
        QStringLiteral("12:00 PM"),
        QStringLiteral("12:55 PM")
        );

    QString warningTitle;
    QString warningMessage;
    QVERIFY(!saveAndAcceptWarning(page, &warningTitle, &warningMessage));

    QCOMPARE(warningTitle, QStringLiteral("Intensive Schedule Conflicts"));
    QCOMPARE(warningMessage, QStringLiteral("intensive conflict query failed"));
    QCOMPARE(conflictPort.requests.size(), std::size_t(2));
    QCOMPARE(
        conflictPort.requests[0].mode,
        Application::ClassDetailsScheduleMode::Regular
        );
    QCOMPARE(
        conflictPort.requests[1].mode,
        Application::ClassDetailsScheduleMode::Intensive
        );
    QCOMPARE(savePort.callCount, 0);
    QVERIFY(page.hasUnsavedChanges());
}

void ClassDetailsSavePageTests::noninteractiveConflictBlocksSilently()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(
        services,
        QStringLiteral("Class Details Silent Conflict")
        );
    QVERIFY(classId > 0);

    RecordingClassDetailsSavePort savePort;
    RecordingClassDetailsScheduleConflictPort conflictPort;
    conflictPort.regularResult =
        Application::ClassDetailsScheduleConflictResult::success({
            conflict(
                u"Class Details Silent Conflict",
                u"Monday",
                u"9:00 AM",
                u"9:55 AM",
                u"Silent Source"
                )
        });
    ClassDetailsPage page(
        &services,
        false,
        nullptr,
        &savePort,
        nullptr,
        &conflictPort
        );
    page.setSaveMode(SaveMode::Automatic);
    page.loadClass(
        Classroom(QStringLiteral("Class Details Silent Conflict"), classId)
        );
    ClassTimeRow* regular = addScheduleRow(page, ScheduleType::Regular);
    QVERIFY(regular);
    setSchedule(
        regular,
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:55 AM")
        );

    AutosaveCoordinator* const autosave =
        page.findChild<AutosaveCoordinator*>();
    QVERIFY(autosave);
    autosave->requestSave(false);

    QVERIFY(!DialogServices::promptTestDriver().activePrompt());
    QCOMPARE(conflictPort.requests.size(), std::size_t(1));
    QCOMPARE(savePort.callCount, 0);
    QVERIFY(page.hasUnsavedChanges());
}

QTEST_MAIN(ClassDetailsSavePageTests)

#include "class_details_save_page_tests.moc"
