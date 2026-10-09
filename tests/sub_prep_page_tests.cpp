#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "fakes/fake_user_prompt_service.h"
#include "data/data_service.h"
#include "data/database/database_schema_manager.h"
#include "features/sub_prep/ui/sub_prep_page.h"
#include "features/sub_prep/ui/sub_prep_class_information_list_model.h"
#include "features/sub_prep/ui/sub_prep_print_dialog.h"
#include "features/sub_prep/services/sub_prep_package_service.h"
#include "next/application/sub_prep_class_details_query.h"
#include "next/application/sub_prep_calendar_event_intervals_query.h"
#include "next/application/sub_prep_schedule_summary_query.h"
#include "next/application/sub_prep_print_source_query.h"
#include "ui/shared/widgets/sectioncards/class_info_section_card.h"
#include "features/schedule/ui/schedule_widget.h"
#include "ui/shared/widgets/navigation_tab_widget.h"
#include "ui/shared/widgets/on_screen_keyboard.h"

#include <QtTest>

#include <algorithm>

#include <QLineEdit>
#include <QApplication>
#include <QLabel>
#include <QScrollArea>
#include <QCheckBox>
#include <QDialog>
#include <QDateEdit>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QListView>
#include <QMetaObject>
#include <QPdfDocument>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTextEdit>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>
#include <QVBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <string>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ScheduleWidgetTestStubs
{
void reset();
void setDatabaseOpen(bool open);
void setIncludeAdditionalClass(bool include);
void setExistingIntensiveHours(bool exists);
}

namespace
{
using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

Domain::ClassId typedClassId(int value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

Domain::TeacherId typedTeacherId(int value)
{
    return *Domain::TeacherId::fromString(std::to_string(value));
}

class SubPrepTestSummaryReadPort final
    : public SubPrepScheduleSummaryReadPort
{
public:
    SubPrepScheduleSummaryReadResult loadSummaries(
        const SubPrepScheduleScopeRequest& request
        ) override
    {
        lastRequest = request;
        ClassSummaryProjectionInput input;
        std::unordered_set<int> includedTeachers;
        for (std::size_t index = 0;
             index < request.visibleClassIds.size();
             ++index)
        {
            const Domain::ClassId& id = request.visibleClassIds[index];
            const int legacyId = std::stoi(id.value());
            const bool secondClass = legacyId == 43;
            const bool fridayClass = legacyId == 44;
            const int teacher = fridayClass ? 9 : secondClass ? 8 : 7;
            const std::string grade = fridayClass
                ? "E6"
                : secondClass ? "E5" : "E4";
            const std::string level = fridayClass
                ? "Apollo"
                : secondClass ? "Athena" : "Hercules";
            if (includedTeachers.insert(teacher).second)
            {
                input.teachers.push_back({
                    .id = typedTeacherId(teacher),
                    .displayName = fridayClass
                        ? "Morgan"
                        : secondClass ? "Thomas" : "Susan",
                    .facilities = {},
                    .notes = {}
                });
            }
            input.classes.push_back({
                .id = id,
                .teacherId = typedTeacherId(teacher),
                .grade = grade,
                .level = level,
                .displayLabel = grade + " " + level,
                .meetingText = fridayClass
                    ? "Fri 6pm"
                    : secondClass ? "Thurs 5pm" : "Tues 4pm",
                .studentCount = 9,
                .order = static_cast<std::int32_t>(index)
            });
        }

        return SubPrepScheduleSummaryReadResult::success(
            std::move(input)
            );
    }

    SubPrepScheduleScopeRequest lastRequest;
};

class SubPrepTestDetailsReadPort final
    : public SubPrepClassDetailsReadPort
{
public:
    SubPrepClassDetailsReadResult loadDetails(
        const Domain::ClassId& classId
        ) override
    {
        ++loadCount;
        loadedClassIds.push_back(classId.value());
        const bool secondClass = classId.value() == "43";
        const int teacher = secondClass ? 8 : 7;
        return SubPrepClassDetailsReadResult::success({
            .classId = classId,
            .teacherId = typedTeacherId(teacher),
            .classNotes = secondClass
                ? "Review the vocabulary list."
                : "Read chapter three.",
            .teacherDisplayName = secondClass ? "Thomas" : "Susan",
            .teacherFacilities = {
                .room = secondClass ? "512" : "413",
                .wifiName = secondClass ? "Thomas WiFi" : "Susan WiFi",
                .wifiPassword = "wifi secret",
                .internetType = "WiFi",
                .zoomId = secondClass ? "thomas.zoom" : "susan.zoom",
                .zoomPassword = "zoom secret",
                .projectionType = "HDMI"
            },
            .teacherNotes = secondClass
                ? "Use the classroom projector."
                : "Call before class."
        });
    }

    int loadCount = 0;
    std::vector<std::string> loadedClassIds;
};

class SubPrepTestPrintSourceReadPort final
    : public SubPrepPrintSourceReadPort
{
public:
    SubPrepPrintSourceReadResult loadSource(
        const SubPrepPrintSourceRequest& request
        ) override
    {
        ++loadCount;
        lastRequest = request;
        SubPrepPrintSourceInput selectedInput;
        std::vector<ClassMngr::Next::Domain::TeacherId>
            selectedTeacherIds;
        for (const SubPrepPrintClass& classRecord : sourceInput.classes)
        {
            if (std::find(
                    request.selectedClassIds.cbegin(),
                    request.selectedClassIds.cend(),
                    classRecord.id
                    ) == request.selectedClassIds.cend())
            {
                continue;
            }

            selectedInput.classes.push_back(classRecord);
            if (classRecord.teacherId)
            {
                selectedTeacherIds.push_back(*classRecord.teacherId);
            }
        }

        for (const SubPrepPrintTeacher& teacher : sourceInput.teachers)
        {
            if (std::find(
                    selectedTeacherIds.cbegin(),
                    selectedTeacherIds.cend(),
                    teacher.id
                    ) != selectedTeacherIds.cend())
            {
                selectedInput.teachers.push_back(teacher);
            }
        }

        lastSelectedInput = selectedInput;
        return SubPrepPrintSourceReadResult::success(
            std::move(selectedInput)
            );
    }

    int loadCount = 0;
    SubPrepPrintSourceRequest lastRequest;
    SubPrepPrintSourceInput lastSelectedInput;
    SubPrepPrintSourceInput sourceInput;
};

class SubPrepTestCalendarIntervalsReadPort final
    : public SubPrepCalendarEventIntervalsReadPort
{
public:
    SubPrepCalendarEventIntervalsReadResult loadIntervals(
        const SubPrepCalendarEventIntervalsReadRequest& request
        ) override
    {
        ++loadCount;
        lastRequest = request;
        return SubPrepCalendarEventIntervalsReadResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "calendar read failed",
            .recoverable = false
        });
    }

    int loadCount = 0;
    SubPrepCalendarEventIntervalsReadRequest lastRequest;
};

class SubPrepPageHarness final
{
public:
    explicit SubPrepPageHarness(ApplicationServices* services)
        : page(services, summaryReadPort, detailsReadPort)
    {
    }

    SubPrepPageHarness(
        ApplicationServices* services,
        SubPrepCalendarEventIntervalsReadPort& calendarIntervalsReadPort
        )
        : page(
              services,
              summaryReadPort,
              detailsReadPort,
              printSourceReadPort,
              calendarIntervalsReadPort
              )
    {
    }

    SubPrepTestSummaryReadPort summaryReadPort;
    SubPrepTestDetailsReadPort detailsReadPort;
    SubPrepTestPrintSourceReadPort printSourceReadPort;
    SubPrepPage page;
};

void saveSettingOrFail(
    DataService* dataService,
    const QString& key,
    const QVariant& value
    )
{
    QVERIFY(dataService);
    QVERIFY(dataService->saveSetting(key, value).has_value());
}

int layoutIndexForSection(
    QVBoxLayout* layout,
    const QString& section
    )
{
    for (int index = 0; index < layout->count(); ++index)
    {
        QWidget* widget =
            layout->itemAt(index)->widget();

        if (
            widget
            && widget->property("subPrepSection").toString() == section
            )
        {
            return index;
        }
    }

    return -1;
}

CalendarEvent calendarEvent(
    const QString& eventType,
    const QDate& startDate,
    const QDate& endDate
    )
{
    CalendarEvent event;
    event.eventType = eventType;
    event.startDate = startDate;
    event.endDate = endDate;
    return event;
}

void activatePage(SubPrepPage& page)
{
    page.setDatabaseOpen(true);
    page.activate();
}
}

class SubPrepPageTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    void sectionsAppearInRequestedOrderAndUseExpectedEditability();
    void headerKeyboardOpensUntargeted();
    void gradeAndLevelTabsSelectOneClassAndPreserveSelection();
    void scheduleDisplayModeRefreshesClassInformationInPlace();
    void pageDeactivationReleasesClassInformationForTheNextEntry();
    void freshAndExistingGradingSettingsResolveWithoutDataLoss();
    void savedCampusSelectionUsesTypedRead();
    void emptyCampusDetailsDisplayNotAvailable();
    void zoomUnavailableHidesStoredCredentials();
    void unavailablePreferenceLoadsPreservePageValues();
    void unavailablePreferenceSaveHasNoSideEffects();
    void printDialogSelectsNextVacationBlock();
    void printDialogOnlyOffersVacationModeWithinFourWeeks();
    void printDialogCombinesVacationDatesAcrossHolidayBlocks();
    void printDialogCalendarReadUsesTwoYearWindowAndFallsBackOnFailure();
    void packageFolderNamesCoverDateRangesAndUnsafeCharacters();
    void printDialogRequiresAndSavesMissingUserName();
    void clearDatabaseStateStopsAutosaveAndRemovesLoadedContent();
    void pageGenerationUsesSelectedTypedPrintSourceAndWritesInformationPdf();
    void pageGenerationForwardsSelectedDaysAndClassesInDisplayOrder();

private:
    std::unique_ptr<QTemporaryDir> m_generationDatabaseDirectory;
    QSqlDatabase m_generationDatabase;
    bool m_generationDatabaseCreated = false;
    bool m_originalQpaFontDirWasSet = false;
    QByteArray m_originalQpaFontDir;
};

void SubPrepPageTests::initTestCase()
{
    m_originalQpaFontDirWasSet =
        qEnvironmentVariableIsSet("QT_QPA_FONTDIR");
    m_originalQpaFontDir = qgetenv("QT_QPA_FONTDIR");
    const QString configuredFontDirectory = qEnvironmentVariable(
        "QT_QPA_FONTDIR"
        );
    if (
        !configuredFontDirectory.isEmpty()
        && QDir(configuredFontDirectory).exists()
        )
    {
        return;
    }

    const QStringList fontLocations = QStandardPaths::standardLocations(
        QStandardPaths::FontsLocation
        );
    if (!fontLocations.isEmpty() && QDir(fontLocations.first()).exists())
    {
        qputenv(
            "QT_QPA_FONTDIR",
            fontLocations.first().toLocal8Bit()
            );
    }
}

void SubPrepPageTests::cleanupTestCase()
{
    if (m_originalQpaFontDirWasSet)
    {
        qputenv("QT_QPA_FONTDIR", m_originalQpaFontDir);
    }
    else
    {
        qunsetenv("QT_QPA_FONTDIR");
    }
}

void SubPrepPageTests::init()
{
    ScheduleWidgetTestStubs::reset();
}

void SubPrepPageTests::cleanup()
{
    if (m_generationDatabaseCreated)
    {
        m_generationDatabase.close();
        m_generationDatabase = QSqlDatabase();
        if (QSqlDatabase::contains(QSqlDatabase::defaultConnection))
        {
            QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
        }
        m_generationDatabaseCreated = false;
    }
    m_generationDatabaseDirectory.reset();
}

void SubPrepPageTests::headerKeyboardOpensUntargeted()
{
    ApplicationServices services;
    SubPrepPageHarness harness(&services);
    SubPrepPage& page = harness.page;
    page.resize(1100, 800);
    page.show();
    QApplication::processEvents();

    auto* trigger = page.findChild<QPushButton*>(
        QStringLiteral("subPrepKoreanKeyboardButton")
        );
    auto* keyboard = page.findChild<OnScreenKeyboard*>();
    QVERIFY(trigger);
    QVERIFY(keyboard);
    QVERIFY(!trigger->icon().isNull());
    QCOMPARE(trigger->accessibleName(), QStringLiteral("Korean Keyboard"));

    trigger->click();
    QApplication::processEvents();
    QVERIFY(keyboard->isVisible());
    QVERIFY(!keyboard->target());
    keyboard->close();
}

void SubPrepPageTests
    ::sectionsAppearInRequestedOrderAndUseExpectedEditability()
{
    ApplicationServices services;
    SubPrepPageHarness harness(&services);
    SubPrepPage& page = harness.page;
    activatePage(page);

    auto* scrollArea =
        page.findChild<QScrollArea*>();
    QVERIFY(scrollArea);
    QVERIFY(scrollArea->widget());

    auto* layout =
        qobject_cast<QVBoxLayout*>(
            scrollArea->widget()->layout()
            );
    QVERIFY(layout);

    const QStringList sections{
        QStringLiteral("campus"),
        QStringLiteral("zoom"),
        QStringLiteral("materials"),
        QStringLiteral("grading"),
        QStringLiteral("schedule"),
        QStringLiteral("class_information")
    };

    int previousIndex = -1;

    for (const QString& section : sections)
    {
        const int index =
            layoutIndexForSection(
                layout,
                section
                );
        QVERIFY2(index > previousIndex, qPrintable(section));
        previousIndex = index;
    }

    auto* zoomLogin =
        page.findChild<QLineEdit*>(
            QStringLiteral("subPrepZoomLoginIdEdit")
            );
    auto* zoomPassword =
        page.findChild<QLineEdit*>(
            QStringLiteral("subPrepZoomPasswordEdit")
            );
    auto* materials =
        page.findChild<QTextEdit*>(
            QStringLiteral("subPrepClassMaterialsEdit")
            );
    auto* grading =
        page.findChild<QTextEdit*>(
            QStringLiteral("subPrepGradingInstructionsEdit")
            );
    auto* special =
        page.findChild<QTextEdit*>(
            QStringLiteral("subPrepSpecialInstructionsEdit")
            );
    auto* notes =
        page.findChild<QTextEdit*>(
            QStringLiteral("subPrepNotesEdit")
            );
    auto* schedule =
        page.findChild<ScheduleWidget*>(
            QStringLiteral("subPrepScheduleWidget")
            );

    QVERIFY(zoomLogin && zoomLogin->isReadOnly());
    QVERIFY(zoomPassword && zoomPassword->isReadOnly());
    QVERIFY(materials && !materials->isReadOnly());
    QVERIFY(grading && !grading->isReadOnly());
    QVERIFY(special && !special->isReadOnly());
    QVERIFY(notes && !notes->isReadOnly());
    QVERIFY(schedule);

    auto* materialsCard =
        qobject_cast<SectionCard*>(materials->parentWidget());
    QVERIFY(materialsCard);
    QCOMPARE(notes->parentWidget(), materialsCard);
    QCOMPARE(
        notes->minimumHeight(),
        grading->minimumHeight()
        );
    QVERIFY(materials->minimumHeight() < grading->minimumHeight());

    const QList<QString> centeredCardTitles{
        QStringLiteral("Campus Information"),
        QStringLiteral("Personal Zoom Information"),
        QStringLiteral("Class Materials & Lesson Notes"),
        QStringLiteral("Book Report Grading")
    };

    for (const QString& title : centeredCardTitles)
    {
        bool found = false;

        for (SectionCard* card : page.findChildren<SectionCard*>())
        {
            auto* cardTitle =
                card->findChild<QLabel*>(
                    QStringLiteral("sectionTitle"),
                    Qt::FindDirectChildrenOnly
                    );

            if (!cardTitle || cardTitle->text() != title)
            {
                continue;
            }

            found = true;
            QCOMPARE(cardTitle->alignment(), Qt::AlignCenter);
            break;
        }

        QVERIFY2(found, qPrintable(title));
    }

    auto* materialsCardTitle =
        materialsCard->findChild<QLabel*>(
            QStringLiteral("sectionTitle"),
            Qt::FindDirectChildrenOnly
            );
    QVERIFY(materialsCardTitle);
    QCOMPARE(
        materialsCardTitle->text(),
        QStringLiteral("Class Materials & Lesson Notes")
        );

    const QList<QLabel*> materialsLabels =
        materialsCard->findChildren<QLabel*>(
            QString(),
            Qt::FindDirectChildrenOnly
            );
    QVERIFY(std::any_of(
        materialsLabels.cbegin(),
        materialsLabels.cend(),
        [](const QLabel* label)
        {
            return label->text() == QStringLiteral("Materials Location");
        }
        ));
    QVERIFY(std::any_of(
        materialsLabels.cbegin(),
        materialsLabels.cend(),
        [](const QLabel* label)
        {
            return label->text()
                == QStringLiteral("Detailed Class & Lesson Notes");
        }
        ));

    auto* importantHeading =
        page.findChild<QLabel*>(
            QStringLiteral("subPrepImportantInformationHeading")
            );
    auto* scheduleHeading =
        page.findChild<QLabel*>(
            QStringLiteral("subPrepScheduleHeading")
            );
    auto* classInformationHeading =
        page.findChild<QLabel*>(
            QStringLiteral("subPrepClassInformationHeading")
            );

    QVERIFY(importantHeading);
    QVERIFY(scheduleHeading);
    QVERIFY(classInformationHeading);
    QCOMPARE(scheduleHeading->font(), importantHeading->font());
    QCOMPARE(classInformationHeading->font(), importantHeading->font());
    QCOMPARE(scheduleHeading->alignment(), importantHeading->alignment());
    QCOMPARE(
        classInformationHeading->alignment(),
        importantHeading->alignment()
        );

    auto* gradeTabs =
        page.findChild<NavigationTabStrip*>(
            QStringLiteral("subPrepGradeTabBar")
            );
    QVERIFY(gradeTabs);
    QCOMPARE(gradeTabs->count(), 1);
    QCOMPARE(gradeTabs->tabText(0), QStringLiteral("E4"));

    auto* classList =
        page.findChild<QListView*>(QStringLiteral("subPrepClassList"));
    QVERIFY(classList);
    QCOMPARE(classList->model()->rowCount(), 1);
    QVERIFY(
        classList->model()->data(classList->model()->index(0, 0))
            .toString()
            .contains(QStringLiteral("Hercules"))
        );

    auto* scheduleCard =
        qobject_cast<SectionCard*>(schedule->parentWidget());
    QVERIFY(scheduleCard);
    auto* scheduleCardTitle =
        scheduleCard->findChild<QLabel*>(
            QStringLiteral("sectionTitle")
            );
    QVERIFY(scheduleCardTitle && scheduleCardTitle->isHidden());

    QTextEdit* classNotes = nullptr;
    QTextEdit* teacherNotes = nullptr;

    for (QTextEdit* textEdit : page.findChildren<QTextEdit*>())
    {
        if (textEdit->property("classId").toInt() == 42)
        {
            classNotes = textEdit;
        }

        if (textEdit->property("teacherId").toInt() == 7)
        {
            teacherNotes = textEdit;
        }
    }

    QVERIFY(classNotes && classNotes->isReadOnly());
    QCOMPARE(
        classNotes->toPlainText(),
        QStringLiteral("Read chapter three.")
        );
    QVERIFY(teacherNotes && teacherNotes->isReadOnly());
    QCOMPARE(
        teacherNotes->toPlainText(),
        QStringLiteral("Call before class.")
        );

    const auto teacherCards =
        page.findChildren<SectionCard*>(
            QStringLiteral("subPrepTeacherSectionCard")
            );
    QCOMPARE(teacherCards.size(), 1);
    QCOMPARE(teacherCards.first()->property("teacherId").toInt(), 7);

    auto* teacherHeading =
        teacherCards.first()->findChild<QLabel*>(
            QStringLiteral("sectionTitle")
            );
    QVERIFY(teacherHeading);
    QCOMPARE(
        teacherHeading->text(),
        QStringLiteral("Susan: E4 Hercules")
        );

    auto* classDetails =
        page.findChild<QWidget*>(
            QStringLiteral("subPrepClassDetails")
            );
    QVERIFY(classDetails);
    QCOMPARE(classDetails->property("classId").toInt(), 42);
    QVERIFY(classDetails->layout());
    QCOMPARE(classDetails->layout()->contentsMargins().left(), 0);

    auto* controls =
        schedule->findChild<QWidget*>(
            QStringLiteral("scheduleControls")
            );
    QVERIFY(controls && controls->isHidden());
    QCOMPARE(
        page.currentSectionKey(),
        QStringLiteral("sub_prep_important")
        );
    page.scrollToSection(SubPrepSection::SubNotes);
    QCOMPARE(
        page.currentSectionKey(),
        QStringLiteral("sub_prep_notes")
        );
    bool foundDetailedClassAndLessonNotes = false;

    for (QLabel* label : page.findChildren<QLabel*>())
    {
        if (
            label->text()
            == QStringLiteral("Detailed Class & Lesson Notes")
            )
        {
            foundDetailedClassAndLessonNotes = true;
            break;
        }
    }

    QVERIFY(foundDetailedClassAndLessonNotes);
}

void SubPrepPageTests
    ::gradeAndLevelTabsSelectOneClassAndPreserveSelection()
{
    ScheduleWidgetTestStubs::setIncludeAdditionalClass(true);

    ApplicationServices services;
    SubPrepPageHarness harness(&services);
    SubPrepPage& page = harness.page;
    activatePage(page);

    auto* gradeTabs =
        page.findChild<NavigationTabStrip*>(
            QStringLiteral("subPrepGradeTabBar")
            );
    QVERIFY(gradeTabs);
    QCOMPARE(gradeTabs->count(), 2);

    int secondGradeIndex = -1;

    for (int index = 0; index < gradeTabs->count(); ++index)
    {
        if (gradeTabs->tabText(index) == QStringLiteral("E5"))
        {
            secondGradeIndex = index;
            break;
        }
    }

    QVERIFY(secondGradeIndex >= 0);
    gradeTabs->setCurrentIndex(secondGradeIndex);
    QCOMPARE(harness.detailsReadPort.loadCount, 2);
    QCOMPARE(harness.detailsReadPort.loadedClassIds.back(), std::string("43"));

    auto* classList =
        page.findChild<QListView*>(QStringLiteral("subPrepClassList"));
    QVERIFY(classList);
    QCOMPARE(classList->model()->rowCount(), 1);
    QCOMPARE(
        classList->model()
            ->data(classList->model()->index(0, 0),
                   SubPrepClassInformationListModel::ClassIdRole)
            .toString(),
        QStringLiteral("43")
        );
    QVERIFY(
        classList->model()->data(classList->model()->index(0, 0))
            .toString()
            .contains(QStringLiteral("Athena"))
        );

    auto* selectedDetails =
        page.findChild<QWidget*>(QStringLiteral("subPrepClassDetails"));
    QVERIFY(selectedDetails);
    QCOMPARE(
        selectedDetails->property("classId").toInt(),
        43
        );

    page.refresh();

    gradeTabs =
        page.findChild<NavigationTabStrip*>(
            QStringLiteral("subPrepGradeTabBar")
            );
    QVERIFY(gradeTabs);
    QCOMPARE(
        classList->model()
            ->data(classList->currentIndex(),
                   SubPrepClassInformationListModel::ClassIdRole)
            .toString()
            .toInt(),
        43
        );
    QCOMPARE(harness.detailsReadPort.loadCount, 3);

    ScheduleWidgetTestStubs::setIncludeAdditionalClass(false);
    page.refresh();

    QCOMPARE(
        selectedDetails->property("classId").toInt(),
        42
        );
    QCOMPARE(
        classList->model()
            ->data(classList->currentIndex(),
                   SubPrepClassInformationListModel::ClassIdRole)
            .toString()
            .toInt(),
        42
        );
    QCOMPARE(harness.detailsReadPort.loadCount, 4);
    QCOMPARE(harness.detailsReadPort.loadedClassIds.back(), std::string("42"));
}

void SubPrepPageTests
    ::scheduleDisplayModeRefreshesClassInformationInPlace()
{
    ScheduleWidgetTestStubs::setExistingIntensiveHours(true);

    ApplicationServices services;
    SubPrepPageHarness harness(&services);
    SubPrepPage& page = harness.page;
    activatePage(page);

    auto* schedule = page.findChild<ScheduleWidget*>(
        QStringLiteral("subPrepScheduleWidget")
        );
    QVERIFY(schedule);
    QCOMPARE(
        harness.summaryReadPort.lastRequest.mode,
        ScheduleViewMode::Regular
        );
    const int originalRebuildCount =
        page.runtimeMetrics().classInformationRebuildCount;

    QVERIFY(QMetaObject::invokeMethod(
        schedule,
        "setDisplayMode",
        Qt::DirectConnection,
        Q_ARG(int, static_cast<int>(ScheduleDisplayMode::Intensive))
        ));

    QCOMPARE(
        harness.summaryReadPort.lastRequest.mode,
        ScheduleViewMode::Intensive
        );
    QCOMPARE(
        page.runtimeMetrics().classInformationRebuildCount,
        originalRebuildCount + 1
        );
    QCOMPARE(
        page.runtimeMetrics().classInformationVisibleClassCount,
        1
        );
}

void SubPrepPageTests
    ::pageDeactivationReleasesClassInformationForTheNextEntry()
{
    ApplicationServices services;
    SubPrepPageHarness harness(&services);
    SubPrepPage& page = harness.page;
    activatePage(page);

    auto* classList =
        page.findChild<QListView*>(QStringLiteral("subPrepClassList"));
    auto* detailsCard =
        page.findChild<SectionCard*>(QStringLiteral("subPrepTeacherSectionCard"));
    auto* details =
        page.findChild<QWidget*>(QStringLiteral("subPrepClassDetails"));
    QVERIFY(classList);
    QVERIFY(detailsCard);
    QVERIFY(details);
    QCOMPARE(classList->model()->rowCount(), 1);
    QCOMPARE(details->property("classId").toInt(), 42);
    QCOMPARE(harness.detailsReadPort.loadCount, 1);

    page.deactivate();

    QVERIFY(page.needsRefresh());
    QCOMPARE(classList->model()->rowCount(), 0);
    QVERIFY(detailsCard->isHidden());
    QCOMPARE(details->property("classId").toInt(), -1);
    QCOMPARE(harness.detailsReadPort.loadCount, 1);

    page.activate();

    QCOMPARE(classList->model()->rowCount(), 1);
    QCOMPARE(details->property("classId").toInt(), 42);
    QCOMPARE(harness.detailsReadPort.loadCount, 2);
}

void SubPrepPageTests
    ::freshAndExistingGradingSettingsResolveWithoutDataLoss()
{
    ApplicationServices services;

    {
        SubPrepPageHarness freshHarness(&services);
        SubPrepPage& fresh = freshHarness.page;
        activatePage(fresh);
        auto* grading =
            fresh.findChild<QTextEdit*>(
                QStringLiteral("subPrepGradingInstructionsEdit")
                );
        auto* special =
            fresh.findChild<QTextEdit*>(
                QStringLiteral("subPrepSpecialInstructionsEdit")
                );

        QVERIFY(grading);
        QVERIFY(special);
        QVERIFY(
            grading->toPlainText().startsWith(
                QStringLiteral("Scoring: 0 / 20 / 40")
                )
            );
        QVERIFY(
            !grading->toPlainText().contains(
                QStringLiteral("Additional Rules")
                )
            );
        QCOMPARE(special->toPlainText(), QStringLiteral("N/A"));
    }

    ScheduleWidgetTestStubs::reset();
    saveSettingOrFail(services.dataService(),
        QStringLiteral("subPrep/bookReportGrading"),
        QStringLiteral("Custom legacy grading\nAdditional Rules: keep here")
        );
    saveSettingOrFail(services.dataService(),
        QStringLiteral("subPrep/subComments"),
        QStringLiteral("Existing substitute note")
        );

    SubPrepPageHarness existingHarness(&services);
    SubPrepPage& existing = existingHarness.page;
    activatePage(existing);
    auto* grading =
        existing.findChild<QTextEdit*>(
            QStringLiteral("subPrepGradingInstructionsEdit")
            );
    auto* special =
        existing.findChild<QTextEdit*>(
            QStringLiteral("subPrepSpecialInstructionsEdit")
            );
    auto* notes =
        existing.findChild<QTextEdit*>(
            QStringLiteral("subPrepNotesEdit")
            );

    QCOMPARE(
        grading->toPlainText(),
        QStringLiteral("Custom legacy grading\nAdditional Rules: keep here")
        );
    QVERIFY(special->toPlainText().isEmpty());
    QCOMPARE(
        notes->toPlainText(),
        QStringLiteral("Existing substitute note")
        );

    special->setPlainText(QStringLiteral("Bring spare books"));
    notes->setPlainText(QStringLiteral("Updated substitute note"));
    QVERIFY(existing.saveChanges());
    QCOMPARE(
        services.dataService()
            ->loadSetting(
                QStringLiteral("subPrep/bookReportSpecialInstructions")
                )
            ->toString(),
        QStringLiteral("Bring spare books")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(
                QStringLiteral("subPrep/subComments")
                )
            ->toString(),
        QStringLiteral("Updated substitute note")
        );
}

void SubPrepPageTests
    ::savedCampusSelectionUsesTypedRead()
{
    ApplicationServices services;
    saveSettingOrFail(
        services.dataService(),
        QStringLiteral("myInfo/campus"),
        QStringLiteral("  BUNDANG  ")
        );

    SubPrepPageHarness harness(&services);
    SubPrepPage& page = harness.page;
    activatePage(page);

    auto* officeNumber =
        page.findChild<QLineEdit*>(
            QStringLiteral("subPrepOfficeNumberEdit")
            );
    auto* officeWifi =
        page.findChild<QLineEdit*>(
            QStringLiteral("subPrepOfficeWifiEdit")
            );
    auto* officeWifiPassword =
        page.findChild<QLineEdit*>(
            QStringLiteral("subPrepOfficeWifiPasswordEdit")
            );
    auto* photocopierCode =
        page.findChild<QLineEdit*>(
            QStringLiteral("subPrepPhotocopierCodeEdit")
            );

    QVERIFY(officeNumber);
    QVERIFY(officeWifi);
    QVERIFY(officeWifiPassword);
    QVERIFY(photocopierCode);
    QCOMPARE(officeNumber->text(), QStringLiteral("418"));
    QCOMPARE(officeWifi->text(), QStringLiteral("Native Room_5G"));
    QCOMPARE(officeWifiPassword->text(), QStringLiteral("dyb418000"));
    QCOMPARE(photocopierCode->text(), QStringLiteral("N/A"));
}

void SubPrepPageTests::emptyCampusDetailsDisplayNotAvailable()
{
    ApplicationServices services;
    saveSettingOrFail(
        services.dataService(),
        QStringLiteral("myInfo/campus"),
        QStringLiteral("j")
        );

    SubPrepPageHarness harness(&services);
    SubPrepPage& page = harness.page;
    activatePage(page);

    for (const QString& objectName : {
             QStringLiteral("subPrepOfficeNumberEdit"),
             QStringLiteral("subPrepOfficeWifiEdit"),
             QStringLiteral("subPrepOfficeWifiPasswordEdit"),
             QStringLiteral("subPrepPhotocopierCodeEdit")
             })
    {
        auto* field = page.findChild<QLineEdit*>(objectName);
        QVERIFY(field);
        QCOMPARE(field->text(), QStringLiteral("N/A"));
    }
}

void SubPrepPageTests
    ::zoomUnavailableHidesStoredCredentials()
{
    ApplicationServices services;
    saveSettingOrFail(services.dataService(),
        QStringLiteral("myInfo/zoomLoginId"),
        QStringLiteral("teacher@example.com")
        );
    saveSettingOrFail(services.dataService(),
        QStringLiteral("myInfo/zoomPassword"),
        QStringLiteral("secret")
        );
    saveSettingOrFail(services.dataService(),
        QStringLiteral("myInfo/zoomNotAvailable"),
        true
        );

    SubPrepPageHarness unavailableHarness(&services);
    SubPrepPage& unavailable = unavailableHarness.page;
    activatePage(unavailable);
    QCOMPARE(
        unavailable
            .findChild<QLineEdit*>(
                QStringLiteral("subPrepZoomLoginIdEdit")
                )
            ->text(),
        QStringLiteral("N/A")
        );
    QCOMPARE(
        unavailable
            .findChild<QLineEdit*>(
                QStringLiteral("subPrepZoomPasswordEdit")
                )
            ->text(),
        QStringLiteral("N/A")
        );

    saveSettingOrFail(services.dataService(),
        QStringLiteral("myInfo/zoomNotAvailable"),
        false
        );

    SubPrepPageHarness availableHarness(&services);
    SubPrepPage& available = availableHarness.page;
    activatePage(available);
    QCOMPARE(
        available
            .findChild<QLineEdit*>(
                QStringLiteral("subPrepZoomLoginIdEdit")
                )
            ->text(),
        QStringLiteral("teacher@example.com")
        );
    QCOMPARE(
        available
            .findChild<QLineEdit*>(
                QStringLiteral("subPrepZoomPasswordEdit")
            )
            ->text(),
        QStringLiteral("secret")
        );

    ScheduleWidgetTestStubs::reset();
    ApplicationServices legacyServices;
    saveSettingOrFail(legacyServices.dataService(),
        QStringLiteral("subPrep/personalZoomEmail"),
        QStringLiteral("legacy@example.com")
        );
    saveSettingOrFail(legacyServices.dataService(),
        QStringLiteral("subPrep/personalZoomPassword"),
        QStringLiteral("legacy secret")
        );
    saveSettingOrFail(legacyServices.dataService(),
        QStringLiteral("subPrep/personalZoomNotAvailable"),
        false
        );

    SubPrepPageHarness legacyHarness(&legacyServices);
    SubPrepPage& legacy = legacyHarness.page;
    activatePage(legacy);
    QCOMPARE(
        legacy
            .findChild<QLineEdit*>(
                QStringLiteral("subPrepZoomLoginIdEdit")
                )
            ->text(),
        QStringLiteral("legacy@example.com")
        );
    QCOMPARE(
        legacyServices.dataService()
            ->loadSetting(
                QStringLiteral("myInfo/zoomLoginId")
                )
            ->toString(),
        QStringLiteral("legacy@example.com")
        );
}

void SubPrepPageTests
    ::unavailablePreferenceLoadsPreservePageValues()
{
    ScheduleWidgetTestStubs::setDatabaseOpen(false);
    ApplicationServices services;
    SubPrepPageHarness harness(&services);
    SubPrepPage& page = harness.page;

    auto* materials = page.findChild<QTextEdit*>(
        QStringLiteral("subPrepClassMaterialsEdit")
        );
    auto* grading = page.findChild<QTextEdit*>(
        QStringLiteral("subPrepGradingInstructionsEdit")
        );
    auto* special = page.findChild<QTextEdit*>(
        QStringLiteral("subPrepSpecialInstructionsEdit")
        );
    auto* notes = page.findChild<QTextEdit*>(
        QStringLiteral("subPrepNotesEdit")
        );
    auto* zoomLogin = page.findChild<QLineEdit*>(
        QStringLiteral("subPrepZoomLoginIdEdit")
        );
    auto* zoomPassword = page.findChild<QLineEdit*>(
        QStringLiteral("subPrepZoomPasswordEdit")
        );
    auto* officeNumber = page.findChild<QLineEdit*>(
        QStringLiteral("subPrepOfficeNumberEdit")
        );
    auto* officeWifi = page.findChild<QLineEdit*>(
        QStringLiteral("subPrepOfficeWifiEdit")
        );
    auto* officeWifiPassword = page.findChild<QLineEdit*>(
        QStringLiteral("subPrepOfficeWifiPasswordEdit")
        );
    auto* photocopierCode = page.findChild<QLineEdit*>(
        QStringLiteral("subPrepPhotocopierCodeEdit")
        );

    QVERIFY(materials && grading && special && notes);
    QVERIFY(zoomLogin && zoomPassword);
    QVERIFY(officeNumber && officeWifi && officeWifiPassword);
    QVERIFY(photocopierCode);
    QVERIFY(!services.hasOpenDatabase());
    QVERIFY(!services.settingsService()->isAvailable());

    {
        const QSignalBlocker materialsBlocker(materials);
        const QSignalBlocker gradingBlocker(grading);
        const QSignalBlocker specialBlocker(special);
        const QSignalBlocker notesBlocker(notes);
        const QSignalBlocker zoomLoginBlocker(zoomLogin);
        const QSignalBlocker zoomPasswordBlocker(zoomPassword);
        const QSignalBlocker officeNumberBlocker(officeNumber);
        const QSignalBlocker officeWifiBlocker(officeWifi);
        const QSignalBlocker officeWifiPasswordBlocker(officeWifiPassword);
        const QSignalBlocker photocopierCodeBlocker(photocopierCode);

        materials->setPlainText(QStringLiteral("Keep materials"));
        grading->setPlainText(QStringLiteral("Keep grading"));
        special->setPlainText(QStringLiteral("Keep special rules"));
        notes->setPlainText(QStringLiteral("Keep notes"));
        zoomLogin->setText(QStringLiteral("keep-login@example.com"));
        zoomPassword->setText(QStringLiteral("keep-password"));
        officeNumber->setText(QStringLiteral("Keep office"));
        officeWifi->setText(QStringLiteral("Keep Wi-Fi"));
        officeWifiPassword->setText(QStringLiteral("Keep Wi-Fi password"));
        photocopierCode->setText(QStringLiteral("Keep copier code"));
    }

    QVERIFY(!page.hasUnsavedChanges());
    page.refresh();

    QCOMPARE(materials->toPlainText(), QStringLiteral("Keep materials"));
    QCOMPARE(grading->toPlainText(), QStringLiteral("Keep grading"));
    QCOMPARE(special->toPlainText(), QStringLiteral("Keep special rules"));
    QCOMPARE(notes->toPlainText(), QStringLiteral("Keep notes"));
    QCOMPARE(zoomLogin->text(), QStringLiteral("keep-login@example.com"));
    QCOMPARE(zoomPassword->text(), QStringLiteral("keep-password"));
    QCOMPARE(officeNumber->text(), QStringLiteral("Keep office"));
    QCOMPARE(officeWifi->text(), QStringLiteral("Keep Wi-Fi"));
    QCOMPARE(
        officeWifiPassword->text(),
        QStringLiteral("Keep Wi-Fi password")
        );
    QCOMPARE(photocopierCode->text(), QStringLiteral("Keep copier code"));
}

void SubPrepPageTests
    ::unavailablePreferenceSaveHasNoSideEffects()
{
    ApplicationServices services;
    saveSettingOrFail(
        services.dataService(),
        QStringLiteral("subPrep/classMaterials"),
        QStringLiteral("Stored materials")
        );
    saveSettingOrFail(
        services.dataService(),
        QStringLiteral("subPrep/bookReportGrading"),
        QStringLiteral("Stored grading")
        );
    saveSettingOrFail(
        services.dataService(),
        QStringLiteral("subPrep/bookReportSpecialInstructions"),
        QStringLiteral("Stored special rules")
        );
    saveSettingOrFail(
        services.dataService(),
        QStringLiteral("subPrep/subComments"),
        QStringLiteral("Stored notes")
        );
    ScheduleWidgetTestStubs::setDatabaseOpen(false);
    SubPrepPageHarness harness(&services);
    SubPrepPage& page = harness.page;

    auto* materials = page.findChild<QTextEdit*>(
        QStringLiteral("subPrepClassMaterialsEdit")
        );
    auto* grading = page.findChild<QTextEdit*>(
        QStringLiteral("subPrepGradingInstructionsEdit")
        );
    QVERIFY(materials && grading);
    QVERIFY(!services.hasOpenDatabase());
    QVERIFY(!services.settingsService()->isAvailable());

    materials->setPlainText(QStringLiteral("Unsaved materials"));
    {
        const QSignalBlocker gradingBlocker(grading);
        grading->clear();
    }
    QVERIFY(page.hasUnsavedChanges());

    const QList<QTimer*> timers = page.findChildren<QTimer*>(
        QString(),
        Qt::FindDirectChildrenOnly
        );
    QVERIFY(!timers.isEmpty());
    QVERIFY(timers.first()->isActive());

    page.saveData();

    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(timers.first()->isActive());
    QCOMPARE(materials->toPlainText(), QStringLiteral("Unsaved materials"));
    QVERIFY(grading->toPlainText().isEmpty());

    ScheduleWidgetTestStubs::setDatabaseOpen(true);
    QCOMPARE(
        services.dataService()
            ->loadSetting(QStringLiteral("subPrep/classMaterials"))
            .value()
            .toString(),
        QStringLiteral("Stored materials")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QStringLiteral("subPrep/bookReportGrading"))
            .value()
            .toString(),
        QStringLiteral("Stored grading")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(
                QStringLiteral("subPrep/bookReportSpecialInstructions")
                )
            .value()
            .toString(),
        QStringLiteral("Stored special rules")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QStringLiteral("subPrep/subComments"))
            .value()
            .toString(),
        QStringLiteral("Stored notes")
        );
}

void SubPrepPageTests
    ::clearDatabaseStateStopsAutosaveAndRemovesLoadedContent()
{
    ApplicationServices services;
    saveSettingOrFail(services.dataService(),
        QStringLiteral("subPrep/classMaterials"),
        QStringLiteral("Stored database A material")
        );
    saveSettingOrFail(services.dataService(),
        QStringLiteral("myInfo/zoomLoginId"),
        QStringLiteral("database-a@example.com")
        );
    saveSettingOrFail(services.dataService(),
        QStringLiteral("myInfo/zoomNotAvailable"),
        false
        );

    SubPrepPageHarness harness(&services);
    SubPrepPage& page = harness.page;
    page.setDatabaseOpen(true);
    page.activate();
    auto* materials =
        page.findChild<QTextEdit*>(
            QStringLiteral("subPrepClassMaterialsEdit")
            );
    auto* zoomLogin =
        page.findChild<QLineEdit*>(
            QStringLiteral("subPrepZoomLoginIdEdit")
            );
    auto* schedule =
        page.findChild<ScheduleWidget*>(
            QStringLiteral("subPrepScheduleWidget")
            );
    auto* classList =
        page.findChild<QListView*>(QStringLiteral("subPrepClassList"));

    QVERIFY(materials);
    QVERIFY(zoomLogin);
    QVERIFY(schedule);
    QVERIFY(classList);
    QCOMPARE(
        materials->toPlainText(),
        QStringLiteral("Stored database A material")
        );
    QCOMPARE(
        zoomLogin->text(),
        QStringLiteral("database-a@example.com")
        );
    QCOMPARE(schedule->visibleClassIds(), QSet<int>{42});

    materials->setPlainText(
        QStringLiteral("Unsaved database A material")
        );
    QVERIFY(page.hasUnsavedChanges());

    ScheduleWidgetTestStubs::setDatabaseOpen(false);
    page.clearDatabaseState();
    QCoreApplication::sendPostedEvents(
        nullptr,
        QEvent::DeferredDelete
        );

    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(materials->toPlainText().isEmpty());
    QVERIFY(zoomLogin->text().isEmpty());
    QVERIFY(schedule->visibleClassIds().isEmpty());
    auto* classDetails = page.findChild<QWidget*>(
        QStringLiteral("subPrepClassDetails")
        );
    QVERIFY(classDetails);
    QCOMPARE(classDetails->property("classId").toInt(), -1);
    QCOMPARE(classList->model()->rowCount(), 0);

    QTest::qWait(850);
    QCOMPARE(
        services.dataService()
            ->loadSetting(
                QStringLiteral("subPrep/classMaterials")
                )
            ->toString(),
        QStringLiteral("Stored database A material")
        );
}

void SubPrepPageTests
    ::printDialogSelectsNextVacationBlock()
{
    const QDate wednesday(2026, 7, 15);
    const CalendarEvent pastVacation =
        calendarEvent(
            QStringLiteral("Vacation"),
            QDate(2026, 7, 6),
            QDate(2026, 7, 7)
            );
    const CalendarEvent currentWeekVacation =
        calendarEvent(
            QStringLiteral("Vacation"),
            QDate(2026, 7, 16),
            QDate(2026, 7, 17)
            );
    const CalendarEvent nextWeekVacation =
        calendarEvent(
            QStringLiteral("Vacation"),
            QDate(2026, 7, 20),
            QDate(2026, 7, 21)
            );
    const CalendarEvent laterVacation =
        calendarEvent(
            QStringLiteral("Vacation"),
            QDate(2026, 7, 27),
            QDate(2026, 7, 28)
            );
    const QList<CalendarEvent> calendarEvents{
        laterVacation,
        nextWeekVacation,
        pastVacation,
        currentWeekVacation
    };

    QCOMPARE(
        SubPrepPrintDialog::defaultSelectedDays(
            calendarEvents,
            wednesday
            ),
        QStringList({
            QStringLiteral("Monday"),
            QStringLiteral("Tuesday"),
            QStringLiteral("Thursday"),
            QStringLiteral("Friday")
        })
        );
    QCOMPARE(
        SubPrepPrintDialog::defaultSelectedDates(
            calendarEvents,
            wednesday
            ),
        QList<QDate>({
            QDate(2026, 7, 16),
            QDate(2026, 7, 17),
            QDate(2026, 7, 20),
            QDate(2026, 7, 21)
        })
        );
    QVERIFY(
        SubPrepPrintDialog::defaultSelectedDays(
            {
                calendarEvent(
                    QStringLiteral("Holiday"),
                    QDate(2026, 7, 20),
                    QDate(2026, 7, 24)
                    )
            },
            wednesday
            ).isEmpty()
        );
    QCOMPARE(
        SubPrepPrintDialog::defaultSelectedDates(
            {
                calendarEvent(
                    QStringLiteral("Vacation"),
                    QDate(2026, 7, 14),
                    QDate(2026, 7, 16)
                    )
            },
            wednesday
            ),
        QList<QDate>({
            QDate(2026, 7, 14),
            QDate(2026, 7, 15),
            QDate(2026, 7, 16)
        })
        );

    SubPrepPrintDialog dialog(
        calendarEvents,
        wednesday
        );
    QCOMPARE(dialog.windowTitle(), QStringLiteral("Generate Sub Prep"));
    QCOMPARE(
        SubPrepPrintDialog::defaultWeekStart(
            calendarEvents,
            wednesday
            ),
        QDate(2026, 7, 13)
        );
    QCOMPARE(
        dialog.findChild<QDateEdit*>(
            QStringLiteral("subPrepWeekOfEdit")
            ),
        nullptr
        );
    auto* daysLayout = dialog.findChild<QGridLayout*>(
        QStringLiteral("subPrepDaysLayout")
        );
    QVERIFY(daysLayout);
    QCOMPARE(daysLayout->columnCount(), 3);
    QCOMPARE(daysLayout->rowCount(), 3);
    auto* nextVacationCheck =
        dialog.findChild<QCheckBox*>(
            QStringLiteral("subPrepNextVacationCheckBox")
            );
    QVERIFY(nextVacationCheck);
    QCOMPARE(
        nextVacationCheck->text(),
        QStringLiteral("Next Vacation on the Calendar")
        );
    QVERIFY(nextVacationCheck->isChecked());
    QCOMPARE(
        daysLayout->itemAtPosition(0, 0)->widget(),
        nextVacationCheck
        );
    QVERIFY(
        dialog.findChild<QCheckBox*>(
            QStringLiteral("subPrepPrintMondayCheckBox")
            )->isChecked()
        );
    QVERIFY(
        dialog.findChild<QCheckBox*>(
            QStringLiteral("subPrepPrintTuesdayCheckBox")
            )->isChecked()
        );
    QVERIFY(
        dialog.findChild<QCheckBox*>(
            QStringLiteral("subPrepPrintThursdayCheckBox")
            )->isChecked()
        );
    QVERIFY(
        dialog.findChild<QCheckBox*>(
            QStringLiteral("subPrepPrintFridayCheckBox")
            )->isChecked()
        );
    for (const QString& day : QStringList{
             QStringLiteral("Monday"),
             QStringLiteral("Tuesday"),
             QStringLiteral("Wednesday"),
             QStringLiteral("Thursday"),
             QStringLiteral("Friday")
         })
    {
        QVERIFY(
            !dialog.findChild<QCheckBox*>(
                QStringLiteral("subPrepPrint%1CheckBox").arg(day)
                )->isEnabled()
            );
    }
    QCOMPARE(
        dialog.findChild<QCheckBox*>(
            QStringLiteral("subPrepPrintTuesdayCheckBox")
            )->text(),
        QStringLiteral("Tuesday")
        );
    QCOMPARE(
        dialog.selectedDates(),
        QList<QDate>({
            QDate(2026, 7, 16),
            QDate(2026, 7, 17),
            QDate(2026, 7, 20),
            QDate(2026, 7, 21)
        })
        );
    auto* nameEdit = dialog.findChild<QLineEdit*>(
        QStringLiteral("subPrepUserNameEdit")
        );
    auto* outputPreview = dialog.findChild<QLabel*>(
        QStringLiteral("subPrepOutputFolderPreview")
        );
    QVERIFY(nameEdit);
    QVERIFY(outputPreview);
    nameEdit->setText(QStringLiteral("Jamie"));
    QCOMPARE(
        outputPreview->text(),
        QStringLiteral(".../Jamie (16 - 21 Jul 2026)")
        );

    nextVacationCheck->setChecked(false);
    for (const QString& day : QStringList{
             QStringLiteral("Monday"),
             QStringLiteral("Tuesday"),
             QStringLiteral("Wednesday"),
             QStringLiteral("Thursday"),
             QStringLiteral("Friday")
         })
    {
        QVERIFY(
            dialog.findChild<QCheckBox*>(
                QStringLiteral("subPrepPrint%1CheckBox").arg(day)
                )->isEnabled()
            );
    }
    nextVacationCheck->setChecked(true);
    QCOMPARE(
        dialog.selectedDates(),
        QList<QDate>({
            QDate(2026, 7, 16),
            QDate(2026, 7, 17),
            QDate(2026, 7, 20),
            QDate(2026, 7, 21)
        })
        );
    QVERIFY(
        dialog.findChild<QPushButton*>(
            QStringLiteral("subPrepPrintCancelButton")
            )
        );
    QVERIFY(
        dialog.findChild<QPushButton*>(
            QStringLiteral("subPrepSelectFolderButton")
            )
        );
    QVERIFY(
        dialog.findChild<QPushButton*>(
            QStringLiteral("subPrepGenerateOkButton")
            )
        );
    QVERIFY(
        dialog.findChild<QCheckBox*>(
            QStringLiteral("subPrepCreateFolderCheckBox")
            )->isChecked()
        );
    QVERIFY(
        dialog.findChild<QCheckBox*>(
            QStringLiteral("subPrepOpenFolderCheckBox")
            )->isChecked()
        );
    QVERIFY(
        !dialog.findChild<QCheckBox*>(
            QStringLiteral("subPrepPrintPaperCopiesCheckBox")
            )->isChecked()
        );
    QVERIFY(
        !dialog.findChild<QWidget*>(
            QStringLiteral("subPrepRosterTemplateCombo")
            )
        );
    QString documentsPath =
        QStandardPaths::writableLocation(
            QStandardPaths::DocumentsLocation
            );
    if (documentsPath.isEmpty())
    {
        documentsPath = QDir(
            QStandardPaths::writableLocation(
                QStandardPaths::HomeLocation
                )
            ).filePath(QStringLiteral("Documents"));
    }
    QCOMPARE(
        QDir::cleanPath(
            dialog.findChild<QLineEdit*>(
                QStringLiteral("subPrepTargetFolderEdit")
                )->text()
            ),
        QDir(documentsPath).filePath(QStringLiteral("DYB/Sub_Prep"))
        );
}

void SubPrepPageTests
    ::printDialogOnlyOffersVacationModeWithinFourWeeks()
{
    const QDate referenceDate(2026, 7, 6);
    const CalendarEvent boundaryVacation =
        calendarEvent(
            QStringLiteral("Vacation"),
            referenceDate.addDays(28),
            referenceDate.addDays(28)
            );
    const CalendarEvent tooDistantVacation =
        calendarEvent(
            QStringLiteral("Vacation"),
            referenceDate.addDays(29),
            referenceDate.addDays(29)
            );

    QCOMPARE(
        SubPrepPrintDialog::defaultSelectedDates(
            {boundaryVacation},
            referenceDate
            ),
        QList<QDate>({referenceDate.addDays(28)})
        );
    SubPrepPrintDialog boundaryDialog(
        {boundaryVacation},
        referenceDate
        );
    QVERIFY(
        boundaryDialog.findChild<QCheckBox*>(
            QStringLiteral("subPrepNextVacationCheckBox")
            )
        );

    const QList<CalendarEvent> connectedBlockPastLookahead{
        calendarEvent(
            QStringLiteral("Vacation"),
            referenceDate.addDays(28),
            referenceDate.addDays(28)
            ),
        calendarEvent(
            QStringLiteral("Holiday"),
            referenceDate.addDays(29),
            referenceDate.addDays(30)
            ),
        calendarEvent(
            QStringLiteral("Vacation"),
            referenceDate.addDays(31),
            referenceDate.addDays(32)
            )
    };
    const QList<QDate> connectedBlockDates{
        referenceDate.addDays(28),
        referenceDate.addDays(31),
        referenceDate.addDays(32)
    };
    QCOMPARE(
        SubPrepPrintDialog::defaultSelectedDates(
            connectedBlockPastLookahead,
            referenceDate
            ),
        connectedBlockDates
        );
    SubPrepPrintDialog connectedBlockDialog(
        connectedBlockPastLookahead,
        referenceDate
        );
    QVERIFY(
        connectedBlockDialog.findChild<QCheckBox*>(
            QStringLiteral("subPrepNextVacationCheckBox")
            )
        );
    QCOMPARE(connectedBlockDialog.selectedDates(), connectedBlockDates);

    QVERIFY(
        SubPrepPrintDialog::defaultSelectedDates(
            {tooDistantVacation},
            referenceDate
            ).isEmpty()
        );
    SubPrepPrintDialog manualDialog(
        {tooDistantVacation},
        referenceDate
        );
    QVERIFY(
        !manualDialog.findChild<QCheckBox*>(
            QStringLiteral("subPrepNextVacationCheckBox")
            )
        );

    auto* daysLayout = manualDialog.findChild<QGridLayout*>(
        QStringLiteral("subPrepDaysLayout")
        );
    auto* nameEdit = manualDialog.findChild<QLineEdit*>(
        QStringLiteral("subPrepUserNameEdit")
        );
    auto* outputPreview = manualDialog.findChild<QLabel*>(
        QStringLiteral("subPrepOutputFolderPreview")
        );
    auto* validationLabel = manualDialog.findChild<QLabel*>(
        QStringLiteral("subPrepGenerationValidationLabel")
        );
    QVERIFY(daysLayout);
    QVERIFY(nameEdit);
    QVERIFY(outputPreview);
    QVERIFY(validationLabel);
    QCOMPARE(daysLayout->rowCount(), 2);
    QVERIFY(outputPreview->text().isEmpty());
    QCOMPARE(
        validationLabel->text(),
        QStringLiteral(
            "Select days and enter your name to preview the output folder."
            )
        );

    for (const QString& day : QStringList{
             QStringLiteral("Monday"),
             QStringLiteral("Tuesday"),
             QStringLiteral("Wednesday"),
             QStringLiteral("Thursday"),
             QStringLiteral("Friday")
         })
    {
        auto* dayCheck = manualDialog.findChild<QCheckBox*>(
            QStringLiteral("subPrepPrint%1CheckBox").arg(day)
            );
        QVERIFY(dayCheck);
        QVERIFY(dayCheck->isEnabled());
        QVERIFY(!dayCheck->isChecked());
    }

    nameEdit->setText(QStringLiteral("Jamie"));
    QCOMPARE(
        validationLabel->text(),
        QStringLiteral("Select days to preview the output folder.")
        );
    QVERIFY(outputPreview->text().isEmpty());

    for (const QString& objectName : QStringList{
             QStringLiteral("subPrepDaysGroup"),
             QStringLiteral("subPrepCreateFolderCheckBox"),
             QStringLiteral("subPrepFolderOptions"),
             QStringLiteral("subPrepOutputFolderPreview"),
             QStringLiteral("subPrepPrintPaperCopiesCheckBox"),
             QStringLiteral("subPrepGenerationValidationLabel")
         })
    {
        auto* section = manualDialog.findChild<QWidget*>(objectName);
        QVERIFY2(section, qPrintable(objectName));
        QVERIFY(section->minimumHeight() > 0);
        QCOMPARE(section->minimumHeight(), section->maximumHeight());
    }
}

void SubPrepPageTests
    ::printDialogCombinesVacationDatesAcrossHolidayBlocks()
{
    const auto vacation =
        [](const QDate& startDate, const QDate& endDate)
        {
            return calendarEvent(
                QStringLiteral("Vacation"),
                startDate,
                endDate
                );
        };
    const auto holiday =
        [](const QDate& startDate, const QDate& endDate)
        {
            return calendarEvent(
                QStringLiteral("Holiday"),
                startDate,
                endDate
                );
        };
    const QDate referenceDate(2026, 7, 1);
    const QList<CalendarEvent> multipleBeforeHoliday{
        vacation(QDate(2026, 7, 6), QDate(2026, 7, 7)),
        holiday(QDate(2026, 7, 8), QDate(2026, 7, 9)),
        vacation(QDate(2026, 7, 10), QDate(2026, 7, 10))
    };

    QCOMPARE(
        SubPrepPrintDialog::defaultSelectedDates(
            multipleBeforeHoliday,
            referenceDate
            ),
        QList<QDate>({
            QDate(2026, 7, 6),
            QDate(2026, 7, 7),
            QDate(2026, 7, 10)
        })
        );
    SubPrepPrintDialog bridgedDialog(
        multipleBeforeHoliday,
        referenceDate
        );
    auto* bridgedNameEdit = bridgedDialog.findChild<QLineEdit*>(
        QStringLiteral("subPrepUserNameEdit")
        );
    auto* bridgedOutputPreview = bridgedDialog.findChild<QLabel*>(
        QStringLiteral("subPrepOutputFolderPreview")
        );
    QVERIFY(bridgedNameEdit);
    QVERIFY(bridgedOutputPreview);
    bridgedNameEdit->setText(QStringLiteral("Jamie"));
    QCOMPARE(
        bridgedOutputPreview->text(),
        QStringLiteral(".../Jamie (06 - 10 Jul 2026)")
        );
    QCOMPARE(
        SubPrepPrintDialog::defaultSelectedDates(
            {
                vacation(QDate(2026, 7, 6), QDate(2026, 7, 6)),
                holiday(QDate(2026, 7, 7), QDate(2026, 7, 8)),
                vacation(QDate(2026, 7, 9), QDate(2026, 7, 10))
            },
            referenceDate
            ),
        QList<QDate>({
            QDate(2026, 7, 6),
            QDate(2026, 7, 9),
            QDate(2026, 7, 10)
        })
        );
    QCOMPARE(
        SubPrepPrintDialog::defaultSelectedDates(
            {
                vacation(QDate(2026, 7, 6), QDate(2026, 7, 7)),
                holiday(QDate(2026, 7, 8), QDate(2026, 7, 8)),
                vacation(QDate(2026, 7, 9), QDate(2026, 7, 10))
            },
            referenceDate
            ),
        QList<QDate>({
            QDate(2026, 7, 6),
            QDate(2026, 7, 7),
            QDate(2026, 7, 9),
            QDate(2026, 7, 10)
        })
        );
    QCOMPARE(
        SubPrepPrintDialog::defaultSelectedDates(
            {
                vacation(QDate(2026, 7, 6), QDate(2026, 7, 7)),
                holiday(QDate(2026, 7, 8), QDate(2026, 7, 8)),
                vacation(QDate(2026, 7, 9), QDate(2026, 7, 10))
            },
            QDate(2026, 7, 8)
            ),
        QList<QDate>({
            QDate(2026, 7, 6),
            QDate(2026, 7, 7),
            QDate(2026, 7, 9),
            QDate(2026, 7, 10)
        })
        );

    QCOMPARE(
        SubPrepPrintDialog::defaultSelectedDates(
            {
                vacation(QDate(2026, 7, 6), QDate(2026, 7, 7)),
                holiday(QDate(2026, 7, 8), QDate(2026, 7, 8)),
                vacation(QDate(2026, 7, 10), QDate(2026, 7, 10))
            },
            referenceDate
            ),
        QList<QDate>({
            QDate(2026, 7, 6),
            QDate(2026, 7, 7)
        })
        );

    const QList<CalendarEvent> spanningVacation{
        vacation(QDate(2026, 7, 8), QDate(2026, 7, 14))
    };
    QCOMPARE(
        SubPrepPrintDialog::defaultSelectedDays(
            spanningVacation,
            referenceDate
            ),
        QStringList({
            QStringLiteral("Monday"),
            QStringLiteral("Tuesday"),
            QStringLiteral("Wednesday"),
            QStringLiteral("Thursday"),
            QStringLiteral("Friday")
        })
        );
    QCOMPARE(
        SubPrepPrintDialog::defaultSelectedDates(
            spanningVacation,
            referenceDate
            ),
        QList<QDate>({
            QDate(2026, 7, 8),
            QDate(2026, 7, 9),
            QDate(2026, 7, 10),
            QDate(2026, 7, 13),
            QDate(2026, 7, 14)
        })
        );
    QCOMPARE(
        SubPrepPrintDialog::defaultWeekStart(
            spanningVacation,
            referenceDate
            ),
        QDate(2026, 7, 6)
        );

    QCOMPARE(
        SubPrepPrintDialog::defaultSelectedDates(
            {
                vacation(QDate(2026, 7, 10), QDate(2026, 7, 10)),
                vacation(QDate(2026, 7, 13), QDate(2026, 7, 13))
            },
            referenceDate
            ),
        QList<QDate>({
            QDate(2026, 7, 10),
            QDate(2026, 7, 13)
        })
        );
}

void SubPrepPageTests::
printDialogCalendarReadUsesTwoYearWindowAndFallsBackOnFailure()
{
    ApplicationServices services;
    SubPrepTestCalendarIntervalsReadPort calendarIntervalsReadPort;
    SubPrepPageHarness harness(&services, calendarIntervalsReadPort);
    const QDate referenceDate = QDate::currentDate();
    const QDate expectedStartDate(referenceDate.year(), 1, 1);
    const QDate expectedEndDate(referenceDate.year() + 1, 12, 31);

    bool dialogOpened = false;
    bool emptyCalendarFallbackShown = false;
    QTimer::singleShot(
        0,
        [&dialogOpened, &emptyCalendarFallbackShown]()
        {
            auto* dialog = qobject_cast<SubPrepPrintDialog*>(
                QApplication::activeModalWidget()
                );
            if (!dialog)
            {
                return;
            }

            dialogOpened = true;
            emptyCalendarFallbackShown =
                dialog->findChild<QCheckBox*>(
                    QStringLiteral("subPrepNextVacationCheckBox")
                    ) == nullptr;
            dialog->reject();
        }
        );

    QVERIFY(QMetaObject::invokeMethod(
        &harness.page,
        "generateSubPrep",
        Qt::DirectConnection
        ));

    QVERIFY(dialogOpened);
    QVERIFY(emptyCalendarFallbackShown);
    QCOMPARE(calendarIntervalsReadPort.loadCount, 1);
    QCOMPARE(
        calendarIntervalsReadPort.lastRequest.startDate.value(),
        expectedStartDate.toString(Qt::ISODate).toStdString()
        );
    QCOMPARE(
        calendarIntervalsReadPort.lastRequest.endDate.value(),
        expectedEndDate.toString(Qt::ISODate).toStdString()
        );
}

void SubPrepPageTests::
pageGenerationUsesSelectedTypedPrintSourceAndWritesInformationPdf()
{
    QTemporaryDir outputRoot;
    QVERIFY(outputRoot.isValid());

    m_generationDatabaseDirectory = std::make_unique<QTemporaryDir>();
    QVERIFY(m_generationDatabaseDirectory->isValid());
    QVERIFY(!QSqlDatabase::contains(QSqlDatabase::defaultConnection));
    m_generationDatabase = QSqlDatabase::addDatabase(
        QStringLiteral("QSQLITE")
        );
    m_generationDatabaseCreated = true;
    m_generationDatabase.setDatabaseName(
        m_generationDatabaseDirectory->filePath(
            QStringLiteral("sub-prep-page-tests.tps")
            )
        );
    QVERIFY(m_generationDatabase.open());
    QVERIFY(DatabaseSchemaManager::ensureSchema(m_generationDatabase));

    ApplicationServices services;
    QVERIFY(services.openDatabase(
        m_generationDatabaseDirectory->filePath(
            QStringLiteral("sub-prep-page-tests.tps")
            )
        ));

    QString databaseSeedError;
    const auto executeSeed =
        [this, &databaseSeedError](
            const QString& sql,
            const QVariantList& values
        )
        {
            QSqlQuery query(m_generationDatabase);
            if (!query.prepare(sql))
            {
                databaseSeedError = query.lastError().text();
                return false;
            }
            for (const QVariant& value : values)
            {
                query.addBindValue(value);
            }
            if (!query.exec())
            {
                databaseSeedError = query.lastError().text();
                return false;
            }
            return true;
        };
    QVERIFY2(executeSeed(
        QStringLiteral(
            "INSERT INTO teachers (id, teacher_en, teacher_kr, "
            "preferred_romanization, preferred_name, room_number, "
            "wifi_name, wifi_password, internet_type, zoom_id, "
            "zoom_password, projection_type, notes) "
            "VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
            ),
        {
            QStringLiteral("F380 Roster Teacher"),
            QStringLiteral("F380 Roster Teacher KR"),
            QStringLiteral("F380 Roster Teacher"),
            QStringLiteral("F380 Roster Teacher"),
            QStringLiteral("418"),
            QStringLiteral("F380 WiFi"),
            QStringLiteral("F380 WiFi Password"),
            QStringLiteral("WiFi"),
            QStringLiteral("f380.zoom"),
            QStringLiteral("F380 Zoom Password"),
            QStringLiteral("HDMI"),
            QStringLiteral("F380 Roster Teacher Note")
        }
        ), qPrintable(databaseSeedError));
    QVERIFY2(executeSeed(
        QStringLiteral("INSERT INTO classes (id, name) VALUES (42, ?)"),
        {QStringLiteral("F380 Package Class")}
        ), qPrintable(databaseSeedError));
    QVERIFY2(executeSeed(
        QStringLiteral(
            "INSERT INTO class_info (class_id, teacher_id, class_grade, "
            "class_level, class_color, font_color) "
            "VALUES (42, 1, 'E4', 'F380 Package Class', '#ffffff', '#000000')"
            ),
        {}
        ), qPrintable(databaseSeedError));
    QVERIFY2(executeSeed(
        QStringLiteral(
            "INSERT INTO class_times (class_id, day, start_time, end_time) "
            "VALUES (42, 'Tuesday', '4:00 PM', '4:50 PM')"
            ),
        {}
        ), qPrintable(databaseSeedError));
    for (const auto& column : {
             QPair<QString, int>{QStringLiteral("English"), 0},
             QPair<QString, int>{QStringLiteral("Korean"), 1}
         })
    {
        QVERIFY2(executeSeed(
            QStringLiteral(
                "INSERT INTO roster_columns (class_id, name, position, width) "
                "VALUES (42, ?, ?, 0)"
                ),
            {column.first, column.second}
            ), qPrintable(databaseSeedError));
    }
    QVERIFY2(executeSeed(
        QStringLiteral(
            "INSERT INTO roster_data (class_id, row_index, col_index, value) "
            "VALUES (42, 0, 0, 'F380 Roster Student')"
            ),
        {}
        ), qPrintable(databaseSeedError));
    QVERIFY2(executeSeed(
        QStringLiteral(
            "INSERT INTO roster_data (class_id, row_index, col_index, value) "
            "VALUES (42, 0, 1, 'F380 Roster Student KR')"
            ),
        {}
        ), qPrintable(databaseSeedError));

    const int selectedClassId = 42;

    const auto selectedId = typedClassId(selectedClassId);
    const auto selectedTeacherId = typedTeacherId(701);
    const auto unselectedTeacherId = typedTeacherId(702);

    SubPrepTestCalendarIntervalsReadPort calendarIntervalsReadPort;
    SubPrepPageHarness harness(&services, calendarIntervalsReadPort);
    harness.printSourceReadPort.sourceInput.teachers = {
        {
            .id = selectedTeacherId,
            .englishName = "F380_SELECTED_TEACHER",
            .room = "F380_SELECTED_ROOM",
            .teacherNotes = "F380_SELECTED_TEACHER_NOTE"
        },
        {
            .id = unselectedTeacherId,
            .englishName = "F380_UNSELECTED_SENTINEL",
            .room = "F380_UNSELECTED_ROOM"
        }
    };
    harness.printSourceReadPort.sourceInput.classes = {
        {
            .id = selectedId,
            .teacherId = selectedTeacherId,
            .grade = "F380_SELECTED_GRADE",
            .level = "F380_SELECTED_CLASS",
            .classNotes = "F380_SELECTED_CLASS_NOTE",
            .classColor = "#ffffff",
            .fontColor = "#000000",
            .studentCount = 1,
            .meetings = {
                {
                    .weekday = SubPrepWeekday::Tuesday,
                    .startTime = "4:00 PM",
                    .endTime = "4:50 PM"
                }
            }
        },
        {
            .id = typedClassId(900),
            .teacherId = unselectedTeacherId,
            .grade = "F380_UNSELECTED_GRADE",
            .level = "F380_UNSELECTED_SENTINEL",
            .classNotes = "F380_UNSELECTED_NOTE",
            .meetings = {
                {
                    .weekday = SubPrepWeekday::Tuesday,
                    .startTime = "5:00 PM",
                    .endTime = "5:50 PM"
                }
            }
        }
    };

    activatePage(harness.page);

    bool dialogOpened = false;
    bool dialogAccepted = false;
    QString dialogAutomationError;
    QTimer::singleShot(
        0,
        [&]
        {
            auto* dialog = qobject_cast<SubPrepPrintDialog*>(
                QApplication::activeModalWidget()
                );
            if (!dialog)
            {
                dialogAutomationError = QStringLiteral(
                    "Sub Prep print dialog did not open."
                    );
                return;
            }
            dialogOpened = true;

            auto* tuesdayCheck = dialog->findChild<QCheckBox*>(
                QStringLiteral("subPrepPrintTuesdayCheckBox")
                );
            auto* targetEdit = dialog->findChild<QLineEdit*>(
                QStringLiteral("subPrepTargetFolderEdit")
                );
            auto* nameEdit = dialog->findChild<QLineEdit*>(
                QStringLiteral("subPrepUserNameEdit")
                );
            auto* openFolderCheck = dialog->findChild<QCheckBox*>(
                QStringLiteral("subPrepOpenFolderCheckBox")
                );
            auto* acceptButton = dialog->findChild<QPushButton*>(
                QStringLiteral("subPrepGenerateOkButton")
                );
            if (
                !tuesdayCheck
                || !targetEdit
                || !nameEdit
                || !openFolderCheck
                || !acceptButton
            )
            {
                dialogAutomationError = QStringLiteral(
                    "Sub Prep print dialog controls are incomplete."
                    );
                dialog->reject();
                return;
            }

            for (const QString& day : QStringList{
                     QStringLiteral("Monday"),
                     QStringLiteral("Tuesday"),
                     QStringLiteral("Wednesday"),
                     QStringLiteral("Thursday"),
                     QStringLiteral("Friday")
                 })
            {
                auto* dayCheck = dialog->findChild<QCheckBox*>(
                    QStringLiteral("subPrepPrint%1CheckBox").arg(day)
                    );
                if (!dayCheck)
                {
                    dialogAutomationError = QStringLiteral(
                        "A weekday selection control is missing."
                        );
                    dialog->reject();
                    return;
                }
                dayCheck->setChecked(false);
            }

            tuesdayCheck->setChecked(true);
            targetEdit->setText(outputRoot.path());
            nameEdit->setText(QStringLiteral("F380"));
            openFolderCheck->setChecked(false);
            acceptButton->click();
            dialogAccepted = dialog->result() == QDialog::Accepted;
        }
        );

    FakeUserPromptService promptService;
    DialogServices::setUserPromptServiceForTesting(&promptService);
    const bool generationInvoked = QMetaObject::invokeMethod(
        &harness.page,
        "generateSubPrep",
        Qt::DirectConnection
        );
    DialogServices::setUserPromptServiceForTesting(nullptr);
    QVERIFY(generationInvoked);

    QVERIFY2(dialogOpened, qPrintable(dialogAutomationError));
    QVERIFY2(dialogAccepted, qPrintable(dialogAutomationError));
    const QStringList generationMessages = [&promptService]
    {
        QStringList messages;
        for (const PromptRequest& prompt : promptService.messages)
        {
            messages.append(
                QStringLiteral("%1: %2").arg(prompt.title, prompt.message)
                );
        }
        return messages;
    }();
    QVERIFY2(
        promptService.messages.isEmpty(),
        qPrintable(generationMessages.join(QLatin1Char('\n')))
        );
    QCOMPARE(harness.printSourceReadPort.loadCount, 1);
    QCOMPARE(
        harness.printSourceReadPort.lastRequest.selectedClassIds.size(),
        std::size_t(1)
        );
    QCOMPARE(
        harness.printSourceReadPort.lastRequest.selectedClassIds.front().value(),
        std::string("42")
        );
    QCOMPARE(
        harness.printSourceReadPort.lastRequest.selectedDays.size(),
        std::size_t(1)
        );
    QCOMPARE(
        static_cast<int>(
            harness.printSourceReadPort.lastRequest.selectedDays.front()
            ),
        static_cast<int>(SubPrepWeekday::Tuesday)
        );
    QCOMPARE(
        harness.printSourceReadPort.lastRequest.mode,
        ScheduleViewMode::Regular
        );

    const QStringList packageDirectories = QDir(outputRoot.path()).entryList(
        QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::Name
        );
    QCOMPARE(packageDirectories.size(), 1);
    const QString outputDirectory = QDir(outputRoot.path()).filePath(
        packageDirectories.first()
        );
    QVERIFY(QFileInfo(outputDirectory).isDir());
    const QStringList classDirectories = QDir(outputDirectory).entryList(
        QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::Name
        );
    QCOMPARE(classDirectories.size(), 1);
    const QString selectedClassDirectory = QDir(outputDirectory).filePath(
        classDirectories.first()
        );
    QVERIFY(QFileInfo(selectedClassDirectory).isDir());
    const QString informationPdfPath = QDir(outputDirectory).filePath(
        QStringLiteral("Sub Prep.pdf")
        );
    const QFileInfo informationPdfInfo(informationPdfPath);
    QVERIFY(informationPdfInfo.exists());
    QVERIFY(informationPdfInfo.size() > 0);

    QPdfDocument informationPdf;
    QCOMPARE(
        informationPdf.load(informationPdfPath),
        QPdfDocument::Error::None
        );
    QCOMPARE(informationPdf.status(), QPdfDocument::Status::Ready);
    QVERIFY(informationPdf.pageCount() > 0);
    QStringList informationPdfPages;
    for (int index = 0; index < informationPdf.pageCount(); ++index)
    {
        informationPdfPages.append(
            informationPdf.getAllText(index).text()
            );
    }
    const QString informationPdfText =
        informationPdfPages.join(QLatin1Char(' '));
    QVERIFY(informationPdfText.contains(
        QStringLiteral("F380_SELECTED_GRADE")
        ));
    QVERIFY(informationPdfText.contains(
        QStringLiteral("F380_SELECTED_CLASS")
        ));
    QVERIFY(informationPdfText.contains(
        QStringLiteral("F380_SELECTED_CLASS_NOTE")
        ));
    QVERIFY(informationPdfText.contains(
        QStringLiteral("F380_SELECTED_TEACHER")
        ));
    QVERIFY(!informationPdfText.contains(
        QStringLiteral("F380_UNSELECTED_SENTINEL")
        ));

    const QJsonObject transcript{
        {QStringLiteral("case"),
         QStringLiteral("sub_prep_page_generation")},
        {QStringLiteral("package"),
         QJsonObject{
             {QStringLiteral("directory"), true},
             {QStringLiteral("selected_class_directory"), true},
             {QStringLiteral("roster_writer_stubbed"), true}
         }},
        {QStringLiteral("generation_warning_count"),
         promptService.messages.size()},
        {QStringLiteral("request"),
         QJsonObject{
             {QStringLiteral("class_ids"), QJsonArray{"42"}},
             {QStringLiteral("days"), QJsonArray{"Tuesday"}},
             {QStringLiteral("mode"), QStringLiteral("regular")}
         }},
        {QStringLiteral("pdf"),
         QJsonObject{
             {QStringLiteral("loadable"), true},
             {QStringLiteral("nonempty"), informationPdfInfo.size() > 0},
             {QStringLiteral("selected_class"), true},
             {QStringLiteral("selected_teacher"), true},
             {QStringLiteral("excluded_sentinel"), true}
         }}
    };
    qInfo().noquote()
        << "F380_TRANSCRIPT="
        << QString::fromUtf8(
               QJsonDocument(transcript)
                   .toJson(QJsonDocument::Compact)
                   .constData()
               );
}

void SubPrepPageTests::
pageGenerationForwardsSelectedDaysAndClassesInDisplayOrder()
{
    QTemporaryDir outputRoot;
    QVERIFY(outputRoot.isValid());

    m_generationDatabaseDirectory = std::make_unique<QTemporaryDir>();
    QVERIFY(m_generationDatabaseDirectory->isValid());
    QVERIFY(!QSqlDatabase::contains(QSqlDatabase::defaultConnection));
    m_generationDatabase = QSqlDatabase::addDatabase(
        QStringLiteral("QSQLITE")
        );
    m_generationDatabaseCreated = true;
    const QString databasePath = m_generationDatabaseDirectory->filePath(
        QStringLiteral("sub-prep-page-tests.tps")
        );
    m_generationDatabase.setDatabaseName(databasePath);
    QVERIFY(m_generationDatabase.open());
    QVERIFY(DatabaseSchemaManager::ensureSchema(m_generationDatabase));

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath));

    QString databaseSeedError;
    const auto executeSeed =
        [this, &databaseSeedError](
            const QString& sql,
            const QVariantList& values
        )
        {
            QSqlQuery query(m_generationDatabase);
            if (!query.prepare(sql))
            {
                databaseSeedError = query.lastError().text();
                return false;
            }
            for (const QVariant& value : values)
            {
                query.addBindValue(value);
            }
            if (!query.exec())
            {
                databaseSeedError = query.lastError().text();
                return false;
            }
            return true;
        };

    const QStringList classMarkers{
        QStringLiteral("F507_TUESDAY"),
        QStringLiteral("F507_THURSDAY"),
        QStringLiteral("F507_FRIDAY_SENTINEL")
    };
    const QStringList weekdays{
        QStringLiteral("Tuesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday")
    };
    const QList<int> classIds{100, 43, 44};

    for (int index = 0; index < classIds.size(); ++index)
    {
        const int teacherId = index + 1;
        const QString marker = classMarkers.at(index);
        QVERIFY2(executeSeed(
            QStringLiteral(
                "INSERT INTO teachers (id, teacher_en, teacher_kr, "
                "preferred_romanization, preferred_name, room_number, "
                "wifi_name, wifi_password, internet_type, zoom_id, "
                "zoom_password, projection_type, notes) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
                ),
            {
                teacherId,
                marker + QStringLiteral("_DB_TEACHER"),
                marker + QStringLiteral("_KR"),
                marker + QStringLiteral("_ROMANIZATION"),
                marker + QStringLiteral("_PREFERRED"),
                QStringLiteral("401"),
                marker + QStringLiteral("_WIFI"),
                QStringLiteral("F507 WiFi Password"),
                QStringLiteral("WiFi"),
                marker + QStringLiteral(".zoom"),
                QStringLiteral("F507 Zoom Password"),
                QStringLiteral("HDMI"),
                marker + QStringLiteral("_DB_TEACHER_NOTE")
            }
            ), qPrintable(databaseSeedError));

        const int classId = classIds.at(index);
        QVERIFY2(executeSeed(
            QStringLiteral("INSERT INTO classes (id, name) VALUES (?, ?)"),
            {classId, marker + QStringLiteral("_DB_CLASS")}
            ), qPrintable(databaseSeedError));
        QVERIFY2(executeSeed(
            QStringLiteral(
                "INSERT INTO class_info (class_id, teacher_id, class_grade, "
                "class_level, class_color, font_color) "
                "VALUES (?, ?, ?, ?, '#ffffff', '#000000')"
                ),
            {
                classId,
                teacherId,
                QStringLiteral("E5"),
                marker + QStringLiteral("_DB_LEVEL")
            }
            ), qPrintable(databaseSeedError));
        QVERIFY2(executeSeed(
            QStringLiteral(
                "INSERT INTO class_times (class_id, day, start_time, end_time) "
                "VALUES (?, ?, ?, ?)"
                ),
            {
                classId,
                weekdays.at(index),
                QStringLiteral("4:00 PM"),
                QStringLiteral("4:50 PM")
            }
            ), qPrintable(databaseSeedError));

        QVERIFY2(executeSeed(
            QStringLiteral(
                "INSERT INTO roster_columns (class_id, name, position, width) "
                "VALUES (?, ?, ?, 0)"
                ),
            {classId, QStringLiteral("English"), 0}
            ), qPrintable(databaseSeedError));
        QVERIFY2(executeSeed(
            QStringLiteral(
                "INSERT INTO roster_columns (class_id, name, position, width) "
                "VALUES (?, ?, ?, 0)"
                ),
            {classId, QStringLiteral("Korean"), 1}
            ), qPrintable(databaseSeedError));
        QVERIFY2(executeSeed(
            QStringLiteral(
                "INSERT INTO roster_data (class_id, row_index, col_index, value) "
                "VALUES (?, 0, 0, ?)"
                ),
            {classId, marker + QStringLiteral("_STUDENT")}
            ), qPrintable(databaseSeedError));
        QVERIFY2(executeSeed(
            QStringLiteral(
                "INSERT INTO roster_data (class_id, row_index, col_index, value) "
                "VALUES (?, 0, 1, ?)"
                ),
            {classId, marker + QStringLiteral("_STUDENT_KR")}
            ), qPrintable(databaseSeedError));
    }

    SubPrepTestCalendarIntervalsReadPort calendarIntervalsReadPort;
    SubPrepPageHarness harness(&services, calendarIntervalsReadPort);
    harness.printSourceReadPort.sourceInput.teachers = {
        {
            .id = typedTeacherId(701),
            .englishName = "F507_TUESDAY_TEACHER",
            .room = "F507_TUESDAY_ROOM",
            .teacherNotes = "F507_TUESDAY_TEACHER_NOTE"
        },
        {
            .id = typedTeacherId(702),
            .englishName = "F507_THURSDAY_TEACHER",
            .room = "F507_THURSDAY_ROOM",
            .teacherNotes = "F507_THURSDAY_TEACHER_NOTE"
        },
        {
            .id = typedTeacherId(703),
            .englishName = "F507_FRIDAY_SENTINEL_TEACHER",
            .room = "F507_FRIDAY_SENTINEL_ROOM",
            .teacherNotes = "F507_FRIDAY_SENTINEL_TEACHER_NOTE"
        }
    };
    harness.printSourceReadPort.sourceInput.classes = {
        {
            .id = typedClassId(100),
            .teacherId = typedTeacherId(701),
            .grade = "F507_TUESDAY_GRADE",
            .level = "F507_TUESDAY_CLASS",
            .classNotes = "F507_TUESDAY_CLASS_NOTE",
            .classColor = "#ffffff",
            .fontColor = "#000000",
            .studentCount = 1,
            .meetings = {
                {
                    .weekday = SubPrepWeekday::Tuesday,
                    .startTime = "4:00 PM",
                    .endTime = "4:50 PM"
                }
            }
        },
        {
            .id = typedClassId(43),
            .teacherId = typedTeacherId(702),
            .grade = "F507_THURSDAY_GRADE",
            .level = "F507_THURSDAY_CLASS",
            .classNotes = "F507_THURSDAY_CLASS_NOTE",
            .classColor = "#ffffff",
            .fontColor = "#000000",
            .studentCount = 1,
            .meetings = {
                {
                    .weekday = SubPrepWeekday::Thursday,
                    .startTime = "4:00 PM",
                    .endTime = "4:50 PM"
                }
            }
        },
        {
            .id = typedClassId(44),
            .teacherId = typedTeacherId(703),
            .grade = "F507_FRIDAY_SENTINEL_GRADE",
            .level = "F507_FRIDAY_SENTINEL_CLASS",
            .classNotes = "F507_FRIDAY_SENTINEL_CLASS_NOTE",
            .classColor = "#ffffff",
            .fontColor = "#000000",
            .studentCount = 1,
            .meetings = {
                {
                    .weekday = SubPrepWeekday::Friday,
                    .startTime = "4:00 PM",
                    .endTime = "4:50 PM"
                }
            }
        }
    };

    activatePage(harness.page);
    auto* const scheduleWidget = harness.page.findChild<ScheduleWidget*>(
        QStringLiteral("subPrepScheduleWidget")
        );
    QVERIFY(scheduleWidget);
    ScheduleViewModel generationSchedule;
    generationSchedule.days = {
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday")
    };
    ScheduleRowView generationRow;
    for (const QString& day : generationSchedule.days)
    {
        ScheduleCellView cell;
        cell.day = day;
        if (day == QStringLiteral("Tuesday"))
        {
            ScheduleEntry entry;
            entry.classId = 100;
            cell.entries.append(entry);
        }
        else if (day == QStringLiteral("Thursday"))
        {
            ScheduleEntry entry;
            entry.classId = 43;
            cell.entries.append(entry);
        }
        else if (day == QStringLiteral("Friday"))
        {
            ScheduleEntry entry;
            entry.classId = 44;
            cell.entries.append(entry);
        }
        generationRow.cells.append(cell);
    }
    generationSchedule.rows.append(generationRow);
    scheduleWidget->setPreviewModel(generationSchedule);
    const QSet<int> expectedVisibleClassIds{100, 43, 44};
    QCOMPARE(scheduleWidget->visibleClassIds(), expectedVisibleClassIds);

    bool dialogOpened = false;
    bool dialogAccepted = false;
    bool safetyCloseTriggered = false;
    QString dialogAutomationError;
    QTimer::singleShot(
        0,
        &harness.page,
        [&]
        {
            auto* const dialog = qobject_cast<SubPrepPrintDialog*>(
                QApplication::activeModalWidget()
                );
            if (!dialog)
            {
                dialogAutomationError = QStringLiteral(
                    "Sub Prep print dialog did not open."
                    );
                return;
            }
            dialogOpened = true;

            auto* const targetEdit = dialog->findChild<QLineEdit*>(
                QStringLiteral("subPrepTargetFolderEdit")
                );
            auto* const nameEdit = dialog->findChild<QLineEdit*>(
                QStringLiteral("subPrepUserNameEdit")
                );
            auto* const openFolderCheck = dialog->findChild<QCheckBox*>(
                QStringLiteral("subPrepOpenFolderCheckBox")
                );
            auto* const acceptButton = dialog->findChild<QPushButton*>(
                QStringLiteral("subPrepGenerateOkButton")
                );
            if (!targetEdit || !nameEdit || !openFolderCheck || !acceptButton)
            {
                dialogAutomationError = QStringLiteral(
                    "Sub Prep print dialog controls are incomplete."
                    );
                dialog->reject();
                return;
            }

            QCheckBox* tuesdayCheck = nullptr;
            QCheckBox* thursdayCheck = nullptr;
            for (const QString& day : QStringList{
                     QStringLiteral("Monday"),
                     QStringLiteral("Tuesday"),
                     QStringLiteral("Wednesday"),
                     QStringLiteral("Thursday"),
                     QStringLiteral("Friday")
                 })
            {
                auto* const dayCheck = dialog->findChild<QCheckBox*>(
                    QStringLiteral("subPrepPrint%1CheckBox").arg(day)
                    );
                if (!dayCheck)
                {
                    dialogAutomationError = QStringLiteral(
                        "A weekday selection control is missing."
                        );
                    dialog->reject();
                    return;
                }
                dayCheck->setChecked(false);
                if (day == QStringLiteral("Tuesday"))
                {
                    tuesdayCheck = dayCheck;
                }
                else if (day == QStringLiteral("Thursday"))
                {
                    thursdayCheck = dayCheck;
                }
            }
            if (!tuesdayCheck || !thursdayCheck)
            {
                dialogAutomationError = QStringLiteral(
                    "Tuesday or Thursday selection control is missing."
                    );
                dialog->reject();
                return;
            }

            tuesdayCheck->setChecked(true);
            thursdayCheck->setChecked(true);
            targetEdit->setText(outputRoot.path());
            nameEdit->setText(QStringLiteral("F507"));
            openFolderCheck->setChecked(false);
            if (!acceptButton->isEnabled())
            {
                dialogAutomationError = QStringLiteral(
                    "Sub Prep generation did not become enabled."
                    );
                dialog->reject();
                return;
            }
            acceptButton->click();
            dialogAccepted = dialog->result() == QDialog::Accepted;
        }
        );
    QTimer safetyCloseTimer;
    safetyCloseTimer.setSingleShot(true);
    QObject::connect(&safetyCloseTimer, &QTimer::timeout, &harness.page, [&]
    {
        if (QWidget* const modal = QApplication::activeModalWidget())
        {
            safetyCloseTriggered = true;
            if (dialogAutomationError.isEmpty())
            {
                dialogAutomationError = QStringLiteral(
                    "Sub Prep print dialog automation timed out."
                    );
            }
            modal->close();
        }
    });

    FakeUserPromptService promptService;
    DialogServices::setUserPromptServiceForTesting(&promptService);
    safetyCloseTimer.start(5'000);
    const bool generationInvoked = QMetaObject::invokeMethod(
        &harness.page,
        "generateSubPrep",
        Qt::DirectConnection
        );
    safetyCloseTimer.stop();
    DialogServices::setUserPromptServiceForTesting(nullptr);
    QVERIFY(generationInvoked);

    QVERIFY2(!safetyCloseTriggered, qPrintable(dialogAutomationError));
    QVERIFY2(dialogOpened, qPrintable(dialogAutomationError));
    QVERIFY2(dialogAccepted, qPrintable(dialogAutomationError));
    const QStringList generationMessages = [&promptService]
    {
        QStringList messages;
        for (const PromptRequest& prompt : promptService.messages)
        {
            messages.append(
                QStringLiteral("%1: %2").arg(prompt.title, prompt.message)
                );
        }
        return messages;
    }();
    QVERIFY2(
        promptService.messages.isEmpty(),
        qPrintable(generationMessages.join(QLatin1Char('\n')))
        );

    QCOMPARE(harness.printSourceReadPort.loadCount, 1);
    const SubPrepPrintSourceRequest& request =
        harness.printSourceReadPort.lastRequest;
    QCOMPARE(request.selectedDays.size(), std::size_t(2));
    QCOMPARE(
        static_cast<int>(request.selectedDays.at(0)),
        static_cast<int>(SubPrepWeekday::Tuesday)
        );
    QCOMPARE(
        static_cast<int>(request.selectedDays.at(1)),
        static_cast<int>(SubPrepWeekday::Thursday)
        );
    QCOMPARE(request.selectedClassIds.size(), std::size_t(2));
    QCOMPARE(request.selectedClassIds.at(0).value(), std::string("100"));
    QCOMPARE(request.selectedClassIds.at(1).value(), std::string("43"));
    QCOMPARE(request.mode, ScheduleViewMode::Regular);
    const SubPrepPrintSourceInput& selectedSource =
        harness.printSourceReadPort.lastSelectedInput;
    QCOMPARE(selectedSource.classes.size(), std::size_t(2));
    QCOMPARE(selectedSource.classes.at(0).id.value(), std::string("100"));
    QCOMPARE(
        selectedSource.classes.at(0).grade,
        std::string("F507_TUESDAY_GRADE")
        );
    QCOMPARE(
        selectedSource.classes.at(0).level,
        std::string("F507_TUESDAY_CLASS")
        );
    QCOMPARE(selectedSource.classes.at(1).id.value(), std::string("43"));
    QCOMPARE(
        selectedSource.classes.at(1).grade,
        std::string("F507_THURSDAY_GRADE")
        );
    QCOMPARE(
        selectedSource.classes.at(1).level,
        std::string("F507_THURSDAY_CLASS")
        );
    QCOMPARE(selectedSource.teachers.size(), std::size_t(2));
    QCOMPARE(
        selectedSource.teachers.at(0).englishName,
        std::string("F507_TUESDAY_TEACHER")
        );
    QCOMPARE(
        selectedSource.teachers.at(1).englishName,
        std::string("F507_THURSDAY_TEACHER")
        );
    QVERIFY(std::none_of(
        selectedSource.classes.cbegin(),
        selectedSource.classes.cend(),
        [](const SubPrepPrintClass& classRecord)
        {
            return classRecord.id.value() == "44";
        }
        ));
    QVERIFY(std::none_of(
        selectedSource.teachers.cbegin(),
        selectedSource.teachers.cend(),
        [](const SubPrepPrintTeacher& teacher)
        {
            return teacher.englishName.find("F507_FRIDAY_SENTINEL")
                != std::string::npos;
        }
        ));

    const QStringList packageDirectories = QDir(outputRoot.path()).entryList(
        QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::Name
        );
    QCOMPARE(packageDirectories.size(), 1);
    const QString outputDirectory = QDir(outputRoot.path()).filePath(
        packageDirectories.first()
        );
    QVERIFY(QFileInfo(outputDirectory).isDir());
    const QStringList classDirectories = QDir(outputDirectory).entryList(
        QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::Name
        );
    QCOMPARE(classDirectories.size(), 2);
    for (const QString& classDirectory : classDirectories)
    {
        QVERIFY(QFileInfo(QDir(outputDirectory).filePath(classDirectory)).isDir());
    }

    const QString informationPdfPath = QDir(outputDirectory).filePath(
        QStringLiteral("Sub Prep.pdf")
        );
    const QFileInfo informationPdfInfo(informationPdfPath);
    QVERIFY(informationPdfInfo.exists());
    QVERIFY(informationPdfInfo.size() > 0);

    QPdfDocument informationPdf;
    QCOMPARE(
        informationPdf.load(informationPdfPath),
        QPdfDocument::Error::None
        );
    QCOMPARE(informationPdf.status(), QPdfDocument::Status::Ready);
    QVERIFY(informationPdf.pageCount() > 0);
    QStringList informationPdfPages;
    for (int index = 0; index < informationPdf.pageCount(); ++index)
    {
        informationPdfPages.append(
            informationPdf.getAllText(index).text()
            );
    }
    const QString informationPdfText =
        informationPdfPages.join(QLatin1Char(' '));
    QVERIFY(informationPdfText.contains(
        QStringLiteral("F507_TUESDAY_GRADE")
        ));
    QVERIFY(informationPdfText.contains(
        QStringLiteral("F507_TUESDAY_CLASS")
        ));
    QVERIFY(informationPdfText.contains(
        QStringLiteral("F507_TUESDAY_TEACHER")
        ));
    QVERIFY(informationPdfText.contains(
        QStringLiteral("F507_THURSDAY_GRADE")
        ));
    QVERIFY(informationPdfText.contains(
        QStringLiteral("F507_THURSDAY_CLASS")
        ));
    QVERIFY(informationPdfText.contains(
        QStringLiteral("F507_THURSDAY_TEACHER")
        ));
    QVERIFY(!informationPdfText.contains(
        QStringLiteral("F507_FRIDAY_SENTINEL")
        ));
}

void SubPrepPageTests
    ::packageFolderNamesCoverDateRangesAndUnsafeCharacters()
{
    QCOMPARE(
        SubPrepPackageService::datedFolderName(
            QStringLiteral("Alex"),
            {QDate(2026, 7, 20)}
            ),
        QStringLiteral("Alex (20 Jul 2026)")
        );
    QCOMPARE(
        SubPrepPackageService::datedFolderName(
            QStringLiteral("Alex"),
            {QDate(2026, 7, 20), QDate(2026, 7, 22)}
            ),
        QStringLiteral("Alex (20 - 22 Jul 2026)")
        );
    QCOMPARE(
        SubPrepPackageService::datedFolderName(
            QStringLiteral("Alex"),
            {QDate(2026, 7, 31), QDate(2026, 8, 3)}
            ),
        QStringLiteral("Alex (31 Jul - 03 Aug 2026)")
        );
    QCOMPARE(
        SubPrepPackageService::datedFolderName(
            QStringLiteral("Alex"),
            {QDate(2026, 12, 31), QDate(2027, 1, 1)}
            ),
        QStringLiteral("Alex (31 Dec 2026 - 01 Jan 2027)")
        );
    QCOMPARE(
        SubPrepPackageService::safePathComponent(
            QStringLiteral("E4 / Susan: 4:00")
            ),
        QStringLiteral("E4 - Susan. 4.00")
        );
}

void SubPrepPageTests::printDialogRequiresAndSavesMissingUserName()
{
    ApplicationServices services;
    QTemporaryDir targetRoot;
    QVERIFY(targetRoot.isValid());

    ScheduleViewModel schedule;
    schedule.days = {QStringLiteral("Tuesday")};
    ScheduleRowView row;
    ScheduleCellView cell;
    cell.day = QStringLiteral("Tuesday");
    ScheduleEntry entry;
    entry.classId = 42;
    cell.entries.append(entry);
    row.cells.append(cell);
    schedule.rows.append(row);

    CalendarEvent vacation;
    vacation.eventType = QStringLiteral("Vacation");
    vacation.startDate = QDate(2026, 7, 14);
    vacation.endDate = QDate(2026, 7, 14);

    SubPrepPrintDialog dialog(
        &services,
        schedule,
        {vacation},
        QDate(2026, 7, 13)
        );
    auto* nameEdit = dialog.findChild<QLineEdit*>(
        QStringLiteral("subPrepUserNameEdit")
        );
    auto* targetEdit = dialog.findChild<QLineEdit*>(
        QStringLiteral("subPrepTargetFolderEdit")
        );
    auto* okButton = dialog.findChild<QPushButton*>(
        QStringLiteral("subPrepGenerateOkButton")
        );
    auto* createFolderCheck = dialog.findChild<QCheckBox*>(
        QStringLiteral("subPrepCreateFolderCheckBox")
        );
    auto* printPaperCheck = dialog.findChild<QCheckBox*>(
        QStringLiteral("subPrepPrintPaperCopiesCheckBox")
        );
    auto* nextVacationCheck = dialog.findChild<QCheckBox*>(
        QStringLiteral("subPrepNextVacationCheckBox")
        );
    auto* tuesdayCheck = dialog.findChild<QCheckBox*>(
        QStringLiteral("subPrepPrintTuesdayCheckBox")
        );
    auto* folderOptions = dialog.findChild<QWidget*>(
        QStringLiteral("subPrepFolderOptions")
        );
    auto* outputPreview = dialog.findChild<QLabel*>(
        QStringLiteral("subPrepOutputFolderPreview")
        );
    auto* validationLabel = dialog.findChild<QLabel*>(
        QStringLiteral("subPrepGenerationValidationLabel")
        );
    auto* nameHint = dialog.findChild<QLabel*>(
        QStringLiteral("subPrepUserNameHintLabel")
        );
    QVERIFY(nameEdit);
    QVERIFY(targetEdit);
    QVERIFY(okButton);
    QVERIFY(createFolderCheck);
    QVERIFY(printPaperCheck);
    QVERIFY(nextVacationCheck);
    QVERIFY(tuesdayCheck);
    QVERIFY(folderOptions);
    QVERIFY(outputPreview);
    QVERIFY(validationLabel);
    QVERIFY(nameHint);
    auto* rootLayout = qobject_cast<QVBoxLayout*>(dialog.layout());
    QVERIFY(rootLayout);
    QCOMPARE(
        rootLayout->itemAt(rootLayout->indexOf(printPaperCheck))->alignment(),
        Qt::Alignment(Qt::AlignTop)
        );
    QVERIFY(nameEdit->isVisibleTo(&dialog));
    QVERIFY(nameHint->isVisibleTo(&dialog));
    QCOMPARE(nameHint->text(), QStringLiteral("Enter your name to continue."));
    QVERIFY(nameHint->wordWrap());
    QVERIFY(!okButton->isEnabled());

    createFolderCheck->setChecked(false);
    QVERIFY(!folderOptions->isEnabled());
    QVERIFY(!okButton->isEnabled());
    printPaperCheck->setChecked(true);
    QVERIFY(okButton->isEnabled());
    printPaperCheck->setChecked(false);
    createFolderCheck->setChecked(true);

    targetEdit->setText(targetRoot.path());
    nameEdit->setText(QStringLiteral("Jamie"));
    QVERIFY(okButton->isEnabled());
    auto* folderLayout = qobject_cast<QGridLayout*>(folderOptions->layout());
    QVERIFY(folderLayout);
    QCOMPARE(
        folderLayout->itemAtPosition(0, 0)->alignment(),
        Qt::Alignment(Qt::AlignVCenter)
        );
    QCOMPARE(
        folderLayout->itemAtPosition(0, 1)->alignment(),
        Qt::Alignment(Qt::AlignVCenter)
        );
    QCOMPARE(
        folderLayout->itemAtPosition(0, 2)->alignment(),
        Qt::Alignment(Qt::AlignVCenter)
        );
    QCOMPARE(
        folderLayout->itemAtPosition(2, 0)->alignment(),
        Qt::Alignment(Qt::AlignVCenter)
        );
    QCOMPARE(
        folderLayout->itemAtPosition(2, 1)->alignment(),
        Qt::Alignment(Qt::AlignVCenter)
        );
    QCOMPARE(
        folderLayout->itemAtPosition(3, 1)->widget(),
        nameHint
        );
    QCOMPARE(
        folderLayout->itemAtPosition(4, 0)->alignment(),
        Qt::Alignment(Qt::AlignVCenter)
        );
    QCOMPARE(
        folderLayout->itemAtPosition(4, 1)->alignment(),
        Qt::Alignment(Qt::AlignVCenter)
        );
    auto* targetFolderLabel = qobject_cast<QLabel*>(
        folderLayout->itemAtPosition(0, 0)->widget()
        );
    auto* outputFolderLabel = qobject_cast<QLabel*>(
        folderLayout->itemAtPosition(4, 0)->widget()
        );
    QVERIFY(targetFolderLabel);
    QVERIFY(outputFolderLabel);
    QCOMPARE(
        folderLayout->columnMinimumWidth(0),
        std::max(
            targetFolderLabel->sizeHint().width(),
            outputFolderLabel->sizeHint().width()
            ) + folderLayout->horizontalSpacing()
        );
    QCOMPARE(
        outputPreview->text(),
        QStringLiteral(".../Jamie (14 Jul 2026)")
        );

    nextVacationCheck->setChecked(false);
    tuesdayCheck->setChecked(false);
    QVERIFY(outputPreview->text().isEmpty());
    QVERIFY(!okButton->isEnabled());
    nextVacationCheck->setChecked(true);
    QVERIFY(tuesdayCheck->isChecked());
    QCOMPARE(
        outputPreview->text(),
        QStringLiteral(".../Jamie (14 Jul 2026)")
        );
    QVERIFY(okButton->isEnabled());

    dialog.show();
    QTest::qWait(1);
    const QSize readySize = dialog.size();
    QCOMPARE(dialog.minimumSize(), readySize);
    QCOMPARE(dialog.maximumSize(), readySize);
    QVERIFY(nameHint->geometry().top() > nameEdit->geometry().bottom());
    dialog.resize(readySize + QSize(100, 100));
    QCOMPARE(dialog.size(), readySize);
    createFolderCheck->setChecked(false);
    QCOMPARE(
        validationLabel->text(),
        QStringLiteral(
            "Select Create Folder and/or Print Paper Copies to continue."
            )
        );
    QTest::qWait(1);
    QCOMPARE(dialog.size(), readySize);

    createFolderCheck->setChecked(true);
    okButton->click();

    QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
    QCOMPARE(
        services.dataService()
            ->loadSetting(QStringLiteral("myInfo/name"))
            ->toString(),
        QStringLiteral("Jamie")
        );
}

QTEST_MAIN(SubPrepPageTests)

#include "sub_prep_page_tests.moc"
