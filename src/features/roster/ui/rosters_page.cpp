#include "rosters_page.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/utils/sidebar_node_naming.h"
#include "data/data_service.h"
#include "domain/models/class_info.h"
#include "domain/models/teacher.h"
#include "features/classes/class_navigation_preferences.h"
#include "features/classes/models/class_tab_navigation_model.h"
#include "features/classes/ui/classes_navigation_settings_dialog.h"
#include "features/roster/ui/roster_editor_widget.h"
#include "features/schedule/schedule_display_mode_preferences.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/widgets/navigation_pill_button.h"
#include "ui/shared/widgets/uniform_width_tab_bar.h"

#include <QDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QPushButton>
#include <QSizePolicy>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <utility>

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
        {QStringLiteral("Monday"), QStringLiteral("rosterMondayFilterButton")},
        {QStringLiteral("Tuesday"), QStringLiteral("rosterTuesdayFilterButton")},
        {QStringLiteral("Wednesday"), QStringLiteral("rosterWednesdayFilterButton")},
        {QStringLiteral("Thursday"), QStringLiteral("rosterThursdayFilterButton")},
        {QStringLiteral("Friday"), QStringLiteral("rosterFridayFilterButton")},
        {QStringLiteral("Wkend"), QStringLiteral("rosterWeekendFilterButton")}
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

QString sidebarClassDisplayName(
    DataService* dataService,
    int classId
    )
{
    if (!dataService || !dataService->isOpen() || classId <= 0)
    {
        return {};
    }

    const ClassInfo classInfo =
        dataService->loadClassInfo(
            classId
            );

    Teacher teacher;

    if (classInfo.teacherId > 0)
    {
        teacher =
            dataService->getTeacher(
                classInfo.teacherId
                );
    }

    return SidebarNodeNaming::formatClassDisplayName(
        classInfo,
        teacher
        );
}
}

RostersPage::RostersPage(
    ApplicationServices* services,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
{
    setProperty("role", UiRoles::RostersPage);
    buildUi();
}

void RostersPage::loadClass(
    const Classroom& classroom
    )
{
    loadRosters(
        classroom.id
        );
}

void RostersPage::loadRosters(
    int selectedClassId
    )
{
    auto* dataService =
        m_services
            ? m_services->dataService()
            : nullptr;

    if (!dataService || !dataService->isOpen())
    {
        m_rosterClasses.clear();
        m_currentClassroom = {};
        rebuildRosterTabs(-1);
        m_editor->loadClass({});
        setRosterEditorAvailable(false);
        updateHeaderText();
        return;
    }

    m_rosterClasses =
        dataService->getClasses();

    setScheduleSource(
        scheduleSourceForMode(
            ScheduleDisplayModePreferences::load(dataService)
            )
        );
    setVisibilityScope(
        ClassNavigationPreferences::load(dataService)
        );

    int classId =
        selectedClassId > 0
            ? selectedClassId
            : m_currentClassroom.id;

    if (classroomById(classId).id <= 0)
    {
        classId =
            firstRosterClassId();
    }

    rebuildRosterTabs(
        classId
        );

    m_currentClassroom =
        classroomById(classId);
    m_editor->loadClass(
        m_currentClassroom
        );
    setRosterEditorAvailable(
        m_currentClassroom.id > 0
        );
    updateHeaderText();
}

void RostersPage::setScheduleDisplayMode(
    ScheduleDisplayMode mode
    )
{
    setScheduleSource(
        scheduleSourceForMode(mode)
        );
}

void RostersPage::saveData()
{
    if (m_editor)
    {
        m_editor->saveData();
    }
}

bool RostersPage::saveChanges()
{
    return !m_editor
        || m_editor->saveChanges();
}

bool RostersPage::hasUnsavedChanges() const
{
    return m_editor
        && m_editor->hasUnsavedChanges();
}

void RostersPage::discardChanges()
{
    if (m_editor)
    {
        m_editor->discardChanges();
    }
}

QString RostersPage::unsavedChangesTitle() const
{
    return tr("Unsaved Roster Changes");
}

QString RostersPage::unsavedChangesMessage() const
{
    return tr("This roster has unsaved changes.");
}

void RostersPage::setSaveMode(
    SaveMode mode
    )
{
    BasePage::setSaveMode(mode);

    if (m_editor)
    {
        m_editor->setSaveMode(mode);
    }
}

void RostersPage::clearDatabaseState()
{
    m_rosterClasses.clear();
    m_currentClassroom = {};
    rebuildRosterTabs(-1);

    if (m_editor)
    {
        m_editor->clearDatabaseState();
    }

    setRosterEditorAvailable(false);
    updateHeaderText();
}

void RostersPage::retranslateUi()
{
    updateHeaderText();

    if (m_emptyLabel)
    {
        m_emptyLabel->setText(
            tr("No classes available")
            );
    }

    if (m_editor)
    {
        m_editor->retranslateUi();
    }
}

PageOutputCapabilities RostersPage::outputCapabilities() const
{
    return isDatabaseOpen() && m_editor
        ? m_editor->outputCapabilities()
        : PageOutputCapabilities{};
}

void RostersPage::printCurrentPage()
{
    if (m_editor)
    {
        m_editor->printCurrentPage();
    }
}

void RostersPage::saveCurrentPageAs()
{
    if (m_editor)
    {
        m_editor->saveCurrentPageAs();
    }
}

void RostersPage::buildUi()
{
    contentLayout()->setContentsMargins(
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        0
        );
    contentLayout()->setSpacing(
        UiConstants::Pages::Spacing
        );

    auto* headerLayout =
        new QVBoxLayout;
    headerLayout->setContentsMargins(
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin
        );
    headerLayout->setSpacing(
        UiConstants::Pages::HeaderSpacing
        );

    m_titleLabel =
        new QLabel(
            tr("Rosters"),
            this
            );
    m_titleLabel->setObjectName("pageTitle");
    m_titleLabel->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::TitleFontSize,
            QFont::Bold
            )
        );

    m_subtitleLabel =
        new QLabel(
            tr("No classes available"),
            this
            );
    m_subtitleLabel->setObjectName("pageSubtitle");
    m_subtitleLabel->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::SubtitleFontSize
            )
        );

    headerLayout->addWidget(m_titleLabel);
    headerLayout->addWidget(m_subtitleLabel);
    contentLayout()->addLayout(headerLayout);
    contentLayout()->addSpacing(
        UiConstants::Pages::HeaderContentSpacing
        );

    m_tabsContainer =
        new QWidget(this);
    m_tabsContainer->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Maximum
        );
    m_tabsLayout =
        new QVBoxLayout(m_tabsContainer);
    m_tabsLayout->setContentsMargins(0, 0, 0, 0);
    m_tabsLayout->setSpacing(8);
    contentLayout()->addWidget(
        m_tabsContainer
        );

    m_emptyLabel =
        new QLabel(
            tr("No classes available"),
            this
            );
    m_emptyLabel->setObjectName("pageSubtitle");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setFont(
        FontManager::getUiFont(12)
        );
    contentLayout()->addWidget(
        m_emptyLabel,
        1
        );

    m_editor =
        new RosterEditorWidget(
            m_services,
            true,
            this
            );
    contentLayout()->addWidget(
        m_editor,
        1
        );
    connect(
        m_editor,
        &BasePage::outputCapabilitiesChanged,
        this,
        &BasePage::outputCapabilitiesChanged
        );

    setRosterEditorAvailable(false);
}

void RostersPage::rebuildRosterTabs(
    int selectedClassId
    )
{
    if (!m_tabsLayout || !m_tabsContainer)
    {
        return;
    }

    m_rebuildingRosterTabs = true;

    while (QLayoutItem* item = m_tabsLayout->takeAt(0))
    {
        if (QWidget* widget = item->widget())
        {
            widget->deleteLater();
        }

        delete item;
    }

    m_rosterTabs = nullptr;

    auto* dataService =
        m_services
            ? m_services->dataService()
            : nullptr;
    QList<ClassTabNavigation::ClassEntry> entries;

    if (dataService)
    {
        for (const Classroom& classroom : std::as_const(m_rosterClasses))
        {
            if (classroom.id <= 0)
            {
                continue;
            }

            const ClassInfo info =
                dataService->loadClassInfo(
                    classroom.id
                    );
            Teacher teacher;

            if (info.teacherId > 0)
            {
                teacher =
                    dataService->getTeacher(
                        info.teacherId
                        );
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
            ClassTabNavigation::GroupingPolicy::Adaptive,
            m_dayFilter
            );

    const auto createTabPage =
        [](QWidget* parent, int classId)
        {
            auto* page =
                new QWidget(parent);
            page->setProperty(
                "class_id",
                classId
                );
            return page;
        };

    const auto connectClassTabs =
        [this](QTabWidget* tabs)
        {
            connect(
                tabs,
                &QTabWidget::currentChanged,
                this,
                [this, tabs](int)
                {
                    if (m_rebuildingRosterTabs || m_restoringRosterTabs)
                    {
                        return;
                    }

                    setNavigationSelectionVisible(true);
                    activateRosterClass(
                        currentClassIdFromTabs(tabs)
                        );
                }
                );
        };

    if (navigation.mode == ClassTabNavigation::Mode::Flat)
    {
        auto* tabs =
            new UniformWidthTabWidget(
                UniformWidthTabKind::Class,
                QStringLiteral("rosterClassTabBar"),
                m_tabsContainer
                );
        tabs->setSizePolicy(
            QSizePolicy::Expanding,
            QSizePolicy::Maximum
            );
        tabs->setObjectName("rosterClassTabs");
        createDayFilterControls(tabs);

        for (const ClassTabNavigation::ClassTab& tab
             : navigation.flatClasses)
        {
            tabs->addTab(
                createTabPage(
                    tabs,
                    tab.classId
                    ),
                tab.label
                );
        }

        connectClassTabs(tabs);
        m_rosterTabs = tabs;
        m_tabsLayout->addWidget(tabs);
    }
    else
    {
        auto* gradeTabs =
            new UniformWidthTabWidget(
                UniformWidthTabKind::Grade,
                QStringLiteral("rosterGradeTabBar"),
                m_tabsContainer
                );
        gradeTabs->setSizePolicy(
            QSizePolicy::Expanding,
            QSizePolicy::Maximum
            );
        gradeTabs->setObjectName("rosterGradeTabs");
        createDayFilterControls(gradeTabs);

        for (const ClassTabNavigation::GradeGroup& group
             : navigation.gradeGroups)
        {
            auto* gradePage =
                new QWidget(gradeTabs);
            auto* gradeLayout =
                new QVBoxLayout(gradePage);
            gradeLayout->setContentsMargins(0, 0, 0, 0);
            gradeLayout->setSpacing(8);
            gradeLayout->setAlignment(Qt::AlignTop);

            auto* classTabs =
                new UniformWidthTabWidget(
                    UniformWidthTabKind::Class,
                    QStringLiteral("rosterClassTabBar"),
                    gradePage
                    );
            classTabs->setSizePolicy(
                QSizePolicy::Expanding,
                QSizePolicy::Maximum
                );
            classTabs->setObjectName("rosterClassTabs");

            for (const ClassTabNavigation::ClassTab& tab
                 : group.classes)
            {
                classTabs->addTab(
                    createTabPage(
                        classTabs,
                        tab.classId
                        ),
                    tab.label
                    );
            }

            connectClassTabs(classTabs);
            gradeLayout->addWidget(classTabs);
            gradeTabs->addTab(
                gradePage,
                group.label
                );
        }

        connect(
            gradeTabs,
            &QTabWidget::currentChanged,
            this,
            [this, gradeTabs](int)
            {
                if (m_rebuildingRosterTabs || m_restoringRosterTabs)
                {
                    return;
                }

                setNavigationSelectionVisible(true);
                activateRosterClass(
                    currentClassIdFromTabs(gradeTabs)
                    );
            }
            );

        m_rosterTabs = gradeTabs;
        m_tabsLayout->addWidget(gradeTabs);
    }

    m_tabsContainer->setVisible(
        m_rosterTabs && !m_rosterClasses.isEmpty()
        );
    syncTabWidgetToClass(
        m_rosterTabs,
        selectedClassId
        );
    m_rebuildingRosterTabs = false;
}

void RostersPage::createDayFilterControls(
    UniformWidthTabWidget* tabs
    )
{
    if (!tabs)
    {
        return;
    }

    auto* controls = new QWidget(tabs);
    controls->setObjectName(
        QStringLiteral("rosterDayFilterControls")
        );
    auto* layout = new QHBoxLayout(controls);
    layout->setContentsMargins(DayFilterSpacer, 0, 0, 0);
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
        button->setChecked(dayFilterEnabled(definition.key));
        button->setProperty("rosterDayFilter", definition.key);
        button->setAccessibleName(button->text());
        layout->addWidget(button);

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

    auto* settingsButton = new QPushButton(controls);
    settingsButton->setObjectName(
        QStringLiteral("rosterNavigationSettingsButton")
        );
    UniformWidthTabWidget::configureNavigationSettingsButton(
        settingsButton
        );
    settingsButton->setAccessibleName(tr("Rosters Settings"));
    settingsButton->setToolTip(tr("Rosters Settings"));
    layout->addWidget(settingsButton);

    connect(
        settingsButton,
        &QPushButton::clicked,
        this,
        &RostersPage::openNavigationSettings
        );

    tabs->setCornerWidget(controls, Qt::TopRightCorner);
}

void RostersPage::setDayFilterEnabled(
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

    rebuildRosterTabs(m_currentClassroom.id);
    restoreRosterTabSelection();
}

bool RostersPage::dayFilterEnabled(
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

void RostersPage::openNavigationSettings()
{
    ClassesNavigationSettingsDialog dialog(
        {m_dayFilter.visibilityScope},
        this
        );

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const ClassesNavigationSettingsValues values = dialog.values();
    ClassNavigationPreferences::save(
        m_services ? m_services->dataService() : nullptr,
        values.visibilityScope
        );
    setVisibilityScope(values.visibilityScope);
}

void RostersPage::setScheduleSource(
    ClassTabNavigation::ScheduleSource source
    )
{
    if (m_dayFilter.scheduleSource == source)
    {
        return;
    }

    m_dayFilter.scheduleSource = source;

    if (m_rosterTabs && !m_rebuildingRosterTabs)
    {
        rebuildRosterTabs(m_currentClassroom.id);
        restoreRosterTabSelection();
    }
}

void RostersPage::setVisibilityScope(
    ClassTabNavigation::VisibilityScope visibilityScope
    )
{
    if (m_dayFilter.visibilityScope == visibilityScope)
    {
        return;
    }

    m_dayFilter.visibilityScope = visibilityScope;

    if (m_rosterTabs && !m_rebuildingRosterTabs)
    {
        rebuildRosterTabs(m_currentClassroom.id);
        restoreRosterTabSelection();
    }
}

void RostersPage::setNavigationSelectionVisible(
    bool visible
    )
{
    if (!m_rosterTabs)
    {
        return;
    }

    const QList<UniformWidthTabBar*> tabBars =
        m_rosterTabs->findChildren<UniformWidthTabBar*>();

    for (UniformWidthTabBar* tabBar : tabBars)
    {
        if (tabBar)
        {
            tabBar->setCurrentSelectionVisible(visible);
        }
    }
}

bool RostersPage::activateRosterClass(
    int classId
    )
{
    if (
        classId <= 0
        || m_rebuildingRosterTabs
        || m_restoringRosterTabs
        )
    {
        return false;
    }

    if (classId == m_currentClassroom.id)
    {
        return true;
    }

    if (m_currentClassroom.id > 0 && !saveChanges())
    {
        restoreRosterTabSelection();
        return false;
    }

    const Classroom classroom =
        classroomById(classId);

    if (classroom.id <= 0)
    {
        restoreRosterTabSelection();
        return false;
    }

    m_currentClassroom = classroom;
    m_editor->loadClass(classroom);
    setRosterEditorAvailable(true);
    updateHeaderText();
    return true;
}

void RostersPage::restoreRosterTabSelection()
{
    if (!m_rosterTabs)
    {
        return;
    }

    m_restoringRosterTabs = true;
    syncTabWidgetToClass(
        m_rosterTabs,
        m_currentClassroom.id
        );
    m_restoringRosterTabs = false;
}

void RostersPage::syncTabWidgetToClass(
    QTabWidget* tabs,
    int classId
    )
{
    if (!tabs)
    {
        return;
    }

    for (int index = 0; index < tabs->count(); ++index)
    {
        QWidget* page =
            tabs->widget(index);

        if (page && page->property("class_id").toInt() == classId)
        {
            setNavigationSelectionVisible(true);
            tabs->setCurrentIndex(index);
            return;
        }

        auto* nestedTabs =
            page
                ? page->findChild<QTabWidget*>(
                    QStringLiteral("rosterClassTabs")
                    )
                : nullptr;

        if (!nestedTabs)
        {
            continue;
        }

        for (int childIndex = 0; childIndex < nestedTabs->count(); ++childIndex)
        {
            QWidget* childPage =
                nestedTabs->widget(childIndex);

            if (
                childPage
                && childPage->property("class_id").toInt() == classId
            )
        {
            setNavigationSelectionVisible(true);
            tabs->setCurrentIndex(index);
            nestedTabs->setCurrentIndex(childIndex);
            return;
            }
        }
    }

    if (classId > 0)
    {
        setNavigationSelectionVisible(false);
        return;
    }

    if (tabs->count() > 0)
    {
        setNavigationSelectionVisible(true);
        tabs->setCurrentIndex(0);

        if (auto* nestedTabs =
                tabs->currentWidget()
                    ? tabs->currentWidget()->findChild<QTabWidget*>(
                        QStringLiteral("rosterClassTabs")
                        )
                    : nullptr)
        {
            nestedTabs->setCurrentIndex(0);
        }
    }
}

int RostersPage::currentClassIdFromTabs(
    QTabWidget* tabs
    ) const
{
    if (!tabs || tabs->currentIndex() < 0)
    {
        return -1;
    }

    if (const auto* tabBar = qobject_cast<const UniformWidthTabBar*>(tabs->tabBar());
        tabBar && !tabBar->currentSelectionVisible())
    {
        return -1;
    }

    QWidget* page =
        tabs->currentWidget();
    const int pageClassId =
        page
            ? page->property("class_id").toInt()
            : -1;

    if (pageClassId > 0)
    {
        return pageClassId;
    }

    auto* nestedTabs =
        page
            ? page->findChild<QTabWidget*>(
                QStringLiteral("rosterClassTabs")
                )
            : nullptr;

    if (!nestedTabs || nestedTabs->currentIndex() < 0)
    {
        return -1;
    }

    QWidget* nestedPage =
        nestedTabs->currentWidget();
    return nestedPage
        ? nestedPage->property("class_id").toInt()
        : -1;
}

Classroom RostersPage::classroomById(
    int classId
    ) const
{
    for (const Classroom& classroom : m_rosterClasses)
    {
        if (classroom.id == classId)
        {
            return classroom;
        }
    }

    return {};
}

int RostersPage::firstRosterClassId() const
{
    for (const Classroom& classroom : m_rosterClasses)
    {
        if (classroom.id > 0)
        {
            return classroom.id;
        }
    }

    return -1;
}

void RostersPage::setRosterEditorAvailable(
    bool available
    )
{
    if (m_emptyLabel)
    {
        m_emptyLabel->setVisible(
            !available
            );
    }

    if (m_editor)
    {
        m_editor->setVisible(
            available
            );
        m_editor->setEnabled(
            available
            );
    }
}

void RostersPage::updateHeaderText()
{
    if (!m_titleLabel || !m_subtitleLabel)
    {
        return;
    }

    m_titleLabel->setText(
        tr("Rosters")
        );

    if (m_currentClassroom.id <= 0)
    {
        m_subtitleLabel->setText(
            tr("No classes available")
            );
        return;
    }

    const QString sidebarName =
        sidebarClassDisplayName(
            m_services
                ? m_services->dataService()
                : nullptr,
            m_currentClassroom.id
            );

    if (!sidebarName.isEmpty())
    {
        m_subtitleLabel->setText(
            sidebarName
            );
        return;
    }

    m_subtitleLabel->setText(
        m_currentClassroom.name.trimmed().isEmpty()
            ? tr("Class %1").arg(m_currentClassroom.id)
            : m_currentClassroom.name.trimmed()
        );
}
