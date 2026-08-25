#include "classes_page.h"

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/memory_usage_diagnostics.h"
#include "core/resource_paths.h"
#include "core/utils/sidebar_node_naming.h"
#include "domain/models/class_info.h"
#include "domain/models/teacher.h"
#include "features/classes/class_navigation_preferences.h"
#include "features/classes/ui/class_co_teacher_page.h"
#include "features/classes/ui/class_details_page.h"
#include "features/classes/ui/class_notes_page.h"
#include "features/classes/ui/class_analytics_page.h"
#include "features/schedule/schedule_display_mode_preferences.h"
#include "features/roster/ui/roster_editor_widget.h"
#include "features/speaking_eval/ui/speaking_eval_page.h"
#include "features/speaking_eval/ui/speaking_eval_report_assets_p.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/widgets/navigation_pill_button.h"
#include "ui/shared/widgets/navigation_pill_style.h"
#include "ui/shared/widgets/navigation_tab_widget.h"
#include "ui/shared/widgets/on_screen_keyboard.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <utility>

#include <QFont>
#include <QDir>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QSet>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
constexpr int MinimumGradeFilterGap = 60;

QString allGradesSelectionKey()
{
    return QStringLiteral("__all_grades__");
}

QString classesSectionIdentifier(ClassesSection section)
{
    switch (section)
    {
    case ClassesSection::Details: return QStringLiteral("details");
    case ClassesSection::Roster: return QStringLiteral("roster");
    case ClassesSection::Analytics: return QStringLiteral("analytics");
    case ClassesSection::Evaluations: return QStringLiteral("evaluations");
    case ClassesSection::CoTeacher: return QStringLiteral("co_teacher");
    case ClassesSection::Notes: return QStringLiteral("notes");
    }

    return QStringLiteral("unknown");
}

bool isMiddleSchoolGrade(const QString& grade)
{
    const QString normalizedGrade = grade.trimmed().toUpper();
    return normalizedGrade == QStringLiteral("M1")
        || normalizedGrade == QStringLiteral("M2")
        || normalizedGrade == QStringLiteral("M3");
}

QString classesSectionText(ClassesSection section)
{
    switch (section)
    {
    case ClassesSection::Details: return ClassesPage::tr("Details");
    case ClassesSection::Roster: return ClassesPage::tr("Roster");
    case ClassesSection::Analytics: return ClassesPage::tr("Analytics");
    case ClassesSection::Evaluations: return ClassesPage::tr("Evaluations");
    case ClassesSection::CoTeacher: return ClassesPage::tr("Co-Teacher");
    case ClassesSection::Notes: return ClassesPage::tr("Notes");
    }

    return {};
}

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

    m_showMiddleSchoolAnalyticsAndEvaluations =
        ClassNavigationPreferences::showMiddleSchoolAnalyticsAndEvaluations(
            m_services ? m_services->settingsService() : nullptr
            );

    MemoryUsageDiagnostics::registerMemoryBreakdownProvider(this, this);
    buildUi();
}

bool ClassesPage::openClass(
    int classId,
    ClassesSection section
    )
{
    if (section == ClassesSection::Evaluations)
    {
        if (const Status status = acquireEvaluationResources(); !status)
        {
            return false;
        }
    }

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
        if (m_currentSection == ClassesSection::Evaluations
            && section != ClassesSection::Evaluations)
        {
            releaseEvaluationResources();
        }
        m_classes.clear();
        m_currentClassId = -1;
        m_currentSection = section;
        rebuildClassTabs(-1);
        rebuildSectionTabs();
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
        rebuildSectionTabs();
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
        if (m_currentSection == ClassesSection::Evaluations
            && section != ClassesSection::Evaluations)
        {
            releaseEvaluationResources();
        }
        m_currentClassId = -1;
        m_currentSection = section;
        rebuildSectionTabs();
        setEditorAvailable(false);
        updateHeaderText();
        return true;
    }

    if (m_currentSection == ClassesSection::Evaluations
        && section != ClassesSection::Evaluations)
    {
        releaseEvaluationResources();
    }

    m_currentClassId = classroom.id;
    m_currentSection = section;
    rebuildSectionTabs();
    m_loadedEditorClassIds.clear();
    loadActiveEditor();
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

QString dayFilterButtonText(
    const QString& key
    )
{
    if (key == QStringLiteral("Monday"))
    {
        return ClassesPage::tr("M");
    }
    if (key == QStringLiteral("Tuesday"))
    {
        return ClassesPage::tr("T");
    }
    if (key == QStringLiteral("Wednesday"))
    {
        return ClassesPage::tr("W");
    }
    if (key == QStringLiteral("Thursday"))
    {
        return ClassesPage::tr("Th");
    }
    if (key == QStringLiteral("Friday"))
    {
        return ClassesPage::tr("F");
    }
    return ClassesPage::tr("Wkd");
}

bool ClassesPage::openEvaluation(
    int classId,
    const QString& evaluationName
    )
{
    if (!openClass(classId, ClassesSection::Evaluations))
    {
        return false;
    }

    const Classroom classroom = classroomById(m_currentClassId);
    if (classroom.id <= 0 || evaluationName.trimmed().isEmpty())
    {
        return classroom.id > 0;
    }

    if (m_evaluationsPage)
    {
        m_evaluationsPage->loadEvaluation(classroom, evaluationName);
    }

    return true;
}

int ClassesPage::currentClassId() const
{
    return m_currentClassId;
}

ClassesSection ClassesPage::currentSection() const
{
    return m_currentSection;
}

bool ClassesPage::isEditorInstantiated(
    ClassesSection section
    ) const
{
    switch (section)
    {
    case ClassesSection::Details:
        return m_detailsPage;

    case ClassesSection::Roster:
        return m_rosterEditor;

    case ClassesSection::Analytics:
        return m_analyticsPage;

    case ClassesSection::Evaluations:
        return m_evaluationsPage;

    case ClassesSection::CoTeacher:
        return m_coTeacherPage;

    case ClassesSection::Notes:
        return m_notesPage;
    }

    return false;
}

QList<MemoryBreakdownEntry> ClassesPage::memoryBreakdown() const
{
    const quint64 instantiatedEditors =
        static_cast<quint64>(m_detailsPage != nullptr)
        + static_cast<quint64>(m_rosterEditor != nullptr)
        + static_cast<quint64>(m_analyticsPage != nullptr)
        + static_cast<quint64>(m_evaluationsPage != nullptr)
        + static_cast<quint64>(m_coTeacherPage != nullptr)
        + static_cast<quint64>(m_notesPage != nullptr);

    return {
        {
            QStringLiteral("Classes shell and class list"),
            QStringLiteral("Classes"),
            static_cast<quint64>(m_classes.size()) * sizeof(Classroom),
            static_cast<quint64>(m_classes.size()) + instantiatedEditors,
            QStringLiteral("class records=%1; editors=%2/6; active editor=%3; active class=%4")
                .arg(m_classes.size())
                .arg(instantiatedEditors)
                .arg(classesSectionIdentifier(m_currentSection))
                .arg(m_currentClassId > 0 ? QStringLiteral("loaded") : QStringLiteral("none")),
            true
        }
    };
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
    m_saveMode = mode;

    for (BasePage* editor : {
             static_cast<BasePage*>(m_detailsPage),
             static_cast<BasePage*>(m_rosterEditor),
             static_cast<BasePage*>(m_analyticsPage),
             static_cast<BasePage*>(m_evaluationsPage),
             static_cast<BasePage*>(m_coTeacherPage),
             static_cast<BasePage*>(m_notesPage)
         })
    {
        if (editor)
        {
            editor->setSaveMode(mode);
        }
    }
}

void ClassesPage::refresh()
{
    BasePage::refresh();

    if (isVisible())
    {
        loadClasses();
    }
}

void ClassesPage::refreshNavigationPreferences()
{
    setVisibilityScope(
        ClassNavigationPreferences::load(
            m_services ? m_services->settingsService() : nullptr
            )
        );
    setShowMiddleSchoolAnalyticsAndEvaluations(
        ClassNavigationPreferences::showMiddleSchoolAnalyticsAndEvaluations(
            m_services ? m_services->settingsService() : nullptr
            )
        );
}

void ClassesPage::clearDatabaseState()
{
    releaseEvaluationResources();
    m_classes.clear();
    m_currentClassId = -1;
    m_currentSection = ClassesSection::Details;
    m_selectedGrade.clear();
    m_selectedClassIds.clear();
    m_loadedEditorClassIds.clear();

    rebuildClassTabs(-1);
    rebuildSectionTabs();

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

    if (m_coTeacherPage)
    {
        m_coTeacherPage->clearDatabaseState();
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

    for (BasePage* editor : {
             static_cast<BasePage*>(m_detailsPage),
             static_cast<BasePage*>(m_rosterEditor),
             static_cast<BasePage*>(m_analyticsPage),
             static_cast<BasePage*>(m_evaluationsPage),
             static_cast<BasePage*>(m_coTeacherPage),
             static_cast<BasePage*>(m_notesPage)
         })
    {
        if (editor)
        {
            editor->retranslateUi();
        }
    }

    rebuildSectionTabs();
    rebuildClassTabs(m_currentClassId);
    restoreSelections();
    updateHeaderText();
}

Status ClassesPage::prepareForActivation()
{
    return m_currentSection == ClassesSection::Evaluations
        ? acquireEvaluationResources()
        : Status{};
}

void ClassesPage::releaseFeatureResources()
{
    releaseEvaluationResources();
}

void ClassesPage::setEmbeddedDatabaseOpen(bool databaseOpen)
{
    m_embeddedDatabaseStateSet = true;
    m_embeddedDatabaseOpen = databaseOpen;

    for (BasePage* editor : {
             static_cast<BasePage*>(m_detailsPage),
             static_cast<BasePage*>(m_rosterEditor),
             static_cast<BasePage*>(m_analyticsPage),
             static_cast<BasePage*>(m_evaluationsPage),
             static_cast<BasePage*>(m_coTeacherPage),
             static_cast<BasePage*>(m_notesPage)
         })
    {
        if (editor)
        {
            editor->setDatabaseOpen(databaseOpen);
        }
    }
}

PageOutputCapabilities ClassesPage::outputCapabilities() const
{
    const BasePage* editor = activeEditor();
    return isDatabaseOpen() && editor
        ? editor->outputCapabilities()
        : PageOutputCapabilities{};
}

void ClassesPage::printCurrentPage()
{
    if (BasePage* editor = activeEditor())
    {
        editor->printCurrentPage();
    }
}

void ClassesPage::saveCurrentPageAs()
{
    if (BasePage* editor = activeEditor())
    {
        editor->saveCurrentPageAs();
    }
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
    navigationLayout->setSpacing(NavigationPillStyle::RowSpacing);

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
    navigationLayout->addWidget(m_sectionTabs);
    rebuildSectionTabs();
    contentLayout()->addWidget(m_navigationContainer);

    m_emptyLabel = new QLabel(tr("No classes available"), this);
    m_emptyLabel->setObjectName("pageSubtitle");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setFont(FontManager::getUiFont(12));
    contentLayout()->addWidget(m_emptyLabel, 1);

    m_editorStack = new QStackedWidget(this);
    contentLayout()->addWidget(m_editorStack, 1);

    connect(
        m_koreanKeyboardButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            if (m_currentSection == ClassesSection::Evaluations)
            {
                if (m_evaluationsPage)
                {
                    m_evaluationsPage->showKoreanKeyboard();
                }
            }
            else if (m_onScreenKeyboard)
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
            if (
                m_rebuildingTabs
                || m_rebuildingSectionTabs
                || m_restoringTabs
                || index < 0
                || index >= m_visibleSections.size()
                )
            {
                return;
            }

            activateSection(
                m_visibleSections.at(index)
                );
        }
        );

    setEditorAvailable(false);
    showActiveEditor();
}

void ClassesPage::resizeEvent(QResizeEvent* event)
{
    BasePage::resizeEvent(event);
    scheduleFirstRowLayout();
}

void ClassesPage::hideEvent(QHideEvent* event)
{
    BasePage::hideEvent(event);

    auto* settingsService =
        m_services
            ? m_services->settingsService()
            : nullptr;

    if (
        ClassNavigationPreferences::dayFilterResetPolicy(settingsService)
        == ClassNavigationPreferences::SessionResetPolicy::OnPageLeave
        )
    {
        discardDayFilterState();
    }

    if (
        ClassNavigationPreferences::classSelectionResetPolicy(settingsService)
        == ClassNavigationPreferences::SessionResetPolicy::OnPageLeave
        )
    {
        discardClassSelectionState();
    }
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
    m_dayFilterControls = nullptr;

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
                classService->classInfo(classroom.id).value_or(ClassInfo{});
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

    m_weekendClassesAvailable = hasWeekendClasses(entries);
    if (!m_weekendClassesAvailable)
    {
        m_dayFilter.selectedDays.remove(QStringLiteral("Saturday"));
        m_dayFilter.selectedDays.remove(QStringLiteral("Sunday"));
    }

    if (entries.isEmpty())
    {
        m_selectedGrade.clear();
        m_selectedClassIds.clear();
    }

    QSet<int> availableClassIds;
    for (const Classroom& classroom : std::as_const(m_classes))
    {
        if (classroom.id > 0)
        {
            availableClassIds.insert(classroom.id);
        }
    }
    for (auto iterator = m_selectedClassIds.begin();
         iterator != m_selectedClassIds.end();)
    {
        if (!availableClassIds.contains(iterator.value()))
        {
            iterator = m_selectedClassIds.erase(iterator);
        }
        else
        {
            ++iterator;
        }
    }

    const ClassTabNavigation::Model navigation =
        ClassTabNavigation::build(
            entries,
            ClassTabNavigation::GroupingPolicy::AlwaysGradeGrouped,
            m_dayFilter
            );

    if (
        m_selectedGrade.isNull()
        && !navigation.allClasses.isEmpty()
        )
    {
        m_selectedGrade = allGradesSelectionKey();
    }

    auto* gradeTabs =
        new NavigationTabWidget(
            NavigationTabKind::Grade,
            QStringLiteral("classesGradeTabBar"),
            m_classTabsContainer
            );
    gradeTabs->setObjectName("classesGradeTabs");
    createDayFilterControls(gradeTabs);

    const auto connectClassTabs =
        [this](NavigationTabWidget* tabs, const QString& grade)
        {
            connect(
                tabs,
                &NavigationTabWidget::currentChanged,
                this,
                [this, tabs, grade](int)
                {
                    if (m_rebuildingTabs || m_restoringTabs)
                    {
                        return;
                    }

                    setNavigationSelectionVisible(true);
                    const int classId = currentClassIdFromTabs(tabs);
                    rememberClassSelection(grade, classId);
                    m_selectedGrade = grade;
                    activateClass(classId);
                }
                );
        };

    for (const ClassTabNavigation::GradeGroup& group
         : navigation.gradeGroups)
    {
        auto* gradePage = new QWidget(gradeTabs);
        gradePage->setProperty("classGrade", group.grade);
        auto* gradeLayout = new QVBoxLayout(gradePage);
        gradeLayout->setContentsMargins(0, 0, 0, 0);
        gradeLayout->setSpacing(NavigationPillStyle::RowSpacing);
        gradeLayout->setAlignment(Qt::AlignTop);

        auto* classTabs =
            new NavigationTabWidget(
                NavigationTabKind::Class,
                QStringLiteral("classesLevelTabBar"),
                gradePage
                );
        classTabs->setObjectName("classesLevelTabs");
        // These tab pages are placeholders; the actual content is shown in
        // m_editorStack, so they must not add a second gap below this row.
        classTabs->setPageSpacing(0);

        for (const ClassTabNavigation::ClassTab& classTab
             : group.classes)
        {
            classTabs->addTab(
                tabPage(classTabs, classTab.classId),
                classTab.label
                );
        }

        connectClassTabs(classTabs, group.grade);
        gradeLayout->addWidget(classTabs);
        gradeTabs->addTab(gradePage, group.label);
    }

    if (!navigation.allClasses.isEmpty())
    {
        auto* allGradesPage = new QWidget(gradeTabs);
        allGradesPage->setProperty(
            "classGrade",
            allGradesSelectionKey()
            );
        auto* allGradesLayout = new QVBoxLayout(allGradesPage);
        allGradesLayout->setContentsMargins(0, 0, 0, 0);
        allGradesLayout->setSpacing(NavigationPillStyle::RowSpacing);
        allGradesLayout->setAlignment(Qt::AlignTop);

        auto* allClassesTabs =
            new NavigationTabWidget(
                NavigationTabKind::Class,
                QStringLiteral("classesLevelTabBar"),
                allGradesPage
                );
        allClassesTabs->setObjectName("classesLevelTabs");
        allClassesTabs->setPageSpacing(0);

        for (const ClassTabNavigation::ClassTab& classTab
             : navigation.allClasses)
        {
            allClassesTabs->addTab(
                tabPage(allClassesTabs, classTab.classId),
                classTab.label
                );
        }

        connectClassTabs(allClassesTabs, allGradesSelectionKey());
        allGradesLayout->addWidget(allClassesTabs);
        gradeTabs->addTab(allGradesPage, tr("All"));
    }

    connect(
        gradeTabs,
        &NavigationTabWidget::currentChanged,
        this,
        [this, gradeTabs](int gradeIndex)
        {
            if (m_rebuildingTabs || m_restoringTabs)
            {
                return;
            }

            QWidget* gradePage = gradeTabs->widget(gradeIndex);
            const QString grade =
                gradePage
                    ? gradePage->property("classGrade").toString()
                    : QString();
            m_selectedGrade = grade;
            setNavigationSelectionVisible(true);
            const int classId = currentClassIdFromTabs(gradeTabs);
            rememberClassSelection(grade, classId);
            activateClass(classId);
        }
        );

    m_classTabs = gradeTabs;
    m_classTabsLayout->addWidget(gradeTabs);
    syncTabsToClass(selectedClassId);
    m_rebuildingTabs = false;
    scheduleFirstRowLayout();
}

void ClassesPage::rebuildSectionTabs()
{
    if (!m_sectionTabs)
    {
        return;
    }

    bool hideMiddleSchoolAnalyticsAndEvaluations = false;
    const Classroom classroom = classroomById(m_currentClassId);
    auto* classService =
        m_services
            ? m_services->classService()
            : nullptr;
    if (
        !m_showMiddleSchoolAnalyticsAndEvaluations
        && classroom.id > 0
        && classService
        && classService->isAvailable()
        )
    {
        hideMiddleSchoolAnalyticsAndEvaluations = isMiddleSchoolGrade(
            classService->classInfo(classroom.id)
                .value_or(ClassInfo{})
                .classGrade
            );
    }

    QList<ClassesSection> visibleSections{
        ClassesSection::Details,
        ClassesSection::Roster
    };
    if (!hideMiddleSchoolAnalyticsAndEvaluations)
    {
        visibleSections.append(ClassesSection::Analytics);
        visibleSections.append(ClassesSection::Evaluations);
    }
    visibleSections.append(ClassesSection::CoTeacher);
    visibleSections.append(ClassesSection::Notes);

    m_rebuildingSectionTabs = true;
    while (m_sectionTabs->count() > 0)
    {
        QWidget* page = m_sectionTabs->widget(0);
        m_sectionTabs->removeTab(0);
        delete page;
    }

    m_visibleSections = visibleSections;
    for (const ClassesSection section : std::as_const(m_visibleSections))
    {
        m_sectionTabs->addTab(
            tabPage(m_sectionTabs),
            classesSectionText(section)
            );
    }
    m_sectionTabs->setCurrentIndex(
        m_visibleSections.indexOf(m_currentSection)
        );
    m_rebuildingSectionTabs = false;
}

void ClassesPage::createDayFilterControls(
    NavigationTabWidget* gradeTabs
    )
{
    if (!gradeTabs)
    {
        return;
    }

    m_dayFilterControls = new QWidget(gradeTabs);
    m_dayFilterControls->setObjectName(
        QStringLiteral("classesDayFilterControls")
        );
    auto* layout = new QHBoxLayout(m_dayFilterControls);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setAlignment(Qt::AlignTop);
    layout->setSpacing(NavigationPillStyle::Gap);

    for (const DayFilterButtonDefinition& definition
         : dayFilterButtonDefinitions())
    {
        auto* button = new NavigationPillButton(m_dayFilterControls);
        button->setObjectName(definition.objectName);
        button->setText(dayFilterButtonText(definition.key));
        button->setFixedWidth(button->sizeHint().width());
        button->setCheckable(true);
        button->setChecked(
            dayFilterEnabled(definition.key)
            );
        button->setProperty(
            "classesDayFilter",
            definition.key
            );
        button->setAccessibleName(button->text());
        button->setVisible(
            definition.key != QStringLiteral("Wkend")
            || m_weekendClassesAvailable
            );
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

    m_dayFilterControls->setSizePolicy(
        QSizePolicy::Fixed,
        QSizePolicy::Minimum
        );
    m_dayFilterControls->setFixedWidth(layout->sizeHint().width());
    gradeTabs->setTrailingWidget(m_dayFilterControls);
    gradeTabs->tabStrip()->setTrailingMinimumGap(MinimumGradeFilterGap);
}

void ClassesPage::updateFirstRowLayout()
{
    if (
        m_updatingFirstRowLayout
        || !m_classTabs
        || !m_dayFilterControls
        )
    {
        return;
    }

    NavigationTabStrip* gradeStrip = m_classTabs->tabStrip();
    if (!gradeStrip)
    {
        return;
    }

    m_updatingFirstRowLayout = true;

    gradeStrip->setVisibleTabCount(gradeStrip->count());
    gradeStrip->refreshLayout();

    const int availableWidth = gradeStrip->width();
    const int filterGroupWidth =
        MinimumGradeFilterGap + m_dayFilterControls->sizeHint().width();
    int visibleGradeCount = gradeStrip->count();
    while (
        visibleGradeCount > 1
        && gradeStrip->tabGroupWidth(visibleGradeCount)
            + filterGroupWidth > availableWidth
        )
    {
        --visibleGradeCount;
    }

    gradeStrip->setVisibleTabCount(visibleGradeCount);
    gradeStrip->refreshLayout();
    m_updatingFirstRowLayout = false;
}

void ClassesPage::scheduleFirstRowLayout()
{
    if (m_firstRowLayoutQueued)
    {
        return;
    }

    m_firstRowLayoutQueued = true;
    QTimer::singleShot(
        0,
        this,
        [this]
        {
            m_firstRowLayoutQueued = false;
            updateFirstRowLayout();
        }
        );
}

bool ClassesPage::hasWeekendClasses(
    const QList<ClassTabNavigation::ClassEntry>& entries
    ) const
{
    const auto includesWeekend = [](const QList<ClassTime>& times)
    {
        for (const ClassTime& time : times)
        {
            const QString day = time.day.trimmed().toCaseFolded();
            if (
                day == QStringLiteral("saturday")
                || day == QStringLiteral("sunday")
                )
            {
                return true;
            }
        }

        return false;
    };

    for (const ClassTabNavigation::ClassEntry& entry : entries)
    {
        if (
            includesWeekend(entry.regularTimes)
            || includesWeekend(entry.intensiveTimes)
            )
        {
            return true;
        }
    }

    return false;
}

void ClassesPage::rememberClassSelection(
    const QString& grade,
    int classId
    )
{
    if (classId > 0)
    {
        m_selectedClassIds.insert(grade, classId);
    }
}

int ClassesPage::rememberedClassId(const QString& grade) const
{
    return m_selectedClassIds.value(grade, -1);
}

void ClassesPage::discardClassSelectionState()
{
    m_selectedClassIds.clear();
    m_currentClassId = -1;
}

void ClassesPage::discardDayFilterState()
{
    m_dayFilter.selectedDays.clear();
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

void ClassesPage::setShowMiddleSchoolAnalyticsAndEvaluations(
    bool show
    )
{
    if (m_showMiddleSchoolAnalyticsAndEvaluations == show)
    {
        return;
    }

    m_showMiddleSchoolAnalyticsAndEvaluations = show;
    rebuildSectionTabs();

    if (m_currentClassId > 0)
    {
        loadActiveEditor();
        restoreSelections();
        updateHeaderText();
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
    rebuildSectionTabs();
    loadActiveEditor();
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
        if (section == ClassesSection::Evaluations)
        {
            if (const Status status = acquireEvaluationResources(); !status)
            {
                return false;
            }
        }
        loadActiveEditor();
        return true;
    }

    if (!commitActiveEditor())
    {
        restoreSelections();
        return false;
    }

    if (section == ClassesSection::Evaluations)
    {
        if (const Status status = acquireEvaluationResources(); !status)
        {
            restoreSelections();
            return false;
        }
    }

    if (m_currentSection == ClassesSection::Evaluations)
    {
        releaseEvaluationResources();
    }

    m_currentSection = section;
    restoreSelections();
    loadActiveEditor();
    emit outputCapabilitiesChanged();
    return true;
}

bool ClassesPage::commitActiveEditor()
{
    BasePage* editor = activeEditor();
    return !editor
        || !editor->hasUnsavedChanges()
        || editor->saveChanges();
}

BasePage* ClassesPage::ensureEditor(
    ClassesSection section
    )
{
    if (!m_editorStack)
    {
        return nullptr;
    }

    BasePage* editor = nullptr;

    switch (section)
    {
    case ClassesSection::Details:
        if (!m_detailsPage)
        {
            m_detailsPage =
                new ClassDetailsPage(m_services, true, m_editorStack);
            connect(
                m_detailsPage,
                &ClassDetailsPage::classInfoSaved,
                this,
                &ClassesPage::handleClassInfoSaved
                );
        }
        editor = m_detailsPage;
        break;

    case ClassesSection::Roster:
        if (!m_rosterEditor)
        {
            m_rosterEditor =
                new RosterEditorWidget(m_services, true, m_editorStack);
            m_rosterEditor->setBottomKeyboardButtonVisible(false);
            connect(
                m_rosterEditor,
                &BasePage::outputCapabilitiesChanged,
                this,
                [this]()
                {
                    if (activeEditor() == m_rosterEditor)
                    {
                        emit outputCapabilitiesChanged();
                    }
                }
                );
        }
        editor = m_rosterEditor;
        break;

    case ClassesSection::Analytics:
        if (!m_analyticsPage)
        {
            m_analyticsPage =
                new ClassAnalyticsPage(m_services, true, m_editorStack);
        }
        editor = m_analyticsPage;
        break;

    case ClassesSection::Evaluations:
        if (!m_evaluationsPage)
        {
            m_evaluationsPage =
                new SpeakingEvalPage(m_services, true, m_editorStack);
            connect(
                m_evaluationsPage,
                &BasePage::outputCapabilitiesChanged,
                this,
                [this]()
                {
                    if (activeEditor() == m_evaluationsPage)
                    {
                        emit outputCapabilitiesChanged();
                    }
                }
                );
        }
        editor = m_evaluationsPage;
        break;

    case ClassesSection::CoTeacher:
        if (!m_coTeacherPage)
        {
            m_coTeacherPage =
                new ClassCoTeacherPage(m_services, true, m_editorStack);
            connect(
                m_coTeacherPage,
                &ClassCoTeacherPage::classInfoSaved,
                this,
                &ClassesPage::handleClassInfoSaved
                );
        }
        editor = m_coTeacherPage;
        break;

    case ClassesSection::Notes:
        if (!m_notesPage)
        {
            m_notesPage =
                new ClassNotesPage(m_services, true, m_editorStack);
        }
        editor = m_notesPage;
        break;
    }

    if (!editor)
    {
        return nullptr;
    }

    if (m_editorStack->indexOf(editor) < 0)
    {
        m_editorStack->addWidget(editor);
        applyEditorState(editor);
    }

    return editor;
}

Status ClassesPage::acquireEvaluationResources()
{
    if (m_evaluationResourceLease.isValid())
    {
        return {};
    }

    auto lease = ResourcePaths::Templates::acquireSpeakingEval();
    if (!lease)
    {
        // Focused report tests provide the legacy resource tree directly.
        if (QDir(QStringLiteral(":/assets/templates/speaking-eval")).exists())
        {
            return {};
        }
        return std::unexpected(lease.error());
    }

    m_evaluationResourceLease = std::move(*lease);
    return {};
}

void ClassesPage::releaseEvaluationResources()
{
    if (!m_evaluationResourceLease.isValid())
    {
        return;
    }

    releaseSpeakingEvalTemplateAssets();
    m_evaluationResourceLease.reset();
}

void ClassesPage::loadActiveEditor()
{
    const Classroom classroom = classroomById(m_currentClassId);

    if (classroom.id <= 0)
    {
        showActiveEditor();
        return;
    }

    BasePage* editor = ensureEditor(m_currentSection);

    if (!editor)
    {
        return;
    }

    const int sectionKey = static_cast<int>(m_currentSection);

    if (m_loadedEditorClassIds.value(sectionKey, -1) != classroom.id)
    {
        switch (m_currentSection)
        {
        case ClassesSection::Details:
            m_detailsPage->loadClass(classroom);
            break;

        case ClassesSection::Roster:
            m_rosterEditor->loadClass(classroom);
            break;

        case ClassesSection::Analytics:
            m_analyticsPage->loadClass(classroom);
            break;

        case ClassesSection::Evaluations:
            m_evaluationsPage->loadEvaluation(classroom, {});
            break;

        case ClassesSection::CoTeacher:
            m_coTeacherPage->loadClass(classroom);
            break;

        case ClassesSection::Notes:
            m_notesPage->loadClass(classroom);
            break;
        }

        m_loadedEditorClassIds.insert(sectionKey, classroom.id);
    }

    showActiveEditor();
}

void ClassesPage::restoreSelections()
{
    m_restoringTabs = true;
    syncTabsToClass(m_currentClassId);

    if (m_sectionTabs)
    {
        m_sectionTabs->setCurrentIndex(
            m_visibleSections.indexOf(m_currentSection)
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

    int selectedGradeIndex = -1;
    int selectedClassIndex = -1;
    int fallbackGradeIndex = -1;
    int allGradesIndex = -1;
    const QString allGradesKey = allGradesSelectionKey();

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
        if (!gradePage || !classTabs)
        {
            continue;
        }

        const QString grade = gradePage->property("classGrade").toString();
        if (grade == allGradesKey)
        {
            allGradesIndex = gradeIndex;
        }
        if (
            !m_selectedGrade.isNull()
            && grade == m_selectedGrade
            )
        {
            fallbackGradeIndex = gradeIndex;
        }

        const int rememberedId = rememberedClassId(grade);
        int rememberedIndex = -1;

        for (int classIndex = 0;
             classIndex < classTabs->count();
             ++classIndex)
        {
            QWidget* classPage = classTabs->widget(classIndex);
            const int candidateId =
                classPage
                    ? classPage->property("class_id").toInt()
                    : -1;

            if (candidateId == rememberedId)
            {
                rememberedIndex = classIndex;
            }

            if (
                candidateId == classId
                && (
                    grade != allGradesKey
                    || m_selectedGrade == allGradesKey
                    )
                )
            {
                selectedGradeIndex = gradeIndex;
                selectedClassIndex = classIndex;
            }
        }

        if (rememberedIndex < 0 && classTabs->count() > 0)
        {
            rememberedIndex = 0;
            QWidget* firstClassPage = classTabs->widget(rememberedIndex);
            rememberClassSelection(
                grade,
                firstClassPage
                    ? firstClassPage->property("class_id").toInt()
                    : -1
                );
        }

        if (rememberedIndex >= 0)
        {
            classTabs->setCurrentIndex(rememberedIndex);
        }
    }

    if (selectedGradeIndex >= 0 && selectedClassIndex >= 0)
    {
        QWidget* gradePage = m_classTabs->widget(selectedGradeIndex);
        auto* classTabs =
            gradePage
                ? gradePage->findChild<NavigationTabWidget*>(
                    QStringLiteral("classesLevelTabs")
                    )
                : nullptr;
        m_selectedGrade = gradePage->property("classGrade").toString();
        rememberClassSelection(m_selectedGrade, classId);
        setNavigationSelectionVisible(true);
        m_classTabs->setCurrentIndex(selectedGradeIndex);
        if (classTabs)
        {
            classTabs->setCurrentIndex(selectedClassIndex);
        }
        return;
    }

    if (m_classTabs->count() <= 0)
    {
        setNavigationSelectionVisible(false);
        return;
    }

    if (fallbackGradeIndex < 0)
    {
        fallbackGradeIndex =
            allGradesIndex >= 0
            ? allGradesIndex
            : 0;
        QWidget* gradePage = m_classTabs->widget(fallbackGradeIndex);
        m_selectedGrade = gradePage
            ? gradePage->property("classGrade").toString()
            : QString();
    }

    m_classTabs->setCurrentIndex(fallbackGradeIndex);
    setNavigationSelectionVisible(classId <= 0);
    m_classTabs->setSelectionVisible(true);
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

    case ClassesSection::CoTeacher:
        return m_coTeacherPage;

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

void ClassesPage::applyEditorState(
    BasePage* editor
    )
{
    if (!editor)
    {
        return;
    }

    editor->setSaveMode(m_saveMode);

    if (m_embeddedDatabaseStateSet)
    {
        editor->setDatabaseOpen(m_embeddedDatabaseOpen);
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

    const ClassInfo info = classService->classInfo(classroom.id).value_or(ClassInfo{});
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
        rebuildSectionTabs();
        restoreSelections();
        updateHeaderText();
    }

    emit classInfoSaved(classId);
}
