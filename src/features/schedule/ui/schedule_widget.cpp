#include "schedule_widget.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "schedule_cell_hit_test.h"
#include "schedule_table_renderer.h"
#include "schedule_widget_delegates.h"

#include "ui/shared/widgets/text_fit_push_button.h"

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/startup_profiler.h"
#include "core/theme_service.h"
#include "data/database/workspace_store.h"
#include "domain/rules/schedule_value_parser.h"
#include "features/schedule/ui/schedule_editor_dialog.h"
#include "features/schedule/ui/schedule_print_dialog.h"
#include "features/schedule/ui/testing_assignment_dialog.h"
#include "features/schedule/services/schedule_print_service.h"
#include "features/schedule/services/schedule_output_controller.h"
#include "next/application/schedule_builder_source_snapshot.h"
#include "next/application/schedule_slot_state_read_query.h"
#include "next/application/schedule_testing_assignment_read_query.h"
#include "next/application/schedule_testing_assignment_save_use_case.h"
#include "next/application/schedule_slot_state_save_use_case.h"
#include "next/platform/application_services_schedule_builder_source_port.h"
#include "next/platform/application_services_schedule_slot_state_read_port.h"
#include "next/platform/application_services_schedule_testing_class_choices_read_port.h"
#include "next/platform/application_services_schedule_testing_assignment_read_port.h"
#include "next/platform/application_services_schedule_testing_assignment_save_port.h"
#include "next/platform/application_services_schedule_slot_state_save_port.h"
#include "next/platform/application_services_schedule_display_preferences_port.h"
#include "next/platform/application_services_schedule_display_mode_preferences_port.h"
#include "ui/shared/styles/roles.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <utility>

#include <QButtonGroup>
#include <QDebug>
#include <QDialog>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTableWidget>
#include <QVBoxLayout>

namespace
{
using ScheduleWidgetDelegates::TimeColumnDelegateObjectName;

[[nodiscard]] std::optional<
    ClassMngr::Next::Application::ScheduleWeekday
    > scheduleWeekday(const QString& value)
{
    using ClassMngr::Next::Application::ScheduleWeekday;

    const auto parsed = ScheduleValueParser::parseWeekday(value);
    if (!parsed || parsed->text != value)
    {
        return std::nullopt;
    }

    switch (parsed->value)
    {
    case ScheduleValueParser::Weekday::Monday:
        return ScheduleWeekday::Monday;
    case ScheduleValueParser::Weekday::Tuesday:
        return ScheduleWeekday::Tuesday;
    case ScheduleValueParser::Weekday::Wednesday:
        return ScheduleWeekday::Wednesday;
    case ScheduleValueParser::Weekday::Thursday:
        return ScheduleWeekday::Thursday;
    case ScheduleValueParser::Weekday::Friday:
        return ScheduleWeekday::Friday;
    case ScheduleValueParser::Weekday::Saturday:
        return ScheduleWeekday::Saturday;
    case ScheduleValueParser::Weekday::Sunday:
        return ScheduleWeekday::Sunday;
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<
    ClassMngr::Next::Application::ScheduleSlotStartMinute
    > scheduleStartMinute(
    const QString& value
    )
{
    if (value != value.trimmed())
    {
        return std::nullopt;
    }

    const qsizetype separator = value.indexOf(QLatin1Char(':'));
    if (separator < 1
        || separator > 2
        || value.size() - separator != 3)
    {
        return std::nullopt;
    }

    const auto isAsciiDigit = [](const QChar character)
    {
        return character >= QLatin1Char('0')
            && character <= QLatin1Char('9');
    };
    for (qsizetype index = 0; index < value.size(); ++index)
    {
        if (index != separator && !isAsciiDigit(value.at(index)))
        {
            return std::nullopt;
        }
    }

    constexpr std::array allowedFormats{
        ScheduleValueParser::DisplayTimeFormat::HourMinute24Hour,
        ScheduleValueParser::DisplayTimeFormat::PaddedHourMinute24Hour
    };
    const auto parsed = ScheduleValueParser::parseDisplayTime(
        value,
        allowedFormats
        );
    if (!parsed)
    {
        return std::nullopt;
    }

    const int hour = parsed->hour();
    const int minute = parsed->minute();
    if (hour < 0
        || hour >= 24
        || minute < 0
        || minute >= 60)
    {
        return std::nullopt;
    }

    return ClassMngr::Next::Application::ScheduleSlotStartMinute{
        hour * 60 + minute
    };
}

[[nodiscard]] std::optional<
    ClassMngr::Next::Application::ScheduleSlotState
    > scheduleSlotState(const QString& value)
{
    using ClassMngr::Next::Application::ScheduleSlotState;

    if (value == QStringLiteral("empty"))
    {
        return ScheduleSlotState::Empty;
    }
    if (value == QStringLiteral("essay"))
    {
        return ScheduleSlotState::Essay;
    }
    if (value == QStringLiteral("lunch"))
    {
        return ScheduleSlotState::Lunch;
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<
    ClassMngr::Next::Application::ScheduleSlotStateSaveRequest
    > scheduleSlotStateSaveRequest(const ScheduleCellHit& hit)
{
    const auto weekday = scheduleWeekday(hit.day);
    const auto startMinute = scheduleStartMinute(hit.timeLabel);
    const auto selectedState = scheduleSlotState(hit.nextState);
    const auto defaultState = scheduleSlotState(hit.defaultState);
    if (!weekday || !startMinute || !selectedState || !defaultState)
    {
        return std::nullopt;
    }

    return ClassMngr::Next::Application::ScheduleSlotStateSaveRequest{
        .weekday = *weekday,
        .startMinute = *startMinute,
        .selectedState = *selectedState,
        .defaultState = *defaultState
    };
}
}

ScheduleWidget::ScheduleWidget(
    ApplicationServices* services,
    QWidget* parent,
    ScheduleMode mode
    )
    : QWidget(parent)
    , m_services(services)
    , m_mode(mode)
{
    setProperty("role", UiRoles::Schedule);
    setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Fixed
        );

    if (StartupProfiler::isActive())
    {
        StartupProfiler::recordScheduleWidgetCreated(
            startupProfilingOwner()
            );
    }

    buildUi();
}

ScheduleWidget::~ScheduleWidget()
{
    StartupProfiler::recordScheduleWidgetDestroyed();
}

void ScheduleWidget::refreshSchedule()
{
    loadSettings();
    updateButtons();
    loadSchedule();
}

void ScheduleWidget::releaseGeneratedResources()
{
    m_interactionState.clear();
    m_scheduleModel = std::make_shared<const ScheduleViewModel>();
    m_previewModel = {};
    m_hasPreviewModel = false;
    setScheduleSourceReadStatus(false, false);
    ScheduleTableRenderer::clear(m_table);
}

void ScheduleWidget::clearDatabaseState()
{
    m_use24h = false;
    m_showKoreanTeacherEnglishNames = false;
    m_showAllHours = false;
    m_showWeekends = false;
    m_testingAffectsM1 = false;
    m_displayMode =
        ScheduleDisplayMode::Regular;
    m_regularWeekdaySlotTogglingEnabled = false;
    m_interactionState.clear();
    m_scheduleModel = std::make_shared<const ScheduleViewModel>();
    setScheduleSourceReadStatus(false, false);

    updateButtons();
}

void ScheduleWidget::retranslateUi()
{
    updateButtons();
    ScheduleTableRenderer::invalidate(m_table);
    loadSchedule();
}

ScheduleDisplayState ScheduleWidget::displayState() const
{
    ScheduleDisplayState state;
    state.use24HourTime = m_use24h;
    state.showKoreanTeacherEnglishNames =
        m_showKoreanTeacherEnglishNames;
    state.showAllHours = m_showAllHours;
    state.showWeekends = m_showWeekends;
    state.testingAffectsM1 = m_testingAffectsM1;
    state.displayMode = m_displayMode;

    return state;
}

ScheduleWidgetRuntimeMetrics ScheduleWidget::runtimeMetrics() const
{
    ScheduleWidgetRuntimeMetrics metrics;
    metrics.modelRowCount = m_scheduleModel->rows.size();
    metrics.visibleClassCount = visibleClassIds().size();

    for (const ScheduleRowView& row : m_scheduleModel->rows)
    {
        metrics.modelCellCount += row.cells.size();
        for (const ScheduleCellView& cell : row.cells)
        {
            metrics.modelEntryCount += cell.entries.size();
        }
    }

    if (!m_table)
    {
        return metrics;
    }

    metrics.tableRowCount = m_table->rowCount();
    metrics.tableColumnCount = m_table->columnCount();
    for (int row = 0; row < metrics.tableRowCount; ++row)
    {
        for (int column = 0;
             column < metrics.tableColumnCount;
             ++column)
        {
            if (m_table->item(row, column))
            {
                ++metrics.tableItemCount;
            }
            if (m_table->cellWidget(row, column))
            {
                ++metrics.tableCellWidgetCount;
            }
        }
    }

    return metrics;
}

const ScheduleViewModel& ScheduleWidget::scheduleModel() const
{
    return *m_scheduleModel;
}

QSet<int> ScheduleWidget::visibleClassIds() const
{
    QSet<int> classIds;

    for (const ScheduleRowView& row : m_scheduleModel->rows)
    {
        for (const ScheduleCellView& cell : row.cells)
        {
            for (const ScheduleEntry& entry : cell.entries)
            {
                if (entry.classId > 0)
                {
                    classIds.insert(entry.classId);
                }
            }
        }
    }

    return classIds;
}

void ScheduleWidget::setMaximumVisibleRows(
    int maximumVisibleRows
    )
{
    const int normalizedMaximum =
        std::max(0, maximumVisibleRows);

    if (m_maximumVisibleRows == normalizedMaximum)
    {
        return;
    }

    m_maximumVisibleRows = normalizedMaximum;
    loadSchedule();
}

void ScheduleWidget::setCompactPreview(
    bool compactPreview
    )
{
    if (m_compactPreview == compactPreview)
    {
        return;
    }

    m_compactPreview = compactPreview;
    loadSchedule();
}

void ScheduleWidget::setPreviewModel(
    const ScheduleViewModel& model
    )
{
    m_previewModel = std::make_shared<const ScheduleViewModel>(model);
    m_hasPreviewModel = true;
    loadSchedule();
}

void ScheduleWidget::clearPreviewModel()
{
    m_previewModel = {};
    m_hasPreviewModel = false;
    loadSchedule();
}

void ScheduleWidget::setDisplayMode(
    int modeId
    )
{
    if (
        modeId < static_cast<int>(ScheduleDisplayMode::Regular)
        || modeId > static_cast<int>(ScheduleDisplayMode::Testing)
        )
    {
        return;
    }

    const auto mode =
        static_cast<ScheduleDisplayMode>(modeId);

    if (m_displayMode == mode)
    {
        return;
    }

    if (m_services)
    {
        ClassMngr::Next::Platform::
            ApplicationServicesScheduleDisplayModePreferencesPort port(
                *m_services
                );
        ClassMngr::Next::Application::ScheduleDisplayMode storedMode =
            ClassMngr::Next::Application::ScheduleDisplayMode::Regular;
        switch (mode)
        {
        case ::ScheduleDisplayMode::Intensive:
            storedMode =
                ClassMngr::Next::Application::ScheduleDisplayMode::Intensive;
            break;
        case ::ScheduleDisplayMode::Testing:
            storedMode =
                ClassMngr::Next::Application::ScheduleDisplayMode::Testing;
            break;
        case ::ScheduleDisplayMode::Regular:
        default:
            break;
        }
        const auto saved = port.save(storedMode);
        if (!saved)
        {
            updateButtons();
            DialogServices::showWarning(
                this,
                tr("Schedule Display Mode"),
                QString::fromStdString(saved.error().message)
                );
            return;
        }
    }

    m_displayMode = mode;
    updateButtons();
    loadSchedule();
    emit displayModeChanged(m_displayMode);
}

void ScheduleWidget::onCellClicked(
    int row,
    int column
    )
{
    if (m_mode == ScheduleMode::ReadOnly)
    {
        return;
    }

    if (column <= 0 || row < 0 || row >= m_scheduleModel->rows.size()
        || column > m_scheduleModel->rows.at(row).cells.size())
    {
        return;
    }

    const ScheduleCellHit hit =
        ScheduleCellHitTest::hit(&m_scheduleModel->rows.at(row).cells.at(column - 1));
    if (hit.command == ScheduleCellCommand::None)
    {
        return;
    }
    if (hit.command == ScheduleCellCommand::EditTestingAssignment)
    {
        const TestingAssignmentView* assignment =
            m_interactionState.testingAssignment(hit.day, hit.timeLabel);
        if (assignment)
        {
            editTestingAssignment(
                hit.day,
                hit.timeLabel,
                &assignment->assignment
                );
        }
        else if (!hit.day.isEmpty() && !hit.timeLabel.isEmpty())
        {
            editTestingAssignment(hit.day, hit.timeLabel, nullptr);
        }
        return;
    }
    if (hit.command == ScheduleCellCommand::ToggleSlot)
    {
        ClassMngr::Next::Platform::
            ApplicationServicesScheduleSlotStateSavePort port(m_services);
        bool savedSuccessfully = false;

        if (port.isAvailable())
        {
            const auto request = scheduleSlotStateSaveRequest(hit);
            if (!request)
            {
                DialogServices::showWarning(
                    this,
                    tr("Update Schedule"),
                    tr("The selected schedule slot is invalid.")
                    );
                return;
            }

            const auto saved =
                ClassMngr::Next::Application::
                    ScheduleSlotStateSaveUseCase::execute(
                        *request,
                        port
                        );
            if (!saved)
            {
                DialogServices::showWarning(
                    this,
                    tr("Update Schedule"),
                    QString::fromUtf8(
                        saved.error().message.data(),
                        static_cast<qsizetype>(
                            saved.error().message.size()
                            )
                        )
                    );
                return;
            }

            savedSuccessfully = true;
        }
        loadSchedule();
        if (savedSuccessfully)
        {
            emit scheduleDataChanged();
        }
        return;
    }

    ScheduleEditorDialog dialog(
        m_services,
        hit.classId,
        this
        );

    connect(
        &dialog,
        &ScheduleEditorDialog::saved,
        this,
        [this](int savedClassId)
        {
            loadSchedule();
            emit classInfoSaved(savedClassId);
            emit scheduleDataChanged();
        }
        );

    dialog.exec();
}

void ScheduleWidget::editTestingAssignment(
    const QString& day,
    const QString& timeLabel,
    const TestingAssignment* existingAssignment
    )
{
    WorkspaceStore* const store =
        m_services
            ? m_services->workspaceStore()
            : nullptr;

    if (!store || !store->isOpen())
    {
        DialogServices::showWarning(
            this,
            tr("Testing Assignment"),
            tr("No Teacher Profile is open.")
            );
        return;
    }

    ClassMngr::Next::Platform::
        ApplicationServicesScheduleTestingClassChoicesReadPort
            testingClassChoicesReadPort(m_services);

    TestingAssignmentDialog dialog(
        testingClassChoicesReadPort,
        existingAssignment,
        this
        );

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    if (
        dialog.selectedAction()
            == TestingAssignmentDialog::Action::ManageTestingClasses
        )
    {
        emit testingClassesRequested(
            dialog.selectedClassId(),
            day,
            timeLabel
            );
        return;
    }

    const bool replaceExisting =
        existingAssignment != nullptr;
    ClassMngr::Next::Application::
        ScheduleTestingAssignmentSaveRequest request{
            .day = day.toStdU16String(),
            .startTime = timeLabel.toStdU16String(),
            .replaceExisting = replaceExisting
        };

    switch (dialog.selectedAction())
    {
    case TestingAssignmentDialog::Action::RemoveAssignment:
        request.mutation =
            ClassMngr::Next::Application::
                ScheduleTestingAssignmentMutation::RemoveAssignment;
        break;

    case TestingAssignmentDialog::Action::AssignTestingClass:
    {
        request.mutation =
            ClassMngr::Next::Application::
                ScheduleTestingAssignmentMutation::AssignTestingClass;
        const int selectedClassId = dialog.selectedClassId();
        if (selectedClassId > 0)
        {
            const std::string classId = std::to_string(selectedClassId);
            request.classId =
                ClassMngr::Next::Domain::ClassId::fromString(classId);
        }
        break;
    }

    case TestingAssignmentDialog::Action::SavePlainTesting:
        request.mutation =
            ClassMngr::Next::Application::
                ScheduleTestingAssignmentMutation::SavePlainTesting;
        request.room = dialog.room().toStdU16String();
        break;

    case TestingAssignmentDialog::Action::ManageTestingClasses:
        return;
    }

    ClassMngr::Next::Platform::
        ApplicationServicesScheduleTestingAssignmentSavePort port(m_services);
    const auto result =
        ClassMngr::Next::Application::
            ScheduleTestingAssignmentSaveUseCase::execute(
                request,
                port,
                ClassMngr::Next::Platform::
                    ApplicationServicesScheduleTestingAssignmentSavePort::
                        qtCaseInsensitiveWeekdayEquals
                );

    if (!result)
    {
        DialogServices::showWarning(
            this,
            tr("Testing Assignment"),
            QString::fromUtf8(
                result.error().message.data(),
                static_cast<qsizetype>(result.error().message.size())
                )
            );
        return;
    }

    loadSchedule();
    emit scheduleDataChanged();
}

void ScheduleWidget::printSchedule()
{
    outputSchedule(true);
}

void ScheduleWidget::saveScheduleAs()
{
    outputSchedule(false);
}

void ScheduleWidget::outputSchedule(
    bool print
    )
{
    const Theme currentTheme =
        m_services && m_services->themeService()
            ? m_services->themeService()->currentTheme()
            : SchedulePrintService::Request{}.currentTheme;

    ScheduleOutputController::execute(
        print
            ? ScheduleOutputController::Action::Print
            : ScheduleOutputController::Action::SaveAs,
        this,
        m_services,
        buildScheduleModel(),
        currentTheme,
        m_showKoreanTeacherEnglishNames
        );
}

void ScheduleWidget::buildUi()
{
    auto* layout =
        new QVBoxLayout(this);

    layout->setContentsMargins(
        0,
        0,
        0,
        0
        );
    layout->setSpacing(8);

    m_controlsWidget =
        new QWidget(this);
    m_controlsWidget->setObjectName(
        QStringLiteral("scheduleControls")
        );

    auto* controlsLayout =
        new QHBoxLayout(m_controlsWidget);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(8);

    m_modeButtonGroup =
        new QButtonGroup(this);
    m_modeButtonGroup->setExclusive(true);

    m_regularModeButton =
        new TextFitPushButton(this);
    m_regularModeButton->setObjectName(
        QStringLiteral("scheduleRegularModeButton")
        );
    m_intensiveModeButton =
        new TextFitPushButton(this);
    m_intensiveModeButton->setObjectName(
        QStringLiteral("scheduleIntensiveModeButton")
        );
    m_testingModeButton =
        new TextFitPushButton(this);
    m_testingModeButton->setObjectName(
        QStringLiteral("scheduleTestingModeButton")
        );

    const QList<QPushButton*> modeButtons{
        m_regularModeButton,
        m_intensiveModeButton,
        m_testingModeButton
    };

    for (QPushButton* button : modeButtons)
    {
        button->setCheckable(true);
        button->setMinimumWidth(96);
        button->setMinimumHeight(34);
        button->setProperty("schedule_mode_button", true);
        controlsLayout->addWidget(button);
    }

    m_modeButtonGroup->addButton(
        m_regularModeButton,
        static_cast<int>(ScheduleDisplayMode::Regular)
        );
    m_modeButtonGroup->addButton(
        m_intensiveModeButton,
        static_cast<int>(ScheduleDisplayMode::Intensive)
        );
    m_modeButtonGroup->addButton(
        m_testingModeButton,
        static_cast<int>(ScheduleDisplayMode::Testing)
        );

    controlsLayout->addStretch(1);

    m_testingClassesButton =
        new TextFitPushButton(this);
    m_testingClassesButton->setObjectName(
        QStringLiteral("scheduleTestingClassesButton")
        );
    m_testingClassesButton->setMinimumWidth(132);
    controlsLayout->addWidget(m_testingClassesButton);

    m_importButton =
        new TextFitPushButton(this);
    m_importButton->setObjectName(
        QStringLiteral("scheduleImportButton")
        );
    m_importButton->setMinimumWidth(110);
    controlsLayout->addWidget(m_importButton);

    layout->addWidget(m_controlsWidget);

    m_testingBanner =
        new QLabel(this);
    m_testingBanner->setObjectName(
        QStringLiteral("scheduleTestingBanner")
        );
    m_testingBanner->setAlignment(Qt::AlignCenter);
    m_testingBanner->setWordWrap(true);
    m_testingBanner->setMinimumHeight(36);
    layout->addWidget(m_testingBanner);

    m_sourceErrorBanner = new QWidget(this);
    m_sourceErrorBanner->setObjectName(
        QStringLiteral("scheduleSourceErrorBanner")
        );
    auto* sourceErrorLayout = new QHBoxLayout(m_sourceErrorBanner);
    sourceErrorLayout->setContentsMargins(10, 6, 10, 6);
    sourceErrorLayout->setSpacing(8);
    m_sourceErrorLabel = new QLabel(m_sourceErrorBanner);
    m_sourceErrorLabel->setObjectName(
        QStringLiteral("scheduleSourceErrorLabel")
        );
    m_sourceErrorLabel->setWordWrap(true);
    sourceErrorLayout->addWidget(m_sourceErrorLabel, 1);
    m_sourceRetryButton = new QPushButton(m_sourceErrorBanner);
    m_sourceRetryButton->setObjectName(
        QStringLiteral("scheduleSourceRetryButton")
        );
    sourceErrorLayout->addWidget(m_sourceRetryButton);
    layout->addWidget(m_sourceErrorBanner);
    m_sourceErrorBanner->setHidden(true);

    m_table =
        new QTableWidget(
            0,
            6,
            this
            );
    m_table->setObjectName(
        QStringLiteral("scheduleTable")
        );

    m_table->setProperty(
        "role",
        UiRoles::ScheduleTable
        );

    ScheduleTableRenderer::initialize(m_table);

    layout->addWidget(m_table);

    m_controlsWidget->setHidden(
        m_mode == ScheduleMode::ReadOnly
        );
    if (m_mode == ScheduleMode::ReadOnly)
    {
        layout->addStretch();
    }

    connect(
        m_testingClassesButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            emit testingClassesRequested(
                -1,
                QString(),
                QString()
                );
        }
        );

    connect(
        m_modeButtonGroup,
        &QButtonGroup::idClicked,
        this,
        &ScheduleWidget::setDisplayMode
        );

    connect(
        m_importButton,
        &QPushButton::clicked,
        this,
        &ScheduleWidget::scheduleImportRequested
        );

    connect(
        m_sourceRetryButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            loadSchedule();
        }
        );


    connect(
        m_table,
        &QTableWidget::cellClicked,
        this,
        &ScheduleWidget::onCellClicked
        );

    updateButtons();
}

void ScheduleWidget::loadSettings()
{
    ClassMngr::Next::Application::ScheduleDisplayPreferences settings;
    if (m_services)
    {
        ClassMngr::Next::Platform::
            ApplicationServicesScheduleDisplayPreferencesPort port(*m_services);
        const auto loaded = port.load();
        if (loaded)
        {
            settings = loaded.value();
        }
    }

    m_use24h = settings.use24HourTime;
    m_showWeekends = settings.showWeekends;
    m_showKoreanTeacherEnglishNames = settings.showEnglishNames;
    m_showAllHours = settings.showAllIntensiveHours;
    m_testingAffectsM1 = settings.testingAffectsM1;

    if (m_services)
    {
        ClassMngr::Next::Platform::
            ApplicationServicesScheduleDisplayModePreferencesPort port(
                *m_services
                );
        const auto storedMode = port.load();
        if (storedMode)
        {
            switch (storedMode.value())
            {
            case ClassMngr::Next::Application::ScheduleDisplayMode::Intensive:
                m_displayMode = ::ScheduleDisplayMode::Intensive;
                break;
            case ClassMngr::Next::Application::ScheduleDisplayMode::Testing:
                m_displayMode = ::ScheduleDisplayMode::Testing;
                break;
            case ClassMngr::Next::Application::ScheduleDisplayMode::Regular:
            default:
                m_displayMode = ::ScheduleDisplayMode::Regular;
                break;
            }
        }
    }
}

void ScheduleWidget::loadSchedule()
{
    if (!m_table)
    {
        return;
    }

    const bool profileStartup = StartupProfiler::isActive();
    const QString profilingOwner = profileStartup
        ? startupProfilingOwner()
        : QString();
    QElapsedTimer renderTimer;
    if (profileStartup)
    {
        renderTimer.start();
        StartupProfiler::recordScheduleRenderStarted(profilingOwner);
    }

    if (m_hasPreviewModel)
    {
        setScheduleSourceReadStatus(false, false);
        m_scheduleModel = m_previewModel;
    }
    else
    {
        m_scheduleModel =
            std::make_shared<const ScheduleViewModel>(buildScheduleModel());
    }

    const ScheduleTableRenderMetrics metrics =
        ScheduleTableRenderer::render(
        m_table,
        m_scheduleModel,
        {
            this,
            m_maximumVisibleRows,
            m_compactPreview,
            m_showKoreanTeacherEnglishNames
        }
        );

    if (profileStartup)
    {
        StartupProfiler::recordScheduleRenderCompleted(
            profilingOwner,
            renderTimer.elapsed(),
            metrics.tableItemsCreated,
            metrics.cellWidgetsCreated,
            metrics.cellWidgetsRemoved,
            metrics.cellWidgetsQueuedForDeletion,
            metrics.fullRender
            );
    }
}

QString ScheduleWidget::startupProfilingOwner() const
{
    QString ownerType = QStringLiteral("unowned");
    for (const QWidget* owner = parentWidget(); owner; owner = owner->parentWidget())
    {
        const QString candidate =
            QString::fromLatin1(owner->metaObject()->className());
        if (candidate != QStringLiteral("QWidget")
            && candidate != QStringLiteral("QScrollArea"))
        {
            ownerType = candidate;
            break;
        }
    }
    const QString mode = m_mode == ScheduleMode::Interactive
        ? QStringLiteral("interactive")
        : QStringLiteral("read-only");
    return QStringLiteral("owner=%1; mode=%2").arg(ownerType, mode);
}

void ScheduleWidget::updateButtons()
{
    if (
        !m_regularModeButton
        || !m_intensiveModeButton
        || !m_testingModeButton
        || !m_testingClassesButton
        || !m_importButton
        || !m_testingBanner
        || !m_sourceErrorBanner
        || !m_sourceErrorLabel
        || !m_sourceRetryButton
        )
    {
        return;
    }

    const QSignalBlocker regularBlocker(m_regularModeButton);
    const QSignalBlocker intensiveBlocker(m_intensiveModeButton);
    const QSignalBlocker testingBlocker(m_testingModeButton);

    m_regularModeButton->setText(tr("Regular"));
    m_intensiveModeButton->setText(tr("Intensive"));
    m_testingModeButton->setText(tr("Testing"));
    m_regularModeButton->setChecked(
        m_displayMode == ScheduleDisplayMode::Regular
        );
    m_intensiveModeButton->setChecked(
        m_displayMode == ScheduleDisplayMode::Intensive
        );
    m_testingModeButton->setChecked(
        m_displayMode == ScheduleDisplayMode::Testing
        );

    m_testingClassesButton->setText(
        tr("Testing Classes")
        );

    m_importButton->setText(
        tr("Import")
        );

    updateScheduleSourceReadStatus();

    const bool testing =
        m_displayMode == ScheduleDisplayMode::Testing;
    m_testingBanner->setVisible(testing);
    m_testingClassesButton->setVisible(
        testing
        && m_mode == ScheduleMode::Interactive
        );

    if (testing)
    {
        m_testingBanner->setText(
            m_testingAffectsM1
                ? tr("Testing View — M1, M2, and M3 classes are hidden")
                : tr("Testing View — M2 and M3 classes are hidden; M1 classes remain")
            );

        const bool dark =
            palette()
                .color(QPalette::Window)
                .lightness() < 128;
        m_testingBanner->setStyleSheet(
            dark
                ? QStringLiteral(
                    "QLabel {"
                    "background:#4B3D20;"
                    "color:#FFF2C2;"
                    "border:1px solid #8D7339;"
                    "border-radius:6px;"
                    "padding:7px;"
                    "font-weight:600;"
                    "}"
                    )
                : QStringLiteral(
                    "QLabel {"
                    "background:#FFF4CC;"
                    "color:#503B00;"
                    "border:1px solid #D5A52E;"
                    "border-radius:6px;"
                    "padding:7px;"
                    "font-weight:600;"
                    "}"
                    )
            );
    }
}

void ScheduleWidget::setScheduleSourceReadStatus(
    const bool visible,
    const bool retryable
    )
{
    m_sourceErrorVisible = visible && !m_hasPreviewModel;
    m_sourceErrorRetryable = m_sourceErrorVisible && retryable;
    updateScheduleSourceReadStatus();
}

void ScheduleWidget::updateScheduleSourceReadStatus()
{
    if (!m_sourceErrorBanner
        || !m_sourceErrorLabel
        || !m_sourceRetryButton)
    {
        return;
    }

    m_sourceErrorLabel->setText(
        tr("Schedule data could not be loaded.")
        );
    m_sourceRetryButton->setText(tr("Retry"));
    m_sourceRetryButton->setVisible(
        m_sourceErrorVisible && m_sourceErrorRetryable
        );
    m_sourceErrorBanner->setVisible(
        m_sourceErrorVisible && !m_hasPreviewModel
        );

    const bool dark =
        palette().color(QPalette::Window).lightness() < 128;
    m_sourceErrorBanner->setStyleSheet(
        dark
            ? QStringLiteral(
                "QWidget#scheduleSourceErrorBanner {"
                "background:#402626;"
                "border:1px solid #774343;"
                "border-radius:6px;"
                "}"
                "QLabel#scheduleSourceErrorLabel {"
                "background:transparent;"
                "border:none;"
                "color:#FFE3E3;"
                "}"
                )
            : QStringLiteral(
                "QWidget#scheduleSourceErrorBanner {"
                "background:#FCE8E8;"
                "border:1px solid #D99A9A;"
                "border-radius:6px;"
                "}"
                "QLabel#scheduleSourceErrorLabel {"
                "background:transparent;"
                "border:none;"
                "color:#5A2020;"
                "}"
                )
        );
}

void ScheduleWidget::reloadSlotStates()
{
    ClassMngr::Next::Platform::
        ApplicationServicesScheduleSlotStateReadPort readPort(m_services);
    const auto states = ClassMngr::Next::Application::
        ScheduleSlotStateReadQueryHandler::execute(
            {},
            readPort
            );
    if (!states)
    {
        if (states.error().code == ClassMngr::Next::Domain::ErrorCode::NotFound)
        {
            return;
        }

        const QString error = QString::fromUtf8(
            states.error().message.data(),
            static_cast<qsizetype>(states.error().message.size())
            );
        qWarning()
            << "Failed to load intensive slot states:"
            << error;
        DialogServices::showWarning(
            this,
            tr("Schedule"),
            error
            );
        return;
    }

    QList<IntensiveSlotState> qtStates;
    qtStates.reserve(static_cast<qsizetype>(states.value().rows.size()));
    for (const auto& row : states.value().rows)
    {
        qtStates.append({
            .day = QString::fromStdU16String(row.identity.day()),
            .startTime = QString::fromStdU16String(
                row.identity.startTime()
                ),
            .state = QString::fromStdU16String(row.state)
        });
    }

    m_interactionState.setSlotStates(qtStates);
}

void ScheduleWidget::reloadTestingBlocks()
{
    ClassMngr::Next::Platform::
        ApplicationServicesScheduleTestingAssignmentReadPort port(
            m_services
            );
    const auto loaded =
        ClassMngr::Next::Application::
            ScheduleTestingAssignmentReadQueryHandler::execute(
                {},
                port
                );

    if (!loaded)
    {
        if (loaded.error().code == ClassMngr::Next::Domain::ErrorCode::NotFound)
        {
            m_interactionState.clearTestingAssignments();
            return;
        }

        const QString error = QString::fromUtf8(
            loaded.error().message.data(),
            static_cast<qsizetype>(loaded.error().message.size())
            );
        qWarning()
            << "Failed to load testing blocks:"
            << error;
        DialogServices::showWarning(
            this,
            tr("Testing Layout"),
            error
            );
        return;
    }

    QMap<QString, TestingAssignmentView> loadedAssignments;

    for (const auto& row : loaded.value().rows)
    {
        const int classId = std::stoi(row.classId.value());
        if (classId > 0 && !row.specialClass)
        {
            qWarning()
                << "Failed to load assigned testing class:"
                << tr("The testing class was not found.");
            continue;
        }

        TestingAssignmentView view;
        const QString day = QString::fromStdU16String(
            row.assignmentKey.day()
            );
        const QString startTime =
            QString::fromStdU16String(row.assignmentKey.startTime());
        view.assignment.day = day;
        view.assignment.startTime = startTime;
        view.assignment.kind = classId > 0
            ? TestingAssignmentKind::SpecialClass
            : TestingAssignmentKind::PlainTesting;
        view.assignment.room = QString::fromStdU16String(row.room);
        view.assignment.classId = classId;

        if (classId > 0 && row.specialClass)
        {
            ScheduleEntry entry;
            entry.classId = classId;
            entry.kind = ScheduleEntryKind::TestingClass;
            entry.className = QString::fromStdU16String(
                row.specialClass->name
                );
            entry.teacherKr = QString::fromStdU16String(
                row.specialClass->teacherKoreanName
                );
            entry.teacherEn = QString::fromStdU16String(
                row.specialClass->teacherEnglishName
                );
            entry.teacherPreferredName = QString::fromStdU16String(
                row.specialClass->teacherPreferredName
                );
            entry.roomNumber = QString::fromStdU16String(
                row.specialClass->room
                );
            entry.classGrade = QString::fromStdU16String(
                row.specialClass->grade
                );
            entry.classLevel = QString::fromStdU16String(
                row.specialClass->level
                );
            entry.classColor = QString::fromStdU16String(
                row.specialClass->classColor
                );
            entry.fontColor = QString::fromStdU16String(
                row.specialClass->fontColor
                );
            view.testingClassEntry = entry;
        }

        loadedAssignments.insert(
            scheduleSlotKey(day, startTime),
            view
            );
    }

    m_interactionState.setTestingAssignments(
        std::move(loadedAssignments)
        );
}

ScheduleViewRequest ScheduleWidget::buildScheduleViewRequest() const
{
    ScheduleViewRequest request;
    request.days =
        visibleScheduleDays(
            m_showWeekends
            );
    request.slotStateOverrides = m_interactionState.slotStates();
    request.testingAssignments = m_interactionState.testingAssignments();
    request.use24h =
        m_use24h;
    request.displayMode =
        m_displayMode;
    request.testingAffectsM1 =
        m_testingAffectsM1;
    request.regularWeekdaySlotTogglingEnabled =
        m_regularWeekdaySlotTogglingEnabled;
    request.rowFilter =
        m_displayMode == ScheduleDisplayMode::Intensive
        && !m_showAllHours
            ? ScheduleRowFilter::TrimEmptyOuterRows
            : ScheduleRowFilter::None;

    return request;
}

ScheduleViewModel ScheduleWidget::buildScheduleModel()
{
    reloadSlotStates();
    reloadTestingBlocks();

    const ScheduleViewRequest request =
        buildScheduleViewRequest();

    const ScheduleBuildResult unavailableResult{
        .days = request.days
    };

    ClassMngr::Next::Platform::
        ApplicationServicesScheduleBuilderSourcePort readPort(m_services);
    const auto source = ClassMngr::Next::Application::
        ScheduleBuilderSourceQueryHandler::execute(
            {},
            readPort
            );
    if (!source)
    {
        if (
            source.error().code
            != ClassMngr::Next::Domain::ErrorCode::NotFound
            )
        {
            qWarning() << QString::fromUtf8(
                source.error().message.data(),
                static_cast<qsizetype>(source.error().message.size())
                );
            setScheduleSourceReadStatus(
                true,
                source.error().recoverable
                );
        }
        else
        {
            setScheduleSourceReadStatus(false, false);
        }
        return buildScheduleViewModel(
            unavailableResult,
            request
            );
    }

    setScheduleSourceReadStatus(false, false);

    const ScheduleBuilder builder;
    const ScheduleBuildResult result = builder.build(
        source.value(),
        scheduleModeUsesIntensiveTimes(
            request.displayMode
            ),
        request.days
        );
    return buildScheduleViewModel(
        result,
        request
        );
}
