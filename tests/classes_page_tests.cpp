#include "core/application_services.h"
#include "features/classes/class_navigation_preferences.h"
#include "features/classes/ui/class_co_teacher_page.h"
#include "features/classes/ui/class_details_page.h"
#include "features/classes/ui/classes_page.h"
#include "features/roster/ui/roster_editor_widget.h"
#include "features/speaking_eval/ui/speaking_eval_page.h"
#include "domain/models/speaking_evaluation.h"
#include "ui/shared/widgets/navigation_pill_button.h"
#include "ui/shared/widgets/navigation_pill_style.h"
#include "ui/shared/widgets/navigation_tab_widget.h"
#include "ui/shared/widgets/on_screen_keyboard.h"

#include <QtTest>

#include <QApplication>
#include <QAbstractButton>
#include <QComboBox>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QTableView>

#include "ui/shared/widgets/sectioncards/class_info_section_card.h"

#include <algorithm>

namespace ScheduleWidgetTestStubs
{
void reset();
void setIncludeAdditionalClass(bool include);
void setClassGrade(int classId, const QString& grade);
void setIncludeAlternativeMatchingClass(bool include);
void setExistingIntensiveHours(bool exists);
void setDistinctIntensiveDays(bool distinct);
void setSpeakingEvaluation(
    int classId,
    const QString& evaluationName,
    const SpeakingEvalRows& rows
    );
QString settingValue(const QString& key);
}

void RosterEditorWidget::importScores()
{
}

void RosterEditorWidget::outputRosters(
    bool print
    )
{
    Q_UNUSED(print);
}

namespace
{
QAbstractButton* dayFilterButton(
    ClassesPage* page,
    const QString& objectName
    )
{
    if (!page)
    {
        return nullptr;
    }

    const QList<QAbstractButton*> buttons =
        page->findChildren<QAbstractButton*>(objectName);

    return buttons.isEmpty()
        ? nullptr
        : buttons.last();
}

NavigationTabWidget* gradeTabs(
    ClassesPage* page
    )
{
    if (!page)
    {
        return nullptr;
    }

    const QList<NavigationTabWidget*> tabs =
        page->findChildren<NavigationTabWidget*>(
            QStringLiteral("classesGradeTabs")
            );

    return tabs.isEmpty()
        ? nullptr
        : tabs.last();
}

NavigationPillButton* navigationPillButton(
    ClassesPage* page,
    const QString& objectName
    )
{
    if (!page)
    {
        return nullptr;
    }

    const QList<NavigationPillButton*> buttons =
        page->findChildren<NavigationPillButton*>(objectName);

    return buttons.isEmpty()
        ? nullptr
        : buttons.last();
}

}

class ClassesPageTests : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void nestedEditorsAreDeferredUntilTheirSectionIsOpened();
    void classDetailsAndCoTeacherTabsSeparateTheirSectionCards();
    void middleSchoolAnalyticsAndEvaluationsTabsFollowPreference();
    void dayFiltersToggleIndependentlyAndRetainHiddenEditor();
    void explicitClassRequestRetainsExcludingFiltersAndAllSelection();
    void testingModeUsesRegularMeetingsForDayFiltering();
    void allGradeTabShowsClassesAcrossGrades();
    void dayFilterSelectsAllWhenSelectedGradeDisappears();
    void selectedGradeRemainsVisibleWhenCurrentClassIsFilteredOut();
    void navigationControlsUsePills();
    void navigationRowsUseUniformSpacing();
    void filterPillsKeepStaticWidthsWhenPageResizes();
    void classInfoShowsInlineValidationAndBlocksManualSave();
    void speakingEvaluationShowsInlineValidationAndBlocksManualSave();
    void evaluationsSectionShowsSelectedSpeakingEvaluation();
    void evaluationTemplatePackReleasesAndReacquiresAcrossSections();
    void headerKeyboardReplacesEmbeddedRosterButton();
};

void ClassesPageTests::init()
{
    ScheduleWidgetTestStubs::reset();
    ScheduleWidgetTestStubs::setIncludeAdditionalClass(true);
}

void ClassesPageTests::nestedEditorsAreDeferredUntilTheirSectionIsOpened()
{
    ApplicationServices services;
    ClassesPage page(&services);

    for (const ClassesSection section : {
             ClassesSection::Details,
             ClassesSection::Roster,
             ClassesSection::Analytics,
             ClassesSection::Evaluations,
             ClassesSection::CoTeacher,
             ClassesSection::Notes
         })
    {
        QVERIFY(!page.isEditorInstantiated(section));
    }

    QVERIFY(page.openClass(42, ClassesSection::Details));
    QVERIFY(page.isEditorInstantiated(ClassesSection::Details));
    QVERIFY(!page.isEditorInstantiated(ClassesSection::Roster));
    QVERIFY(!page.isEditorInstantiated(ClassesSection::Analytics));
    QVERIFY(!page.isEditorInstantiated(ClassesSection::Evaluations));
    QVERIFY(!page.isEditorInstantiated(ClassesSection::CoTeacher));
    QVERIFY(!page.isEditorInstantiated(ClassesSection::Notes));

    QVERIFY(page.openClass(42, ClassesSection::Analytics));
    QVERIFY(page.isEditorInstantiated(ClassesSection::Analytics));
    QVERIFY(!page.isEditorInstantiated(ClassesSection::Evaluations));

    QVERIFY(page.openClass(42, ClassesSection::Evaluations));
    QVERIFY(page.isEditorInstantiated(ClassesSection::Evaluations));
    QVERIFY(!page.isEditorInstantiated(ClassesSection::Roster));
    QVERIFY(!page.isEditorInstantiated(ClassesSection::CoTeacher));
    QVERIFY(!page.isEditorInstantiated(ClassesSection::Notes));
}

void ClassesPageTests::classDetailsAndCoTeacherTabsSeparateTheirSectionCards()
{
    ApplicationServices services;
    ClassesPage page(&services);

    auto* sectionTabs = page.findChild<NavigationTabWidget*>(
        QStringLiteral("classesSectionTabs")
        );
    QVERIFY(sectionTabs);
    const QStringList actualTabLabels{
        sectionTabs->tabText(0),
        sectionTabs->tabText(1),
        sectionTabs->tabText(2),
        sectionTabs->tabText(3),
        sectionTabs->tabText(4),
        sectionTabs->tabText(5)
    };
    const QStringList expectedTabLabels{
        QStringLiteral("Details"),
        QStringLiteral("Roster"),
        QStringLiteral("Analytics"),
        QStringLiteral("Evaluations"),
        QStringLiteral("Co-Teacher"),
        QStringLiteral("Notes")
    };
    QCOMPARE(actualTabLabels, expectedTabLabels);

    QVERIFY(page.openClass(42, ClassesSection::Details));
    auto* details = page.findChild<ClassDetailsPage*>();
    QVERIFY(details);
    QVERIFY(details->findChild<SectionCard*>(
        QStringLiteral("classDetailsCard")
        ));
    QVERIFY(details->findChild<SectionCard*>(
        QStringLiteral("classTimesCard")
        ));
    QVERIFY(!details->findChild<SectionCard*>(
        QStringLiteral("classKoreanTeacherCard")
        ));

    QVERIFY(page.openClass(42, ClassesSection::CoTeacher));
    auto* coTeacher = page.findChild<ClassCoTeacherPage*>();
    QVERIFY(coTeacher);
    QVERIFY(coTeacher->findChild<SectionCard*>(
        QStringLiteral("classKoreanTeacherCard")
        ));
    QVERIFY(!coTeacher->findChild<SectionCard*>(
        QStringLiteral("classDetailsCard")
        ));
    QVERIFY(!coTeacher->findChild<SectionCard*>(
        QStringLiteral("classTimesCard")
        ));
}

void ClassesPageTests
    ::middleSchoolAnalyticsAndEvaluationsTabsFollowPreference()
{
    ApplicationServices services;
    ClassesPage page(&services);
    auto* sectionTabs = page.findChild<NavigationTabWidget*>(
        QStringLiteral("classesSectionTabs")
        );
    QVERIFY(sectionTabs);

    const QStringList defaultLabels{
        QStringLiteral("Details"),
        QStringLiteral("Roster"),
        QStringLiteral("Co-Teacher"),
        QStringLiteral("Notes")
    };
    for (const QString& grade : {
             QStringLiteral("M1"),
             QStringLiteral("M2"),
             QStringLiteral("M3")
         })
    {
        ScheduleWidgetTestStubs::setClassGrade(42, grade);
        QVERIFY(page.openClass(42));
        QCOMPARE(page.currentClassId(), 42);
        QCOMPARE(sectionTabs->count(), defaultLabels.size());
        for (int index = 0; index < defaultLabels.size(); ++index)
        {
            QCOMPARE(sectionTabs->tabText(index), defaultLabels.at(index));
        }
    }

    ClassNavigationPreferences::saveShowMiddleSchoolAnalyticsAndEvaluations(
        services.settingsService(),
        true
        );
    page.refreshNavigationPreferences();

    const QStringList enabledLabels{
        QStringLiteral("Details"),
        QStringLiteral("Roster"),
        QStringLiteral("Analytics"),
        QStringLiteral("Evaluations"),
        QStringLiteral("Co-Teacher"),
        QStringLiteral("Notes")
    };
    QCOMPARE(sectionTabs->count(), enabledLabels.size());
    for (int index = 0; index < enabledLabels.size(); ++index)
    {
        QCOMPARE(sectionTabs->tabText(index), enabledLabels.at(index));
    }
}

void ClassesPageTests::dayFiltersToggleIndependentlyAndRetainHiddenEditor()
{
    ApplicationServices services;
    ClassesPage page(&services);
    page.resize(1200, 800);
    QVERIFY(page.openClass(42));
    page.show();
    QApplication::processEvents();

    const QList<QPair<QString, QString>> buttons{
        {QStringLiteral("classesMondayFilterButton"), QStringLiteral("M")},
        {QStringLiteral("classesTuesdayFilterButton"), QStringLiteral("T")},
        {QStringLiteral("classesWednesdayFilterButton"), QStringLiteral("W")},
        {QStringLiteral("classesThursdayFilterButton"), QStringLiteral("Th")},
        {QStringLiteral("classesFridayFilterButton"), QStringLiteral("F")},
        {QStringLiteral("classesWeekendFilterButton"), QStringLiteral("Wkd")}
    };

    for (const auto& buttonDefinition : buttons)
    {
        auto* button = dayFilterButton(&page, buttonDefinition.first);
        QVERIFY(button);
        QCOMPARE(button->text(), buttonDefinition.second);
        QVERIFY(!button->isChecked());
    }

    auto* tuesday =
        dayFilterButton(
            &page,
            QStringLiteral("classesTuesdayFilterButton")
            );
    QVERIFY(tuesday);
    tuesday->click();
    QApplication::processEvents();

    QCOMPARE(page.currentClassId(), 42);
    QVERIFY(
        dayFilterButton(
            &page,
            QStringLiteral("classesTuesdayFilterButton")
            )->isChecked()
        );
    QCOMPARE(gradeTabs(&page)->count(), 2);

    auto* thursday =
        dayFilterButton(
            &page,
            QStringLiteral("classesThursdayFilterButton")
            );
    QVERIFY(thursday);
    thursday->click();
    QApplication::processEvents();

    QCOMPARE(page.currentClassId(), 42);
    QVERIFY(
        dayFilterButton(
            &page,
            QStringLiteral("classesTuesdayFilterButton")
            )->isChecked()
        );
    QVERIFY(
        dayFilterButton(
            &page,
            QStringLiteral("classesThursdayFilterButton")
            )->isChecked()
        );
    QCOMPARE(gradeTabs(&page)->count(), 3);

    dayFilterButton(
        &page,
        QStringLiteral("classesTuesdayFilterButton")
        )->click();
    QApplication::processEvents();

    QCOMPARE(page.currentClassId(), 42);
    QCOMPARE(gradeTabs(&page)->count(), 2);
    QVERIFY(
        gradeTabs(&page)->selectionVisible()
        );
}

void ClassesPageTests::
    explicitClassRequestRetainsExcludingFiltersAndAllSelection()
{
    ApplicationServices services;
    ClassesPage page(&services);
    QVERIFY(page.openClass(42));

    auto* thursday =
        dayFilterButton(
            &page,
            QStringLiteral("classesThursdayFilterButton")
            );
    QVERIFY(thursday);
    thursday->click();
    QApplication::processEvents();
    QCOMPARE(gradeTabs(&page)->count(), 2);
    QVERIFY(
        gradeTabs(&page)->selectionVisible()
        );

    QVERIFY(page.openClass(42));
    QApplication::processEvents();

    QCOMPARE(page.currentClassId(), 42);
    QVERIFY(
        dayFilterButton(
            &page,
            QStringLiteral("classesThursdayFilterButton")
            )->isChecked()
        );
    QCOMPARE(gradeTabs(&page)->count(), 2);
    QVERIFY(
        gradeTabs(&page)->selectionVisible()
        );
}

void ClassesPageTests::testingModeUsesRegularMeetingsForDayFiltering()
{
    ScheduleWidgetTestStubs::setExistingIntensiveHours(true);
    ScheduleWidgetTestStubs::setDistinctIntensiveDays(true);

    ApplicationServices services;
    ClassesPage page(&services);
    QVERIFY(page.openClass(42));

    auto* tuesday =
        dayFilterButton(
            &page,
            QStringLiteral("classesTuesdayFilterButton")
            );
    QVERIFY(tuesday);
    tuesday->click();
    QApplication::processEvents();
    QCOMPARE(gradeTabs(&page)->count(), 2);

    page.setScheduleDisplayMode(ScheduleDisplayMode::Intensive);
    QApplication::processEvents();
    QCOMPARE(gradeTabs(&page)->count(), 0);
    QVERIFY(
        !gradeTabs(&page)->selectionVisible()
        );

    page.setScheduleDisplayMode(ScheduleDisplayMode::Testing);
    QApplication::processEvents();
    QCOMPARE(gradeTabs(&page)->count(), 2);
    QCOMPARE(page.currentClassId(), 42);
}

void ClassesPageTests::allGradeTabShowsClassesAcrossGrades()
{
    ApplicationServices services;
    ClassesPage page(&services);
    QVERIFY(page.openClass(42));

    auto* tabs = gradeTabs(&page);
    QVERIFY(tabs);
    QVERIFY(tabs->count() > 0);
    QCOMPARE(tabs->tabText(tabs->count() - 1), QStringLiteral("All"));
    QCOMPARE(tabs->currentIndex(), tabs->count() - 1);

    auto* allClassesTabs = tabs->currentWidget()
        ? tabs->currentWidget()->findChild<NavigationTabWidget*>(
            QStringLiteral("classesLevelTabs")
            )
        : nullptr;
    QVERIFY(allClassesTabs);
    QCOMPARE(allClassesTabs->count(), 2);
    QCOMPARE(
        allClassesTabs->tabText(0),
        QStringLiteral("E4 Hercules • T 4:00")
        );
    QCOMPARE(
        allClassesTabs->tabText(1),
        QStringLiteral("E5 Athena • Th 5:00")
        );

    allClassesTabs->setCurrentIndex(1);
    QApplication::processEvents();
    QCOMPARE(page.currentClassId(), 43);
    QCOMPARE(tabs->currentIndex(), tabs->count() - 1);
}

void ClassesPageTests::dayFilterSelectsAllWhenSelectedGradeDisappears()
{
    ApplicationServices services;
    ClassesPage page(&services);
    QVERIFY(page.openClass(42));

    auto* tabs = gradeTabs(&page);
    QVERIFY(tabs);
    tabs->setCurrentIndex(0);
    QCOMPARE(tabs->tabText(tabs->currentIndex()), QStringLiteral("E4"));

    auto* thursday =
        dayFilterButton(
            &page,
            QStringLiteral("classesThursdayFilterButton")
            );
    QVERIFY(thursday);
    thursday->click();
    QApplication::processEvents();

    tabs = gradeTabs(&page);
    QVERIFY(tabs);
    QCOMPARE(page.currentClassId(), 42);
    QCOMPARE(tabs->tabText(tabs->count() - 1), QStringLiteral("All"));
    QCOMPARE(tabs->currentIndex(), tabs->count() - 1);
    QVERIFY(tabs->selectionVisible());
}

void ClassesPageTests::selectedGradeRemainsVisibleWhenCurrentClassIsFilteredOut()
{
    ScheduleWidgetTestStubs::setIncludeAlternativeMatchingClass(true);

    ApplicationServices services;
    ClassesPage page(&services);
    QVERIFY(page.openClass(42));

    auto* tabs = gradeTabs(&page);
    QVERIFY(tabs);
    tabs->setCurrentIndex(0);
    QCOMPARE(tabs->tabText(tabs->currentIndex()), QStringLiteral("E4"));

    auto* monday =
        dayFilterButton(
            &page,
            QStringLiteral("classesMondayFilterButton")
            );
    QVERIFY(monday);
    monday->click();
    QApplication::processEvents();

    auto* tuesday =
        dayFilterButton(
            &page,
            QStringLiteral("classesTuesdayFilterButton")
            );
    QVERIFY(tuesday);
    tuesday->click();
    QApplication::processEvents();

    monday =
        dayFilterButton(
            &page,
            QStringLiteral("classesMondayFilterButton")
            );
    QVERIFY(monday);
    monday->click();
    QApplication::processEvents();

    tabs = gradeTabs(&page);
    QVERIFY(tabs);
    QCOMPARE(page.currentClassId(), 44);
    QCOMPARE(tabs->tabText(tabs->currentIndex()), QStringLiteral("E4"));
    QVERIFY(tabs->selectionVisible());
}

void ClassesPageTests::navigationControlsUsePills()
{
    ApplicationServices services;
    ClassesPage page(&services);
    page.resize(1200, 800);
    QVERIFY(page.openClass(42));
    page.show();
    QApplication::processEvents();

    auto* weekend = navigationPillButton(
        &page,
        QStringLiteral("classesWeekendFilterButton")
        );
    QVERIFY(weekend);
    QVERIFY(weekend->isCheckable());
    auto* gradeTabBar = gradeTabs(&page)->tabStrip();
    QVERIFY(gradeTabBar);
    QCOMPARE(
        weekend->height(),
        gradeTabBar->tabButton(0)->height()
        );
    QCOMPARE(
        weekend->sizeHint().height(),
        gradeTabBar->tabButton(0)->sizeHint().height()
        );
    const QList<NavigationTabStrip*> classTabBars =
        page.findChildren<NavigationTabStrip*>(
            QStringLiteral("classesLevelTabBar")
            );
    QVERIFY(!classTabBars.isEmpty());
    for (const NavigationTabStrip* classTabBar : classTabBars)
    {
        if (classTabBar->count() > 0)
        {
            QCOMPARE(
                classTabBar->tabButton(0)->height(),
                NavigationPillStyle::controlHeight(
                    classTabBar->tabButton(0)->fontMetrics()
                    )
                );
        }
    }
}

void ClassesPageTests::navigationRowsUseUniformSpacing()
{
    ApplicationServices services;
    ClassesPage page(&services);
    page.resize(1200, 800);
    QVERIFY(page.openClass(42));
    page.show();
    QApplication::processEvents();

    auto* gradeTabs = page.findChild<NavigationTabWidget*>(
        QStringLiteral("classesGradeTabs")
        );
    auto* sectionTabs = page.findChild<NavigationTabWidget*>(
        QStringLiteral("classesSectionTabs")
        );
    QVERIFY(gradeTabs);
    QVERIFY(sectionTabs);

    auto* classTabs = gradeTabs->currentWidget()
        ? gradeTabs->currentWidget()->findChild<NavigationTabWidget*>(
            QStringLiteral("classesLevelTabs")
            )
        : nullptr;
    QVERIFY(classTabs);

    const auto rowGap = [&page](const NavigationTabStrip* upper,
                                const NavigationTabStrip* lower)
    {
        return lower->mapTo(&page, QPoint(0, 0)).y()
            - upper->mapTo(&page, QPoint(0, upper->height())).y();
    };

    QCOMPARE(
        rowGap(gradeTabs->tabStrip(), classTabs->tabStrip()),
        rowGap(classTabs->tabStrip(), sectionTabs->tabStrip())
        );
}

void ClassesPageTests::filterPillsKeepStaticWidthsWhenPageResizes()
{
    ApplicationServices services;
    ClassesPage page(&services);
    page.resize(1200, 800);
    QVERIFY(page.openClass(42));
    page.show();
    QApplication::processEvents();

    const QStringList objectNames{
        QStringLiteral("classesMondayFilterButton"),
        QStringLiteral("classesTuesdayFilterButton"),
        QStringLiteral("classesWednesdayFilterButton"),
        QStringLiteral("classesThursdayFilterButton"),
        QStringLiteral("classesFridayFilterButton"),
        QStringLiteral("classesWeekendFilterButton")
    };
    QList<int> widths;
    for (const QString& objectName : objectNames)
    {
        auto* button = navigationPillButton(&page, objectName);
        QVERIFY(button);
        widths.append(button->width());
    }

    page.resize(600, 800);
    QApplication::processEvents();

    for (int index = 0; index < objectNames.size(); ++index)
    {
        auto* button = navigationPillButton(&page, objectNames.at(index));
        QVERIFY(button);
        QCOMPARE(button->width(), widths.at(index));
    }
}

void ClassesPageTests::classInfoShowsInlineValidationAndBlocksManualSave()
{
    ApplicationServices services;
    ClassDetailsPage page(&services);
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(Classroom(QStringLiteral("Class 42"), 42));
    page.show();
    QApplication::processEvents();

    auto* level = page.findChild<QComboBox*>(
        QStringLiteral("classLevelCombo")
        );
    auto* message = page.findChild<QLabel*>(
        QStringLiteral("classLevelValidationMessage")
        );
    auto* saveButton = page.findChild<QPushButton*>(
        QStringLiteral("classInfoSaveButton")
        );
    QVERIFY(level);
    QVERIFY(message);
    QVERIFY(saveButton);

    level->setCurrentIndex(0);
    QApplication::processEvents();

    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(
        level->property("formValidationState").toString(),
        QStringLiteral("error")
        );
    QCOMPARE(message->text(), QStringLiteral("This field is required."));
    QVERIFY(message->isVisible());
    QVERIFY(!saveButton->isEnabled());

    const int validLevel = level->findText(QStringLiteral("Hercules"));
    QVERIFY(validLevel >= 0);
    level->setCurrentIndex(validLevel);
    QApplication::processEvents();

    QVERIFY(!level->property("formValidationState").isValid());
    QVERIFY(message->isHidden());
    QVERIFY(saveButton->isEnabled());
}

void ClassesPageTests::speakingEvaluationShowsInlineValidationAndBlocksManualSave()
{
    ApplicationServices services;
    SpeakingEvalPage page(&services, true);
    page.setSaveMode(SaveMode::Manual);
    page.loadEvaluation(Classroom(QStringLiteral("Class 42"), 42), QStringLiteral("Winter"));
    page.show();
    QApplication::processEvents();

    auto* table = page.findChild<QTableView*>(
        QStringLiteral("classEvaluationsTable")
        );
    auto* message = page.findChild<QLabel*>(
        QStringLiteral("speakingEvalValidationMessage")
        );
    auto* saveButton = page.findChild<QPushButton*>(
        QStringLiteral("speakingEvalSaveButton")
        );
    QVERIFY(table);
    QVERIFY(message);
    QVERIFY(saveButton);

    const QModelIndex english = table->model()->index(0, 1);
    const QModelIndex korean = table->model()->index(0, 2);
    QVERIFY(table->model()->setData(english, QStringLiteral("Amy"), Qt::EditRole));
    QApplication::processEvents();

    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(
        korean.data(Qt::ToolTipRole).toString(),
        QStringLiteral("This field is required.")
        );
    QCOMPARE(
        message->text(),
        QStringLiteral("Correct the highlighted evaluation cells.")
        );
    QVERIFY(message->isVisible());
    QVERIFY(!saveButton->isEnabled());

    QVERIFY(table->model()->setData(korean, QStringLiteral("김아미"), Qt::EditRole));
    QApplication::processEvents();

    QVERIFY(message->isHidden());
    QVERIFY(saveButton->isEnabled());
}

void ClassesPageTests::evaluationsSectionShowsSelectedSpeakingEvaluation()
{
    SpeakingEvalRows winter = SpeakingEval::emptyRows();
    winter[0][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)] =
        QStringLiteral("Winter Student");
    SpeakingEvalRows summer = SpeakingEval::emptyRows();
    summer[0][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)] =
        QStringLiteral("Summer Student");
    ScheduleWidgetTestStubs::setSpeakingEvaluation(
        42,
        QStringLiteral("Winter"),
        winter);
    ScheduleWidgetTestStubs::setSpeakingEvaluation(
        42,
        QStringLiteral("Summer"),
        summer);

    ApplicationServices services;
    ClassesPage page(&services);
    page.resize(1200, 800);
    QVERIFY(page.openEvaluation(42, QStringLiteral("Winter")));
    page.show();
    QApplication::processEvents();

    auto* heading = page.findChild<QLabel*>(
        QStringLiteral("classEvaluationsHeading"));
    auto* evaluationLabel = page.findChild<QLabel*>(
        QStringLiteral("classEvaluationsEvaluationLabel"));
    auto* evaluationCombo = page.findChild<QComboBox*>(
        QStringLiteral("classEvaluationsEvaluationCombo"));
    auto* table = page.findChild<QTableView*>(
        QStringLiteral("classEvaluationsTable"));
    auto* importNamesButton = page.findChild<QPushButton*>(
        QStringLiteral("classEvaluationsImportNamesButton"));
    auto* reportEditorButton = page.findChild<QPushButton*>(
        QStringLiteral("classEvaluationsReportEditorButton"));
    auto* generateCommentsButton = page.findChild<QPushButton*>(
        QStringLiteral("classEvaluationsGenerateCommentsButton"));

    QVERIFY(heading);
    QVERIFY(evaluationLabel);
    QVERIFY(evaluationCombo);
    QVERIFY(table);
    QVERIFY(importNamesButton);
    QVERIFY(reportEditorButton);
    QVERIFY(generateCommentsButton);
    QCOMPARE(heading->text(), QStringLiteral("Speaking Evaluations"));
    QCOMPARE(evaluationLabel->text(), QStringLiteral("Evaluation"));
    QCOMPARE(evaluationCombo->count(), 4);
    QCOMPARE(evaluationCombo->itemText(0), QStringLiteral("Winter"));
    QCOMPARE(evaluationCombo->itemText(1), QStringLiteral("Speech Contest"));
    QCOMPARE(evaluationCombo->itemText(2), QStringLiteral("Summer"));
    QCOMPARE(evaluationCombo->itemText(3), QStringLiteral("Fall"));
    QVERIFY(table->isVisible());
    QVERIFY(importNamesButton->isVisible());
    QVERIFY(reportEditorButton->isVisible());
    QVERIFY(generateCommentsButton->isVisible());
    QCOMPARE(importNamesButton->text(), QStringLiteral("Import Names"));
    QCOMPARE(reportEditorButton->text(), QStringLiteral("Report Editor"));
    QCOMPARE(generateCommentsButton->text(), QStringLiteral("Generate Comments"));
    QCOMPARE(importNamesButton->parentWidget(), reportEditorButton->parentWidget());
    QCOMPARE(reportEditorButton->parentWidget(), generateCommentsButton->parentWidget());
    QVERIFY(importNamesButton->x() < reportEditorButton->x());
    QVERIFY(reportEditorButton->x() < generateCommentsButton->x());
    QCOMPARE(
        generateCommentsButton->geometry().right(),
        generateCommentsButton->parentWidget()->width()
            - generateCommentsButton->parentWidget()
                ->layout()
                ->contentsMargins()
                .right()
            - 1
        );
    QCOMPARE(table->model()->rowCount(), SpeakingEval::RowCount);
    QCOMPARE(table->model()->columnCount(), SpeakingEval::ColumnCount);
    QCOMPARE(
        table->model()
            ->index(0, SpeakingEval::toInt(SpeakingEvalColumn::EnglishName))
            .data()
            .toString(),
        QStringLiteral("Winter Student"));

    evaluationCombo->setCurrentIndex(
        evaluationCombo->findData(QStringLiteral("Summer")));
    QApplication::processEvents();

    QCOMPARE(
        table->model()
            ->index(0, SpeakingEval::toInt(SpeakingEvalColumn::EnglishName))
            .data()
            .toString(),
        QStringLiteral("Summer Student"));
}

void ClassesPageTests::
    evaluationTemplatePackReleasesAndReacquiresAcrossSections()
{
    ApplicationServices services;
    ClassesPage page(&services);
    ResourcePackManager& resourcePacks = ResourcePackManager::instance();

    QVERIFY(page.openClass(42, ClassesSection::Evaluations));
    QVERIFY(resourcePacks.isMounted(QStringLiteral("templates")));

    QVERIFY(page.openClass(42, ClassesSection::Details));
    QVERIFY(!resourcePacks.isMounted(QStringLiteral("templates")));

    QVERIFY(page.openClass(42, ClassesSection::Evaluations));
    QVERIFY(resourcePacks.isMounted(QStringLiteral("templates")));

    page.releaseFeatureResources();
    QVERIFY(!resourcePacks.isMounted(QStringLiteral("templates")));
}

void ClassesPageTests::headerKeyboardReplacesEmbeddedRosterButton()
{
    ApplicationServices services;
    ClassesPage page(&services);
    page.resize(1200, 800);
    QVERIFY(page.openClass(42, ClassesSection::Roster));
    page.show();
    QApplication::processEvents();

    auto* headerTrigger = page.findChild<QPushButton*>(
        QStringLiteral("classesKoreanKeyboardButton")
        );
    auto* embeddedTrigger = page.findChild<QPushButton*>(
        QStringLiteral("rosterKoreanKeyboardButton")
        );
    QVERIFY(headerTrigger);
    QVERIFY(embeddedTrigger);
    QVERIFY(!headerTrigger->icon().isNull());
    QCOMPARE(headerTrigger->accessibleName(), QStringLiteral("Korean Keyboard"));
    QVERIFY(!embeddedTrigger->isVisible());

    headerTrigger->click();
    QApplication::processEvents();

    const auto keyboards = page.findChildren<OnScreenKeyboard*>();
    QVERIFY(std::any_of(
        keyboards.cbegin(),
        keyboards.cend(),
        [](const OnScreenKeyboard* keyboard)
        {
            return keyboard->isVisible() && !keyboard->target();
        }
        ));
    for (OnScreenKeyboard* keyboard : keyboards)
    {
        keyboard->close();
    }

    RosterEditorWidget standalone(&services, true);
    standalone.loadClass(
        Classroom(
            QStringLiteral("Standalone Roster"),
            42
            )
        );
    standalone.resize(900, 500);
    standalone.show();
    QApplication::processEvents();
    auto* standaloneTrigger = standalone.findChild<QPushButton*>(
        QStringLiteral("rosterKoreanKeyboardButton")
        );
    QVERIFY(standaloneTrigger);
    QVERIFY(standaloneTrigger->isVisible());
    QVERIFY(standaloneTrigger->isEnabled());
}

QTEST_MAIN(ClassesPageTests)

#include "classes_page_tests.moc"
