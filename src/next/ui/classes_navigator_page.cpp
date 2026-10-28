#include "next/ui/classes_navigator_page.h"

#include "domain/rules/schedule_value_parser.h"
#include "next/application/positive_decimal_id_validation.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <set>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

QString translated(const char* source)
{
    return QCoreApplication::translate("NextClassesPage", source);
}

QString errorMessage(const Domain::OperationError& error)
{
    return QString::fromUtf8(
        error.message.data(),
        static_cast<qsizetype>(error.message.size())
        );
}

QString groupMapKey(const QString& key)
{
    return key.toStdString().empty()
        ? QString{}
        : key;
}

bool containsId(
    const Application::ClassesListSnapshot& classes,
    const Domain::ClassId& classId
    )
{
    return std::any_of(classes.classes.cbegin(), classes.classes.cend(),
        [&classId](const auto& classroom) { return classroom.classId == classId; });
}

bool hasWeekend(
    const Application::ClassesNavigationSnapshot& navigation
    )
{
    for (const auto& entry : navigation.classes)
    {
        if (!entry.details)
        {
            continue;
        }
        const auto includesWeekend = [](const auto& schedule) {
            return std::any_of(schedule.cbegin(), schedule.cend(), [](const auto& row) {
                const QString day = QString::fromStdU16String(row.day).trimmed();
                const auto parsed = ScheduleValueParser::parseWeekday(day);
                return parsed && parsed->text == day
                    && (parsed->value == ScheduleValueParser::Weekday::Saturday
                        || parsed->value == ScheduleValueParser::Weekday::Sunday);
            });
        };
        if (includesWeekend(entry.details.value().regularSchedule)
            || includesWeekend(entry.details.value().intensiveSchedule))
        {
            return true;
        }
    }
    return false;
}

const std::array<const char*, 6>& daySources()
{
    static constexpr std::array<const char*, 6> values{
        "Mon", "Tues", "Wed", "Thurs", "Fri", "Weekend"
    };
    return values;
}

const std::array<QString, 6>& dayKeys()
{
    static const std::array<QString, 6> values{
        QStringLiteral("monday"),
        QStringLiteral("tuesday"),
        QStringLiteral("wednesday"),
        QStringLiteral("thursday"),
        QStringLiteral("friday"),
        QStringLiteral("weekend")
    };
    return values;
}

QString dayObjectName(const int index)
{
    static const std::array<QString, 6> names{
        QStringLiteral("classesNavigatorMondayFilter"),
        QStringLiteral("classesNavigatorTuesdayFilter"),
        QStringLiteral("classesNavigatorWednesdayFilter"),
        QStringLiteral("classesNavigatorThursdayFilter"),
        QStringLiteral("classesNavigatorFridayFilter"),
        QStringLiteral("classesNavigatorWeekendFilter")
    };
    return index >= 0 && index < static_cast<int>(names.size())
        ? names[static_cast<std::size_t>(index)]
        : QString{};
}

} // namespace

ClassesNavigatorPage::ClassesNavigatorPage(
    const PageContext& context,
    ClassesNavigatorPageActions actions,
    QWidget* parent
    )
    : Page(context, parent)
    , m_actions(std::move(actions))
    , m_model(new ClassesNavigatorModel(this))
{
    setObjectName(QStringLiteral("classesNavigatorPage"));
    buildUi();
}

ClassesNavigatorModel* ClassesNavigatorPage::model() const noexcept
{
    return m_model;
}

QTabBar* ClassesNavigatorPage::gradeTabs() const noexcept
{
    return m_gradeTabs;
}

QListView* ClassesNavigatorPage::classList() const noexcept
{
    return m_classList;
}

QComboBox* ClassesNavigatorPage::scheduleSourceCombo() const noexcept
{
    return m_scheduleSource;
}

QComboBox* ClassesNavigatorPage::visibilityScopeCombo() const noexcept
{
    return m_visibilityScope;
}

const std::optional<Domain::ClassId>&
ClassesNavigatorPage::selectedClassId() const noexcept
{
    return m_selectedClassId;
}

bool ClassesNavigatorPage::weekendAvailable() const noexcept
{
    return m_weekendAvailable;
}

bool ClassesNavigatorPage::navigationMetadataAvailable() const noexcept
{
    return m_navigation.has_value();
}

bool ClassesNavigatorPage::isActive() const noexcept
{
    return m_active;
}

QPushButton* ClassesNavigatorPage::addClassButton() const noexcept
{
    return m_addClassButton;
}

QPushButton* ClassesNavigatorPage::deleteClassButton() const noexcept
{
    return m_deleteClassButton;
}

SelectedClassEditor* ClassesNavigatorPage::selectedClassEditor() const noexcept
{
    return m_selectedClassEditor;
}

PageOperationResult<void> ClassesNavigatorPage::onActivating()
{
    m_active = true;
    static_cast<void>(refreshFromStore());
    return {};
}

void ClassesNavigatorPage::onSuspending() noexcept
{
    if (const auto path = activePath(); path && m_actions.readPreferences)
    {
        try
        {
            const Application::ClassesNavigatorPreferences preferences =
                m_actions.readPreferences(*path);
            if (preferences.dayFilterResetPolicy)
            {
                m_preferences.dayFilterResetPolicy =
                    preferences.dayFilterResetPolicy;
            }
            if (preferences.classSelectionResetPolicy)
            {
                m_preferences.classSelectionResetPolicy =
                    preferences.classSelectionResetPolicy;
            }
        }
        catch (...)
        {
            // A preference read failure keeps the current state, matching the
            // existing navigator's recoverable preference behavior.
        }
    }

    if (m_preferences.dayFilterResetPolicy
        && *m_preferences.dayFilterResetPolicy
            == Application::ClassDayFilterResetPolicy::OnPageLeave)
    {
        discardDayFilterState();
    }
    if (m_preferences.classSelectionResetPolicy
        && *m_preferences.classSelectionResetPolicy
            == Application::ClassSelectionResetPolicy::OnPageLeave)
    {
        discardSelectionState();
        m_preferredGroupKey.clear();
        notifyActiveClassSelection();
    }

    m_model->clear();
    m_classes = {};
    m_navigation.reset();
    clearSelectedClassEditor();
    m_weekendAvailable = false;
    m_active = false;
    m_status = Status::Ready;
    updateStatus();
    updateControls();
}

bool ClassesNavigatorPage::hasUnsavedChanges() const noexcept
{
    return m_selectedClassEditor
        && m_selectedClassEditor->hasUnsavedChanges();
}

PageOperationResult<void> ClassesNavigatorPage::saveChanges()
{
    if (!m_selectedClassEditor)
    {
        return {};
    }
    QString error;
    if (!m_selectedClassEditor->saveChanges(&error))
    {
        return std::unexpected(PageOperationError{
            error.isEmpty() ? translated("Class changes could not be saved.") : error
        });
    }
    return {};
}

PageOperationResult<void> ClassesNavigatorPage::discardChanges()
{
    if (!m_selectedClassEditor)
    {
        return {};
    }
    QString error;
    if (!m_selectedClassEditor->discardChanges(&error))
    {
        return std::unexpected(PageOperationError{
            error.isEmpty() ? translated("Class changes could not be reloaded.") : error
        });
    }
    return {};
}

bool ClassesNavigatorPage::refreshFromStore()
{
    const auto path = activePath();
    if (!path || path->trimmed().isEmpty())
    {
        clearForClosedWorkspace();
        return false;
    }

    QString identity;
    if (m_actions.workspaceIdentity)
    {
        try
        {
            identity = m_actions.workspaceIdentity();
        }
        catch (...)
        {
            identity.clear();
        }
    }
    if (identity.isEmpty())
    {
        identity = *path;
    }
    if (m_workspaceIdentity && *m_workspaceIdentity != identity)
    {
        discardSelectionState();
        notifyActiveClassSelection();
        discardDayFilterState();
        m_preferredGroupKey.clear();
        clearSelectedClassEditor();
        m_classes = {};
        m_navigation.reset();
        m_model->clear();
        m_weekendAvailable = false;
    }
    m_workspaceIdentity = identity;

    if (m_actions.readPreferences)
    {
        try
        {
            const auto preferences = m_actions.readPreferences(*path);
            if (preferences.scheduleDisplayMode)
            {
                m_filter.scheduleSource =
                    *preferences.scheduleDisplayMode
                            == Application::ScheduleDisplayMode::Intensive
                        ? Application::ClassDayScheduleSource::Intensive
                        : Application::ClassDayScheduleSource::Regular;
            }
            if (preferences.visibilityScope)
            {
                m_filter.visibilityScope =
                    *preferences.visibilityScope
                            == Application::ClassVisibilityScope::AllClasses
                        ? Application::ClassDayVisibilityScope::AllClasses
                        : Application::ClassDayVisibilityScope::ActiveSchedule;
            }
            if (preferences.dayFilterResetPolicy)
            {
                m_preferences.dayFilterResetPolicy =
                    preferences.dayFilterResetPolicy;
            }
            if (preferences.classSelectionResetPolicy)
            {
                m_preferences.classSelectionResetPolicy =
                    preferences.classSelectionResetPolicy;
            }
        }
        catch (...)
        {
            // Keep the last successfully read preferences and continue the
            // retryable class navigation load.
        }
    }

    const QSignalBlocker sourceSignals(m_scheduleSource);
    const QSignalBlocker visibilitySignals(m_visibilityScope);
    m_scheduleSource->setCurrentIndex(
        m_scheduleSource->findData(
            m_filter.scheduleSource == Application::ClassDayScheduleSource::Intensive
                ? static_cast<int>(Application::ScheduleDisplayMode::Intensive)
                : static_cast<int>(Application::ScheduleDisplayMode::Regular)
            )
        );
    m_visibilityScope->setCurrentIndex(
        m_visibilityScope->findData(
            m_filter.visibilityScope == Application::ClassDayVisibilityScope::AllClasses
                ? static_cast<int>(Application::ClassVisibilityScope::AllClasses)
                : static_cast<int>(Application::ClassVisibilityScope::ActiveSchedule)
            )
        );

    Application::ClassesListReadResult loaded = [&]() {
        if (!m_actions.readClasses)
        {
            return Application::ClassesListReadResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "The Classes list read action is unavailable.",
                .recoverable = true
            });
        }
        try
        {
            return m_actions.readClasses(*path);
        }
        catch (...)
        {
            return Application::ClassesListReadResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "The class list could not be loaded.",
                .recoverable = true
            });
        }
    }();
    if (!loaded)
    {
        m_classes = {};
        m_navigation.reset();
        m_model->clear();
        m_selectedClassId.reset();
        clearSelectedClassEditor();
        notifyActiveClassSelection();
        m_weekendAvailable = false;
        m_lastError = errorMessage(loaded.error());
        m_status = Status::ListError;
        updateStatus();
        updateControls();
        return false;
    }

    QString invalidListError;
    if (!validateListSnapshot(loaded.value(), &invalidListError))
    {
        m_classes = {};
        m_navigation.reset();
        m_model->clear();
        m_selectedClassId.reset();
        clearSelectedClassEditor();
        notifyActiveClassSelection();
        m_weekendAvailable = false;
        m_lastError = invalidListError;
        m_status = Status::ListError;
        updateStatus();
        updateControls();
        return false;
    }

    m_classes = std::move(loaded.value());
    for (auto iterator = m_selectedByGroup.begin();
         iterator != m_selectedByGroup.end();)
    {
        if (!containsId(m_classes, iterator->second))
        {
            iterator = m_selectedByGroup.erase(iterator);
        }
        else
        {
            ++iterator;
        }
    }
    if (m_selectedClassId && !containsId(m_classes, *m_selectedClassId))
    {
        m_selectedClassId.reset();
    }
    m_navigation.reset();
    m_weekendAvailable = false;
    m_lastError.clear();

    if (!m_classes.classes.empty())
    {
        Application::ClassesNavigationSnapshotQuery query;
        query.classes.reserve(m_classes.classes.size());
        for (const auto& classroom : m_classes.classes)
        {
            query.classes.push_back({
                .classId = classroom.classId,
                .className = classroom.className
            });
        }

        Application::ClassesNavigationSnapshotResult navigation = [&]() {
            if (!m_actions.readNavigation)
            {
                return Application::ClassesNavigationSnapshotResult::failure({
                    .code = Domain::ErrorCode::Technical,
                    .message = "The class navigation metadata read action is unavailable.",
                    .recoverable = true
                });
            }
            try
            {
                return m_actions.readNavigation(*path, query);
            }
            catch (...)
            {
                return Application::ClassesNavigationSnapshotResult::failure({
                    .code = Domain::ErrorCode::Technical,
                    .message = "Class navigation metadata could not be loaded.",
                    .recoverable = true
                });
            }
        }();
        QString invalidNavigationError;
        if (navigation && validateNavigationSnapshot(
                m_classes, navigation.value(), &invalidNavigationError))
        {
            m_navigation = std::move(navigation.value());
            m_weekendAvailable = hasWeekend(*m_navigation);
        }
        else
        {
            m_lastError = navigation
                ? invalidNavigationError
                : errorMessage(navigation.error());
        }
    }
    else
    {
        m_navigation = Application::ClassesNavigationSnapshot{};
    }

    if (!m_weekendAvailable)
    {
        auto& selectedDays = m_filter.selectedDayKeys;
        selectedDays.erase(
            std::remove(selectedDays.begin(), selectedDays.end(), "weekend"),
            selectedDays.end()
            );
    }

    std::optional<Domain::ClassId> preferredClass = m_selectedClassId;
    if (m_actions.activeClassSelection)
    {
        try
        {
            const auto sharedSelection = m_actions.activeClassSelection();
            if (sharedSelection && containsId(m_classes, *sharedSelection))
            {
                preferredClass = sharedSelection;
            }
        }
        catch (...)
        {
            // A failed in-memory selection read leaves the navigator's own
            // preferred class intact.
        }
    }
    const QString preferredGroup = m_preferredGroupKey;
    rebuildProjection(preferredClass, preferredGroup);

    if (m_classes.classes.empty())
    {
        m_status = Status::Empty;
    }
    else if (!m_navigation)
    {
        m_status = Status::NavigationError;
    }
    else if (m_model->visibleClassCount() == 0)
    {
        m_status = Status::NoMatches;
    }
    else
    {
        m_status = Status::Ready;
    }
    updateStatus();
    updateControls();
    return true;
}

void ClassesNavigatorPage::clearForWorkspaceChange()
{
    m_classes = {};
    m_navigation.reset();
    m_workspaceIdentity.reset();
    m_selectedClassId.reset();
    clearSelectedClassEditor();
    notifyActiveClassSelection();
    m_selectedByGroup.clear();
    m_preferredGroupKey.clear();
    m_filter.selectedDayKeys.clear();
    m_filter.scheduleSource = Application::ClassDayScheduleSource::Regular;
    m_filter.visibilityScope = Application::ClassDayVisibilityScope::ActiveSchedule;
    m_weekendAvailable = false;
    m_lastError.clear();
    m_status = Status::WorkspaceChanged;
    m_model->clear();
    updateStatus();
    updateControls();
}

void ClassesNavigatorPage::clearForClosedWorkspace()
{
    m_classes = {};
    m_navigation.reset();
    m_workspaceIdentity.reset();
    m_selectedClassId.reset();
    clearSelectedClassEditor();
    notifyActiveClassSelection();
    m_selectedByGroup.clear();
    m_preferredGroupKey.clear();
    m_filter.selectedDayKeys.clear();
    m_filter.scheduleSource = Application::ClassDayScheduleSource::Regular;
    m_filter.visibilityScope = Application::ClassDayVisibilityScope::ActiveSchedule;
    m_weekendAvailable = false;
    m_lastError.clear();
    m_status = Status::NoWorkspace;
    m_model->clear();
    updateStatus();
    updateControls();
}

void ClassesNavigatorPage::changeEvent(QEvent* event)
{
    Page::changeEvent(event);
    if (event && event->type() == QEvent::LanguageChange)
    {
        retranslateUi();
    }
}

void ClassesNavigatorPage::buildUi()
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(24, 20, 24, 24);
    m_layout->setSpacing(12);

    auto* const title = new QLabel(this);
    title->setObjectName(QStringLiteral("classesNavigatorTitle"));
    title->setProperty("translationSource", QStringLiteral("Classes"));
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 3);
    titleFont.setWeight(QFont::DemiBold);
    title->setFont(titleFont);
    m_layout->addWidget(title);

    auto* const statusRow = new QHBoxLayout();
    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("classesNavigatorStatus"));
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setProperty("role", QStringLiteral("status"));
    m_retryButton = new QToolButton(this);
    m_retryButton->setObjectName(QStringLiteral("classesNavigatorRetry"));
    m_retryButton->setAutoRaise(true);
    statusRow->addWidget(m_statusLabel, 1);
    statusRow->addWidget(m_retryButton, 0, Qt::AlignRight | Qt::AlignTop);
    m_layout->addLayout(statusRow);

    auto* const controls = new QHBoxLayout();
    auto* const sourceLabel = new QLabel(this);
    sourceLabel->setProperty("translationSource", QStringLiteral("Schedule"));
    m_scheduleSource = new QComboBox(this);
    m_scheduleSource->setObjectName(QStringLiteral("classesNavigatorScheduleSource"));
    m_scheduleSource->addItem(translated("Regular"),
        static_cast<int>(Application::ScheduleDisplayMode::Regular));
    m_scheduleSource->addItem(translated("Intensive"),
        static_cast<int>(Application::ScheduleDisplayMode::Intensive));
    auto* const visibilityLabel = new QLabel(this);
    visibilityLabel->setProperty("translationSource", QStringLiteral("Visibility"));
    m_visibilityScope = new QComboBox(this);
    m_visibilityScope->setObjectName(QStringLiteral("classesNavigatorVisibilityScope"));
    m_visibilityScope->addItem(translated("All Classes"),
        static_cast<int>(Application::ClassVisibilityScope::AllClasses));
    m_visibilityScope->addItem(translated("Active Schedule"),
        static_cast<int>(Application::ClassVisibilityScope::ActiveSchedule));
    controls->addWidget(sourceLabel);
    controls->addWidget(m_scheduleSource);
    controls->addSpacing(12);
    controls->addWidget(visibilityLabel);
    controls->addWidget(m_visibilityScope);
    controls->addStretch(1);
    m_layout->addLayout(controls);

    auto* const days = new QHBoxLayout();
    days->setSpacing(6);
    for (int index = 0; index < 6; ++index)
    {
        m_dayButtons[index] = new QToolButton(this);
        m_dayButtons[index]->setObjectName(dayObjectName(index));
        m_dayButtons[index]->setCheckable(true);
        m_dayButtons[index]->setAutoRaise(true);
        m_dayButtons[index]->setProperty(
            "dayFilterKey",
            dayKeys()[static_cast<std::size_t>(index)]
            );
        days->addWidget(m_dayButtons[index]);
        connect(m_dayButtons[index], &QToolButton::toggled, this,
            [this, index](const bool enabled) {
                if (!m_rebuilding)
                {
                    handleDayFilterChanged(
                        dayKeys()[static_cast<std::size_t>(index)], enabled);
                }
            });
    }
    days->addStretch(1);
    m_layout->addLayout(days);

    m_gradeTabs = new QTabBar(this);
    m_gradeTabs->setObjectName(QStringLiteral("classesNavigatorGradeTabs"));
    m_gradeTabs->setExpanding(false);
    m_gradeTabs->setDrawBase(false);
    m_layout->addWidget(m_gradeTabs);

    m_contentSplitter = new QSplitter(Qt::Horizontal, this);
    m_contentSplitter->setObjectName(QStringLiteral("classesNavigatorContentSplitter"));
    m_classListPane = new QWidget(m_contentSplitter);
    auto* const listPaneLayout = new QVBoxLayout(m_classListPane);
    listPaneLayout->setContentsMargins(0, 0, 0, 0);
    auto* const classActions = new QHBoxLayout();
    m_addClassButton = new QPushButton(translated("Add Class"), m_classListPane);
    m_addClassButton->setObjectName(QStringLiteral("classesNavigatorAddClass"));
    m_deleteClassButton = new QPushButton(translated("Delete Class"), m_classListPane);
    m_deleteClassButton->setObjectName(QStringLiteral("classesNavigatorDeleteClass"));
    classActions->addWidget(m_addClassButton);
    classActions->addWidget(m_deleteClassButton);
    listPaneLayout->addLayout(classActions);

    m_classList = new QListView(m_classListPane);
    m_classList->setObjectName(QStringLiteral("classesNavigatorClassList"));
    m_classList->setModel(m_model);
    m_classList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_classList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_classList->setUniformItemSizes(true);
    m_classList->setWordWrap(false);
    listPaneLayout->addWidget(m_classList, 1);
    listPaneLayout->setStretch(1, 1);

    SelectedClassEditorActions editorActions;
    editorActions.saveMode = m_actions.saveMode;
    editorActions.readDetails = m_actions.readClassDetails;
    editorActions.readCoTeacher = m_actions.readClassCoTeacher;
    editorActions.readTeacherChoices = m_actions.readClassCoTeacherChoices;
    editorActions.readTeacherDetails = m_actions.readClassCoTeacherTeacherDetails;
    editorActions.saveDetails = m_actions.saveClassDetails;
    editorActions.saveCoTeacher = m_actions.saveClassCoTeacher;
    editorActions.resolveUnsavedChanges = m_actions.resolveUnsavedChanges;
    m_selectedClassEditor = new SelectedClassEditor(
        std::move(editorActions), m_contentSplitter);
    m_contentSplitter->addWidget(m_classListPane);
    m_contentSplitter->addWidget(m_selectedClassEditor);
    m_contentSplitter->setStretchFactor(0, 0);
    m_contentSplitter->setStretchFactor(1, 1);
    m_contentSplitter->setSizes({300, 700});
    m_layout->addWidget(m_contentSplitter, 1);

    connect(m_gradeTabs, &QTabBar::currentChanged, this, [this](const int index) {
        if (m_rebuilding || index < 0)
        {
            return;
        }
        activateGroup(m_gradeTabs->tabData(index).toString());
    });
    connect(m_classList->selectionModel(),
        &QItemSelectionModel::currentChanged,
        this,
        [this](const QModelIndex& current, const QModelIndex&) {
            if (m_rebuilding)
            {
                return;
            }
            if (const auto selected = m_model->classIdAt(current))
            {
                selectClass(*selected);
            }
        });
    connect(m_scheduleSource, qOverload<int>(&QComboBox::currentIndexChanged),
        this, [this](const int index) {
            if (!m_rebuilding)
            {
                handleScheduleSourceChanged(index);
            }
        });
    connect(m_visibilityScope, qOverload<int>(&QComboBox::currentIndexChanged),
        this, [this](const int index) {
            if (!m_rebuilding)
            {
                handleVisibilityScopeChanged(index);
            }
        });
    connect(m_retryButton, &QToolButton::clicked, this, [this] {
        static_cast<void>(refreshFromStore());
    });
    connect(m_addClassButton, &QPushButton::clicked, this,
        [this] { addClass(); });
    connect(m_deleteClassButton, &QPushButton::clicked, this,
        [this] { deleteSelectedClass(); });

    retranslateUi();
    updateStatus();
    updateControls();
}

void ClassesNavigatorPage::retranslateUi()
{
    m_retranslating = true;
    if (auto* const title = findChild<QLabel*>(
            QStringLiteral("classesNavigatorTitle")))
    {
        title->setText(translated("Classes"));
    }
    for (QObject* object : findChildren<QObject*>())
    {
        const QVariant source = object->property("translationSource");
        if (!source.isValid())
        {
            continue;
        }
        if (auto* const label = qobject_cast<QLabel*>(object))
        {
            label->setText(translated(source.toString().toUtf8().constData()));
        }
    }
    m_retryButton->setText(translated("Retry"));
    m_addClassButton->setText(translated("Add Class"));
    m_deleteClassButton->setText(translated("Delete Class"));
    for (int index = 0; index < 6; ++index)
    {
        m_dayButtons[index]->setText(
            translated(daySources()[static_cast<std::size_t>(index)])
            );
    }
    {
        const QSignalBlocker sourceSignals(m_scheduleSource);
        m_scheduleSource->setItemText(0, translated("Regular"));
        m_scheduleSource->setItemText(1, translated("Intensive"));
    }
    {
        const QSignalBlocker visibilitySignals(m_visibilityScope);
        m_visibilityScope->setItemText(0, translated("All Classes"));
        m_visibilityScope->setItemText(1, translated("Active Schedule"));
    }
    m_model->retranslate();
    rebuildGroupTabs();
    if (m_selectedClassId)
    {
        selectClass(*m_selectedClassId);
    }
    updateStatus();
    m_retranslating = false;
}

void ClassesNavigatorPage::rebuildGroupTabs()
{
    const QSignalBlocker tabsSignals(m_gradeTabs);
    m_rebuilding = true;
    while (m_gradeTabs->count() > 0)
    {
        m_gradeTabs->removeTab(m_gradeTabs->count() - 1);
    }
    int selectedIndex = -1;
    for (const auto& group : m_model->groups())
    {
        const int index = m_gradeTabs->addTab(group.label);
        m_gradeTabs->setTabData(index, group.key);
        if (group.key == m_model->currentGroup())
        {
            selectedIndex = index;
        }
    }
    if (selectedIndex >= 0)
    {
        m_gradeTabs->setCurrentIndex(selectedIndex);
    }
    m_rebuilding = false;
}

void ClassesNavigatorPage::rebuildProjection(
    const std::optional<Domain::ClassId>& preferredClass,
    const QString& preferredGroup
    )
{
    m_rebuilding = true;
    m_model->setSnapshot(m_classes, m_navigation, dayFilterPolicy());
    if (!preferredGroup.isEmpty())
    {
        m_model->setCurrentGroup(preferredGroup);
    }
    if (m_model->currentGroup().isEmpty()
        && !m_model->groups().empty())
    {
        m_model->setCurrentGroup(ClassesNavigatorModel::allClassesGroupKey());
    }
    m_preferredGroupKey = m_model->currentGroup();
    rebuildGroupTabs();

    std::optional<Domain::ClassId> selection;
    if (preferredClass && m_model->indexForClassId(*preferredClass).isValid())
    {
        selection = preferredClass;
    }
    if (!selection)
    {
        const auto remembered = m_selectedByGroup.find(
            groupMapKey(m_model->currentGroup()).toStdString()
            );
        if (remembered != m_selectedByGroup.end()
            && m_model->indexForClassId(remembered->second).isValid())
        {
            selection = remembered->second;
        }
    }
    if (!selection)
    {
        selection = m_model->classIdAtRow(0);
    }

    m_selectedClassId = selection;
    if (selection)
    {
        m_selectedByGroup.insert_or_assign(
            groupMapKey(m_model->currentGroup()).toStdString(),
            *selection
            );
    }
    const QSignalBlocker selectionSignals(m_classList->selectionModel());
    if (selection)
    {
        const QModelIndex index = m_model->indexForClassId(*selection);
        m_classList->setCurrentIndex(index);
        m_classList->selectionModel()->select(
            index,
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows
            );
    }
    else
    {
        m_classList->setCurrentIndex({});
        m_classList->selectionModel()->clearSelection();
    }
    m_rebuilding = false;
    notifyActiveClassSelection();
    loadSelectedClassEditor();
    updateControls();
}

void ClassesNavigatorPage::activateGroup(const QString& groupKey)
{
    if (groupKey.isEmpty() || groupKey == m_model->currentGroup())
    {
        return;
    }
    if (!prepareForSelectionChange(translated("changing class group")))
    {
        const QSignalBlocker blocker(m_gradeTabs);
        for (int index = 0; index < m_gradeTabs->count(); ++index)
        {
            if (m_gradeTabs->tabData(index).toString() == m_model->currentGroup())
            {
                m_gradeTabs->setCurrentIndex(index);
                break;
            }
        }
        return;
    }
    rememberSelectedClass();
    m_model->setCurrentGroup(groupKey);
    m_preferredGroupKey = groupKey;
    std::optional<Domain::ClassId> preferred;
    const auto stored = m_selectedByGroup.find(groupKey.toStdString());
    if (stored != m_selectedByGroup.end())
    {
        preferred = stored->second;
    }
    rebuildProjection(preferred, groupKey);
}

void ClassesNavigatorPage::selectClass(const Domain::ClassId& classId)
{
    if (m_selectedClassId == classId)
    {
        return;
    }
    if (!prepareForSelectionChange(translated("changing the selected class")))
    {
        const QSignalBlocker blocker(m_classList->selectionModel());
        if (m_selectedClassId)
        {
            const QModelIndex oldIndex = m_model->indexForClassId(*m_selectedClassId);
            m_classList->setCurrentIndex(oldIndex);
            m_classList->selectionModel()->select(
                oldIndex,
                QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        }
        else
        {
            m_classList->setCurrentIndex({});
            m_classList->selectionModel()->clearSelection();
        }
        return;
    }
    const QModelIndex index = m_model->indexForClassId(classId);
    if (!index.isValid())
    {
        return;
    }
    m_selectedClassId = classId;
    rememberSelectedClass();
    notifyActiveClassSelection();
    loadSelectedClassEditor();
    m_classList->setCurrentIndex(index);
    m_classList->selectionModel()->select(
        index,
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows
        );
}

bool ClassesNavigatorPage::prepareForSelectionChange(const QString& reason)
{
    return !m_selectedClassEditor
        || m_selectedClassEditor->resolveUnsavedChanges(reason);
}

void ClassesNavigatorPage::notifyActiveClassSelection()
{
    if (!m_actions.setActiveClassSelection)
    {
        return;
    }
    try
    {
        m_actions.setActiveClassSelection(m_selectedClassId);
    }
    catch (...)
    {
        // Selection persistence is in-memory; a callback failure must not
        // invalidate the list selection or the already loaded editor.
    }
}

void ClassesNavigatorPage::loadSelectedClassEditor()
{
    if (!m_selectedClassEditor)
    {
        return;
    }
    if (!m_selectedClassId)
    {
        m_selectedClassEditor->clear();
        return;
    }
    const auto path = activePath();
    if (!path || path->trimmed().isEmpty())
    {
        m_selectedClassEditor->clear();
        return;
    }
    m_selectedClassEditor->loadClass(*path, *m_selectedClassId);
}

void ClassesNavigatorPage::clearSelectedClassEditor()
{
    if (m_selectedClassEditor)
    {
        m_selectedClassEditor->clear();
    }
}

void ClassesNavigatorPage::addClass()
{
    const auto path = activePath();
    if (!path || path->trimmed().isEmpty() || !m_actions.createClass)
    {
        m_lastError = translated("A class cannot be created while no workspace is open.");
        m_status = Status::ActionError;
        updateStatus();
        return;
    }
    if (!prepareForSelectionChange(translated("adding a class")))
    {
        return;
    }
    Application::ClassCreateResult created = [&] {
        try
        {
            return m_actions.createClass(*path);
        }
        catch (...)
        {
            return Application::ClassCreateResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "The class could not be created.",
                .recoverable = true
            });
        }
    }();
    if (!created)
    {
        m_lastError = errorMessage(created.error());
        m_status = Status::ActionError;
        updateStatus();
        return;
    }
    static_cast<void>(refreshFromStore());
    if (containsId(m_classes, created.value()))
    {
        if (m_model->indexForClassId(created.value()).isValid())
        {
            selectClass(created.value());
        }
        else
        {
            // A new class has no schedule yet, so an Active Schedule filter
            // hides it from the list. Keep the filter and select its Details
            // editor directly so the user can populate the class.
            m_selectedClassId = created.value();
            rememberSelectedClass();
            notifyActiveClassSelection();
            loadSelectedClassEditor();
            const QSignalBlocker selectionSignals(m_classList->selectionModel());
            m_classList->setCurrentIndex({});
            m_classList->selectionModel()->clearSelection();
            updateControls();
        }
    }
}

void ClassesNavigatorPage::deleteSelectedClass()
{
    if (!m_selectedClassId)
    {
        return;
    }
    QString className = m_model->labelForClassId(*m_selectedClassId);
    if (className.isEmpty())
    {
        className = QString::fromStdString(m_selectedClassId->value());
    }
    bool confirmed = false;
    if (m_actions.confirmDeleteClass)
    {
        confirmed = m_actions.confirmDeleteClass(className);
    }
    else
    {
        confirmed = QMessageBox::question(
            this,
            translated("Delete Class"),
            translated("Delete %1? This cannot be undone.").arg(className),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No) == QMessageBox::Yes;
    }
    if (!confirmed)
    {
        return;
    }
    if (!prepareForSelectionChange(translated("deleting this class")))
    {
        return;
    }
    const auto path = activePath();
    if (!path || path->trimmed().isEmpty() || !m_actions.deleteClass)
    {
        m_lastError = translated("The selected class cannot be deleted right now.");
        m_status = Status::ActionError;
        updateStatus();
        return;
    }
    Application::ClassDeleteResult deleted = [&] {
        try
        {
            return m_actions.deleteClass(*path, {
                .classId = *m_selectedClassId
            });
        }
        catch (...)
        {
            return Application::ClassDeleteResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "The class could not be deleted.",
                .recoverable = true
            });
        }
    }();
    if (!deleted)
    {
        m_lastError = errorMessage(deleted.error());
        m_status = Status::ActionError;
        updateStatus();
        return;
    }
    const Domain::ClassId removedId = *m_selectedClassId;
    for (auto iterator = m_selectedByGroup.begin();
         iterator != m_selectedByGroup.end();)
    {
        if (iterator->second == removedId)
        {
            iterator = m_selectedByGroup.erase(iterator);
        }
        else
        {
            ++iterator;
        }
    }
    m_selectedClassId.reset();
    notifyActiveClassSelection();
    clearSelectedClassEditor();
    m_lastError.clear();
    static_cast<void>(refreshFromStore());
}

void ClassesNavigatorPage::applyFilter()
{
    rememberSelectedClass();
    const auto preferred = m_selectedClassId;
    const QString group = m_preferredGroupKey;
    rebuildProjection(preferred, group);
    if (m_status != Status::NavigationError)
    {
        m_status = m_model->visibleClassCount() == 0
            ? Status::NoMatches
            : Status::Ready;
    }
    updateStatus();
}

void ClassesNavigatorPage::handleScheduleSourceChanged(const int index)
{
    if (index < 0)
    {
        return;
    }
    const auto requested = static_cast<Application::ScheduleDisplayMode>(
        m_scheduleSource->itemData(index).toInt()
        );
    const auto scope = requested == Application::ScheduleDisplayMode::Intensive
        ? Application::ClassDayScheduleSource::Intensive
        : Application::ClassDayScheduleSource::Regular;
    if (scope == m_filter.scheduleSource)
    {
        return;
    }
    if (!prepareForSelectionChange(translated("changing the class filter")))
    {
        const QSignalBlocker blocker(m_scheduleSource);
        m_scheduleSource->setCurrentIndex(m_scheduleSource->findData(
            m_filter.scheduleSource == Application::ClassDayScheduleSource::Intensive
                ? static_cast<int>(Application::ScheduleDisplayMode::Intensive)
                : static_cast<int>(Application::ScheduleDisplayMode::Regular)));
        return;
    }
    if (const auto path = activePath(); path && m_actions.saveScheduleMode)
    {
        const auto saved = m_actions.saveScheduleMode(*path, requested);
        if (!saved)
        {
            m_lastError = errorMessage(saved.error());
            m_status = Status::PreferenceError;
            updateStatus();
            const QSignalBlocker blocker(m_scheduleSource);
            m_scheduleSource->setCurrentIndex(
                m_scheduleSource->findData(
                    m_filter.scheduleSource
                            == Application::ClassDayScheduleSource::Intensive
                        ? static_cast<int>(Application::ScheduleDisplayMode::Intensive)
                        : static_cast<int>(Application::ScheduleDisplayMode::Regular)
                    )
                );
            return;
        }
    }
    m_filter.scheduleSource = scope;
    m_preferences.scheduleDisplayMode = requested;
    applyFilter();
}

void ClassesNavigatorPage::handleVisibilityScopeChanged(const int index)
{
    if (index < 0)
    {
        return;
    }
    const auto requested = static_cast<Application::ClassVisibilityScope>(
        m_visibilityScope->itemData(index).toInt()
        );
    const auto scope = requested == Application::ClassVisibilityScope::AllClasses
        ? Application::ClassDayVisibilityScope::AllClasses
        : Application::ClassDayVisibilityScope::ActiveSchedule;
    if (scope == m_filter.visibilityScope)
    {
        return;
    }
    if (!prepareForSelectionChange(translated("changing the class filter")))
    {
        const QSignalBlocker blocker(m_visibilityScope);
        m_visibilityScope->setCurrentIndex(m_visibilityScope->findData(
            m_filter.visibilityScope == Application::ClassDayVisibilityScope::AllClasses
                ? static_cast<int>(Application::ClassVisibilityScope::AllClasses)
                : static_cast<int>(Application::ClassVisibilityScope::ActiveSchedule)));
        return;
    }
    if (const auto path = activePath(); path && m_actions.saveVisibilityScope)
    {
        const auto saved = m_actions.saveVisibilityScope(*path, requested);
        if (!saved)
        {
            m_lastError = errorMessage(saved.error());
            m_status = Status::PreferenceError;
            updateStatus();
            const QSignalBlocker blocker(m_visibilityScope);
            m_visibilityScope->setCurrentIndex(
                m_visibilityScope->findData(
                    m_filter.visibilityScope
                            == Application::ClassDayVisibilityScope::AllClasses
                        ? static_cast<int>(Application::ClassVisibilityScope::AllClasses)
                        : static_cast<int>(Application::ClassVisibilityScope::ActiveSchedule)
                    )
                );
            return;
        }
    }
    m_filter.visibilityScope = scope;
    m_preferences.visibilityScope = requested;
    applyFilter();
}

void ClassesNavigatorPage::handleDayFilterChanged(
    const QString& key,
    const bool enabled
    )
{
    if (key == QStringLiteral("weekend") && !m_weekendAvailable)
    {
        return;
    }
    if (!prepareForSelectionChange(translated("changing the class filter")))
    {
        for (int index = 0; index < 6; ++index)
        {
            if (dayKeys()[static_cast<std::size_t>(index)] == key)
            {
                const QSignalBlocker blocker(m_dayButtons[index]);
                const std::string selectedKey = key.toStdString();
                const bool wasEnabled = std::find(
                    m_filter.selectedDayKeys.begin(),
                    m_filter.selectedDayKeys.end(), selectedKey)
                    != m_filter.selectedDayKeys.end();
                m_dayButtons[index]->setChecked(wasEnabled);
                break;
            }
        }
        return;
    }
    const auto found = std::find(
        m_filter.selectedDayKeys.begin(),
        m_filter.selectedDayKeys.end(),
        key.toStdString()
        );
    const bool alreadyEnabled = found != m_filter.selectedDayKeys.end();
    if (enabled && !alreadyEnabled)
    {
        m_filter.selectedDayKeys.push_back(key.toStdString());
    }
    else if (!enabled && alreadyEnabled)
    {
        m_filter.selectedDayKeys.erase(found);
    }
    applyFilter();
}

void ClassesNavigatorPage::updateStatus()
{
    QString message;
    switch (m_status)
    {
    case Status::Ready:
        message.clear();
        break;
    case Status::NoWorkspace:
        message = translated("No Teacher Profile is open.");
        break;
    case Status::Empty:
        message = translated("No classes available.");
        break;
    case Status::ListError:
        message = translated("The class list could not be loaded. Retry this page.");
        break;
    case Status::NavigationError:
        message = translated(
            "Navigation details are unavailable. Showing class names. Retry to load details."
            );
        break;
    case Status::PreferenceError:
        message = translated("A navigation preference could not be saved.");
        break;
    case Status::ActionError:
        message = m_lastError;
        break;
    case Status::NoMatches:
        message = translated("No classes match the selected filters.");
        break;
    case Status::WorkspaceChanged:
        message = translated("Workspace changed. Retry to load Classes.");
        break;
    }
    if (!m_lastError.isEmpty()
        && (m_status == Status::ListError
            || m_status == Status::NavigationError
            || m_status == Status::PreferenceError))
    {
        message += QStringLiteral("\n%1").arg(m_lastError);
    }
    m_statusLabel->setText(message);
    m_statusLabel->setVisible(!message.isEmpty());
    const bool retryVisible = m_status == Status::ListError
        || m_status == Status::NavigationError
        || m_status == Status::WorkspaceChanged;
    m_retryButton->setVisible(retryVisible);
}

void ClassesNavigatorPage::updateControls()
{
    const bool hasWorkspace = m_workspaceIdentity.has_value()
        && m_status != Status::NoWorkspace;
    const bool listReady = m_status != Status::ListError
        && m_status != Status::NoWorkspace
        && m_status != Status::WorkspaceChanged;
    m_scheduleSource->setEnabled(hasWorkspace);
    m_visibilityScope->setEnabled(hasWorkspace);
    m_classList->setEnabled(listReady && m_model->rowCount() > 0);
    m_addClassButton->setEnabled(hasWorkspace);
    m_deleteClassButton->setEnabled(
        listReady && m_selectedClassId.has_value());
    m_gradeTabs->setVisible(!m_model->groups().empty());
    m_gradeTabs->setEnabled(listReady && m_model->groups().size() > 1);
    for (int index = 0; index < 5; ++index)
    {
        m_dayButtons[index]->setEnabled(
            hasWorkspace && m_navigation.has_value() && listReady
            );
    }
    m_dayButtons[5]->setVisible(m_weekendAvailable);
    m_dayButtons[5]->setEnabled(
        hasWorkspace && m_navigation.has_value() && listReady
        );
    m_rebuilding = true;
    for (int index = 0; index < 6; ++index)
    {
        const QSignalBlocker blocker(m_dayButtons[index]);
        const std::string key = dayKeys()[static_cast<std::size_t>(index)]
            .toStdString();
        const bool checked = std::find(
            m_filter.selectedDayKeys.cbegin(),
            m_filter.selectedDayKeys.cend(),
            key
            ) != m_filter.selectedDayKeys.cend();
        m_dayButtons[index]->setChecked(checked);
    }
    m_rebuilding = false;
}

void ClassesNavigatorPage::rememberSelectedClass()
{
    if (m_selectedClassId && !m_model->currentGroup().isEmpty())
    {
        m_selectedByGroup.insert_or_assign(
            m_model->currentGroup().toStdString(),
            *m_selectedClassId
            );
    }
}

void ClassesNavigatorPage::discardSelectionState()
{
    m_selectedByGroup.clear();
    m_selectedClassId.reset();
}

void ClassesNavigatorPage::discardDayFilterState()
{
    m_filter.selectedDayKeys.clear();
}

Application::ClassDayFilterPolicy ClassesNavigatorPage::dayFilterPolicy() const
{
    return m_filter;
}

std::optional<QString> ClassesNavigatorPage::activePath() const
{
    if (!m_actions.activeWorkspacePath)
    {
        return std::nullopt;
    }
    try
    {
        return m_actions.activeWorkspacePath();
    }
    catch (...)
    {
        return std::nullopt;
    }
}

bool ClassesNavigatorPage::validateListSnapshot(
    const Application::ClassesListSnapshot& snapshot,
    QString* error
    ) const
{
    if (snapshot.classes.size()
        > Application::kClassesNavigationMaximumClasses)
    {
        if (error)
        {
            *error = QStringLiteral("The class list exceeds its bounded class limit.");
        }
        return false;
    }
    std::set<std::string> ids;
    for (const auto& classroom : snapshot.classes)
    {
        if (!Application::isCanonicalPositiveDecimalId(classroom.classId.value())
            || !ids.insert(classroom.classId.value()).second
            || classroom.className.size()
                > Application::kClassesNavigationMaximumTextLength)
        {
            if (error)
            {
                *error = QStringLiteral(
                    "The class list contains an invalid, duplicate, or unbounded entry."
                    );
            }
            return false;
        }
    }
    return true;
}

bool ClassesNavigatorPage::validateNavigationSnapshot(
    const Application::ClassesListSnapshot& classes,
    const Application::ClassesNavigationSnapshot& navigation,
    QString* error
    ) const
{
    const auto fail = [error](const char* message) {
        if (error)
        {
            *error = QString::fromUtf8(message);
        }
        return false;
    };
    if (navigation.classes.size() != classes.classes.size())
    {
        return fail("The navigation metadata did not match the class list size.");
    }
    for (std::size_t index = 0; index < classes.classes.size(); ++index)
    {
        const auto& source = classes.classes[index];
        const auto& returned = navigation.classes[index];
        if (returned.classId != source.classId
            || returned.className != source.className)
        {
            return fail("The navigation metadata changed class identity or order.");
        }
        if (!returned.details)
        {
            continue;
        }
        const auto& details = returned.details.value();
        const auto boundedText = [](const std::u16string& value) {
            return value.size()
                <= Application::kClassesNavigationMaximumTextLength;
        };
        const auto boundedRows = [](const auto& rows) {
            return rows.size()
                    <= Application::kClassesNavigationMaximumScheduleRows
                && std::all_of(rows.cbegin(), rows.cend(), [](const auto& row) {
                    return row.day.size()
                            <= Application::kClassesNavigationMaximumScheduleTokenLength
                        && row.startTime.size()
                            <= Application::kClassesNavigationMaximumScheduleTokenLength
                        && row.endTime.size()
                            <= Application::kClassesNavigationMaximumScheduleTokenLength;
                });
        };
        if (!boundedText(details.grade)
            || !boundedText(details.level)
            || !boundedText(details.teacherEnglishName)
            || !boundedText(details.teacherKoreanName)
            || !boundedRows(details.regularSchedule)
            || !boundedRows(details.intensiveSchedule))
        {
            return fail("The navigation metadata exceeded its bounded summary limits.");
        }
    }
    return true;
}

} // namespace ClassMngr::Next::Ui
