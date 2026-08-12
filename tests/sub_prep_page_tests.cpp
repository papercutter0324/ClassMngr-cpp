#include "core/application_services.h"
#include "data/data_service.h"
#include "features/sub_prep/ui/sub_prep_page.h"
#include "features/sub_prep/ui/sub_prep_print_dialog.h"
#include "features/sub_prep/services/sub_prep_package_service.h"
#include "ui/shared/widgets/sectioncards/class_info_section_card.h"
#include "features/schedule/ui/schedule_widget.h"
#include "ui/shared/widgets/navigation_tab_widget.h"

#include <QtTest>

#include <algorithm>

#include <QLineEdit>
#include <QLabel>
#include <QScrollArea>
#include <QCheckBox>
#include <QDateEdit>
#include <QDir>
#include <QGridLayout>
#include <QPushButton>
#include <QTextEdit>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace ScheduleWidgetTestStubs
{
void reset();
void setDatabaseOpen(bool open);
void setIncludeAdditionalClass(bool include);
}

namespace
{
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
}

class SubPrepPageTests : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void sectionsAppearInRequestedOrderAndUseExpectedEditability();
    void gradeAndLevelTabsSelectOneClassAndPreserveSelection();
    void freshAndExistingGradingSettingsResolveWithoutDataLoss();
    void zoomUnavailableHidesStoredCredentials();
    void printDialogSelectsNextVacationBlock();
    void printDialogOnlyOffersVacationModeWithinFourWeeks();
    void printDialogCombinesVacationDatesAcrossHolidayBlocks();
    void packageFolderNamesCoverDateRangesAndUnsafeCharacters();
    void printDialogRequiresAndSavesMissingUserName();
    void clearDatabaseStateStopsAutosaveAndRemovesLoadedContent();
};

void SubPrepPageTests::init()
{
    ScheduleWidgetTestStubs::reset();
}

void SubPrepPageTests
    ::sectionsAppearInRequestedOrderAndUseExpectedEditability()
{
    ApplicationServices services;
    SubPrepPage page(&services);

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
        page.findChild<NavigationTabWidget*>(
            QStringLiteral("subPrepGradeTabs")
            );
    QVERIFY(gradeTabs);
    QCOMPARE(gradeTabs->count(), 1);
    QCOMPARE(
        gradeTabs->tabStrip()->objectName(),
        QStringLiteral("subPrepGradeTabBar")
        );

    auto* levelTabs =
        gradeTabs
            ->currentWidget()
            ->findChild<NavigationTabWidget*>(
                QStringLiteral("subPrepLevelTabs"),
                Qt::FindDirectChildrenOnly
                );
    QVERIFY(levelTabs);
    QCOMPARE(levelTabs->count(), 1);
    QCOMPARE(
        levelTabs->tabStrip()->objectName(),
        QStringLiteral("subPrepLevelTabBar")
        );
    QCOMPARE(
        levelTabs->currentWidget()->property("classId").toInt(),
        42
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
    SubPrepPage page(&services);

    auto* gradeTabs =
        page.findChild<NavigationTabWidget*>(
            QStringLiteral("subPrepGradeTabs")
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

    auto* levelTabs =
        gradeTabs
            ->currentWidget()
            ->findChild<NavigationTabWidget*>(
                QStringLiteral("subPrepLevelTabs"),
                Qt::FindDirectChildrenOnly
                );
    QVERIFY(levelTabs);
    QCOMPARE(levelTabs->count(), 1);
    QVERIFY(
        levelTabs
            ->tabText(0)
            .contains(QStringLiteral("Athena"))
        );

    QCOMPARE(
        levelTabs->currentWidget()->property("classId").toInt(),
        43
        );

    auto* selectedDetails =
        levelTabs
            ->currentWidget()
            ->findChild<QWidget*>(
                QStringLiteral("subPrepClassDetails")
                );
    QVERIFY(selectedDetails);
    QCOMPARE(
        selectedDetails->property("classId").toInt(),
        43
        );

    page.refresh();
    QCoreApplication::sendPostedEvents(
        nullptr,
        QEvent::DeferredDelete
        );

    gradeTabs =
        page.findChild<NavigationTabWidget*>(
            QStringLiteral("subPrepGradeTabs")
            );
    QVERIFY(gradeTabs);

    levelTabs =
        gradeTabs
            ->currentWidget()
            ->findChild<NavigationTabWidget*>(
                QStringLiteral("subPrepLevelTabs"),
                Qt::FindDirectChildrenOnly
                );
    QVERIFY(levelTabs);
    QCOMPARE(
        levelTabs->currentWidget()->property("classId").toInt(),
        43
        );
}

void SubPrepPageTests
    ::freshAndExistingGradingSettingsResolveWithoutDataLoss()
{
    ApplicationServices services;

    {
        SubPrepPage fresh(&services);
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
    services.dataService()->saveSetting(
        QStringLiteral("subPrep/bookReportGrading"),
        QStringLiteral("Custom legacy grading\nAdditional Rules: keep here")
        );
    services.dataService()->saveSetting(
        QStringLiteral("subPrep/subComments"),
        QStringLiteral("Existing substitute note")
        );

    SubPrepPage existing(&services);
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
            .toString(),
        QStringLiteral("Bring spare books")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(
                QStringLiteral("subPrep/subComments")
                )
            .toString(),
        QStringLiteral("Updated substitute note")
        );
}

void SubPrepPageTests
    ::zoomUnavailableHidesStoredCredentials()
{
    ApplicationServices services;
    services.dataService()->saveSetting(
        QStringLiteral("myInfo/zoomLoginId"),
        QStringLiteral("teacher@example.com")
        );
    services.dataService()->saveSetting(
        QStringLiteral("myInfo/zoomPassword"),
        QStringLiteral("secret")
        );
    services.dataService()->saveSetting(
        QStringLiteral("myInfo/zoomNotAvailable"),
        true
        );

    SubPrepPage unavailable(&services);
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

    services.dataService()->saveSetting(
        QStringLiteral("myInfo/zoomNotAvailable"),
        false
        );

    SubPrepPage available(&services);
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
    legacyServices.dataService()->saveSetting(
        QStringLiteral("subPrep/personalZoomEmail"),
        QStringLiteral("legacy@example.com")
        );
    legacyServices.dataService()->saveSetting(
        QStringLiteral("subPrep/personalZoomPassword"),
        QStringLiteral("legacy secret")
        );
    legacyServices.dataService()->saveSetting(
        QStringLiteral("subPrep/personalZoomNotAvailable"),
        false
        );

    SubPrepPage legacy(&legacyServices);
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
            .toString(),
        QStringLiteral("legacy@example.com")
        );
}

void SubPrepPageTests
    ::clearDatabaseStateStopsAutosaveAndRemovesLoadedContent()
{
    ApplicationServices services;
    services.dataService()->saveSetting(
        QStringLiteral("subPrep/classMaterials"),
        QStringLiteral("Stored database A material")
        );
    services.dataService()->saveSetting(
        QStringLiteral("myInfo/zoomLoginId"),
        QStringLiteral("database-a@example.com")
        );
    services.dataService()->saveSetting(
        QStringLiteral("myInfo/zoomNotAvailable"),
        false
        );

    SubPrepPage page(&services);
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

    QVERIFY(materials);
    QVERIFY(zoomLogin);
    QVERIFY(schedule);
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
    QVERIFY(
        page.findChildren<QWidget*>(
            QStringLiteral("subPrepClassDetails")
            ).isEmpty()
        );

    QTest::qWait(850);
    QCOMPARE(
        services.dataService()
            ->loadSetting(
                QStringLiteral("subPrep/classMaterials")
                )
            .toString(),
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
            .toString(),
        QStringLiteral("Jamie")
        );
}

QTEST_MAIN(SubPrepPageTests)

#include "sub_prep_page_tests.moc"
