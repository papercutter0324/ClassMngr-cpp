#include "next/ui/sub_prep_page.h"

#include "features/schedule/ui/schedule_builder.h"
#include "features/schedule/ui/schedule_view_model.h"
#include "features/sub_prep/services/sub_prep_package_service.h"
#include "features/sub_prep/services/sub_prep_print_service.h"
#include "features/sub_prep/ui/sub_prep_class_information_list_model.h"
#include "features/sub_prep/ui/sub_prep_print_source_mapper.h"
#include "next/application/sub_prep_class_information_state.h"
#include "next/application/sub_prep_selected_class_ids.h"
#include "next/application/sub_prep_vacation_selection_policy.h"
#include "next/application/sub_prep_weekday.h"
#include "shared/roster/roster_template_renderer.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFrame>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPalette>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTabBar>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <charconv>
#include <string>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{
constexpr std::array<const char*, 7> DayNames{
    "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"
};

[[nodiscard]] QString pageText(const char* source)
{
    return QCoreApplication::translate("SubPrepPage", source);
}

[[nodiscard]] QString printDialogText(const char* source)
{
    return QCoreApplication::translate("SubPrepPrintDialog", source);
}

[[nodiscard]] QString fromUtf8(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

[[nodiscard]] std::string toUtf8(const QString& value)
{
    const QByteArray encoded = value.toUtf8();
    return std::string(encoded.constData(), static_cast<std::size_t>(encoded.size()));
}

[[nodiscard]] QString operationError(const Domain::OperationError& error)
{
    return fromUtf8(error.message);
}

[[nodiscard]] QString weekdayLabel(const Application::SubPrepWeekday day)
{
    const auto value = static_cast<int>(day)
        - static_cast<int>(Application::SubPrepWeekday::Monday);
    return value >= 0 && value < static_cast<int>(DayNames.size())
        ? QString::fromLatin1(DayNames[static_cast<std::size_t>(value)])
        : QString{};
}

[[nodiscard]] std::optional<Application::SubPrepWeekday> weekdayForLabel(
    const QString& day
    )
{
    for (std::size_t index = 0; index < DayNames.size(); ++index)
    {
        if (day.compare(QString::fromLatin1(DayNames[index]), Qt::CaseInsensitive) == 0)
        {
            return static_cast<Application::SubPrepWeekday>(
                static_cast<int>(Application::SubPrepWeekday::Monday)
                + static_cast<int>(index)
                );
        }
    }
    return std::nullopt;
}

[[nodiscard]] int classIdAsInt(const Domain::ClassId& classId)
{
    int value = -1;
    const std::string& text = classId.value();
    const auto [end, error] = std::from_chars(
        text.data(), text.data() + text.size(), value
        );
    return error == std::errc{} && end == text.data() + text.size()
        ? value
        : -1;
}

[[nodiscard]] QDate toQDate(const Domain::GregorianDate& date)
{
    return QDate(date.year(), date.month(), date.day());
}

[[nodiscard]] QPlainTextEdit* addTextEditor(
    QFormLayout* layout,
    const QString& objectName,
    const QString& label,
    QWidget* parent,
    const bool readOnly
    )
{
    auto* editor = new QPlainTextEdit(parent);
    editor->setObjectName(objectName);
    editor->setReadOnly(readOnly);
    editor->setMaximumHeight(readOnly ? 86 : 96);
    editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    QString labelText = label;
    labelText.replace(QLatin1Char('&'), QStringLiteral("&&"));
    layout->addRow(labelText, editor);
    return editor;
}

void clearTabs(QTabBar* tabs)
{
    while (tabs->count() > 0)
    {
        tabs->removeTab(tabs->count() - 1);
    }
}

} // namespace

SubPrepPage::SubPrepPage(
    const PageContext& context,
    SubPrepPageActions actions,
    QWidget* parent
    )
    : Page(context, parent),
      m_actions(std::move(actions))
{
    buildUi();
    retranslateUi();
}

void SubPrepPage::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(8);

    auto* title = new QLabel(this);
    title->setObjectName(QStringLiteral("subPrepPageTitle"));
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 3);
    titleFont.setWeight(QFont::DemiBold);
    title->setFont(titleFont);
    root->addWidget(title);

    auto* controls = new QHBoxLayout();
    controls->setSpacing(4);
    m_mode = new QComboBox(this);
    m_mode->setObjectName(QStringLiteral("subPrepScheduleMode"));
    m_mode->setMinimumWidth(136);
    controls->addWidget(m_mode, 0);
    for (int index = 0; index < 7; ++index)
    {
        auto* check = new QCheckBox(this);
        check->setObjectName(QStringLiteral("subPrepDay_%1").arg(index));
        check->setProperty("translationSource", QString::fromLatin1(DayNames[static_cast<std::size_t>(index)]));
        check->setChecked(index < 5);
        m_dayChecks[index] = check;
        controls->addWidget(check);
        connect(check, &QCheckBox::toggled, this, [this] {
            if (!m_loading && m_active)
            {
                refreshSummaryScope();
            }
        });
    }
    m_refreshButton = new QPushButton(this);
    m_refreshButton->setObjectName(QStringLiteral("subPrepRefreshButton"));
    m_generateButton = new QPushButton(this);
    m_generateButton->setObjectName(QStringLiteral("subPrepGenerateButton"));
    root->addLayout(controls);
    auto* actions = new QHBoxLayout();
    actions->addStretch(1);
    actions->addWidget(m_refreshButton);
    actions->addWidget(m_generateButton);
    root->addLayout(actions);

    auto* content = new QHBoxLayout();
    content->setSpacing(14);
    auto* navigation = new QWidget(this);
    navigation->setObjectName(QStringLiteral("subPrepClassNavigation"));
    navigation->setMinimumWidth(300);
    auto* navigationLayout = new QVBoxLayout(navigation);
    navigationLayout->setContentsMargins(0, 0, 0, 0);
    m_gradeTabs = new QTabBar(navigation);
    m_gradeTabs->setObjectName(QStringLiteral("subPrepGradeTabs"));
    m_gradeTabs->setExpanding(false);
    m_gradeTabs->setMovable(false);
    navigationLayout->addWidget(m_gradeTabs);
    m_model = new SubPrepClassInformationListModel(this);
    m_classList = new QListView(navigation);
    m_classList->setObjectName(QStringLiteral("subPrepClassSummaryList"));
    m_classList->setModel(m_model);
    m_classList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_classList->setUniformItemSizes(true);
    m_classList->setMinimumHeight(220);
    navigationLayout->addWidget(m_classList, 1);
    content->addWidget(navigation, 1);

    auto* detailsScrollArea = new QScrollArea(this);
    detailsScrollArea->setObjectName(QStringLiteral("subPrepSelectedDetailScrollArea"));
    detailsScrollArea->setWidgetResizable(true);
    detailsScrollArea->setFrameShape(QFrame::NoFrame);
    m_detailPanel = new QGroupBox();
    m_detailPanel->setObjectName(QStringLiteral("subPrepSelectedDetailPanel"));
    m_detailPanel->setMinimumSize(0, 0);
    auto* detailLayout = new QFormLayout(m_detailPanel);
    m_classTitle = new QLabel(m_detailPanel);
    m_classTitle->setObjectName(QStringLiteral("subPrepSelectedClassTitle"));
    detailLayout->addRow(pageText("Class Information"), m_classTitle);
    m_teacherName = new QLabel(m_detailPanel);
    m_teacherName->setObjectName(QStringLiteral("subPrepTeacherName"));
    detailLayout->addRow(pageText("Teacher"), m_teacherName);
    m_studentCount = new QLabel(m_detailPanel);
    m_studentCount->setObjectName(QStringLiteral("subPrepStudentCount"));
    detailLayout->addRow(pageText("# of Students"), m_studentCount);
    m_schedule = new QLabel(m_detailPanel);
    m_schedule->setObjectName(QStringLiteral("subPrepClassSchedule"));
    m_schedule->setWordWrap(true);
    detailLayout->addRow(pageText("Schedule"), m_schedule);

    auto* facilities = new QGridLayout();
    m_room = new QLabel(m_detailPanel);
    m_wifiName = new QLabel(m_detailPanel);
    m_wifiPassword = new QLabel(m_detailPanel);
    m_internet = new QLabel(m_detailPanel);
    m_zoomId = new QLabel(m_detailPanel);
    m_zoomPassword = new QLabel(m_detailPanel);
    m_projection = new QLabel(m_detailPanel);
    const std::array<std::pair<const char*, QLabel*>, 7> facilityFields{{
        {"Room", m_room}, {"WiFi Name", m_wifiName},
        {"WiFi Password", m_wifiPassword}, {"Internet", m_internet},
        {"Zoom ID", m_zoomId}, {"Zoom Password", m_zoomPassword},
        {"Projection", m_projection}
    }};
    for (std::size_t index = 0; index < facilityFields.size(); ++index)
    {
        auto* label = new QLabel(m_detailPanel);
        label->setProperty("translationSource", QString::fromLatin1(facilityFields[index].first));
        facilityFields[index].second->setObjectName(
            QStringLiteral("subPrepSelectedFacility_%1").arg(index)
            );
        facilities->addWidget(label, static_cast<int>(index), 0);
        facilities->addWidget(facilityFields[index].second, static_cast<int>(index), 1);
    }
    detailLayout->addRow(facilities);
    m_classNotes = addTextEditor(
        detailLayout, QStringLiteral("subPrepSelectedClassNotes"),
        pageText("Class Notes"), m_detailPanel, true
        );
    m_teacherNotes = addTextEditor(
        detailLayout, QStringLiteral("subPrepSelectedTeacherNotes"),
        pageText("Teacher Notes"), m_detailPanel, true
        );
    detailsScrollArea->setWidget(m_detailPanel);
    content->addWidget(detailsScrollArea, 2);
    root->addLayout(content, 1);

    auto* preferences = new QGroupBox(this);
    preferences->setObjectName(QStringLiteral("subPrepPreparationSettings"));
    auto* preferencesLayout = new QFormLayout(preferences);
    m_classMaterials = addTextEditor(
        preferencesLayout, QStringLiteral("subPrepClassMaterials"),
        pageText("Class Materials & Lesson Notes"), preferences, false
        );
    m_gradingInstructions = addTextEditor(
        preferencesLayout, QStringLiteral("subPrepGradingInstructions"),
        pageText("Grading Instructions"), preferences, false
        );
    m_specialInstructions = addTextEditor(
        preferencesLayout, QStringLiteral("subPrepSpecialInstructions"),
        pageText("Special Instructions"), preferences, false
        );
    m_subComments = addTextEditor(
        preferencesLayout, QStringLiteral("subPrepSubComments"),
        pageText("Sub Notes"), preferences, false
        );
    auto* settingsButtons = new QHBoxLayout();
    settingsButtons->addStretch(1);
    m_savePreferencesButton = new QPushButton(this);
    m_savePreferencesButton->setObjectName(QStringLiteral("subPrepSavePreferences"));
    settingsButtons->addWidget(m_savePreferencesButton);
    root->addWidget(preferences);
    root->addLayout(settingsButtons);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("subPrepRouteStatus"));
    m_status->setWordWrap(true);
    root->addWidget(m_status);

    connect(m_refreshButton, &QPushButton::clicked, this, [this] {
        static_cast<void>(reload());
    });
    connect(m_generateButton, &QPushButton::clicked, this, &SubPrepPage::generateSubPrep);
    connect(m_savePreferencesButton, &QPushButton::clicked, this, &SubPrepPage::savePreferences);
    connect(m_mode, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_loading && m_active)
        {
            refreshSummaryScope();
        }
    });
    connect(m_gradeTabs, &QTabBar::currentChanged, this, &SubPrepPage::handleGradeChanged);
    connect(m_classList->selectionModel(), &QItemSelectionModel::currentChanged,
        this, [this](const QModelIndex&, const QModelIndex&) { handleSelectionChanged(); });
    connect(m_classMaterials, &QPlainTextEdit::textChanged, this, [this] {
        if (!m_loading) { savePreferences(); }
    });
    connect(m_gradingInstructions, &QPlainTextEdit::textChanged, this, [this] {
        if (!m_loading) { savePreferences(); }
    });
    connect(m_specialInstructions, &QPlainTextEdit::textChanged, this, [this] {
        if (!m_loading) { savePreferences(); }
    });
    connect(m_subComments, &QPlainTextEdit::textChanged, this, [this] {
        if (!m_loading) { savePreferences(); }
    });

    clearSelectedDetail();
    m_detailPanel->hide();
}

void SubPrepPage::retranslateUi()
{
    if (auto* title = findChild<QLabel*>(QStringLiteral("subPrepPageTitle")))
    {
        title->setText(pageText("Sub Prep"));
    }
    const int previousMode = m_mode->currentData().toInt();
    const QSignalBlocker blocker(m_mode);
    m_mode->clear();
    m_mode->addItem(pageText("Regular Schedule"), static_cast<int>(Application::ScheduleViewMode::Regular));
    m_mode->addItem(pageText("Intensive Schedule"), static_cast<int>(Application::ScheduleViewMode::Intensive));
    const int restoredMode = m_mode->findData(previousMode);
    if (restoredMode >= 0) { m_mode->setCurrentIndex(restoredMode); }
    m_refreshButton->setText(pageText("Refresh"));
    m_generateButton->setText(pageText("Generate Sub Prep"));
    m_savePreferencesButton->setText(pageText("Save"));
    for (int index = 0; index < 7; ++index)
    {
        m_dayChecks[index]->setText(pageText(DayNames[static_cast<std::size_t>(index)]));
    }
    m_detailPanel->setTitle(pageText("Class Information"));
    for (QLabel* const label : m_detailPanel->findChildren<QLabel*>())
    {
        const QVariant source = label->property("translationSource");
        if (source.isValid())
        {
            const QByteArray key = source.toString().toUtf8();
            label->setText(pageText(key.constData()));
        }
    }
    if (auto* const detailForm = qobject_cast<QFormLayout*>(
            m_detailPanel->layout()))
    {
        const std::array<std::pair<QWidget*, const char*>, 10> fields{{
            {m_classTitle, "Class Information"},
            {m_teacherName, "Teacher"},
            {m_studentCount, "# of Students"},
            {m_schedule, "Schedule"},
            {m_classNotes, "Class Notes"},
            {m_teacherNotes, "Teacher Notes"},
            {m_room, "Room"},
            {m_wifiName, "WiFi Name"},
            {m_wifiPassword, "WiFi Password"},
            {m_internet, "Internet"}
        }};
        for (const auto& [field, labelText] : fields)
        {
            if (QWidget* const label = detailForm->labelForField(field))
            {
                if (auto* const text = qobject_cast<QLabel*>(label))
                {
                    text->setText(pageText(labelText));
                }
            }
        }
        const std::array<std::pair<QWidget*, const char*>, 2> zoomFields{{
            {m_zoomId, "Zoom ID"},
            {m_zoomPassword, "Zoom Password"}
        }};
        for (const auto& [field, labelText] : zoomFields)
        {
            if (QWidget* const label = detailForm->labelForField(field))
            {
                if (auto* const text = qobject_cast<QLabel*>(label))
                {
                    text->setText(pageText(labelText));
                }
            }
        }
    }
    if (auto* const settings = findChild<QGroupBox*>(
            QStringLiteral("subPrepPreparationSettings")))
    {
        settings->setTitle(pageText("Sub Prep"));
        if (auto* const settingsForm = qobject_cast<QFormLayout*>(
                settings->layout()))
        {
            const std::array<std::pair<QWidget*, const char*>, 4> fields{{
                {m_classMaterials, "Class Materials & Lesson Notes"},
                {m_gradingInstructions, "Grading Instructions"},
                {m_specialInstructions, "Special Instructions"},
                {m_subComments, "Sub Notes"}
            }};
            for (const auto& [field, labelText] : fields)
            {
                if (QWidget* const label = settingsForm->labelForField(field))
                {
                    if (auto* const text = qobject_cast<QLabel*>(label))
                    {
                        text->setText(pageText(labelText));
                    }
                }
            }
        }
    }
}

PageOperationResult<void> SubPrepPage::onActivating()
{
    m_active = true;
    const QString identity = m_actions.workspaceIdentity ? m_actions.workspaceIdentity() : QString{};
    if (m_workspaceIdentity && *m_workspaceIdentity != identity)
    {
        m_informationState = {};
        clearSelectedDetail();
    }
    m_workspaceIdentity = identity;
    QString error;
    static_cast<void>(reload(&error));
    return {};
}

void SubPrepPage::onSuspending() noexcept
{
    if (m_active)
    {
        savePreferences();
    }
    m_active = false;
    m_informationState = {};
    m_workspaceIdentity.reset();
    clearPageData();
}

bool SubPrepPage::reload(QString* error)
{
    if (!m_actions.readSchedulePage)
    {
        setStatus(pageText("The schedule service is unavailable."), true);
        if (error) { *error = m_status->text(); }
        clearPageData();
        return false;
    }

    ++m_scheduleReadCount;
    Application::SchedulePageReadResult schedule = m_actions.readSchedulePage();
    if (!schedule)
    {
        const QString message = operationError(schedule.error());
        setStatus(message, true);
        clearPageData();
        if (error) { *error = message; }
        return false;
    }

    m_loading = true;
    if (!m_modeInitialized)
    {
        const auto mode = schedule.value().displayMode == Application::ScheduleDisplayMode::Intensive
            ? Application::ScheduleViewMode::Intensive
            : Application::ScheduleViewMode::Regular;
        const int index = m_mode->findData(static_cast<int>(mode));
        if (index >= 0) { m_mode->setCurrentIndex(index); }
        m_modeInitialized = true;
    }
    const bool showWeekends = schedule.value().displayPreferences.showWeekends;
    for (int index = 5; index < 7; ++index)
    {
        m_dayChecks[index]->setEnabled(showWeekends);
        m_dayChecks[index]->setVisible(showWeekends);
    }
    if (m_actions.readPreferences)
    {
        auto preferences = m_actions.readPreferences();
        if (preferences)
        {
            m_preferences = std::move(preferences.value());
            m_classMaterials->setPlainText(fromUtf8(m_preferences.classMaterials));
            m_gradingInstructions->setPlainText(m_preferences.bookReportGrading
                ? fromUtf8(*m_preferences.bookReportGrading) : QString{});
            m_specialInstructions->setPlainText(m_preferences.bookReportSpecialInstructions
                ? fromUtf8(*m_preferences.bookReportSpecialInstructions) : QString{});
            m_subComments->setPlainText(fromUtf8(m_preferences.subComments));
        }
    }
    m_loading = false;
    refreshSummaryScopeWithSchedule(std::move(schedule.value()));
    if (error) { error->clear(); }
    return true;
}

void SubPrepPage::refreshSummaryScope()
{
    if (!m_active || !m_actions.readSchedulePage)
    {
        return;
    }
    ++m_scheduleReadCount;
    auto schedule = m_actions.readSchedulePage();
    if (!schedule)
    {
        const QString message = operationError(schedule.error());
        setStatus(message, true);
        clearPageData();
        return;
    }
    refreshSummaryScopeWithSchedule(std::move(schedule.value()));
}

void SubPrepPage::refreshSummaryScopeWithSchedule(
    Application::SchedulePageReadSnapshot schedule
    )
{
    const Application::ScheduleViewMode mode =
        static_cast<Application::ScheduleViewMode>(m_mode->currentData().toInt());
    const std::vector<Application::SubPrepWeekday> days = selectedWeekdays();
    std::vector<Domain::ClassId> visibleIds;
    if (!days.empty())
    {
        for (const auto& classInfo : schedule.source.classes)
        {
            const auto& entries = mode == Application::ScheduleViewMode::Intensive
                ? classInfo.intensiveSchedule : classInfo.regularSchedule;
            const bool meetsSelectedDay = std::any_of(entries.cbegin(), entries.cend(),
                [&days](const Application::ScheduleBuilderSourceScheduleRow& row) {
                    QString day = QString::fromStdU16String(row.day.value()).trimmed();
                    if (day.isEmpty()) { day = QStringLiteral("Monday"); }
                    return std::any_of(days.cbegin(), days.cend(), [&day](const auto selected) {
                        return day.compare(weekdayLabel(selected), Qt::CaseInsensitive) == 0;
                    });
                });
            if (meetsSelectedDay)
            {
                visibleIds.push_back(classInfo.classId);
            }
        }
    }

    Application::SubPrepScheduleScopeRequest request{
        .visibleClassIds = std::move(visibleIds),
        .selectedDays = days,
        .mode = mode
    };
    Application::SubPrepScheduleSummaryQueryResult summaries =
        Application::ClassSummaryProjection::create({});
    if (m_actions.readSummaries)
    {
        ++m_summaryReadCount;
        summaries = m_actions.readSummaries(request);
    }
    else
    {
        summaries = Application::SubPrepScheduleSummaryQueryResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "The Sub Prep summary service is unavailable.",
            .recoverable = true
        });
    }

    if (!summaries)
    {
        const QString message = operationError(summaries.error());
        m_informationState = {};
        auto empty = Application::ClassSummaryProjection::create({});
        if (empty) { m_model->setProjection(std::move(empty.value())); }
        clearTabs(m_gradeTabs);
        m_detailPanel->hide();
        clearSelectedDetail();
        setStatus(message, true);
        return;
    }

    auto nextState = m_informationState.refreshScope(summaries);
    if (nextState)
    {
        m_informationState = std::move(nextState.value());
    }
    else
    {
        m_informationState = {};
    }
    m_loading = true;
    m_model->setProjection(summaries.value());

    const QString preferredGrade = [&] {
        if (!m_informationState.selectedClassId()) { return QString{}; }
        const auto selected = m_model->projection().findClass(*m_informationState.selectedClassId());
        return selected ? fromUtf8(selected->grade) : QString{};
    }();
    if (!preferredGrade.isEmpty()) { m_model->setCurrentGrade(preferredGrade); }
    {
        const QSignalBlocker blocker(m_gradeTabs);
        clearTabs(m_gradeTabs);
        for (const QString& grade : m_model->grades())
        {
            m_gradeTabs->addTab(grade.isEmpty() ? pageText("Other") : grade);
        }
        int gradeIndex = -1;
        for (int index = 0; index < m_gradeTabs->count(); ++index)
        {
            if (m_gradeTabs->tabText(index) == m_model->currentGrade())
            {
                gradeIndex = index;
                break;
            }
        }
        if (gradeIndex >= 0) { m_gradeTabs->setCurrentIndex(gradeIndex); }
        m_gradeTabs->setVisible(m_gradeTabs->count() > 1);
    }

    m_classList->selectionModel()->clear();
    m_classList->setCurrentIndex({});
    const auto selectedId = m_informationState.selectedClassId();
    int row = selectedId ? m_model->rowForClassId(*selectedId) : -1;
    if (row < 0 && m_model->rowCount() > 0)
    {
        row = 0;
    }
    if (row >= 0)
    {
        const QModelIndex index = m_model->index(row, 0);
        m_classList->setCurrentIndex(index);
        m_classList->selectionModel()->select(index,
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
    m_loading = false;

    if (m_model->rowCount() == 0)
    {
        m_informationState = {};
        m_detailPanel->hide();
        clearSelectedDetail();
        setStatus(pageText("No scheduled class information available."));
        return;
    }
    m_detailPanel->show();
    setStatus(QString{});
    if (row >= 0)
    {
        const auto id = m_model->classIdAt(row);
        if (id)
        {
            auto state = m_informationState.selectVisibleClass(*id, m_model->projection());
            if (state) { m_informationState = std::move(state.value()); }
            if (m_informationState.details())
            {
                renderSelectedDetails(*m_informationState.details());
            }
            else
            {
                loadSelectedDetails(*id);
            }
        }
    }
}

void SubPrepPage::handleGradeChanged(const int index)
{
    if (m_loading || index < 0 || index >= m_gradeTabs->count())
    {
        return;
    }
    m_loading = true;
    m_model->setCurrentGrade(m_gradeTabs->tabText(index));
    const int row = m_model->rowCount() > 0 ? 0 : -1;
    if (row >= 0)
    {
        const QModelIndex current = m_model->index(row, 0);
        m_classList->setCurrentIndex(current);
        m_classList->selectionModel()->select(current,
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
    m_loading = false;
    handleSelectionChanged();
}

void SubPrepPage::handleSelectionChanged()
{
    if (m_loading)
    {
        return;
    }
    const QModelIndex index = m_classList->currentIndex();
    const auto id = index.isValid() ? m_model->classIdAt(index.row()) : std::nullopt;
    if (!id)
    {
        m_informationState = {};
        clearSelectedDetail();
        return;
    }
    auto selected = m_informationState.selectVisibleClass(*id, m_model->projection());
    if (!selected)
    {
        setStatus(operationError(selected.error()), true);
        return;
    }
    m_informationState = std::move(selected.value());
    if (m_informationState.details())
    {
        renderSelectedDetails(*m_informationState.details());
    }
    else
    {
        loadSelectedDetails(*id);
    }
}

void SubPrepPage::loadSelectedDetails(const Domain::ClassId& classId)
{
    clearSelectedDetail();
    if (!m_actions.readSelectedDetails)
    {
        setStatus(pageText("Class information is unavailable."), true);
        return;
    }
    ++m_selectedDetailReadCount;
    auto details = m_actions.readSelectedDetails(classId);
    if (!details)
    {
        setStatus(operationError(details.error()), true);
        return;
    }
    auto applied = m_informationState.applyDetails(std::move(details.value()));
    if (!applied)
    {
        setStatus(operationError(applied.error()), true);
        return;
    }
    m_informationState = std::move(applied.value());
    renderSelectedDetails(*m_informationState.details());
}

void SubPrepPage::renderSelectedDetails(
    const Application::SubPrepClassDetails& details
    )
{
    const auto summary = m_model->projection().findClass(details.classId);
    if (!summary)
    {
        clearSelectedDetail();
        return;
    }
    m_classTitle->setText(QStringLiteral("%1 %2").arg(fromUtf8(summary->grade), fromUtf8(summary->level)).simplified());
    m_teacherName->setText(details.teacherDisplayName.empty()
        ? pageText("N/A") : fromUtf8(details.teacherDisplayName));
    m_studentCount->setText(QString::number(summary->studentCount.rawValue()));
    m_schedule->setText(fromUtf8(summary->meetingText));
    m_room->setText(fromUtf8(details.teacherFacilities.room));
    m_wifiName->setText(fromUtf8(details.teacherFacilities.wifiName));
    m_wifiPassword->setText(fromUtf8(details.teacherFacilities.wifiPassword));
    m_internet->setText(fromUtf8(details.teacherFacilities.internetType));
    m_zoomId->setText(fromUtf8(details.teacherFacilities.zoomId));
    m_zoomPassword->setText(fromUtf8(details.teacherFacilities.zoomPassword));
    m_projection->setText(fromUtf8(details.teacherFacilities.projectionType));
    m_classNotes->setPlainText(fromUtf8(details.classNotes));
    m_teacherNotes->setPlainText(fromUtf8(details.teacherNotes));
    m_detailPanel->setTitle(pageText("Class Information"));
}

void SubPrepPage::clearSelectedDetail()
{
    m_classTitle->clear();
    m_teacherName->clear();
    m_studentCount->clear();
    m_schedule->clear();
    m_room->clear();
    m_wifiName->clear();
    m_wifiPassword->clear();
    m_internet->clear();
    m_zoomId->clear();
    m_zoomPassword->clear();
    m_projection->clear();
    m_classNotes->clear();
    m_teacherNotes->clear();
}

void SubPrepPage::clearPageData()
{
    m_loading = true;
    clearTabs(m_gradeTabs);
    m_gradeTabs->hide();
    m_classList->selectionModel()->clear();
    m_classList->setCurrentIndex({});
    auto empty = Application::ClassSummaryProjection::create({});
    if (empty) { m_model->setProjection(std::move(empty.value())); }
    m_informationState = {};
    clearSelectedDetail();
    m_detailPanel->hide();
    m_loading = false;
}

void SubPrepPage::setStatus(const QString& message, const bool error)
{
    m_status->setText(message);
    m_status->setProperty("error", error);
    const QColor background = m_status->palette().color(QPalette::Window);
    const QColor foreground = background.lightness() < 128
        ? QColor(QStringLiteral("#ffb4ab"))
        : QColor(QStringLiteral("#b3261e"));
    m_status->setStyleSheet(error
        ? QStringLiteral("color: %1;").arg(foreground.name())
        : QString{});
}

std::vector<Application::SubPrepWeekday> SubPrepPage::selectedWeekdays() const
{
    std::vector<Application::SubPrepWeekday> days;
    for (int index = 0; index < 7; ++index)
    {
        if (m_dayChecks[index]->isEnabled() && m_dayChecks[index]->isChecked())
        {
            days.push_back(static_cast<Application::SubPrepWeekday>(
                static_cast<int>(Application::SubPrepWeekday::Monday) + index));
        }
    }
    return days;
}

void SubPrepPage::savePreferences()
{
    Application::SubPrepPreferences updated{
        .classMaterials = toUtf8(m_classMaterials->toPlainText()),
        .bookReportGrading = toUtf8(m_gradingInstructions->toPlainText()),
        .bookReportSpecialInstructions = toUtf8(m_specialInstructions->toPlainText()),
        .subComments = toUtf8(m_subComments->toPlainText())
    };
    if (updated == m_preferences)
    {
        return;
    }
    if (!m_actions.savePreferences)
    {
        setStatus(pageText("Sub Prep settings could not be saved. Your changes are still here."), true);
        return;
    }
    auto saved = m_actions.savePreferences(updated);
    if (!saved)
    {
        setStatus(operationError(saved.error()), true);
        return;
    }
    m_preferences = std::move(updated);
    setStatus(pageText("Save status"));
}

bool SubPrepPage::selectClassForTesting(const Domain::ClassId& classId)
{
    const int row = m_model->rowForClassId(classId);
    if (row < 0)
    {
        const auto summary = m_model->projection().findClass(classId);
        const QString targetGrade = summary
            ? fromUtf8(summary->grade) : QString{};
        for (int index = 0; index < m_gradeTabs->count(); ++index)
        {
            if (summary && m_gradeTabs->tabText(index) == targetGrade)
            {
                const bool wasLoading = m_loading;
                m_loading = true;
                m_gradeTabs->setCurrentIndex(index);
                m_model->setCurrentGrade(targetGrade);
                m_loading = wasLoading;
                break;
            }
        }
    }
    const int visibleRow = m_model->rowForClassId(classId);
    if (visibleRow < 0)
    {
        return false;
    }
    const QModelIndex index = m_model->index(visibleRow, 0);
    m_classList->setCurrentIndex(index);
    m_classList->selectionModel()->select(index,
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    return true;
}

SubPrepClassInformationListModel* SubPrepPage::summaryModel() const noexcept
{
    return m_model;
}

QListView* SubPrepPage::classList() const noexcept { return m_classList; }
QTabBar* SubPrepPage::gradeTabs() const noexcept { return m_gradeTabs; }

const std::optional<Domain::ClassId>& SubPrepPage::selectedClassId() const noexcept
{
    return m_informationState.selectedClassId();
}

const std::optional<Application::SubPrepClassDetails>& SubPrepPage::selectedDetails() const noexcept
{
    return m_informationState.details();
}

SubPrepPageVisualMetrics SubPrepPage::visualMetrics() const
{
    return {
        .summaryModelCount = static_cast<int>(findChildren<SubPrepClassInformationListModel*>().size()),
        .summaryRowCount = m_model->rowCount(),
        .selectedDetailPanelCount = static_cast<int>(findChildren<QGroupBox*>(QStringLiteral("subPrepSelectedDetailPanel")).size()),
        .textEditorCount = static_cast<int>(findChildren<QPlainTextEdit*>().size()),
        .summaryReadCount = m_summaryReadCount,
        .selectedDetailReadCount = m_selectedDetailReadCount,
        .scheduleReadCount = m_scheduleReadCount,
        .visibleSummaryCount = static_cast<int>(m_model->projection().classes().size()),
        .selectedDetailsRetained = m_informationState.details().has_value()
    };
}

void SubPrepPage::changeEvent(QEvent* event)
{
    if (event && event->type() == QEvent::LanguageChange)
    {
        retranslateUi();
    }
    Page::changeEvent(event);
}

void SubPrepPage::generateSubPrep()
{
    if (!m_actions.readSchedulePage || !m_actions.readPrintSource
        || !m_actions.createRosterOutputSourceReadPort)
    {
        setStatus(pageText("Sub Prep output is unavailable."), true);
        return;
    }

    auto scheduleRead = m_actions.readSchedulePage();
    ++m_scheduleReadCount;
    if (!scheduleRead)
    {
        setStatus(operationError(scheduleRead.error()), true);
        return;
    }

    const QDate today = QDate::currentDate();
    const QDate monday = today.addDays(Qt::Monday - today.dayOfWeek());
    std::vector<Domain::GregorianDate> vacationDates;
    if (m_actions.readCalendarIntervals)
    {
        const auto typedDate = Application::CalendarEventDate::fromString(
            today.toString(Qt::ISODate).toStdString());
        if (typedDate)
        {
            auto intervals = m_actions.readCalendarIntervals({.referenceDate = *typedDate});
            if (intervals)
            {
                vacationDates = Application::selectNextSubPrepVacationBlock(
                    intervals.value(), *typedDate);
            }
        }
    }

    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("subPrepOutputDialog"));
    dialog.setWindowTitle(printDialogText("Generate Sub Prep"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* daysGroup = new QGroupBox(printDialogText("Days to Include"), &dialog);
    auto* daysLayout = new QHBoxLayout(daysGroup);
    QCheckBox* outputDays[7]{};
    for (int index = 0; index < 7; ++index)
    {
        auto* check = new QCheckBox(printDialogText(DayNames[static_cast<std::size_t>(index)]), daysGroup);
        check->setObjectName(QStringLiteral("subPrepOutputDay_%1").arg(index));
        check->setProperty("day", QString::fromLatin1(DayNames[static_cast<std::size_t>(index)]));
        check->setChecked(index < 5);
        check->setVisible(index < 5 || scheduleRead.value().displayPreferences.showWeekends);
        outputDays[index] = check;
        daysLayout->addWidget(check);
    }
    layout->addWidget(daysGroup);

    auto* nextVacation = new QCheckBox(pageText("Use next vacation"), &dialog);
    nextVacation->setObjectName(QStringLiteral("subPrepNextVacationCheckBox"));
    nextVacation->setEnabled(!vacationDates.empty());
    nextVacation->setChecked(!vacationDates.empty());
    layout->addWidget(nextVacation);
    if (!vacationDates.empty())
    {
        for (QCheckBox* check : outputDays) { check->setChecked(false); }
        for (const Domain::GregorianDate& date : vacationDates)
        {
            const int dayIndex = (date.weekday() == Domain::IsoWeekday::Sunday)
                ? 6 : static_cast<int>(date.weekday()) - 1;
            if (dayIndex >= 0 && dayIndex < 7) { outputDays[dayIndex]->setChecked(true); }
        }
    }
    QObject::connect(nextVacation, &QCheckBox::toggled, &dialog, [=](const bool checked) {
        for (QCheckBox* check : outputDays) { check->setEnabled(!checked); }
        if (checked)
        {
            for (QCheckBox* check : outputDays) { check->setChecked(false); }
            for (const Domain::GregorianDate& date : vacationDates)
            {
                const int dayIndex = (date.weekday() == Domain::IsoWeekday::Sunday)
                    ? 6 : static_cast<int>(date.weekday()) - 1;
                if (dayIndex >= 0 && dayIndex < 7) { outputDays[dayIndex]->setChecked(true); }
            }
        }
    });

    auto* createFolder = new QCheckBox(printDialogText("Create Sub Prep Folder"), &dialog);
    createFolder->setObjectName(QStringLiteral("subPrepCreateFolderCheckBox"));
    createFolder->setChecked(true);
    auto* printPaper = new QCheckBox(printDialogText("Print Paper Copies"), &dialog);
    printPaper->setObjectName(QStringLiteral("subPrepPrintPaperCopies"));
    auto* openFolder = new QCheckBox(printDialogText("Open Folder After Generation"), &dialog);
    openFolder->setObjectName(QStringLiteral("subPrepOpenFolderAfterGeneration"));
    openFolder->setChecked(true);
    layout->addWidget(createFolder);
    layout->addWidget(printPaper);
    layout->addWidget(openFolder);

    auto* folderEdit = new QLineEdit(SubPrepPackageService::defaultTargetRoot(), &dialog);
    folderEdit->setObjectName(QStringLiteral("subPrepOutputFolder"));
    auto* nameEdit = new QLineEdit(&dialog);
    nameEdit->setObjectName(QStringLiteral("subPrepOutputUserName"));
    if (m_actions.readFolderUserName)
    {
        const auto name = m_actions.readFolderUserName();
        if (name) { nameEdit->setText(fromUtf8(name.value())); }
    }
    auto* folderRow = new QHBoxLayout();
    folderRow->addWidget(folderEdit, 1);
    auto* chooseFolder = new QPushButton(printDialogText("Select Folder"), &dialog);
    folderRow->addWidget(chooseFolder);
    layout->addLayout(folderRow);
    layout->addWidget(nameEdit);
    QObject::connect(chooseFolder, &QPushButton::clicked, &dialog, [&dialog, folderEdit] {
        const QString chosen = QFileDialog::getExistingDirectory(
            &dialog, printDialogText("Select Folder"), folderEdit->text());
        if (!chosen.isEmpty()) { folderEdit->setText(chosen); }
    });

    auto* rosterTemplate = new QComboBox(&dialog);
    rosterTemplate->setObjectName(QStringLiteral("subPrepRosterTemplate"));
    for (const auto id : RosterTemplateRenderer::availableTemplateIds())
    {
        rosterTemplate->addItem(RosterTemplateRenderer::templateDisplayName(id), static_cast<int>(id));
    }
    layout->addWidget(rosterTemplate);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setObjectName(QStringLiteral("subPrepOutputAccept"));
    buttons->button(QDialogButtonBox::Cancel)->setObjectName(QStringLiteral("subPrepOutputCancel"));
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
    {
        setStatus(pageText("Sub Prep generation canceled."));
        return;
    }

    QStringList selectedDays;
    QList<QDate> selectedDates;
    for (int index = 0; index < 7; ++index)
    {
        if (outputDays[index]->isChecked())
        {
            selectedDays.append(QString::fromLatin1(DayNames[static_cast<std::size_t>(index)]));
            selectedDates.append(monday.addDays(index));
        }
    }
    if (nextVacation->isChecked())
    {
        selectedDates.clear();
        for (const auto& date : vacationDates) { selectedDates.append(toQDate(date)); }
    }
    const Application::ScheduleViewMode mode =
        static_cast<Application::ScheduleViewMode>(m_mode->currentData().toInt());
    const QList<int> legacyIds = SubPrepPackageService::classIdsForDays(
        buildScheduleViewModel(
            ScheduleBuilder{}.build(scheduleRead.value().source,
                mode == Application::ScheduleViewMode::Intensive,
                {QStringLiteral("Monday"), QStringLiteral("Tuesday"), QStringLiteral("Wednesday"),
                 QStringLiteral("Thursday"), QStringLiteral("Friday"), QStringLiteral("Saturday"), QStringLiteral("Sunday")}),
            ScheduleViewRequest{
                .days = {QStringLiteral("Monday"), QStringLiteral("Tuesday"), QStringLiteral("Wednesday"),
                         QStringLiteral("Thursday"), QStringLiteral("Friday"), QStringLiteral("Saturday"), QStringLiteral("Sunday")},
                .displayMode = mode == Application::ScheduleViewMode::Intensive
                    ? Application::ScheduleDisplayMode::Intensive
                    : Application::ScheduleDisplayMode::Regular
            }), selectedDays);
    std::vector<Domain::ClassId> classIds;
    classIds.reserve(static_cast<std::size_t>(legacyIds.size()));
    for (const int id : legacyIds)
    {
        const auto typed = Domain::ClassId::fromString(std::to_string(id));
        if (typed) { classIds.push_back(*typed); }
    }
    std::vector<Application::SubPrepWeekday> selectedWeekdays;
    for (const QString& day : selectedDays)
    {
        if (const auto weekday = weekdayForLabel(day)) { selectedWeekdays.push_back(*weekday); }
    }
    if (classIds.empty() || selectedDates.empty())
    {
        setStatus(pageText("No scheduled class information available."), true);
        return;
    }

    const bool displayMode = mode == Application::ScheduleViewMode::Intensive;
    Application::SubPrepPrintSourceRequest printRequest{
        .selectedClassIds = classIds,
        .selectedDays = selectedWeekdays,
        .mode = mode
    };
    auto printSource = m_actions.readPrintSource(printRequest);
    if (!printSource)
    {
        setStatus(operationError(printSource.error()), true);
        return;
    }
    auto classInformation = SubPrepPrintSourceMapper::toClassInformation(
        printSource.value(), printRequest);
    if (!classInformation)
    {
        setStatus(operationError(classInformation.error()), true);
        return;
    }

    SubPrepPrintService::Request subPrep;
    subPrep.parent = this;
    subPrep.classInformation = std::move(classInformation.value());
    subPrep.classMaterials = m_classMaterials->toPlainText();
    subPrep.gradingInstructions = m_gradingInstructions->toPlainText();
    subPrep.specialInstructions = m_specialInstructions->toPlainText();
    subPrep.subNotes = m_subComments->toPlainText();
    const ScheduleBuildResult scheduleBuild = ScheduleBuilder{}.build(
        scheduleRead.value().source, displayMode, selectedDays);
    subPrep.schedule = buildScheduleViewModel(scheduleBuild,
        ScheduleViewRequest{
            .days = selectedDays,
            .displayMode = displayMode
                ? Application::ScheduleDisplayMode::Intensive
                : Application::ScheduleDisplayMode::Regular
        });

    if (m_actions.readPersonalZoomPreferences)
    {
        const auto zoom = m_actions.readPersonalZoomPreferences();
        if (zoom)
        {
            subPrep.zoom.loginId = fromUtf8(zoom.value().loginId.value());
            subPrep.zoom.password = fromUtf8(zoom.value().password.value());
        }
    }
    if (m_actions.readCurrentCampus && m_actions.readCampuses)
    {
        const auto selectedCampus = m_actions.readCurrentCampus();
        if (selectedCampus)
        {
            const auto campuses = m_actions.readCampuses();
            const auto campus = std::find_if(campuses.cbegin(), campuses.cend(),
                [&selectedCampus](const Application::SubPrepCampusMetadata& candidate) {
                    return candidate.id.value() == selectedCampus.value().value();
                });
            if (campus != campuses.cend())
            {
                subPrep.campus.officeNumber = fromUtf8(campus->officeNumber);
                subPrep.campus.officeWifi = fromUtf8(campus->wifiName);
                subPrep.campus.officeWifiPassword = fromUtf8(campus->wifiPassword);
                subPrep.campus.photocopierCode = fromUtf8(campus->photocopierCode);
            }
        }
    }

    auto rosterSource = m_actions.createRosterOutputSourceReadPort();
    if (!rosterSource)
    {
        setStatus(pageText("Sub Prep roster output is unavailable."), true);
        return;
    }
    SubPrepPackageService::Request packageRequest;
    packageRequest.parent = this;
    packageRequest.rosterOutputSourceReadPort = rosterSource.get();
    packageRequest.subPrep = std::move(subPrep);
    packageRequest.selectedDates = selectedDates;
    packageRequest.selectedClassIds = std::move(classIds);
    packageRequest.useIntensiveSchedule = displayMode;
    packageRequest.createFolder = createFolder->isChecked();
    packageRequest.printPaperCopies = printPaper->isChecked();
    packageRequest.openFolderAfterGeneration = openFolder->isChecked();
    packageRequest.targetRoot = folderEdit->text();
    packageRequest.userName = nameEdit->text();
    packageRequest.rosterTemplate = static_cast<RosterTemplateRenderer::TemplateId>(rosterTemplate->currentData().toInt());

    const QString finalFolderPath = QDir(packageRequest.targetRoot).filePath(
        SubPrepPackageService::datedFolderName(packageRequest.userName, selectedDates));
    if (packageRequest.createFolder && QFileInfo::exists(finalFolderPath))
    {
        const auto answer = QMessageBox::question(this,
            pageText("Replace Existing Sub Prep Folder"),
            pageText("Replace the existing folder and all of its contents?"),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel);
        if (answer != QMessageBox::Yes) { return; }
        packageRequest.replaceExisting = true;
    }

    if (packageRequest.createFolder && m_actions.saveFolderUserName)
    {
        const auto savedName = m_actions.saveFolderUserName(toUtf8(packageRequest.userName.trimmed()));
        if (!savedName)
        {
            QMessageBox::warning(this, pageText("Unable to Save Preference"),
                operationError(savedName.error()));
        }
    }
    const SubPrepPackageService::Result generated = SubPrepPackageService::generate(
        std::move(packageRequest));
    if (generated.status == SubPrepPackageService::Status::Completed)
    {
        setStatus(pageText("The package was created at:\n%1").arg(generated.outputDirectory));
    }
    else if (generated.status == SubPrepPackageService::Status::Canceled)
    {
        setStatus(pageText("Sub Prep generation canceled."));
    }
    else
    {
        setStatus(generated.message, true);
    }
}

} // namespace ClassMngr::Next::Ui
