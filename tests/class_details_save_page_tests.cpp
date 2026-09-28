#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "features/classes/ui/class_details_page.h"
#include "next/application/class_details_save_use_case.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/page_header.h"
#include "ui/shared/widgets/sectioncards/class_time_row.h"
#include "ui/shared/widgets/sections/class_schedule_section.h"

#include <QComboBox>
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
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<Application::ClassDetailsSaveRequest> lastRequest;
    Domain::Result<void> result = Domain::Result<void>::success();
};

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
    void regularAndIntensiveConflictsBlockTheSavePort();
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
    ClassDetailsPage page(&services, false, nullptr, &port);
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
    QVERIFY(port.lastRequest.has_value());
    const Application::ClassDetailsSaveRequest& request = *port.lastRequest;
    QCOMPARE(request.classId.value(), std::to_string(classId));
    QCOMPARE(request.classGrade, std::u16string(u"E4"));
    QCOMPARE(request.classLevel, std::u16string(u"Theseus"));
    QCOMPARE(request.readingBook, std::u16string(u"Reading Explorer 1"));
    QCOMPARE(request.essayBook, std::u16string(u"4A"));
    QCOMPARE(request.regularTimes.size(), std::size_t(2));
    QCOMPARE(request.regularTimes[0].weekdayIndex(), 0);
    QCOMPARE(request.regularTimes[0].startMinute(), 9 * 60);
    QCOMPARE(request.regularTimes[0].endMinute(), 9 * 60 + 55);
    QCOMPARE(request.regularTimes[1].weekdayIndex(), 1);
    QCOMPARE(request.regularTimes[1].startMinute(), 15 * 60);
    QCOMPARE(request.regularTimes[1].endMinute(), 15 * 60 + 55);
    QCOMPARE(request.intensiveTimes.size(), std::size_t(2));
    QCOMPARE(request.intensiveTimes[0].weekdayIndex(), 1);
    QCOMPARE(request.intensiveTimes[0].startMinute(), 12 * 60);
    QCOMPARE(request.intensiveTimes[0].endMinute(), 12 * 60 + 55);
    QCOMPARE(request.intensiveTimes[1].weekdayIndex(), 2);
    QCOMPARE(request.intensiveTimes[1].startMinute(), 0);
    QCOMPARE(request.intensiveTimes[1].endMinute(), 55);
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
    ClassDetailsPage page(&services, false, nullptr, &port);
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(Classroom(QStringLiteral("Class Details Invalid Save"), classId));
    auto* grade = page.findChild<QComboBox*>(QStringLiteral("classGradeCombo"));
    auto* level = page.findChild<QComboBox*>(QStringLiteral("classLevelCombo"));
    QVERIFY(grade && level);
    grade->setCurrentText(QStringLiteral("E4"));
    level->setCurrentIndex(0);

    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(!page.saveChanges());
    QCOMPARE(port.callCount, 0);
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
        setSchedule(
            addScheduleRow(page, conflictCase.type),
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

QTEST_MAIN(ClassDetailsSavePageTests)

#include "class_details_save_page_tests.moc"
