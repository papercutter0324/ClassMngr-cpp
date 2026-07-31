#include "schedule_widget.h"

#include "ui/shared/widgets/text_fit_push_button.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/theme_service.h"
#include "data/data_service.h"
#include "features/schedule/ui/schedule_editor_dialog.h"
#include "features/schedule/ui/schedule_print_dialog.h"
#include "features/schedule/ui/schedule_settings_dialog.h"
#include "features/schedule/ui/testing_block_dialog.h"
#include "features/schedule/services/schedule_print_service.h"
#include "ui/shared/styles/roles.h"

#include <algorithm>
#include <utility>

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QColor>
#include <QDebug>
#include <QDialog>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPolygon>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVariant>
#include <QVBoxLayout>

namespace
{
constexpr int TimeColumnWidth = 90;
constexpr int HeaderHeight = 42;
constexpr int RowHeight = 60;
constexpr int CompactPreviewTimeColumnWidth = 84;
constexpr int CompactPreviewHeaderHeight = 36;
constexpr int CompactPreviewRowHeight = 50;
constexpr int CompactPreviewRowBaseHeight = 24;
constexpr int CompactPreviewRowHeightPerEntry = 40;
constexpr int PreviewFontSizeReduction = 4;
constexpr int PreviewTimeFontSizeReduction = 2;
constexpr int TimeCellRole = Qt::UserRole + 1;
const QString TimeColumnDelegateObjectName =
    QStringLiteral("scheduleTimeColumnDelegate");

class TimeColumnDelegate final : public QStyledItemDelegate
{
public:
    explicit TimeColumnDelegate(
        QObject* parent
        )
        : QStyledItemDelegate(parent)
    {
        setObjectName(TimeColumnDelegateObjectName);
    }

    void paint(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index
        ) const override
    {
        if (!index.data(TimeCellRole).toBool())
        {
            QStyledItemDelegate::paint(
                painter,
                option,
                index
                );
            return;
        }

        QStyleOptionViewItem timeOption(option);
        timeOption.state &=
            ~(QStyle::State_MouseOver | QStyle::State_Selected);
        timeOption.state |=
            QStyle::State_HasFocus;

        QStyledItemDelegate::paint(
            painter,
            timeOption,
            index
            );
    }
};

class TestingCellLabel final : public QLabel
{
public:
    using QLabel::QLabel;

protected:
    void paintEvent(
        QPaintEvent* event
        ) override
    {
        QLabel::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(
            palette().color(QPalette::Highlight)
            );

        constexpr int MarkerSize = 18;
        QPolygon marker;
        marker
            << QPoint(width() - MarkerSize, 0)
            << QPoint(width(), 0)
            << QPoint(width(), MarkerSize);
        painter.drawPolygon(marker);
    }
};

namespace SettingsKeys
{
const QString Use24HourTime =
    QStringLiteral("schedule_use_24h");
const QString ShowKoreanTeacherEnglishNames =
    QStringLiteral("schedule_show_korean_teacher_english_names");
const QString ShowWeekends =
    QStringLiteral("schedule_show_weekends");
const QString ShowIntensive =
    QStringLiteral("schedule_show_intensive");
const QString ShowAllHoursV2 =
    QStringLiteral("schedule_show_all_hours_v2");
const QString DisplayMode =
    QStringLiteral("schedule_display_mode");
const QString TestingAffectsM1 =
    QStringLiteral("schedule_testing_affects_m1");
}

DataService* openDataService(
    ApplicationServices* services
    )
{
    auto* dataService =
        services
            ? services->dataService()
            : nullptr;

    return dataService && dataService->isOpen()
        ? dataService
        : nullptr;
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
    DataService* dataService,
    const QString& key,
    bool value
    )
{
    if (!dataService)
    {
        return;
    }

    dataService->saveSetting(
        key,
        value
            ? QStringLiteral("true")
            : QStringLiteral("false")
        );
}

QString escaped(
    const QString& text
    )
{
    return text.toHtmlEscaped();
}

QString displayModeSetting(
    ScheduleDisplayMode mode
    )
{
    switch (mode)
    {
    case ScheduleDisplayMode::Intensive:
        return QStringLiteral("intensive");

    case ScheduleDisplayMode::Testing:
        return QStringLiteral("testing");

    case ScheduleDisplayMode::Regular:
        return QStringLiteral("regular");
    }

    return QStringLiteral("regular");
}

ScheduleDisplayMode displayModeFromSetting(
    const QVariant& value,
    bool legacyIntensive
    )
{
    const QString normalized =
        value.toString().trimmed().toLower();

    if (normalized == QStringLiteral("intensive"))
    {
        return ScheduleDisplayMode::Intensive;
    }

    if (normalized == QStringLiteral("testing"))
    {
        return ScheduleDisplayMode::Testing;
    }

    if (normalized == QStringLiteral("regular"))
    {
        return ScheduleDisplayMode::Regular;
    }

    return legacyIntensive
        ? ScheduleDisplayMode::Intensive
        : ScheduleDisplayMode::Regular;
}

QString classCellStyle(
    const QString& classColor,
    const QString& fontColor,
    int padding,
    int borderRadius
    )
{
    return QStringLiteral(
        "QLabel {"
        "background:%1;"
        "color:%2;"
        "padding:%3px;"
        "border-radius:%4px;"
        "}"
        )
        .arg(
            classColor.isEmpty()
                ? QStringLiteral("#FFFFFF")
                : classColor
            )
        .arg(
            fontColor.isEmpty()
                ? QStringLiteral("#000000")
                : fontColor
            )
        .arg(padding)
        .arg(borderRadius
            );
}

QString joinedEnglishLine(
    const ScheduleEntry& entry
    )
{
    QStringList parts;

    if (!entry.classGrade.trimmed().isEmpty())
    {
        parts.append(
            entry.classGrade.trimmed()
            );
    }

    if (!entry.classLevel.trimmed().isEmpty())
    {
        parts.append(
            entry.classLevel.trimmed()
            );
    }

    return parts.join(
        QStringLiteral(" - ")
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
    m_intensiveSlotStates.clear();
    m_testingBlockRooms.clear();
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

    if (auto* dataService = openDataService(m_services))
    {
        dataService->saveSetting(
            SettingsKeys::DisplayMode,
            displayModeSetting(m_displayMode)
            );
    }

    updateButtons();
    loadSchedule();
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
        openDataService(m_services),
        initial,
        this
        );

    connect(
        &dialog,
        &ScheduleSettingsDialog::testingBlocksCleared,
        this,
        [this]()
        {
            m_testingBlockRooms.clear();
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

    if (auto* dataService = openDataService(m_services))
    {
        saveBoolSetting(
            dataService,
            SettingsKeys::Use24HourTime,
            m_use24h
            );
        saveBoolSetting(
            dataService,
            SettingsKeys::ShowKoreanTeacherEnglishNames,
            m_showKoreanTeacherEnglishNames
            );
        saveBoolSetting(
            dataService,
            SettingsKeys::ShowWeekends,
            m_showWeekends
            );
        saveBoolSetting(
            dataService,
            SettingsKeys::ShowAllHoursV2,
            m_showAllHours
            );
        saveBoolSetting(
            dataService,
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

    QWidget* widget =
        m_table->cellWidget(
            row,
            column
            );

    if (!widget)
    {
        return;
    }

    if (widget->property("is_slot_cell").toBool())
    {
        const QString day =
            widget->property("day").toString();

        const QString timeLabel =
            widget->property("time_label").toString();

        const QString currentState =
            widget->property("slot_state").toString();

        if (currentState == scheduleTestingSlotState())
        {
            editTestingBlock(
                day,
                timeLabel,
                widget->property("testing_room").toString(),
                true
                );
            return;
        }

        if (
            widget
                ->property("testing_block_creation_enabled")
                .toBool()
            )
        {
            editTestingBlock(
                day,
                timeLabel,
                QString(),
                false
                );
            return;
        }

        if (!widget->property("slot_toggling_enabled").toBool())
        {
            return;
        }

        const QString defaultState =
            widget->property("default_slot_state").toString();

        const QString newState =
            nextScheduleSlotState(
                currentState
                );

        if (auto* dataService = openDataService(m_services))
        {
            dataService->saveIntensiveSlotState(
                day,
                timeLabel,
                newState,
                defaultState
                );
        }

        loadSchedule();
        return;
    }

    const int classId =
        widget
            ->property("class_id")
            .toInt();

    if (classId <= 0)
    {
        return;
    }

    ScheduleEditorDialog dialog(
        m_services,
        classId,
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

void ScheduleWidget::editTestingBlock(
    const QString& day,
    const QString& timeLabel,
    const QString& room,
    bool existingBlock
    )
{
    TestingBlockDialog dialog(
        room,
        existingBlock,
        this
        );

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    auto* dataService =
        openDataService(m_services);

    if (!dataService)
    {
        QMessageBox::warning(
            this,
            tr("Testing Block"),
            tr("No database is open.")
            );
        return;
    }

    const Status result =
        dialog.removeRequested()
            ? dataService->deleteTestingBlock(
                day,
                timeLabel
                )
            : dataService->saveTestingBlock(
                day,
                timeLabel,
                dialog.room()
                );

    if (!result)
    {
        QMessageBox::warning(
            this,
            tr("Testing Block"),
            result.error()
            );
        return;
    }

    loadSchedule();
}

void ScheduleWidget::exportSchedule()
{
    SchedulePrintDialog dialog(this);

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    SchedulePrintService::Request request;
    request.parent = this;
    request.model = buildScheduleModel();
    request.showEnglishNames =
        m_showKoreanTeacherEnglishNames;
    request.style = dialog.selectedStyle();
    request.pageOrientation = dialog.selectedOrientation();

    if (m_services && m_services->themeService())
    {
        request.currentTheme =
            m_services->themeService()->currentTheme();
    }

    if (auto* dataService = openDataService(m_services))
    {
        request.userName =
            dataService
                ->loadSetting(
                    QStringLiteral("myInfo/name"),
                    QString()
                    )
                .toString();
    }

    SchedulePrintService::Result result;

    if (dialog.selectedAction() == SchedulePrintDialog::Action::Print)
    {
        result = SchedulePrintService::printSchedule(request);
    }
    else
    {
        result = SchedulePrintService::saveSchedulePdf(
            request,
            dialog.selectedSavePath()
            );
    }

    if (result.status == SchedulePrintService::Status::Failed)
    {
        QMessageBox::warning(
            this,
            dialog.selectedAction() == SchedulePrintDialog::Action::Print
                ? tr("Print Schedule")
                : tr("Export Schedule"),
            result.message
            );
    }
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
        new QPushButton(this);
    m_regularModeButton->setObjectName(
        QStringLiteral("scheduleRegularModeButton")
        );
    m_intensiveModeButton =
        new QPushButton(this);
    m_intensiveModeButton->setObjectName(
        QStringLiteral("scheduleIntensiveModeButton")
        );
    m_testingModeButton =
        new QPushButton(this);
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

    m_settingsButton =
        new QPushButton(this);
    m_settingsButton->setObjectName(
        QStringLiteral("scheduleSettingsButton")
        );
    m_settingsButton->setProperty(
        "role",
        QStringLiteral("icon_button")
        );
    m_settingsButton->setFixedSize(42, 36);
    m_settingsButton->setAccessibleName(
        tr("Schedule Settings")
        );
    m_settingsButton->setToolTip(
        tr("Schedule Settings")
        );
    controlsLayout->addWidget(m_settingsButton);

    m_importButton =
        new TextFitPushButton(this);
    m_importButton->setObjectName(
        QStringLiteral("scheduleImportButton")
        );
    m_importButton->setMinimumWidth(110);
    controlsLayout->addWidget(m_importButton);

    m_exportButton =
        new TextFitPushButton(this);
    m_exportButton->setObjectName(
        QStringLiteral("scheduleExportButton")
        );
    m_exportButton->setMinimumWidth(110);
    controlsLayout->addWidget(m_exportButton);

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

    m_table->verticalHeader()->setVisible(false);
    m_table->setFrameShape(QFrame::NoFrame);
    m_table->setShowGrid(false);
    m_table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setWordWrap(true);
    m_table->setAlternatingRowColors(false);
    m_table->setItemDelegateForColumn(
        0,
        new TimeColumnDelegate(m_table)
        );
    m_table->verticalHeader()->setDefaultSectionSize(RowHeight);
    m_table->horizontalHeader()->setFont(
        FontManager::getUiFont(
            12,
            QFont::DemiBold
            )
        );
    m_table->horizontalHeader()->setFixedHeight(HeaderHeight);
    m_table->horizontalHeader()->setSectionsClickable(false);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setFocusPolicy(Qt::NoFocus);

    layout->addWidget(m_table);

    m_controlsWidget->setHidden(
        m_mode == ScheduleMode::ReadOnly
        );
    if (m_mode == ScheduleMode::ReadOnly)
    {
        layout->addStretch();
    }

    connect(
        m_modeButtonGroup,
        &QButtonGroup::idClicked,
        this,
        &ScheduleWidget::setDisplayMode
        );

    connect(
        m_settingsButton,
        &QPushButton::clicked,
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
        m_exportButton,
        &QPushButton::clicked,
        this,
        &ScheduleWidget::exportSchedule
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
    auto* dataService =
        openDataService(m_services);

    if (!dataService)
    {
        return;
    }

    m_use24h =
        settingToBool(
            dataService->loadSetting(
                SettingsKeys::Use24HourTime,
                QStringLiteral("false")
                ),
            false
            );

    m_showWeekends =
        settingToBool(
            dataService->loadSetting(
                SettingsKeys::ShowWeekends,
                QStringLiteral("false")
                ),
            false
            );

    m_showKoreanTeacherEnglishNames =
        settingToBool(
            dataService->loadSetting(
                SettingsKeys::ShowKoreanTeacherEnglishNames,
                QStringLiteral("false")
                ),
            false
            );

    m_showAllHours =
        settingToBool(
            dataService->loadSetting(
                SettingsKeys::ShowAllHoursV2,
                QStringLiteral("false")
                ),
            false
            );

    m_testingAffectsM1 =
        settingToBool(
            dataService->loadSetting(
                SettingsKeys::TestingAffectsM1,
                QStringLiteral("false")
                ),
            false
            );

    const QVariant storedMode =
        dataService->loadSetting(
            SettingsKeys::DisplayMode,
            QVariant()
            );
    const bool legacyIntensive =
        settingToBool(
            dataService->loadSetting(
                SettingsKeys::ShowIntensive,
                QStringLiteral("false")
                ),
            false
            );
    m_displayMode =
        displayModeFromSetting(
            storedMode,
            legacyIntensive
            );

    if (!storedMode.isValid())
    {
        dataService->saveSetting(
            SettingsKeys::DisplayMode,
            displayModeSetting(m_displayMode)
            );
    }
}

void ScheduleWidget::loadSchedule()
{
    if (!m_table)
    {
        return;
    }

    const int headerFontSize =
        m_compactPreview
            ? 12 - PreviewFontSizeReduction
            : 12;
    const int headerHeight =
        m_compactPreview
            ? CompactPreviewHeaderHeight
            : HeaderHeight;
    const int rowHeight =
        m_compactPreview
            ? CompactPreviewRowHeight
            : RowHeight;

    m_table->verticalHeader()->setDefaultSectionSize(rowHeight);
    m_table->horizontalHeader()->setFont(
        FontManager::getUiFont(
            headerFontSize,
            QFont::DemiBold
            )
        );
    m_table->horizontalHeader()->setFixedHeight(headerHeight);

    m_scheduleModel =
        buildScheduleModel();

    configureColumns(
        m_scheduleModel.days
        );

    clearTableWidgets();
    m_table->clearContents();
    m_table->clearSpans();
    m_table->setRowCount(
        m_scheduleModel.rows.size()
        );

    for (int rowIndex = 0; rowIndex < m_scheduleModel.rows.size(); ++rowIndex)
    {
        const ScheduleRowView& scheduleRow =
            m_scheduleModel.rows[rowIndex];

        auto* timeItem =
            new QTableWidgetItem(
                scheduleRow.timeRangeLabel
                );

        timeItem->setFlags(Qt::ItemIsEnabled);
        timeItem->setData(TimeCellRole, true);
        timeItem->setTextAlignment(Qt::AlignCenter);
        timeItem->setFont(
            FontManager::getUiFont(
                m_compactPreview
                    ? 11 - PreviewTimeFontSizeReduction
                    : 11,
                QFont::Medium
                )
            );

        m_table->setItem(
            rowIndex,
            0,
            timeItem
            );

        int maxEntryCount = 1;

        for (int dayIndex = 0; dayIndex < scheduleRow.cells.size(); ++dayIndex)
        {
            const ScheduleCellView& cell =
                scheduleRow.cells[dayIndex];

            const int column =
                dayIndex + 1;

            if (cell.entries.isEmpty())
            {
                m_table->setCellWidget(
                    rowIndex,
                    column,
                    createSlotLabel(
                        cell
                        )
                    );

                continue;
            }

            maxEntryCount =
                std::max(
                    maxEntryCount,
                    static_cast<int>(cell.entries.size())
                    );

            if (cell.entries.size() == 1)
            {
                m_table->setCellWidget(
                    rowIndex,
                    column,
                    createScheduleLabel(
                        cell.entries.first()
                        )
                    );
            }
            else
            {
                m_table->setCellWidget(
                    rowIndex,
                    column,
                    createMultiScheduleLabel(cell.entries)
                    );
            }
        }

        m_table->setRowHeight(
            rowIndex,
            std::max(
                rowHeight,
                m_compactPreview
                    ? CompactPreviewRowBaseHeight
                        + (maxEntryCount * CompactPreviewRowHeightPerEntry)
                    : 28 + (maxEntryCount * 48)
                )
            );
    }

    if (m_scheduleModel.rows.isEmpty())
    {
        m_table->setRowCount(1);

        auto* item =
            new QTableWidgetItem(
                tr("No registered class meeting times available.")
                );

        item->setFlags(Qt::ItemIsEnabled);
        item->setTextAlignment(Qt::AlignCenter);

        m_table->setItem(
            0,
            0,
            item
            );

        m_table->setSpan(
            0,
            0,
            1,
            m_table->columnCount()
            );

        m_table->setRowHeight(
            0,
            rowHeight
            );
    }

    updateTableMinimumHeight();
}

void ScheduleWidget::updateButtons()
{
    if (
        !m_regularModeButton
        || !m_intensiveModeButton
        || !m_testingModeButton
        || !m_settingsButton
        || !m_importButton
        || !m_exportButton
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

    m_exportButton->setText(
        tr("Export")
        );
    m_importButton->setText(
        tr("Import")
        );

    const bool testing =
        m_displayMode == ScheduleDisplayMode::Testing;
    m_testingBanner->setVisible(testing);

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

void ScheduleWidget::configureColumns(
    const QStringList& days
    )
{
    QStringList headers{
        tr("Time")
    };

    for (const QString& day : days)
    {
        if (day == QStringLiteral("Monday"))
        {
            headers.append(tr("Monday"));
        }
        else if (day == QStringLiteral("Tuesday"))
        {
            headers.append(tr("Tuesday"));
        }
        else if (day == QStringLiteral("Wednesday"))
        {
            headers.append(tr("Wednesday"));
        }
        else if (day == QStringLiteral("Thursday"))
        {
            headers.append(tr("Thursday"));
        }
        else if (day == QStringLiteral("Friday"))
        {
            headers.append(tr("Friday"));
        }
        else if (day == QStringLiteral("Saturday"))
        {
            headers.append(tr("Saturday"));
        }
        else if (day == QStringLiteral("Sunday"))
        {
            headers.append(tr("Sunday"));
        }
        else
        {
            headers.append(day);
        }
    }

    m_table->setColumnCount(
        headers.size()
        );

    m_table->setHorizontalHeaderLabels(headers);

    m_table->horizontalHeader()->setSectionResizeMode(
        0,
        QHeaderView::Fixed
        );

    m_table->setColumnWidth(
        0,
        m_compactPreview
            ? CompactPreviewTimeColumnWidth
            : TimeColumnWidth
        );

    for (int column = 1; column < headers.size(); ++column)
    {
        m_table->horizontalHeader()->setSectionResizeMode(
            column,
            QHeaderView::Stretch
            );
    }
}

void ScheduleWidget::clearTableWidgets()
{
    for (int row = 0; row < m_table->rowCount(); ++row)
    {
        for (int column = 0; column < m_table->columnCount(); ++column)
        {
            QWidget* widget =
                m_table->cellWidget(
                    row,
                    column
                    );

            if (!widget)
            {
                continue;
            }

            m_table->removeCellWidget(
                row,
                column
                );

            widget->deleteLater();
        }
    }
}

void ScheduleWidget::updateTableMinimumHeight()
{
    if (!m_table)
    {
        return;
    }

    const int rowCount =
        m_table->rowCount();
    const bool hasHiddenRows =
        m_maximumVisibleRows > 0
        && rowCount > m_maximumVisibleRows;
    const int visibleRowCount =
        hasHiddenRows
            ? m_maximumVisibleRows
            : rowCount;

    m_table->setVerticalScrollBarPolicy(
        hasHiddenRows
            ? Qt::ScrollBarAsNeeded
            : Qt::ScrollBarAlwaysOff
        );

    int tableHeight =
        m_table->horizontalHeader()->height()
        + (m_table->frameWidth() * 2);

    for (int row = 0; row < visibleRowCount; ++row)
    {
        tableHeight +=
            m_table->rowHeight(row);
    }

    if (auto* scrollBar = m_table->horizontalScrollBar())
    {
        if (scrollBar->isVisible())
        {
            tableHeight +=
                scrollBar->sizeHint().height();
        }
    }

    m_table->setMinimumHeight(tableHeight);
    m_table->setMaximumHeight(tableHeight);
    updateGeometry();
}

void ScheduleWidget::reloadSlotStates()
{
    m_intensiveSlotStates.clear();

    auto* dataService =
        openDataService(m_services);

    if (!dataService)
    {
        return;
    }

    const QList<IntensiveSlotState> states =
        dataService->loadIntensiveSlotStates();

    for (const IntensiveSlotState& state : states)
    {
        m_intensiveSlotStates.insert(
            scheduleSlotKey(
                state.day,
                state.startTime
                ),
            state.state
            );
    }
}

void ScheduleWidget::reloadTestingBlocks()
{
    auto* dataService =
        openDataService(m_services);

    if (!dataService)
    {
        m_testingBlockRooms.clear();
        return;
    }

    const Result<QList<TestingBlock>> blocks =
        dataService->loadTestingBlocks();

    if (!blocks)
    {
        qWarning()
            << "Failed to load testing blocks:"
            << blocks.error();
        QMessageBox::warning(
            this,
            tr("Testing Layout"),
            blocks.error()
            );
        return;
    }

    QMap<QString, QString> loadedRooms;

    for (const TestingBlock& block : *blocks)
    {
        loadedRooms.insert(
            scheduleSlotKey(
                block.day,
                block.startTime
                ),
            block.room
            );
    }

    m_testingBlockRooms =
        std::move(loadedRooms);
}

ScheduleViewRequest ScheduleWidget::buildScheduleViewRequest() const
{
    ScheduleViewRequest request;
    request.days =
        visibleScheduleDays(
            m_showWeekends
            );
    request.slotStateOverrides =
        m_intensiveSlotStates;
    request.testingBlockRooms =
        m_testingBlockRooms;
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
        openDataService(m_services)
        );

    const ScheduleBuildResult result =
        builder.build(
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

QWidget* ScheduleWidget::createScheduleLabel(
    const ScheduleEntry& entry
    )
{
    auto* label =
        new QLabel(this);

    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setProperty("role", UiRoles::ScheduleCell);
    label->setProperty("class_id", entry.classId);
    label->setAttribute(Qt::WA_TransparentForMouseEvents);
    label->setStyleSheet(
        classCellStyle(
            entry.classColor,
            entry.fontColor,
            m_compactPreview ? 3 : 4,
            m_compactPreview ? 5 : 6
            )
        );

    const QString teacherLine =
        scheduleTeacherRoomLine(
            entry,
            m_showKoreanTeacherEnglishNames
            );

    const QString englishLine =
        joinedEnglishLine(entry);

    FontManager::setManagedRichText(
        label,
        QStringLiteral(
            "<div style=\"text-align:center; line-height:1.25;\">"
            "<div style=\"color:%1; font-family:'%2'; font-size:%3pt; font-weight:600;\">%4</div>"
            "<div style=\"color:%1; font-size:%5pt; font-weight:400;\">%6</div>"
            "</div>"
            )
            .arg(
                entry.fontColor.isEmpty()
                    ? QStringLiteral("#000000")
                    : entry.fontColor
                )
            .arg(
                FontManager::getKoreanFont()
                    .family()
                    .toHtmlEscaped()
                )
            .arg(
                FontManager::getKoreanFont(
                    FontManager::stdKoreanFont
                    - (m_compactPreview ? PreviewFontSizeReduction : 0)
                    ).pointSize()
                )
            .arg(escaped(teacherLine))
            .arg(
                FontManager::adjustedPointSize(
                    14
                    - (m_compactPreview ? PreviewFontSizeReduction : 0)
                    )
                )
            .arg(escaped(englishLine))
        );

    return label;
}

QWidget* ScheduleWidget::createMultiScheduleLabel(
    const QList<ScheduleEntry>& entries
    )
{
    auto* label =
        new QLabel(this);

    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setProperty("role", UiRoles::ScheduleMulti);
    label->setAttribute(Qt::WA_TransparentForMouseEvents);

    if (!entries.isEmpty())
    {
        label->setProperty(
            "class_id",
            entries.first().classId
            );

        label->setStyleSheet(
            classCellStyle(
                entries.first().classColor,
                entries.first().fontColor,
                m_compactPreview ? 3 : 4,
                m_compactPreview ? 5 : 6
                )
            );
    }

    QString html;

    for (const ScheduleEntry& entry : entries)
    {
        const QString teacherLine =
            scheduleTeacherRoomLine(
                entry,
                m_showKoreanTeacherEnglishNames
                );

        const QString englishLine =
            joinedEnglishLine(entry);

        const QString fontColor =
            entry.fontColor.isEmpty()
                ? QStringLiteral("#000000")
                : entry.fontColor;

        html +=
            QStringLiteral(
                "<div style=\"margin-bottom:%7px; text-align:center; line-height:1.2;\">"
                "<div style=\"color:%1; font-family:'%2'; font-size:%3pt; font-weight:600;\">%4</div>"
                "<div style=\"color:%1; font-size:%5pt; font-weight:400;\">%6</div>"
                "</div>"
                )
                .arg(fontColor)
                .arg(
                    FontManager::getKoreanFont()
                        .family()
                        .toHtmlEscaped()
                    )
                .arg(
                    FontManager::getKoreanFont(
                        FontManager::stdKoreanFont
                        - (m_compactPreview ? PreviewFontSizeReduction : 0)
                        ).pointSize()
                    )
                .arg(escaped(teacherLine))
                .arg(
                    FontManager::adjustedPointSize(
                        14
                        - (m_compactPreview ? PreviewFontSizeReduction : 0)
                        )
                    )
                .arg(escaped(englishLine))
                .arg(m_compactPreview ? 6 : 8);
    }

    FontManager::setManagedRichText(
        label,
        html
        );

    return label;
}

QWidget* ScheduleWidget::createSlotLabel(
    const ScheduleCellView& cell
    )
{
    QLabel* label =
        cell.slotState == scheduleTestingSlotState()
            ? static_cast<QLabel*>(
                new TestingCellLabel(this)
                )
            : new QLabel(this);

    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setProperty("role", UiRoles::ScheduleEmpty);
    label->setProperty("is_slot_cell", true);
    label->setProperty("day", cell.day);
    label->setProperty("time_label", cell.timeLabel);
    label->setProperty("default_slot_state", cell.defaultSlotState);
    label->setProperty("slot_state", cell.slotState);
    label->setProperty("slot_toggling_enabled", cell.slotTogglingEnabled);
    label->setProperty(
        "testing_block_creation_enabled",
        cell.testingBlockCreationEnabled
        );
    label->setProperty("testing_room", cell.testingRoom);
    label->setAttribute(Qt::WA_TransparentForMouseEvents);

    if (cell.slotState == scheduleEssaySlotState())
    {
        label->setText(
            tr("Essay")
            );

        label->setFont(
            FontManager::getUiFont(
                16
                    - (m_compactPreview ? PreviewFontSizeReduction : 0),
                QFont::Bold,
                true
                )
            );

        label->setStyleSheet(
            QStringLiteral(
                "QLabel {"
                "background:white;"
                "color:black;"
                "border-radius:%1px;"
                "padding:%2px;"
                "}"
                )
                .arg(m_compactPreview ? 5 : 6)
                .arg(m_compactPreview ? 4 : 6)
            );
    }
    else if (cell.slotState == scheduleTestingSlotState())
    {
        const QString room =
            cell.testingRoom.trimmed();
        label->setText(
            room.isEmpty()
                ? tr("Testing")
                : tr("Testing\nRm: %1").arg(room)
            );
        label->setAccessibleName(
            room.isEmpty()
                ? tr("Testing")
                : tr("Testing, room %1").arg(room)
            );
        label->setFont(
            FontManager::getUiFont(
                14
                    - (m_compactPreview ? PreviewFontSizeReduction : 0),
                QFont::Bold,
                true
                )
            );

        const bool dark =
            palette()
                .color(QPalette::Window)
                .lightness() < 128;
        QPalette testingPalette =
            label->palette();
        testingPalette.setColor(
            QPalette::Highlight,
            dark
                ? QColor(QStringLiteral("#FFD166"))
                : QColor(QStringLiteral("#B66A00"))
            );
        label->setPalette(testingPalette);
        label->setStyleSheet(
            QStringLiteral(
                "QLabel {"
                "background:%1;"
                "color:%2;"
                "border:1px solid %3;"
                "border-radius:%4px;"
                "padding:%5px;"
                "}"
                )
                .arg(
                    dark
                        ? QStringLiteral("#4B3D20")
                        : QStringLiteral("#FFF0B8"),
                    dark
                        ? QStringLiteral("#FFF2C2")
                        : QStringLiteral("#4A3500"),
                    dark
                        ? QStringLiteral("#8D7339")
                        : QStringLiteral("#D39B25")
                    )
                .arg(m_compactPreview ? 5 : 6)
                .arg(m_compactPreview ? 4 : 6)
            );
    }
    else if (cell.slotState == scheduleLunchSlotState())
    {
        label->setText(
            tr("Lunch")
            );

        label->setFont(
            FontManager::getUiFont(
                16
                    - (m_compactPreview ? PreviewFontSizeReduction : 0),
                QFont::Black,
                true
                )
            );

        label->setStyleSheet(
            QStringLiteral(
                "QLabel {"
                "background:#DCDCDC;"
                "color:black;"
                "border-radius:%1px;"
                "padding:%2px;"
                "}"
                )
                .arg(m_compactPreview ? 5 : 6)
                .arg(m_compactPreview ? 4 : 6)
            );
    }
    else
    {
        label->clear();
        label->setStyleSheet(
            QStringLiteral(
                "QLabel {"
                "background:transparent;"
                "border:none;"
                "padding:0px;"
                "}"
                )
            );
    }

    return label;
}
