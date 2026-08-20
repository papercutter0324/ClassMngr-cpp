#include "core/application_services.h"
#include "features/classes/ui/classes_navigation_settings_dialog.h"
#include "features/classes/ui/classes_page.h"
#include "features/roster/ui/roster_editor_widget.h"
#include "domain/models/speaking_evaluation.h"
#include "ui/shared/widgets/navigation_pill_button.h"
#include "ui/shared/widgets/navigation_pill_style.h"
#include "ui/shared/widgets/navigation_settings_button.h"
#include "ui/shared/widgets/navigation_tab_widget.h"

#include <QtTest>

#include <QApplication>
#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QTableView>
#include <QTimer>

namespace ScheduleWidgetTestStubs
{
void reset();
void setIncludeAdditionalClass(bool include);
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
    void dayFiltersToggleIndependentlyAndRetainHiddenEditor();
    void explicitClassRequestRetainsExcludingFiltersAndHiddenSelection();
    void testingModeUsesRegularMeetingsForDayFiltering();
    void navigationControlsUsePillsAndPersistScopeSelection();
    void settingsDialogDefaultsToActiveSchedule();
    void evaluationsSectionShowsSelectedSpeakingEvaluation();
};

void ClassesPageTests::init()
{
    ScheduleWidgetTestStubs::reset();
    ScheduleWidgetTestStubs::setIncludeAdditionalClass(true);
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
        {QStringLiteral("classesMondayFilterButton"), QStringLiteral("Mon.")},
        {QStringLiteral("classesTuesdayFilterButton"), QStringLiteral("Tues.")},
        {QStringLiteral("classesWednesdayFilterButton"), QStringLiteral("Wed.")},
        {QStringLiteral("classesThursdayFilterButton"), QStringLiteral("Thurs.")},
        {QStringLiteral("classesFridayFilterButton"), QStringLiteral("Fri.")},
        {QStringLiteral("classesWeekendFilterButton"), QStringLiteral("Wkend")}
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
    QCOMPARE(gradeTabs(&page)->count(), 1);

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
    QCOMPARE(gradeTabs(&page)->count(), 2);

    dayFilterButton(
        &page,
        QStringLiteral("classesTuesdayFilterButton")
        )->click();
    QApplication::processEvents();

    QCOMPARE(page.currentClassId(), 42);
    QCOMPARE(gradeTabs(&page)->count(), 1);
    QVERIFY(
        !gradeTabs(&page)->selectionVisible()
        );
}

void ClassesPageTests::
    explicitClassRequestRetainsExcludingFiltersAndHiddenSelection()
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
    QCOMPARE(gradeTabs(&page)->count(), 1);
    QVERIFY(
        !gradeTabs(&page)->selectionVisible()
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
    QCOMPARE(gradeTabs(&page)->count(), 1);
    QVERIFY(
        !gradeTabs(&page)->selectionVisible()
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
    QCOMPARE(gradeTabs(&page)->count(), 1);

    page.setScheduleDisplayMode(ScheduleDisplayMode::Intensive);
    QApplication::processEvents();
    QCOMPARE(gradeTabs(&page)->count(), 0);
    QVERIFY(
        !gradeTabs(&page)->selectionVisible()
        );

    page.setScheduleDisplayMode(ScheduleDisplayMode::Testing);
    QApplication::processEvents();
    QCOMPARE(gradeTabs(&page)->count(), 1);
    QCOMPARE(page.currentClassId(), 42);
}

void ClassesPageTests::navigationControlsUsePillsAndPersistScopeSelection()
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
    auto* settings = dayFilterButton(
        &page,
        QStringLiteral("classesNavigationSettingsButton")
        );
    QVERIFY(weekend);
    QVERIFY(settings);
    QVERIFY(weekend->isCheckable());
    QVERIFY(!settings->isCheckable());
    QVERIFY(qobject_cast<NavigationSettingsButton*>(settings));
    QVERIFY(settings->property("role").toString().isEmpty());
    QCOMPARE(settings->width(), 46);
    QCOMPARE(settings->text(), QStringLiteral("\u2699"));
    QCOMPARE(settings->font().pointSize(), 18);
    QCOMPARE(settings->font().weight(), QFont::DemiBold);
    QVERIFY(settings->geometry().left() > weekend->geometry().right());
    auto* gradeTabBar = gradeTabs(&page)->tabStrip();
    QVERIFY(gradeTabBar);
    QVERIFY(
        settings->height() >= NavigationPillStyle::ControlHeight
        );
    QCOMPARE(settings->height(), weekend->height());
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
                NavigationPillStyle::ControlHeight
                );
        }
    }
    QCOMPARE(
        ScheduleWidgetTestStubs::settingValue(
            QStringLiteral("classes_navigation_visibility_scope")
            ),
        QStringLiteral("active_schedule")
        );

    QTimer::singleShot(
        0,
        []()
        {
            auto* dialog = qobject_cast<ClassesNavigationSettingsDialog*>(
                QApplication::activeModalWidget()
                );
            QVERIFY(dialog);

            auto* allClasses = dialog->findChild<QRadioButton*>(
                QStringLiteral("classesNavigationAllClasses")
                );
            auto* buttons = dialog->findChild<QDialogButtonBox*>();
            QVERIFY(allClasses);
            QVERIFY(buttons);
            allClasses->click();
            buttons->button(QDialogButtonBox::Save)->click();
        }
        );

    settings->click();

    QCOMPARE(
        ScheduleWidgetTestStubs::settingValue(
            QStringLiteral("classes_navigation_visibility_scope")
            ),
        QStringLiteral("all_classes")
        );
}

void ClassesPageTests::settingsDialogDefaultsToActiveSchedule()
{
    ClassesNavigationSettingsDialog dialog({});

    auto* activeSchedule = dialog.findChild<QRadioButton*>(
        QStringLiteral("classesNavigationActiveSchedule")
        );
    auto* allClasses = dialog.findChild<QRadioButton*>(
        QStringLiteral("classesNavigationAllClasses")
        );
    QVERIFY(activeSchedule);
    QVERIFY(allClasses);
    QVERIFY(activeSchedule->isChecked());
    QVERIFY(!allClasses->isChecked());

    allClasses->click();
    QCOMPARE(
        dialog.values().visibilityScope,
        ClassTabNavigation::VisibilityScope::AllClasses
        );
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
    QVERIFY(page.openClass(42, ClassesSection::Evaluations));
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

    QVERIFY(heading);
    QVERIFY(evaluationLabel);
    QVERIFY(evaluationCombo);
    QVERIFY(table);
    QCOMPARE(heading->text(), QStringLiteral("Speaking Evaluations"));
    QCOMPARE(evaluationLabel->text(), QStringLiteral("Evaluation"));
    QCOMPARE(evaluationCombo->count(), 4);
    QCOMPARE(evaluationCombo->itemText(0), QStringLiteral("Winter"));
    QCOMPARE(evaluationCombo->itemText(1), QStringLiteral("Speech Contest"));
    QCOMPARE(evaluationCombo->itemText(2), QStringLiteral("Summer"));
    QCOMPARE(evaluationCombo->itemText(3), QStringLiteral("Fall"));
    QVERIFY(table->isVisible());
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

QTEST_MAIN(ClassesPageTests)

#include "classes_page_tests.moc"
