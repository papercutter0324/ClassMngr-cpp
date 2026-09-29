#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/teacher.h"
#include "features/classes/ui/class_details_page.h"
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

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("class-details-save-parity-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

int createClass(ApplicationServices& services, const QString& name)
{
    return services.classService()->create(name).value_or(-1);
}

int createTeacher(ApplicationServices& services)
{
    Teacher teacher;
    teacher.teacherKr = QStringLiteral("\uAE40\uC120\uC0DD\uB2D8");
    teacher.teacherEn = QStringLiteral("Parity Teacher");
    teacher.preferredRomanization = QStringLiteral("Parity Teacher");
    teacher.preferredName = QStringLiteral("Parity Teacher");
    teacher.roomNumber = QStringLiteral("Room 504");
    teacher.wifiName = QStringLiteral("Parity WiFi");
    teacher.wifiPassword = QStringLiteral("parity-wifi-password");
    teacher.internetType = QStringLiteral("Both");
    teacher.zoomId = QStringLiteral("123 456 7890");
    teacher.zoomPassword = QStringLiteral("parity-zoom-password");
    teacher.projectionType = QStringLiteral("Any");
    teacher.notes = QStringLiteral("Teacher profile remains untouched");

    return services.teacherService()->create(teacher).value_or(-1);
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

    const QString buttonText = type == ScheduleType::Regular
        ? QStringLiteral("+ Add Time")
        : QStringLiteral("+ Add Intensive Time");
    for (QPushButton* button : schedule->findChildren<QPushButton*>())
    {
        if (button->text() == buttonText)
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

bool saveAndCaptureWarning(
    ClassDetailsPage& page,
    QString* title,
    QString* message
    )
{
    QTimer::singleShot(0, [&] {
        const auto prompt = DialogServices::promptTestDriver().activePrompt();
        if (!prompt)
        {
            return;
        }
        if (title)
        {
            *title = prompt->title;
        }
        if (message)
        {
            *message = prompt->text;
        }
        DialogServices::promptTestDriver().accept(prompt->id);
    });
    return page.saveChanges();
}

void chooseNewDetails(ClassDetailsPage& page)
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

}

class ClassDetailsPageSaveParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void successfulUiSaveMatchesSeededCommonInputState();
    void regularAndIntensiveConflictSavesMatchCommonBaselineInputs();
};

void ClassDetailsPageSaveParityTests::successfulUiSaveMatchesSeededCommonInputState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const int teacherId = createTeacher(services);
    const int classId = createClass(
        services,
        QStringLiteral("Class Details Save Parity")
        );
    QVERIFY(teacherId > 0);
    QVERIFY(classId > 0);

    auto seededInfoResult = services.classService()->classInfo(classId);
    QVERIFY(seededInfoResult);
    ClassInfo seededInfo = *seededInfoResult;
    seededInfo.teacherId = teacherId;
    seededInfo.classGrade = QStringLiteral("E5");
    seededInfo.classLevel = QStringLiteral("Artemis");
    seededInfo.readingBook = QStringLiteral("Reading Explorer 2");
    seededInfo.essayBook = QStringLiteral("5A");
    seededInfo.classColor = QStringLiteral("#AABBCC");
    seededInfo.fontColor = QStringLiteral("#112233");
    seededInfo.notes = QStringLiteral("Keep these notes exactly.\nSecond line.");
    seededInfo.timeFillerActivities = QStringLiteral(
        "Quiet reading\nWord games"
        );
    seededInfo.classTimes = {
        {
            QStringLiteral("Friday"),
            QStringLiteral("3:00 PM"),
            QStringLiteral("3:55 PM")
        },
        {
            QStringLiteral("Monday"),
            QStringLiteral("9:00 AM"),
            QStringLiteral("9:55 AM")
        }
    };
    seededInfo.intensiveTimes = {
        {
            QStringLiteral("Wednesday"),
            QStringLiteral("1:00 PM"),
            QStringLiteral("1:55 PM")
        },
        {
            QStringLiteral("Tuesday"),
            QStringLiteral("10:00 AM"),
            QStringLiteral("10:55 AM")
        }
    };
    QVERIFY(services.classService()->saveClassInfo(seededInfo));

    ClassDetailsPage page(&services, false);
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(
        Classroom(QStringLiteral("Class Details Save Parity"), classId)
        );

    QSignalSpy savedSpy(&page, &ClassDetailsPage::classInfoSaved);
    QVERIFY(savedSpy.isValid());
    chooseNewDetails(page);

    QVERIFY(page.hasUnsavedChanges());
    auto* saveButton = page.findChild<QPushButton*>(
        QStringLiteral("classInfoSaveButton")
        );
    QVERIFY(saveButton);
    QVERIFY(saveButton->isEnabled());
    saveButton->click();

    QCOMPARE(savedSpy.size(), 1);
    QCOMPARE(savedSpy.at(0).at(0).toInt(), classId);
    QVERIFY(!page.hasUnsavedChanges());

    const auto persistedResult = services.classService()->classInfo(classId);
    QVERIFY(persistedResult);
    const ClassInfo& persisted = *persistedResult;

    QCOMPARE(persisted.classId, classId);
    QCOMPARE(persisted.teacherId, teacherId);
    QCOMPARE(persisted.teacherKr, QStringLiteral("\uAE40\uC120\uC0DD\uB2D8"));
    QCOMPARE(persisted.teacherEn, QStringLiteral("Parity Teacher"));
    QCOMPARE(
        persisted.teacherPreferredName,
        QStringLiteral("Parity Teacher")
        );
    QCOMPARE(persisted.roomNumber, QStringLiteral("Room 504"));
    QCOMPARE(persisted.wifiName, QStringLiteral("Parity WiFi"));
    QCOMPARE(persisted.wifiPassword, QStringLiteral("parity-wifi-password"));
    QCOMPARE(persisted.internetType, QStringLiteral("Both"));
    QCOMPARE(persisted.zoomId, QStringLiteral("123 456 7890"));
    QCOMPARE(persisted.zoomPassword, QStringLiteral("parity-zoom-password"));
    QCOMPARE(persisted.projectionType, QStringLiteral("Any"));
    QCOMPARE(persisted.classGrade, QStringLiteral("E4"));
    QCOMPARE(persisted.classLevel, QStringLiteral("Theseus"));
    QCOMPARE(persisted.readingBook, QStringLiteral("Reading Explorer 1"));
    QCOMPARE(persisted.essayBook, QStringLiteral("4A"));
    QCOMPARE(persisted.classColor, QStringLiteral("#AABBCC"));
    QCOMPARE(persisted.fontColor, QStringLiteral("#112233"));
    QCOMPARE(
        persisted.notes,
        QStringLiteral("Keep these notes exactly.\nSecond line.")
        );
    QCOMPARE(
        persisted.timeFillerActivities,
        QStringLiteral("Quiet reading\nWord games")
        );

    QCOMPARE(persisted.classTimes.size(), 2);
    QCOMPARE(persisted.classTimes.at(0).day, QStringLiteral("Friday"));
    QCOMPARE(persisted.classTimes.at(0).startTime, QStringLiteral("3:00 PM"));
    QCOMPARE(persisted.classTimes.at(0).endTime, QStringLiteral("3:55 PM"));
    QCOMPARE(persisted.classTimes.at(1).day, QStringLiteral("Monday"));
    QCOMPARE(persisted.classTimes.at(1).startTime, QStringLiteral("9:00 AM"));
    QCOMPARE(persisted.classTimes.at(1).endTime, QStringLiteral("9:55 AM"));

    QCOMPARE(persisted.intensiveTimes.size(), 2);
    QCOMPARE(persisted.intensiveTimes.at(0).day, QStringLiteral("Wednesday"));
    QCOMPARE(
        persisted.intensiveTimes.at(0).startTime,
        QStringLiteral("1:00 PM")
        );
    QCOMPARE(
        persisted.intensiveTimes.at(0).endTime,
        QStringLiteral("1:55 PM")
        );
    QCOMPARE(persisted.intensiveTimes.at(1).day, QStringLiteral("Tuesday"));
    QCOMPARE(
        persisted.intensiveTimes.at(1).startTime,
        QStringLiteral("10:00 AM")
        );
    QCOMPARE(
        persisted.intensiveTimes.at(1).endTime,
        QStringLiteral("10:55 AM")
        );
}

void ClassDetailsPageSaveParityTests::
regularAndIntensiveConflictSavesMatchCommonBaselineInputs()
{
    const struct ConflictCase
    {
        ScheduleType mode;
        QString day;
        QString start;
        QString end;
        QString sourceName;
        QString warningTitle;
    } cases[] = {
        {
            ScheduleType::Regular,
            QStringLiteral("Monday"),
            QStringLiteral("9:00 AM"),
            QStringLiteral("9:55 AM"),
            QStringLiteral("Common Input Regular Source"),
            QStringLiteral("Regular Schedule Conflicts")
        },
        {
            ScheduleType::Intensive,
            QStringLiteral("Tuesday"),
            QStringLiteral("12:00 PM"),
            QStringLiteral("12:55 PM"),
            QStringLiteral("Common Input Intensive Source"),
            QStringLiteral("Intensive Schedule Conflicts")
        }
    };

    for (const ConflictCase& conflictCase : cases)
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ApplicationServices services;
        QVERIFY(services.openDatabase(databasePath(directory)));

        const int targetId = createClass(
            services,
            QStringLiteral("Class Details Common Input Target")
            );
        const int sourceId = createClass(services, conflictCase.sourceName);
        QVERIFY(targetId > 0);
        QVERIFY(sourceId > 0);

        auto source = services.classService()->classInfo(sourceId);
        QVERIFY(source);
        const ClassTime overlap{
            conflictCase.day,
            conflictCase.start,
            conflictCase.end
        };
        if (conflictCase.mode == ScheduleType::Regular)
        {
            source->classTimes.append(overlap);
        }
        else
        {
            source->intensiveTimes.append(overlap);
        }
        QVERIFY(services.classService()->saveClassInfo(*source));

        ClassDetailsPage page(&services, false);
        page.setSaveMode(SaveMode::Manual);
        page.loadClass(
            Classroom(
                QStringLiteral("Class Details Common Input Target"),
                targetId
                )
            );
        ClassTimeRow* candidate = addScheduleRow(page, conflictCase.mode);
        QVERIFY(candidate);
        setSchedule(
            candidate,
            conflictCase.day,
            conflictCase.start,
            conflictCase.end
            );

        auto* header = page.findChild<PageHeader*>();
        QVERIFY(header);
        const QString subtitleBefore = header->subtitle();
        QSignalSpy savedSpy(&page, &ClassDetailsPage::classInfoSaved);
        QVERIFY(savedSpy.isValid());
        QVERIFY(page.hasUnsavedChanges());

        QString warningTitle;
        QString warningMessage;
        QVERIFY(!saveAndCaptureWarning(page, &warningTitle, &warningMessage));

        QCOMPARE(warningTitle, conflictCase.warningTitle);
        QCOMPARE(
            warningMessage,
            QStringLiteral(
                "Please resolve these schedule conflicts before saving:\n\n%1 "
                "%2-%3 conflicts with %4."
                )
                .arg(conflictCase.day)
                .arg(conflictCase.start)
                .arg(conflictCase.end)
                .arg(conflictCase.sourceName)
            );
        QCOMPARE(savedSpy.size(), 0);
        QVERIFY(page.hasUnsavedChanges());
        QCOMPARE(header->subtitle(), subtitleBefore);

        const auto unchangedTarget =
            services.classService()->classInfo(targetId);
        QVERIFY(unchangedTarget);
        QVERIFY(unchangedTarget->classTimes.isEmpty());
        QVERIFY(unchangedTarget->intensiveTimes.isEmpty());

        const auto unchangedSource =
            services.classService()->classInfo(sourceId);
        QVERIFY(unchangedSource);
        if (conflictCase.mode == ScheduleType::Regular)
        {
            QCOMPARE(unchangedSource->classTimes.size(), 1);
            QVERIFY(unchangedSource->intensiveTimes.isEmpty());
        }
        else
        {
            QVERIFY(unchangedSource->classTimes.isEmpty());
            QCOMPARE(unchangedSource->intensiveTimes.size(), 1);
        }
    }
}

QTEST_MAIN(ClassDetailsPageSaveParityTests)

#include "class_details_page_save_parity_tests.moc"
