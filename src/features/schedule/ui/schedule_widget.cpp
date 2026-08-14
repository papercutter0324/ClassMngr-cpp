#include "schedule_widget.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "schedule_cell_hit_test.h"
#include "schedule_table_renderer.h"
#include "schedule_widget_delegates.h"

#include "ui/shared/widgets/text_fit_push_button.h"
#include "ui/shared/widgets/navigation_settings_button.h"

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/theme_service.h"
#include "features/schedule/schedule_display_mode_preferences.h"
#include "features/schedule/ui/schedule_editor_dialog.h"
#include "features/schedule/ui/schedule_print_dialog.h"
#include "features/schedule/ui/schedule_settings_dialog.h"
#include "features/schedule/ui/testing_assignment_dialog.h"
#include "features/schedule/services/schedule_print_service.h"
#include "features/schedule/services/schedule_output_controller.h"
#include "ui/shared/styles/roles.h"

#include <algorithm>
#include <utility>

#include <QButtonGroup>
#include <QDebug>
#include <QDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTableWidget>
#include <QVariant>
#include <QVBoxLayout>

namespace
{
using ScheduleWidgetDelegates::TimeColumnDelegateObjectName;
namespace SettingsKeys
{
const QString Use24HourTime =
    QStringLiteral("schedule_use_24h");
const QString ShowKoreanTeacherEnglishNames =
    QStringLiteral("schedule_show_korean_teacher_english_names");
const QString ShowWeekends =
    QStringLiteral("schedule_show_weekends");
const QString ShowAllHoursV2 =
    QStringLiteral("schedule_show_all_hours_v2");
const QString TestingAffectsM1 =
    QStringLiteral("schedule_testing_affects_m1");
}

bool settingToBool(
    const QVariant& value,
    bool defaultValue
    )
{
    if (!value.isValid())
    {
        return defaultValue;
    }

    const QString text =
        value.toString().trimmed().toLower();

    if (text == QStringLiteral("true") || text == QStringLiteral("1"))
    {
        return true;
    }

    if (text == QStringLiteral("false") || text == QStringLiteral("0"))
    {
        return false;
    }

    return value.toBool();
}

void saveBoolSetting(
    SettingsService* settingsService,
    const QString& key,
    bool value
    )
{
    if (!settingsService || !settingsService->isAvailable())
    {
        return;
    }

    settingsService->save(
        key,
        value
            ? QStringLiteral("true")
            : QStringLiteral("false")
        );
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

    loadSettings();
    buildUi();
    loadSchedule();
}

void ScheduleWidget::refreshSchedule()
{
    loadSettings();
    updateButtons();
    loadSchedule();
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
    m_scheduleModel = {};

    updateButtons();
    loadSchedule();
}

void ScheduleWidget::retranslateUi()
{
    updateButtons();
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

ScheduleViewModel ScheduleWidget::scheduleModel() const
{
    return m_scheduleModel;
}

QSet<int> ScheduleWidget::visibleClassIds() const
{
    QSet<int> classIds;

    for (const ScheduleRowView& row : m_scheduleModel.rows)
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
    m_previewModel = model;
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

    m_displayMode = mode;

    ScheduleDisplayModePreferences::save(
        m_services
            ? m_services->settingsService()
            : nullptr,
        m_displayMode
        );

    updateButtons();
    loadSchedule();
    emit displayModeChanged(m_displayMode);
}

void ScheduleWidget::openSettings()
{
    ScheduleSettingsValues initial;
    initial.use24HourTime = m_use24h;
    initial.showEnglishNames =
        m_showKoreanTeacherEnglishNames;
    initial.showWeekends = m_showWeekends;
    initial.showAllIntensiveHours = m_showAllHours;
    initial.testingAffectsM1 = m_testingAffectsM1;

    ScheduleSettingsDialog dialog(
        m_services
            ? m_services->scheduleService()
            : nullptr,
        initial,
        this
        );

    connect(
        &dialog,
        &ScheduleSettingsDialog::testingBlocksCleared,
        this,
        [this]()
        {
            m_interactionState.clearTestingAssignments();
            loadSchedule();
        }
        );

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const ScheduleSettingsValues values =
        dialog.values();
    m_use24h = values.use24HourTime;
    m_showKoreanTeacherEnglishNames =
        values.showEnglishNames;
    m_showWeekends = values.showWeekends;
    m_showAllHours =
        values.showAllIntensiveHours;
    m_testingAffectsM1 =
        values.testingAffectsM1;

    auto* settingsService =
        m_services
            ? m_services->settingsService()
            : nullptr;

    if (settingsService && settingsService->isAvailable())
    {
        saveBoolSetting(
            settingsService,
            SettingsKeys::Use24HourTime,
            m_use24h
            );
        saveBoolSetting(
            settingsService,
            SettingsKeys::ShowKoreanTeacherEnglishNames,
            m_showKoreanTeacherEnglishNames
            );
        saveBoolSetting(
            settingsService,
            SettingsKeys::ShowWeekends,
            m_showWeekends
            );
        saveBoolSetting(
            settingsService,
            SettingsKeys::ShowAllHoursV2,
            m_showAllHours
            );
        saveBoolSetting(
            settingsService,
            SettingsKeys::TestingAffectsM1,
            m_testingAffectsM1
            );
    }

    updateButtons();
    loadSchedule();
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

    if (column == 0)
    {
        return;
    }

    const ScheduleCellHit hit =
        ScheduleCellHitTest::hit(m_table->cellWidget(row, column));
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
        auto* scheduleService =
            m_services
                ? m_services->scheduleService()
                : nullptr;

        if (scheduleService && scheduleService->isAvailable())
        {
            const Status saved = scheduleService->saveIntensiveSlotState(
                hit.day,
                hit.timeLabel,
                hit.nextState,
                hit.defaultState
                );
            if (!saved)
            {
                DialogServices::showWarning(
                    this,
                    tr("Update Schedule"),
                    saved.error()
                    );
                return;
            }
        }
        loadSchedule();
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
    auto* scheduleService =
        m_services
            ? m_services->scheduleService()
            : nullptr;

    if (!scheduleService || !scheduleService->isAvailable())
    {
        DialogServices::showWarning(
            this,
            tr("Testing Assignment"),
            tr("No Teacher Profile is open.")
            );
        return;
    }

    TestingAssignmentDialog dialog(
        scheduleService,
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
    Status result;

    switch (dialog.selectedAction())
    {
    case TestingAssignmentDialog::Action::RemoveAssignment:
        result =
            scheduleService->deleteTestingAssignment(
                day,
                timeLabel
                );
        break;

    case TestingAssignmentDialog::Action::AssignTestingClass:
        result =
            scheduleService->assignTestingClass(
                day,
                timeLabel,
                dialog.selectedClassId(),
                replaceExisting
                );
        break;

    case TestingAssignmentDialog::Action::SavePlainTesting:
        result =
            scheduleService->saveTestingBlock(
                day,
                timeLabel,
                dialog.room(),
                replaceExisting
                );
        break;

    case TestingAssignmentDialog::Action::ManageTestingClasses:
        return;
    }

    if (!result)
    {
        DialogServices::showWarning(
            this,
            tr("Testing Assignment"),
            result.error()
            );
        return;
    }

    loadSchedule();
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
    ScheduleOutputController::execute(
        print
            ? ScheduleOutputController::Action::Print
            : ScheduleOutputController::Action::SaveAs,
        this,
        m_services,
        buildScheduleModel(),
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

    m_settingsButton =
        new NavigationSettingsButton(this);
    m_settingsButton->setObjectName(
        QStringLiteral("scheduleSettingsButton")
        );
    m_settingsButton->setAccessibleName(
        tr("Schedule Settings")
        );
    m_settingsButton->setToolTip(
        tr("Schedule Settings")
        );
    controlsLayout->addWidget(m_settingsButton);

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
        m_settingsButton,
        &NavigationSettingsButton::clicked,
        this,
        &ScheduleWidget::openSettings
        );

    connect(
        m_importButton,
        &QPushButton::clicked,
        this,
        &ScheduleWidget::scheduleImportRequested
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
    auto* settingsService =
        m_services
            ? m_services->settingsService()
            : nullptr;

    if (!settingsService || !settingsService->isAvailable())
    {
        return;
    }

    m_use24h =
        settingToBool(
            settingsService->load(
                SettingsKeys::Use24HourTime,
                QStringLiteral("false")
                ),
            false
            );

    m_showWeekends =
        settingToBool(
            settingsService->load(
                SettingsKeys::ShowWeekends,
                QStringLiteral("false")
                ),
            false
            );

    m_showKoreanTeacherEnglishNames =
        settingToBool(
            settingsService->load(
                SettingsKeys::ShowKoreanTeacherEnglishNames,
                QStringLiteral("false")
                ),
            false
            );

    m_showAllHours =
        settingToBool(
            settingsService->load(
                SettingsKeys::ShowAllHoursV2,
                QStringLiteral("false")
                ),
            false
            );

    m_testingAffectsM1 =
        settingToBool(
            settingsService->load(
                SettingsKeys::TestingAffectsM1,
                QStringLiteral("false")
                ),
            false
            );

    m_displayMode =
        ScheduleDisplayModePreferences::load(
            settingsService
            );
}

void ScheduleWidget::loadSchedule()
{
    if (!m_table)
    {
        return;
    }

    m_scheduleModel =
        buildScheduleModel();

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
}

void ScheduleWidget::updateButtons()
{
    if (
        !m_regularModeButton
        || !m_intensiveModeButton
        || !m_testingModeButton
        || !m_settingsButton
        || !m_testingClassesButton
        || !m_importButton
        || !m_testingBanner
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

    m_settingsButton->setText(
        QStringLiteral("\u2699")
        );
    m_settingsButton->setFont(
        FontManager::getUiFont(
            18,
            QFont::DemiBold
            )
        );
    m_settingsButton->setAccessibleName(
        tr("Schedule Settings")
        );
    m_settingsButton->setToolTip(
        tr("Schedule Settings")
        );
    m_testingClassesButton->setText(
        tr("Testing Classes")
        );

    m_importButton->setText(
        tr("Import")
        );

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

void ScheduleWidget::reloadSlotStates()
{
    auto* scheduleService =
        m_services
            ? m_services->scheduleService()
            : nullptr;

    if (!scheduleService || !scheduleService->isAvailable())
    {
        return;
    }

    const QList<IntensiveSlotState> states =
        scheduleService->intensiveSlotStates();

    m_interactionState.setSlotStates(states);
}

void ScheduleWidget::reloadTestingBlocks()
{
    auto* scheduleService =
        m_services
            ? m_services->scheduleService()
            : nullptr;
    auto* classService =
        m_services
            ? m_services->classService()
            : nullptr;

    if (!scheduleService || !scheduleService->isAvailable())
    {
        m_interactionState.clearTestingAssignments();
        return;
    }

    const Result<QList<TestingAssignment>> assignments =
        scheduleService->testingAssignments();

    if (!assignments)
    {
        qWarning()
            << "Failed to load testing blocks:"
            << assignments.error();
        DialogServices::showWarning(
            this,
            tr("Testing Layout"),
            assignments.error()
            );
        return;
    }

    QMap<QString, TestingAssignmentView> loadedAssignments;

    for (const TestingAssignment& assignment : *assignments)
    {
        TestingAssignmentView view;
        view.assignment = assignment;

        if (
            assignment.kind
                == TestingAssignmentKind::SpecialClass
            )
        {
            if (!classService || !classService->isAvailable())
            {
                continue;
            }

            const Result<TestingClass> testingClass =
                scheduleService->testingClass(
                    assignment.classId
                    );

            if (!testingClass)
            {
                qWarning()
                    << "Failed to load assigned testing class:"
                    << testingClass.error();
                continue;
            }

            const ClassInfo info =
                classService->classInfo(
                    assignment.classId
                    );
            ScheduleEntry entry;
            entry.classId = assignment.classId;
            entry.kind = ScheduleEntryKind::TestingClass;
            entry.className = testingClass->name;
            entry.teacherKr = info.teacherKr;
            entry.teacherEn = info.teacherEn;
            entry.teacherPreferredName =
                info.teacherPreferredName;
            entry.roomNumber = testingClass->room;
            entry.classGrade = testingClass->grade;
            entry.classLevel = testingClass->level;
            entry.classColor = testingClass->classColor;
            entry.fontColor = testingClass->fontColor;
            view.testingClassEntry = entry;
        }

        loadedAssignments.insert(
            scheduleSlotKey(
                assignment.day,
                assignment.startTime
                ),
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
    if (m_hasPreviewModel)
    {
        return m_previewModel;
    }

    reloadSlotStates();
    reloadTestingBlocks();

    const ScheduleViewRequest request =
        buildScheduleViewRequest();

    ScheduleBuilder builder(
        m_services
            ? m_services->classService()
            : nullptr
        );

    const Result<ScheduleBuildResult> result =
        builder.build(
            scheduleModeUsesIntensiveTimes(
                request.displayMode
                ),
            request.days
            );

    if (!result)
    {
        qWarning() << result.error();
        return buildScheduleViewModel(
            ScheduleBuildResult{.days = request.days},
            request
            );
    }

    return buildScheduleViewModel(
        *result,
        request
        );
}
