#include "classes_page.h"

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/utils/sidebar_node_naming.h"
#include "domain/models/class_info.h"
#include "domain/models/teacher.h"
#include "features/classes/class_navigation_preferences.h"
#include "features/classes/ui/class_details_page.h"
#include "features/classes/ui/classes_navigation_settings_dialog.h"
#include "features/classes/ui/class_evaluations_page.h"
#include "features/classes/ui/class_notes_page.h"
#include "features/classes/ui/class_analytics_page.h"
#include "features/schedule/schedule_display_mode_preferences.h"
#include "features/roster/ui/roster_editor_widget.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/widgets/navigation_pill_button.h"
#include "ui/shared/widgets/navigation_settings_button.h"
#include "ui/shared/widgets/navigation_tab_widget.h"
#include "ui/shared/widgets/on_screen_keyboard.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <utility>

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
constexpr int DayFilterSpacer = 16;

struct DayFilterButtonDefinition
{
    QString key;
    QString objectName;
};

const QList<DayFilterButtonDefinition>& dayFilterButtonDefinitions()
{
    static const QList<DayFilterButtonDefinition> definitions{
        {
            QStringLiteral("Monday"),
            QStringLiteral("classesMondayFilterButton")
        },
        {
            QStringLiteral("Tuesday"),
            QStringLiteral("classesTuesdayFilterButton")
        },
        {
            QStringLiteral("Wednesday"),
            QStringLiteral("classesWednesdayFilterButton")
        },
        {
            QStringLiteral("Thursday"),
            QStringLiteral("classesThursdayFilterButton")
        },
        {
            QStringLiteral("Friday"),
            QStringLiteral("classesFridayFilterButton")
        },
        {
            QStringLiteral("Wkend"),
            QStringLiteral("classesWeekendFilterButton")
        }
    };

    return definitions;
}

ClassTabNavigation::ScheduleSource scheduleSourceForMode(
    ScheduleDisplayMode mode
    )
{
    return mode == ScheduleDisplayMode::Intensive
        ? ClassTabNavigation::ScheduleSource::Intensive
        : ClassTabNavigation::ScheduleSource::Regular;
}

QWidget* tabPage(
    QWidget* parent,
    int classId = -1
    )
{
    auto* page = new QWidget(parent);

    if (classId > 0)
    {
        page->setProperty("class_id", classId);
    }

    return page;
}
}

ClassesPage::ClassesPage(
    ApplicationServices* services,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
{
    setProperty("role", UiRoles::Classes);

    buildUi();

    connect(
        m_detailsPage,
        &ClassDetailsPage::classInfoSaved,
        this,
        &ClassesPage::handleClassInfoSaved
        );
}

bool ClassesPage::openClass(
    int classId,
    ClassesSection section
    )
{
    auto* classService =
        m_services
            ? m_services->classService()
            : nullptr;
    auto* settingsService =
        m_services
            ? m_services->settingsService()
            : nullptr;

    if (!classService || !classService->isAvailable())
    {
        m_classes.clear();
        m_currentClassId = -1;
        m_currentSection = section;
        rebuildClassTabs(-1);
        setEditorAvailable(false);
        updateHeaderText();
        return false;
    }

    setScheduleSource(
        scheduleSourceForMode(
            ScheduleDisplayModePreferences::load(settingsService)
            )
        );
    setVisibilityScope(
        ClassNavigationPreferences::load(settingsService)
        );

    if (!commitActiveEditor())
    {
        restoreSelections();
        return false;
    }

    const Result<QList<Classroom>> loadedClasses = classService->classes();
    if (!loadedClasses)
    {
        DialogServices::showWarning(
            this,
            tr("Load Classes"),
            tr("Classes could not be loaded."),
            loadedClasses.error()
            );
        m_classes.clear();
        m_currentClassId = -1;
        rebuildClassTabs(-1);
        setEditorAvailable(false);
        updateHeaderText();
        return false;
    }
    m_classes = *loadedClasses;

    int selectedClassId =
        classId > 0
            ? classId
            : m_currentClassId;

    if (classroomById(selectedClassId).id <= 0)
    {
        selectedClassId = -1;
    }

    rebuildClassTabs(selectedClassId);

    if (selectedClassId <= 0)
    {
        selectedClassId = firstNavigationClassId();
        syncTabsToClass(selectedClassId);
    }

    const Classroom classroom =
        classroomById(selectedClassId);

    if (classroom.id <= 0)
    {
        m_currentClassId = -1;
        m_currentSection = section;
        setEditorAvailable(false);
        updateHeaderText();
        return true;
    }

    m_currentClassId = classroom.id;
    m_currentSection = section;
    loadEditors(classroom);
    restoreSelections();
    setEditorAvailable(true);
    updateHeaderText();
    return true;
}

bool ClassesPage::loadClasses()
{
    return openClass(
        m_currentClassId,
        m_currentSection
        );
}

int ClassesPage::currentClassId() const
{
    return m_currentClassId;
}

ClassesSection ClassesPage::currentSection() const
{
    return m_currentSection;
}

void ClassesPage::setScheduleDisplayMode(
    ScheduleDisplayMode mode
    )
{
    setScheduleSource(
        scheduleSourceForMode(mode)
        );
}

void ClassesPage::saveData()
{
    if (BasePage* editor = activeEditor())
    {
        editor->saveData();
    }
}

bool ClassesPage::saveChanges()
{
    return commitActiveEditor();
}

bool ClassesPage::hasUnsavedChanges() const
{
    const BasePage* editor = activeEditor();
    return editor && editor->hasUnsavedChanges();
}

void ClassesPage::discardChanges()
{
    if (BasePage* editor = activeEditor())
    {
        editor->discardChanges();
    }
}

QString ClassesPage::unsavedChangesTitle() const
{
    const BasePage* editor = activeEditor();
    return editor
        ? editor->unsavedChangesTitle()
        : BasePage::unsavedChangesTitle();
}

QString ClassesPage::unsavedChangesMessage() const
{
    const BasePage* editor = activeEditor();
    return editor
        ? editor->unsavedChangesMessage()
        : BasePage::unsavedChangesMessage();
}

void ClassesPage::setSaveMode(
    SaveMode mode
    )
{
    m_detailsPage->setSaveMode(mode);
    m_rosterEditor->setSaveMode(mode);
    m_analyticsPage->setSaveMode(mode);
    m_evaluationsPage->setSaveMode(mode);
    m_notesPage->setSaveMode(mode);
}

void ClassesPage::refresh()
{
    BasePage::refresh();

    if (isVisible())
    {
        loadClasses();
    }
}

void ClassesPage::clearDatabaseState()
{
    m_classes.clear();
    m_currentClassId = -1;
    m_currentSection = ClassesSection::Details;

    rebuildClassTabs(-1);

    if (m_detailsPage)
    {
        m_detailsPage->clearDatabaseState();
    }

    if (m_rosterEditor)
    {
        m_rosterEditor->clearDatabaseState();
    }

    if (m_analyticsPage)
    {
        m_analyticsPage->clearDatabaseState();
    }

    if (m_evaluationsPage)
    {
        m_evaluationsPage->clearDatabaseState();
    }

    if (m_notesPage)
    {
        m_notesPage->clearDatabaseState();
    }

    restoreSelections();
    showActiveEditor();
    setEditorAvailable(false);
    updateHeaderText();
}

void ClassesPage::retranslateUi()
{
    m_titleLabel->setText(tr("Classes"));
    m_emptyLabel->setText(tr("No classes available"));

    if (m_koreanKeyboardButton)
    {
        m_koreanKeyboardButton->setToolTip(
            tr("Open Korean / English on-screen keyboard")
            );
        m_koreanKeyboardButton->setAccessibleName(
            tr("Korean Keyboard")
            );
    }

    if (m_sectionTabs && m_sectionTabs->count() >= 5)
    {
        m_sectionTabs->setTabText(0, tr("Details"));
        m_sectionTabs->setTabText(1, tr("Roster"));
        m_sectionTabs->setTabText(2, tr("Analytics"));
        m_sectionTabs->setTabText(3, tr("Evaluations"));
        m_sectionTabs->setTabText(4, tr("Notes"));
    }

    m_detailsPage->retranslateUi();
    m_rosterEditor->retranslateUi();
    m_analyticsPage->retranslateUi();
    m_evaluationsPage->retranslateUi();
    m_notesPage->retranslateUi();

    rebuildClassTabs(m_currentClassId);
    restoreSelections();
    updateHeaderText();
}

void ClassesPage::buildUi()
{
    setBottomBarVisible(false);

    contentLayout()->setContentsMargins(
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        0
        );
    contentLayout()->setSpacing(UiConstants::Pages::Spacing);

    auto* headerLayout = new QVBoxLayout;
    headerLayout->setContentsMargins(
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin
        );
    headerLayout->setSpacing(UiConstants::Pages::HeaderSpacing);

    m_titleLabel = new QLabel(tr("Classes"), this);
    m_titleLabel->setObjectName("pageTitle");
    m_titleLabel->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::TitleFontSize,
            QFont::Bold
            )
        );

    m_subtitleLabel = new QLabel(tr("No class selected"), this);
    m_subtitleLabel->setObjectName("pageSubtitle");
    m_subtitleLabel->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::SubtitleFontSize
            )
        );

    auto* titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(UiConstants::Pages::HeaderSpacing);
    titleRow->addWidget(m_titleLabel);
    titleRow->addStretch();

    m_koreanKeyboardButton = new QPushButton(this);
    m_koreanKeyboardButton->setObjectName(
        QStringLiteral("classesKoreanKeyboardButton")
        );
    m_koreanKeyboardButton->setMinimumSize(44, 40);
    m_koreanKeyboardButton->setMaximumWidth(52);
    m_koreanKeyboardButton->setToolTip(
        tr("Open Korean / English on-screen keyboard")
        );
    m_koreanKeyboardButton->setAccessibleName(
        tr("Korean Keyboard")
        );
    m_onScreenKeyboard = new OnScreenKeyboard(this);
    m_onScreenKeyboard->setTriggerButton(m_koreanKeyboardButton);
    titleRow->addWidget(m_koreanKeyboardButton);

    headerLayout->addLayout(titleRow);
    headerLayout->addWidget(m_subtitleLabel);
    contentLayout()->addLayout(headerLayout);
    contentLayout()->addSpacing(
        UiConstants::Pages::HeaderContentSpacing
        );

    m_navigationContainer = new QWidget(this);
    auto* navigationLayout = new QVBoxLayout(m_navigationContainer);
    navigationLayout->setContentsMargins(0, 0, 0, 0);
    navigationLayout->setSpacing(8);

    m_classTabsContainer = new QWidget(m_navigationContainer);
    m_classTabsLayout = new QVBoxLayout(m_classTabsContainer);
    m_classTabsLayout->setContentsMargins(0, 0, 0, 0);
    m_classTabsLayout->setSpacing(0);
    navigationLayout->addWidget(m_classTabsContainer);

    m_sectionTabs =
        new NavigationTabWidget(
            NavigationTabKind::Section,
            QStringLiteral("classesSectionTabBar"),
            m_navigationContainer
            );
    m_sectionTabs->setObjectName("classesSectionTabs");
    m_sectionTabs->addTab(tabPage(m_sectionTabs), tr("Details"));
    m_sectionTabs->addTab(tabPage(m_sectionTabs), tr("Roster"));
    m_sectionTabs->addTab(tabPage(m_sectionTabs), tr("Analytics"));
    m_sectionTabs->addTab(tabPage(m_sectionTabs), tr("Evaluations"));
    m_sectionTabs->addTab(tabPage(m_sectionTabs), tr("Notes"));
    navigationLayout->addWidget(m_sectionTabs);
    contentLayout()->addWidget(m_navigationContainer);

    m_emptyLabel = new QLabel(tr("No classes available"), this);
    m_emptyLabel->setObjectName("pageSubtitle");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setFont(FontManager::getUiFont(12));
    contentLayout()->addWidget(m_emptyLabel, 1);

    m_editorStack = new QStackedWidget(this);
    m_detailsPage = new ClassDetailsPage(m_services, true, m_editorStack);
    m_rosterEditor = new RosterEditorWidget(m_services, true, m_editorStack);
    m_rosterEditor->setBottomKeyboardButtonVisible(false);
    m_analyticsPage = new ClassAnalyticsPage(m_services, true, m_editorStack);
    m_evaluationsPage = new ClassEvaluationsPage(m_services, true, m_editorStack);
    m_notesPage = new ClassNotesPage(m_services, true, m_editorStack);
    m_editorStack->addWidget(m_detailsPage);
    m_editorStack->addWidget(m_rosterEditor);
    m_editorStack->addWidget(m_analyticsPage);
    m_editorStack->addWidget(m_evaluationsPage);
    m_editorStack->addWidget(m_notesPage);
    contentLayout()->addWidget(m_editorStack, 1);

    connect(
        m_koreanKeyboardButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            if (m_onScreenKeyboard)
            {
                m_onScreenKeyboard->showForFocusScope(this);
            }
        }
        );

    connect(
        m_sectionTabs,
        &NavigationTabWidget::currentChanged,
        this,
        [this](int index)
        {
            if (m_rebuildingTabs || m_restoringTabs || index < 0)
            {
                return;
            }

            activateSection(
                static_cast<ClassesSection>(index)
                );
        }
        );

    setEditorAvailable(false);
    showActiveEditor();
}

void ClassesPage::rebuildClassTabs(
    int selectedClassId
    )
{
    if (!m_classTabsLayout)
    {
        return;
    }

    m_rebuildingTabs = true;
    m_dayFilterButtons.clear();

    while (QLayoutItem* item = m_classTabsLayout->takeAt(0))
    {
        if (QWidget* widget = item->widget())
        {
            widget->deleteLater();
        }

        delete item;
    }

    m_classTabs = nullptr;

    auto* classService =
        m_services
            ? m_services->classService()
            : nullptr;
    auto* teacherService =
        m_services
            ? m_services->teacherService()
            : nullptr;

    QList<ClassTabNavigation::ClassEntry> entries;

    if (
        classService
        && classService->isAvailable()
        && teacherService
        && teacherService->isAvailable()
        )
    {
        for (const Classroom& classroom : std::as_const(m_classes))
        {
            if (classroom.id <= 0)
            {
                continue;
            }

            const ClassInfo info =
                classService->classInfo(classroom.id);
            Teacher teacher;

            if (info.teacherId > 0)
            {
                teacher = teacherService->teacher(info.teacherId)
                    .value_or(Teacher{});
            }

            ClassTabNavigation::ClassEntry entry;
            entry.classId = classroom.id;
            entry.classroomName = classroom.name;
            entry.grade = info.classGrade;
            entry.level = info.classLevel;
            entry.regularTimes = info.classTimes;
            entry.intensiveTimes = info.intensiveTimes;
            entry.teacherEn = teacher.teacherEn;
            entry.teacherKr = teacher.teacherKr;
            entries.append(entry);
        }
    }

    const ClassTabNavigation::Model navigation =
        ClassTabNavigation::build(
            entries,
            ClassTabNavigation::GroupingPolicy::AlwaysGradeGrouped,
            m_dayFilter
            );

    auto* gradeTabs =
        new NavigationTabWidget(
            NavigationTabKind::Grade,
            QStringLiteral("classesGradeTabBar"),
            m_classTabsContainer
            );
    gradeTabs->setObjectName("classesGradeTabs");
    createDayFilterControls(gradeTabs);

    const auto connectClassTabs =
        [this](NavigationTabWidget* tabs)
        {
            connect(
                tabs,
                &NavigationTabWidget::currentChanged,
                this,
                [this, tabs](int)
                {
                    if (m_rebuildingTabs || m_restoringTabs)
                    {
                        return;
                    }

                    setNavigationSelectionVisible(true);
                    activateClass(currentClassIdFromTabs(tabs));
                }
                );
        };

    for (const ClassTabNavigation::GradeGroup& group
         : navigation.gradeGroups)
    {
        auto* gradePage = new QWidget(gradeTabs);
        auto* gradeLayout = new QVBoxLayout(gradePage);
        gradeLayout->setContentsMargins(0, 0, 0, 0);
        gradeLayout->setSpacing(8);
        gradeLayout->setAlignment(Qt::AlignTop);

        auto* classTabs =
            new NavigationTabWidget(
                NavigationTabKind::Class,
                QStringLiteral("classesLevelTabBar"),
                gradePage
                );
        classTabs->setObjectName("classesLevelTabs");

        for (const ClassTabNavigation::ClassTab& classTab
             : group.classes)
        {
            classTabs->addTab(
                tabPage(classTabs, classTab.classId),
                classTab.label
                );
        }

        connectClassTabs(classTabs);
        gradeLayout->addWidget(classTabs);
        gradeTabs->addTab(gradePage, group.label);
    }

    connect(
        gradeTabs,
        &NavigationTabWidget::currentChanged,
        this,
        [this, gradeTabs](int)
        {
            if (m_rebuildingTabs || m_restoringTabs)
            {
                return;
            }

            setNavigationSelectionVisible(true);
            activateClass(currentClassIdFromTabs(gradeTabs));
        }
        );

    m_classTabs = gradeTabs;
    m_classTabsLayout->addWidget(gradeTabs);
    syncTabsToClass(selectedClassId);
    m_rebuildingTabs = false;
}

void ClassesPage::createDayFilterControls(
    NavigationTabWidget* gradeTabs
    )
{
    if (!gradeTabs)
    {
        return;
    }

    auto* controls = new QWidget(gradeTabs);
    controls->setObjectName(
        QStringLiteral("classesDayFilterControls")
        );
    auto* layout = new QHBoxLayout(controls);
    layout->setContentsMargins(
        DayFilterSpacer,
        0,
        0,
        0
        );
    layout->setAlignment(Qt::AlignTop);
    layout->setSpacing(6);

    for (const DayFilterButtonDefinition& definition
         : dayFilterButtonDefinitions())
    {
        auto* button = new NavigationPillButton(controls);
        button->setObjectName(definition.objectName);
        button->setText(
            definition.key == QStringLiteral("Monday")
                ? tr("Mon.")
                : definition.key == QStringLiteral("Tuesday")
                    ? tr("Tues.")
                    : definition.key == QStringLiteral("Wednesday")
                        ? tr("Wed.")
                        : definition.key == QStringLiteral("Thursday")
                            ? tr("Thurs.")
                            : definition.key == QStringLiteral("Friday")
                                ? tr("Fri.")
                                : tr("Wkend")
            );
        button->setCheckable(true);
        button->setChecked(
            dayFilterEnabled(definition.key)
            );
        button->setProperty(
            "classesDayFilter",
            definition.key
            );
        button->setAccessibleName(button->text());
        layout->addWidget(button);

        m_dayFilterButtons.insert(
            definition.key,
            button
            );

        connect(
            button,
            &QPushButton::toggled,
            this,
            [this, key = definition.key](bool enabled)
            {
                setDayFilterEnabled(key, enabled);
            }
            );
    }

    auto* settingsButton = new NavigationSettingsButton(controls);
    settingsButton->setObjectName(
        QStringLiteral("classesNavigationSettingsButton")
        );
    settingsButton->setAccessibleName(
        tr("Classes Settings")
        );
    settingsButton->setToolTip(
        tr("Classes Settings")
        );
    layout->addWidget(settingsButton);
    connect(
        settingsButton,
        &QPushButton::clicked,
        this,
        &ClassesPage::openNavigationSettings
        );

    gradeTabs->setTrailingWidget(controls);
}

void ClassesPage::setDayFilterEnabled(
    const QString& key,
    bool enabled
    )
{
    if (dayFilterEnabled(key) == enabled)
    {
        return;
    }

    if (key == QStringLiteral("Wkend"))
    {
        if (enabled)
        {
            m_dayFilter.selectedDays.insert(QStringLiteral("Saturday"));
            m_dayFilter.selectedDays.insert(QStringLiteral("Sunday"));
        }
        else
        {
            m_dayFilter.selectedDays.remove(QStringLiteral("Saturday"));
            m_dayFilter.selectedDays.remove(QStringLiteral("Sunday"));
        }
    }
    else if (enabled)
    {
        m_dayFilter.selectedDays.insert(key);
    }
    else
    {
        m_dayFilter.selectedDays.remove(key);
    }

    rebuildClassTabs(m_currentClassId);
    restoreSelections();
}

bool ClassesPage::dayFilterEnabled(
    const QString& key
    ) const
{
    if (key == QStringLiteral("Wkend"))
    {
        return m_dayFilter.selectedDays.contains(QStringLiteral("Saturday"))
            || m_dayFilter.selectedDays.contains(QStringLiteral("Sunday"));
    }

    return m_dayFilter.selectedDays.contains(key);
}

void ClassesPage::setNavigationSelectionVisible(
    bool visible
    )
{
    if (!m_classTabs)
    {
        return;
    }

    const QList<NavigationTabStrip*> tabBars =
        m_classTabs->findChildren<NavigationTabStrip*>();

    for (NavigationTabStrip* tabBar : tabBars)
    {
        if (tabBar)
        {
            tabBar->setSelectionVisible(visible);
        }
    }
}

void ClassesPage::openNavigationSettings()
{
    ClassesNavigationSettingsDialog dialog(
        {m_dayFilter.visibilityScope},
        this
        );

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const ClassesNavigationSettingsValues values =
        dialog.values();
    auto* settingsService =
        m_services
            ? m_services->settingsService()
            : nullptr;
    ClassNavigationPreferences::save(
        settingsService,
        values.visibilityScope
        );
    setVisibilityScope(values.visibilityScope);
}

void ClassesPage::setScheduleSource(
    ClassTabNavigation::ScheduleSource source
    )
{
    if (m_dayFilter.scheduleSource == source)
    {
        return;
    }

    m_dayFilter.scheduleSource = source;

    if (m_classTabs && !m_rebuildingTabs)
    {
        rebuildClassTabs(m_currentClassId);
        restoreSelections();
    }
}

void ClassesPage::setVisibilityScope(
    ClassTabNavigation::VisibilityScope visibilityScope
    )
{
    if (m_dayFilter.visibilityScope == visibilityScope)
    {
        return;
    }

    m_dayFilter.visibilityScope = visibilityScope;

    if (m_classTabs && !m_rebuildingTabs)
    {
        rebuildClassTabs(m_currentClassId);
        restoreSelections();
    }
}

bool ClassesPage::activateClass(
    int classId
    )
{
    if (
        classId <= 0
        || classId == m_currentClassId
        || m_rebuildingTabs
        || m_restoringTabs
        )
    {
        return classId == m_currentClassId;
    }

    if (!commitActiveEditor())
    {
        restoreSelections();
        return false;
    }

    const Classroom classroom = classroomById(classId);

    if (classroom.id <= 0)
    {
        restoreSelections();
        return false;
    }

    m_currentClassId = classroom.id;
    loadEditors(classroom);
    restoreSelections();
    updateHeaderText();
    return true;
}

bool ClassesPage::activateSection(
    ClassesSection section
    )
{
    if (section == m_currentSection)
    {
        showActiveEditor();
        return true;
    }

    if (!commitActiveEditor())
    {
        restoreSelections();
        return false;
    }

    m_currentSection = section;
    restoreSelections();
    showActiveEditor();
    return true;
}

bool ClassesPage::commitActiveEditor()
{
    BasePage* editor = activeEditor();
    return !editor
        || !editor->hasUnsavedChanges()
        || editor->saveChanges();
}

void ClassesPage::loadEditors(
    const Classroom& classroom
    )
{
    m_detailsPage->loadClass(classroom);
    m_rosterEditor->loadClass(classroom);
    m_analyticsPage->loadClass(classroom);
    m_evaluationsPage->loadClass(classroom);
    m_notesPage->loadClass(classroom);
    showActiveEditor();
}

void ClassesPage::restoreSelections()
{
    m_restoringTabs = true;
    syncTabsToClass(m_currentClassId);

    if (m_sectionTabs)
    {
        m_sectionTabs->setCurrentIndex(
            static_cast<int>(m_currentSection)
            );
    }

    m_restoringTabs = false;
}

void ClassesPage::syncTabsToClass(
    int classId
    )
{
    if (!m_classTabs)
    {
        return;
    }

    for (int gradeIndex = 0;
         gradeIndex < m_classTabs->count();
         ++gradeIndex)
    {
        QWidget* gradePage = m_classTabs->widget(gradeIndex);
        auto* classTabs =
            gradePage
                ? gradePage->findChild<NavigationTabWidget*>(
                    QStringLiteral("classesLevelTabs")
                    )
                : nullptr;

        if (!classTabs)
        {
            continue;
        }

        for (int classIndex = 0;
             classIndex < classTabs->count();
             ++classIndex)
        {
            QWidget* classPage = classTabs->widget(classIndex);

            if (
                classPage
                && classPage->property("class_id").toInt() == classId
                )
            {
                setNavigationSelectionVisible(true);
                m_classTabs->setCurrentIndex(gradeIndex);
                classTabs->setCurrentIndex(classIndex);
                return;
            }
        }
    }

    if (classId > 0)
    {
        setNavigationSelectionVisible(false);
        return;
    }

    if (m_classTabs->count() > 0)
    {
        setNavigationSelectionVisible(true);
        m_classTabs->setCurrentIndex(0);

        if (auto* classTabs =
            m_classTabs->currentWidget()->findChild<NavigationTabWidget*>(
                QStringLiteral("classesLevelTabs")
                ))
        {
            classTabs->setCurrentIndex(0);
        }
    }
}

int ClassesPage::currentClassIdFromTabs(
    NavigationTabWidget* tabs
    ) const
{
    if (
        !tabs
        || tabs->currentIndex() < 0
        )
    {
        return -1;
    }

    if (!tabs->selectionVisible())
    {
        return -1;
    }

    QWidget* page = tabs->currentWidget();
    const int directClassId =
        page
            ? page->property("class_id").toInt()
            : -1;

    if (directClassId > 0)
    {
        return directClassId;
    }

    auto* classTabs =
        page
            ? page->findChild<NavigationTabWidget*>(
                QStringLiteral("classesLevelTabs")
                )
            : nullptr;

    if (!classTabs || classTabs->currentIndex() < 0)
    {
        return -1;
    }

    QWidget* classPage = classTabs->currentWidget();
    return classPage
        ? classPage->property("class_id").toInt()
        : -1;
}

int ClassesPage::firstNavigationClassId() const
{
    return currentClassIdFromTabs(m_classTabs);
}

Classroom ClassesPage::classroomById(
    int classId
    ) const
{
    for (const Classroom& classroom : m_classes)
    {
        if (classroom.id == classId)
        {
            return classroom;
        }
    }

    return {};
}

BasePage* ClassesPage::activeEditor() const
{
    switch (m_currentSection)
    {
    case ClassesSection::Details:
        return m_detailsPage;

    case ClassesSection::Roster:
        return m_rosterEditor;

    case ClassesSection::Analytics:
        return m_analyticsPage;

    case ClassesSection::Evaluations:
        return m_evaluationsPage;

    case ClassesSection::Notes:
        return m_notesPage;
    }

    return nullptr;
}

void ClassesPage::showActiveEditor()
{
    if (!m_editorStack)
    {
        return;
    }

    if (QWidget* editor = activeEditor())
    {
        m_editorStack->setCurrentWidget(editor);
    }
}

void ClassesPage::updateHeaderText()
{
    m_titleLabel->setText(tr("Classes"));

    const Classroom classroom = classroomById(m_currentClassId);
    auto* classService =
        m_services
            ? m_services->classService()
            : nullptr;
    auto* teacherService =
        m_services
            ? m_services->teacherService()
            : nullptr;

    if (
        !classService
        || !classService->isAvailable()
        || !teacherService
        || !teacherService->isAvailable()
        || classroom.id <= 0
        )
    {
        m_subtitleLabel->setText(tr("No class selected"));
        return;
    }

    const ClassInfo info = classService->classInfo(classroom.id);
    Teacher teacher;

    if (info.teacherId > 0)
    {
        teacher = teacherService->teacher(info.teacherId)
            .value_or(Teacher{});
    }

    const QString displayName =
        SidebarNodeNaming::formatClassDisplayName(info, teacher).trimmed();

    m_subtitleLabel->setText(
        !displayName.isEmpty()
            ? displayName
            : !classroom.name.trimmed().isEmpty()
                ? classroom.name.trimmed()
                : tr("Class %1").arg(classroom.id)
        );
}

void ClassesPage::setEditorAvailable(
    bool available
    )
{
    m_navigationContainer->setVisible(available);
    m_editorStack->setVisible(available);
    m_emptyLabel->setVisible(!available);
}

void ClassesPage::handleClassInfoSaved(
    int classId
    )
{
    auto* classService =
        m_services
            ? m_services->classService()
            : nullptr;

    if (classService && classService->isAvailable())
    {
        const Result<QList<Classroom>> loadedClasses =
            classService->classes();
        if (!loadedClasses)
        {
            DialogServices::showWarning(
                this,
                tr("Load Classes"),
                tr("Classes could not be reloaded after saving."),
                loadedClasses.error()
                );
            return;
        }
        m_classes = *loadedClasses;
        rebuildClassTabs(classId);
        restoreSelections();
        updateHeaderText();
    }

    emit classInfoSaved(classId);
}
