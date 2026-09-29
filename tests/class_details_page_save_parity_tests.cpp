#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/teacher.h"
#include "features/classes/ui/class_details_page.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/page_header.h"
#include "ui/shared/validation/form_validation_binder.h"
#include "ui/shared/widgets/sectioncards/class_time_row.h"
#include "ui/shared/widgets/sections/class_schedule_section.h"

#include <QCoreApplication>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QSet>
#include <QTimer>
#include <QtTest/QtTest>
#include <QUuid>
#include <QVariantList>

#include <vector>

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

bool seedClassInfo(ApplicationServices& services, const int classId)
{
    const auto info = services.classService()->classInfo(classId);
    return info && services.classService()->saveClassInfo(*info).has_value();
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

void setRawEndTime(ClassTimeRow* row, const QString& value)
{
    Q_ASSERT(row);
    row->endCombo()->addItem(value);
    row->endCombo()->setCurrentText(value);
}

bool sameSchedule(
    const QList<ClassTime>& left,
    const QList<ClassTime>& right
    )
{
    if (left.size() != right.size())
    {
        return false;
    }
    for (qsizetype index = 0; index < left.size(); ++index)
    {
        if (left.at(index).day != right.at(index).day
            || left.at(index).startTime != right.at(index).startTime
            || left.at(index).endTime != right.at(index).endTime)
        {
            return false;
        }
    }
    return true;
}

bool sameClassInfo(const ClassInfo& left, const ClassInfo& right)
{
    return left.classId == right.classId
        && left.teacherId == right.teacherId
        && left.teacherKr == right.teacherKr
        && left.teacherEn == right.teacherEn
        && left.teacherPreferredName == right.teacherPreferredName
        && left.roomNumber == right.roomNumber
        && left.wifiName == right.wifiName
        && left.wifiPassword == right.wifiPassword
        && left.internetType == right.internetType
        && left.zoomId == right.zoomId
        && left.zoomPassword == right.zoomPassword
        && left.projectionType == right.projectionType
        && left.classGrade == right.classGrade
        && left.classLevel == right.classLevel
        && left.readingBook == right.readingBook
        && left.essayBook == right.essayBook
        && left.classColor == right.classColor
        && left.fontColor == right.fontColor
        && left.notes == right.notes
        && left.timeFillerActivities == right.timeFillerActivities
        && sameSchedule(left.classTimes, right.classTimes)
        && sameSchedule(left.intensiveTimes, right.intensiveTimes);
}

bool saveAndCaptureWarning(
    ClassDetailsPage& page,
    QString* title,
    QString* message
    )
{
    QTimer::singleShot(0, [title, message] {
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

bool saveAndCaptureOptionalWarning(
    ClassDetailsPage& page,
    QString* title,
    QString* message
    )
{
    const bool saved = saveAndCaptureWarning(page, title, message);
    QCoreApplication::processEvents();
    return saved;
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
    void invalidUiSaveMatchesCommonBaselineInputState();
    void invalidScheduleUiSavesMatchCommonBaselineInputs();
    void staleHiddenValidationDataBlocksCommonBaselineSaveInputs();
    void validVisibleSavePreservesUpdatedHiddenValues();
    void freshInvalidTeacherIdBlocksCommonBaselineSaveInputs();
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

    QSqlQuery hiddenFields(services.databaseSession()->database());
    hiddenFields.prepare(QStringLiteral(
        "UPDATE class_info SET notes=?, time_filler_activities=? "
        "WHERE class_id=?"
        ));
    hiddenFields.addBindValue(
        QStringLiteral("  Keep these notes exactly.\nSecond line.  ")
        );
    hiddenFields.addBindValue(QStringLiteral("  Quiet reading\nWord games  "));
    hiddenFields.addBindValue(classId);
    QVERIFY(hiddenFields.exec());

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

void ClassDetailsPageSaveParityTests::
invalidUiSaveMatchesCommonBaselineInputState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(
        services,
        QStringLiteral("Class Details Invalid Save Parity")
        );
    QVERIFY(classId > 0);

    auto seededInfoResult = services.classService()->classInfo(classId);
    QVERIFY(seededInfoResult);
    ClassInfo seededInfo = *seededInfoResult;
    seededInfo.classGrade = QStringLiteral("E4");
    seededInfo.classLevel = QStringLiteral("Theseus");
    seededInfo.readingBook = QStringLiteral("Reading Explorer 1");
    seededInfo.essayBook = QStringLiteral("4A");
    seededInfo.classColor = QStringLiteral("#AABBCC");
    seededInfo.fontColor = QStringLiteral("#112233");
    seededInfo.classTimes = {{
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:55 AM")
    }};
    seededInfo.intensiveTimes = {{
        QStringLiteral("Tuesday"),
        QStringLiteral("12:00 PM"),
        QStringLiteral("12:55 PM")
    }};
    QVERIFY(services.classService()->saveClassInfo(seededInfo));

    ClassDetailsPage page(&services, false);
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(
        Classroom(QStringLiteral("Class Details Invalid Save Parity"), classId)
        );

    auto* grade = page.findChild<QComboBox*>(QStringLiteral("classGradeCombo"));
    auto* level = page.findChild<QComboBox*>(QStringLiteral("classLevelCombo"));
    auto* header = page.findChild<PageHeader*>();
    QVERIFY(grade && level && header);
    const QString subtitleBefore = header->subtitle();
    const auto persistedBefore = services.classService()->classInfo(classId);
    QVERIFY(persistedBefore);

    QSignalSpy savedSpy(&page, &ClassDetailsPage::classInfoSaved);
    QVERIFY(savedSpy.isValid());
    grade->setCurrentText(QStringLiteral("E4"));
    level->setCurrentIndex(0);
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(!page.saveChanges());

    auto* validationMessage = page.findChild<QLabel*>(
        QStringLiteral("classLevelValidationMessage")
        );
    QVERIFY(validationMessage);
    QCOMPARE(validationMessage->text(), QStringLiteral("This field is required."));
    QCOMPARE(level->property("formValidationState").toString(),
        QStringLiteral("error"));
    QCOMPARE(savedSpy.size(), 0);
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(header->subtitle(), subtitleBefore);

    const auto persistedAfter = services.classService()->classInfo(classId);
    QVERIFY(persistedAfter);
    QCOMPARE(persistedAfter->classGrade, persistedBefore->classGrade);
    QCOMPARE(persistedAfter->classLevel, persistedBefore->classLevel);
    QCOMPARE(persistedAfter->readingBook, persistedBefore->readingBook);
    QCOMPARE(persistedAfter->essayBook, persistedBefore->essayBook);
    QCOMPARE(persistedAfter->classTimes.size(), persistedBefore->classTimes.size());
    QCOMPARE(
        persistedAfter->intensiveTimes.size(),
        persistedBefore->intensiveTimes.size()
        );
}

void ClassDetailsPageSaveParityTests::
invalidScheduleUiSavesMatchCommonBaselineInputs()
{
    enum class FailureKind
    {
        MalformedRegular,
        MalformedIntensive,
        EndBeforeStart,
        DuplicateRows
    };

    const FailureKind cases[]{
        FailureKind::MalformedRegular,
        FailureKind::MalformedIntensive,
        FailureKind::EndBeforeStart,
        FailureKind::DuplicateRows
    };

    for (const FailureKind failureKind : cases)
    {
        ScheduleType mode = ScheduleType::Regular;
        QString caseName;
        QString sourceName;
        std::vector<ClassTime> candidateTimes;
        ClassTime conflictTime;
        int malformedRow = -1;
        QString malformedValue;
        QString expectedCode;
        QString expectedField;
        QString expectedMessage;
        QString validationLabel;
        QSet<int> expectedDuplicateRows;

        switch (failureKind)
        {
        case FailureKind::MalformedRegular:
            caseName = QStringLiteral("Malformed Regular");
            sourceName = QStringLiteral("Malformed Regular Conflict Source");
            mode = ScheduleType::Regular;
            candidateTimes = {
                {
                    QStringLiteral("Monday"),
                    QStringLiteral("7:00 AM"),
                    QStringLiteral("placeholder")
                },
                {
                    QStringLiteral("Tuesday"),
                    QStringLiteral("8:00 AM"),
                    QStringLiteral("8:55 AM")
                }
            };
            conflictTime = {
                QStringLiteral("Tuesday"),
                QStringLiteral("8:00 AM"),
                QStringLiteral("8:55 AM")
            };
            malformedRow = 0;
            malformedValue = QStringLiteral("not-a-time");
            expectedCode = QStringLiteral("schedule.time.invalid_format");
            expectedField = QStringLiteral("classTimes[0].endTime");
            expectedMessage = QStringLiteral("Enter a valid value.");
            validationLabel = QStringLiteral(
                "classRegularScheduleValidationMessage"
                );
            break;

        case FailureKind::MalformedIntensive:
            caseName = QStringLiteral("Malformed Intensive");
            sourceName = QStringLiteral("Malformed Intensive Conflict Source");
            mode = ScheduleType::Intensive;
            candidateTimes = {
                {
                    QStringLiteral("Wednesday"),
                    QStringLiteral("1:00 PM"),
                    QStringLiteral("placeholder")
                },
                {
                    QStringLiteral("Tuesday"),
                    QStringLiteral("12:00 PM"),
                    QStringLiteral("12:55 PM")
                }
            };
            conflictTime = {
                QStringLiteral("Tuesday"),
                QStringLiteral("12:00 PM"),
                QStringLiteral("12:55 PM")
            };
            malformedRow = 0;
            malformedValue = QStringLiteral("not-a-time");
            expectedCode = QStringLiteral("schedule.time.invalid_format");
            expectedField = QStringLiteral("intensiveTimes[0].endTime");
            expectedMessage = QStringLiteral("Enter a valid value.");
            validationLabel = QStringLiteral(
                "classIntensiveScheduleValidationMessage"
                );
            break;

        case FailureKind::EndBeforeStart:
            caseName = QStringLiteral("End Before Start");
            sourceName = QStringLiteral("End Before Start Conflict Source");
            candidateTimes = {
                {
                    QStringLiteral("Monday"),
                    QStringLiteral("9:00 AM"),
                    QStringLiteral("8:55 AM")
                },
                {
                    QStringLiteral("Tuesday"),
                    QStringLiteral("8:00 AM"),
                    QStringLiteral("8:55 AM")
                }
            };
            conflictTime = {
                QStringLiteral("Tuesday"),
                QStringLiteral("8:00 AM"),
                QStringLiteral("8:55 AM")
            };
            expectedCode = QStringLiteral("schedule.time.end_not_after_start");
            expectedField = QStringLiteral("classTimes[0].endTime");
            expectedMessage = QStringLiteral(
                "The end time must be after the start time."
                );
            validationLabel = QStringLiteral(
                "classRegularScheduleValidationMessage"
                );
            break;

        case FailureKind::DuplicateRows:
            caseName = QStringLiteral("Duplicate Rows");
            sourceName = QStringLiteral("Duplicate Rows Conflict Source");
            candidateTimes = {
                {
                    QStringLiteral("Monday"),
                    QStringLiteral("9:00 AM"),
                    QStringLiteral("9:55 AM")
                },
                {
                    QStringLiteral("Monday"),
                    QStringLiteral("9:00 AM"),
                    QStringLiteral("9:55 AM")
                },
                {
                    QStringLiteral("Tuesday"),
                    QStringLiteral("8:00 AM"),
                    QStringLiteral("8:55 AM")
                },
                {
                    QStringLiteral("Tuesday"),
                    QStringLiteral("8:00 AM"),
                    QStringLiteral("8:55 AM")
                },
                {
                    QStringLiteral("Wednesday"),
                    QStringLiteral("7:00 AM"),
                    QStringLiteral("7:55 AM")
                }
            };
            conflictTime = candidateTimes.front();
            expectedCode = QStringLiteral("class_time.duplicate_slot");
            expectedMessage = QStringLiteral("Enter a valid value.");
            validationLabel = QStringLiteral(
                "classRegularScheduleValidationMessage"
                );
            expectedDuplicateRows = {0, 1, 2, 3};
            break;
        }

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ApplicationServices services;
        QVERIFY(services.openDatabase(databasePath(directory)));
        const QString targetName = QStringLiteral(
            "Common Invalid Schedule Target %1"
            ).arg(caseName);
        const int targetId = createClass(services, targetName);
        const int sourceId = createClass(services, sourceName);
        QVERIFY(targetId > 0);
        QVERIFY(sourceId > 0);

        auto sourceInfoResult = services.classService()->classInfo(sourceId);
        QVERIFY(sourceInfoResult);
        ClassInfo sourceInfo = *sourceInfoResult;
        if (mode == ScheduleType::Regular)
        {
            sourceInfo.classTimes.append(conflictTime);
        }
        else
        {
            sourceInfo.intensiveTimes.append(conflictTime);
        }
        QVERIFY(services.classService()->saveClassInfo(sourceInfo));

        const auto persistedTargetBefore =
            services.classService()->classInfo(targetId);
        const auto persistedSourceBefore =
            services.classService()->classInfo(sourceId);
        QVERIFY(persistedTargetBefore && persistedSourceBefore);

        // The leading public constructor is shared with the pinned baseline;
        // the conflicting source row acts as a preflight trap if validation
        // ever falls through to conflict-warning handling.
        ClassDetailsPage page(&services, false);
        page.setSaveMode(SaveMode::Manual);
        page.loadClass(Classroom(targetName, targetId));
        auto* header = page.findChild<PageHeader*>();
        auto* schedule = page.findChild<ClassScheduleSection*>();
        QVERIFY(header && schedule);
        const QString subtitleBefore = header->subtitle();

        std::vector<ClassTimeRow*> rows;
        rows.reserve(candidateTimes.size());
        for (std::size_t index = 0; index < candidateTimes.size(); ++index)
        {
            ClassTimeRow* row = addScheduleRow(page, mode);
            QVERIFY(row);
            const ClassTime& value = candidateTimes[index];
            setSchedule(row, value.day, value.startTime, value.endTime);
            if (static_cast<int>(index) == malformedRow)
            {
                setRawEndTime(row, malformedValue);
            }
            else if (failureKind == FailureKind::EndBeforeStart && index == 0)
            {
                setRawEndTime(row, value.endTime);
            }
            rows.push_back(row);
        }

        auto* validationMessage = page.findChild<QLabel*>(validationLabel);
        auto* binder = page.findChild<FormValidationBinder*>();
        QVERIFY(validationMessage && binder);
        QCOMPARE(validationMessage->text(), expectedMessage);
        QVERIFY(binder->validation().hasErrors());

        const ValidationIssues& issues = binder->validation().issues();
        if (failureKind == FailureKind::DuplicateRows)
        {
            QSet<int> reportedRows;
            int duplicateIssueCount = 0;
            for (const ValidationIssue& issue : issues)
            {
                if (issue.code != expectedCode)
                {
                    continue;
                }
                ++duplicateIssueCount;
                reportedRows.insert(issue.row);
                QVERIFY(issue.isError());
                QCOMPARE(issue.column, 1);
                QCOMPARE(
                    issue.field,
                    QStringLiteral("classTimes[%1].startTime").arg(issue.row)
                    );

                QSet<int> groupRows;
                const QVariantList group = issue.arguments
                    .value(QStringLiteral("duplicateRows"))
                    .toList();
                for (const QVariant& row : group)
                {
                    groupRows.insert(row.toInt());
                }
                QVERIFY(groupRows.size() == 2);
                if (issue.row == 0 || issue.row == 1)
                {
                    QVERIFY(groupRows.contains(0));
                    QVERIFY(groupRows.contains(1));
                }
                else
                {
                    QVERIFY(issue.row == 2 || issue.row == 3);
                    QVERIFY(groupRows.contains(2));
                    QVERIFY(groupRows.contains(3));
                }
            }
            QCOMPARE(duplicateIssueCount, 4);
            QCOMPARE(reportedRows.size(), expectedDuplicateRows.size());
            for (const int row : expectedDuplicateRows)
            {
                QVERIFY(reportedRows.contains(row));
                QCOMPARE(
                    rows[static_cast<std::size_t>(row)]
                        ->startHourCombo()
                        ->property("formValidationState")
                        .toString(),
                    QStringLiteral("error")
                    );
            }
            QVERIFY(!rows[4]->startHourCombo()
                         ->property("formValidationState")
                         .isValid());
        }
        else
        {
            QCOMPARE(issues.size(), 1);
            const ValidationIssue& issue = issues.front();
            QVERIFY(issue.isError());
            QCOMPARE(issue.code, expectedCode);
            QCOMPARE(issue.field, expectedField);
            QCOMPARE(issue.row, 0);
            QCOMPARE(issue.column, 2);
            if (failureKind == FailureKind::MalformedRegular
                || failureKind == FailureKind::MalformedIntensive)
            {
                QCOMPARE(
                    issue.arguments.value(QStringLiteral("value")).toString(),
                    malformedValue
                    );
            }
            else
            {
                QCOMPARE(
                    issue.arguments.value(QStringLiteral("start")).toString(),
                    QStringLiteral("9:00 AM")
                    );
                QCOMPARE(
                    issue.arguments.value(QStringLiteral("end")).toString(),
                    QStringLiteral("8:55 AM")
                    );
            }
            QCOMPARE(
                rows.front()->endCombo()->property("formValidationState")
                    .toString(),
                QStringLiteral("error")
                );
        }

        QSignalSpy savedSpy(&page, &ClassDetailsPage::classInfoSaved);
        QVERIFY(savedSpy.isValid());
        QVERIFY(page.hasUnsavedChanges());
        QString warningTitle;
        QString warningMessage;
        QVERIFY(!saveAndCaptureOptionalWarning(
            page,
            &warningTitle,
            &warningMessage
            ));
        QVERIFY(warningTitle.isEmpty());
        QVERIFY(warningMessage.isEmpty());
        QVERIFY(!DialogServices::promptTestDriver().activePrompt());
        QCOMPARE(savedSpy.size(), 0);
        QVERIFY(page.hasUnsavedChanges());
        QCOMPARE(header->subtitle(), subtitleBefore);

        const auto persistedTargetAfter =
            services.classService()->classInfo(targetId);
        const auto persistedSourceAfter =
            services.classService()->classInfo(sourceId);
        QVERIFY(persistedTargetAfter && persistedSourceAfter);
        QVERIFY(sameClassInfo(*persistedTargetBefore, *persistedTargetAfter));
        QVERIFY(sameClassInfo(*persistedSourceBefore, *persistedSourceAfter));
    }
}

void ClassDetailsPageSaveParityTests::
staleHiddenValidationDataBlocksCommonBaselineSaveInputs()
{
    const struct HiddenFieldCase
    {
        QString column;
        QString validationField;
    } cases[] = {
        {
            QStringLiteral("notes"),
            QStringLiteral("notes")
        },
        {
            QStringLiteral("time_filler_activities"),
            QStringLiteral("timeFillerActivities")
        }
    };

    for (const HiddenFieldCase& hiddenField : cases)
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ApplicationServices services;
        QVERIFY(services.openDatabase(databasePath(directory)));
        const QString name = QStringLiteral("Stale Hidden Context %1")
            .arg(hiddenField.validationField);
        const int classId = createClass(services, name);
        QVERIFY(classId > 0);
        QVERIFY(seedClassInfo(services, classId));

        ClassDetailsPage page(&services, false);
        page.setSaveMode(SaveMode::Manual);
        page.loadClass(Classroom(name, classId));

        const QString invalidText(10001, QLatin1Char('x'));
        QSqlQuery update(services.databaseSession()->database());
        update.prepare(
            QStringLiteral("UPDATE class_info SET %1=? WHERE class_id=?")
                .arg(hiddenField.column)
            );
        update.addBindValue(invalidText);
        update.addBindValue(classId);
        QVERIFY2(update.exec(), qPrintable(update.lastError().text()));
        QCOMPARE(update.numRowsAffected(), 1);

        const auto persistedBefore = services.classService()->classInfo(classId);
        QVERIFY(persistedBefore);
        QCOMPARE(
            hiddenField.column == QStringLiteral("notes")
                ? persistedBefore->notes.size()
                : persistedBefore->timeFillerActivities.size(),
            10001
            );

        auto* header = page.findChild<PageHeader*>();
        QVERIFY(header);
        const QString subtitleBefore = header->subtitle();
        chooseNewDetails(page);
        QSignalSpy savedSpy(&page, &ClassDetailsPage::classInfoSaved);
        QVERIFY(savedSpy.isValid());
        QVERIFY(page.hasUnsavedChanges());

        QString warningTitle;
        QString warningMessage;
        QVERIFY(!saveAndCaptureOptionalWarning(
            page,
            &warningTitle,
            &warningMessage
            ));
        QVERIFY(warningTitle.isEmpty());
        QVERIFY(warningMessage.isEmpty());
        QVERIFY(!DialogServices::promptTestDriver().activePrompt());
        QCOMPARE(savedSpy.size(), 0);
        QVERIFY(page.hasUnsavedChanges());
        QCOMPARE(header->subtitle(), subtitleBefore);

        FormValidationBinder* const binder =
            page.findChild<FormValidationBinder*>();
        QVERIFY(binder);
        QVERIFY(binder->hasErrors());
        bool foundExpectedIssue = false;
        for (const ValidationIssue& issue : binder->validation().issues())
        {
            if (issue.code == QStringLiteral("validation.length.out_of_bounds")
                && issue.field == hiddenField.validationField)
            {
                foundExpectedIssue = true;
            }
        }
        QVERIFY(foundExpectedIssue);

        const auto persistedAfter = services.classService()->classInfo(classId);
        QVERIFY(persistedAfter);
        QVERIFY(sameClassInfo(*persistedBefore, *persistedAfter));
    }
}

void ClassDetailsPageSaveParityTests::validVisibleSavePreservesUpdatedHiddenValues()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const QString name = QStringLiteral("Updated Hidden Context Save");
    const int classId = createClass(services, name);
    QVERIFY(classId > 0);
    QVERIFY(seedClassInfo(services, classId));

    ClassDetailsPage page(&services, false);
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(Classroom(name, classId));

    const QString notes = QStringLiteral(
        "Updated after load: notes\nsecond line"
        );
    const QString activities = QStringLiteral(
        "Updated after load: activities\nsecond line"
        );
    QSqlQuery update(services.databaseSession()->database());
    update.prepare(QStringLiteral(
        "UPDATE class_info SET notes=?, time_filler_activities=? "
        "WHERE class_id=?"
        ));
    update.addBindValue(notes);
    update.addBindValue(activities);
    update.addBindValue(classId);
    QVERIFY2(update.exec(), qPrintable(update.lastError().text()));
    QCOMPARE(update.numRowsAffected(), 1);

    chooseNewDetails(page);
    QSignalSpy savedSpy(&page, &ClassDetailsPage::classInfoSaved);
    QVERIFY(savedSpy.isValid());
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(page.saveChanges());

    QCOMPARE(savedSpy.size(), 1);
    QCOMPARE(savedSpy.at(0).at(0).toInt(), classId);
    QVERIFY(!page.hasUnsavedChanges());
    const auto persisted = services.classService()->classInfo(classId);
    QVERIFY(persisted);
    QCOMPARE(persisted->classGrade, QStringLiteral("E4"));
    QCOMPARE(persisted->classLevel, QStringLiteral("Theseus"));
    QCOMPARE(persisted->readingBook, QStringLiteral("Reading Explorer 1"));
    QCOMPARE(persisted->essayBook, QStringLiteral("4A"));
    QCOMPARE(persisted->notes, notes);
    QCOMPARE(persisted->timeFillerActivities, activities);
}

void ClassDetailsPageSaveParityTests::
freshInvalidTeacherIdBlocksCommonBaselineSaveInputs()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const QString targetName = QStringLiteral("Fresh Invalid Teacher Target");
    const QString sourceName = QStringLiteral("Fresh Invalid Teacher Conflict Source");
    const int targetId = createClass(services, targetName);
    const int sourceId = createClass(services, sourceName);
    QVERIFY(targetId > 0);
    QVERIFY(sourceId > 0);
    QVERIFY(seedClassInfo(services, targetId));

    auto sourceInfo = services.classService()->classInfo(sourceId);
    QVERIFY(sourceInfo);
    sourceInfo->classTimes.append({
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:55 AM")
    });
    QVERIFY(services.classService()->saveClassInfo(*sourceInfo));
    const auto sourceBefore = services.classService()->classInfo(sourceId);
    QVERIFY(sourceBefore);

    ClassDetailsPage page(&services, false);
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(Classroom(targetName, targetId));
    auto* header = page.findChild<PageHeader*>();
    QVERIFY(header);
    const QString subtitleBefore = header->subtitle();

    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session && session->isOpen());
    QSqlQuery disableForeignKeys(session->database());
    QVERIFY2(disableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys = OFF")),
        qPrintable(disableForeignKeys.lastError().text()));
    disableForeignKeys.finish();

    QSqlQuery updateTeacherId(session->database());
    updateTeacherId.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id=? WHERE class_id=?"
        ));
    updateTeacherId.addBindValue(0);
    updateTeacherId.addBindValue(targetId);
    QVERIFY2(updateTeacherId.exec(),
        qPrintable(updateTeacherId.lastError().text()));
    QCOMPARE(updateTeacherId.numRowsAffected(), 1);
    updateTeacherId.finish();

    QSqlQuery enableForeignKeys(session->database());
    QVERIFY2(enableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys = ON")),
        qPrintable(enableForeignKeys.lastError().text()));
    enableForeignKeys.finish();

    QSqlQuery verifyRawTeacherId(session->database());
    verifyRawTeacherId.prepare(QStringLiteral(
        "SELECT teacher_id FROM class_info WHERE class_id=?"
        ));
    verifyRawTeacherId.addBindValue(targetId);
    QVERIFY2(verifyRawTeacherId.exec(),
        qPrintable(verifyRawTeacherId.lastError().text()));
    QVERIFY(verifyRawTeacherId.next());
    QVERIFY(!verifyRawTeacherId.value(0).isNull());
    QCOMPARE(verifyRawTeacherId.value(0).toInt(), 0);
    verifyRawTeacherId.finish();

    const auto targetBefore = services.classService()->classInfo(targetId);
    QVERIFY(targetBefore);
    QCOMPARE(targetBefore->teacherId, 0);

    ClassTimeRow* const candidate = addScheduleRow(page, ScheduleType::Regular);
    QVERIFY(candidate);
    setSchedule(
        candidate,
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:55 AM")
        );
    QSignalSpy savedSpy(&page, &ClassDetailsPage::classInfoSaved);
    QVERIFY(savedSpy.isValid());
    QVERIFY(page.hasUnsavedChanges());

    QString warningTitle;
    QString warningMessage;
    QVERIFY(!saveAndCaptureOptionalWarning(
        page,
        &warningTitle,
        &warningMessage
        ));
    QVERIFY(warningTitle.isEmpty());
    QVERIFY(warningMessage.isEmpty());
    QVERIFY(!DialogServices::promptTestDriver().activePrompt());
    QCOMPARE(savedSpy.size(), 0);
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(header->subtitle(), subtitleBefore);

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
            QCOMPARE(issue.arguments.value(QStringLiteral("value")).toInt(), 0);
        }
    }
    QVERIFY(foundTeacherIssue);

    const auto targetAfter = services.classService()->classInfo(targetId);
    const auto sourceAfter = services.classService()->classInfo(sourceId);
    QVERIFY(targetAfter && sourceAfter);
    QVERIFY(sameClassInfo(*targetBefore, *targetAfter));
    QVERIFY(sameClassInfo(*sourceBefore, *sourceAfter));
}

QTEST_MAIN(ClassDetailsPageSaveParityTests)

#include "class_details_page_save_parity_tests.moc"
