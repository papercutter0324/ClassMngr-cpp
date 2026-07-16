#include "schedule_widget.h"

#include "ui/shared/widgets/text_fit_push_button.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/theme_service.h"
#include "data/data_service.h"
#include "features/schedule/ui/schedule_editor_dialog.h"
#include "features/schedule/ui/schedule_print_dialog.h"
#include "features/schedule/services/schedule_print_service.h"
#include "ui/shared/styles/roles.h"

#include <algorithm>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVariant>
#include <QVBoxLayout>

namespace
{
constexpr int TimeColumnWidth = 90;
constexpr int HeaderHeight = 42;
constexpr int RowHeight = 60;

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
const QString ShowAllHours =
    QStringLiteral("schedule_show_all_hours");
const QString HideEmptyRows =
    QStringLiteral("schedule_hide_empty_rows");
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

QString classCellStyle(
    const QString& classColor,
    const QString& fontColor
    )
{
    return QStringLiteral(
        "QLabel {"
        "background:%1;"
        "color:%2;"
        "padding:4px;"
        "border-radius:6px;"
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

QString teacherRoomLine(
    const ScheduleEntry& entry,
    bool showEnglishName
    )
{
    const QString preferredName =
        (showEnglishName
            ? entry.teacherEn
            : entry.teacherKr)
            .trimmed();
    const QString fallbackName =
        (showEnglishName
            ? entry.teacherKr
            : entry.teacherEn)
            .trimmed();

    return QStringLiteral("%1 %2")
        .arg(
            preferredName.isEmpty()
                ? fallbackName
                : preferredName,
            entry.roomNumber.trimmed()
            )
        .simplified();
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
    state.showIntensive = m_showIntensive;
    state.showAllHours = m_showAllHours;
    state.hideEmptyRows = m_hideEmptyRows;
    state.showWeekends = m_showWeekends;

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

void ScheduleWidget::setUse24HourTime(
    bool use24h
    )
{
    m_use24h = use24h;

    saveBoolSetting(
        openDataService(m_services),
        SettingsKeys::Use24HourTime,
        m_use24h
        );

    updateButtons();
    loadSchedule();
}

void ScheduleWidget::setShowKoreanTeacherEnglishNames(
    bool showEnglishNames
    )
{
    m_showKoreanTeacherEnglishNames = showEnglishNames;

    saveBoolSetting(
        openDataService(m_services),
        SettingsKeys::ShowKoreanTeacherEnglishNames,
        m_showKoreanTeacherEnglishNames
        );

    updateButtons();
    loadSchedule();
}

void ScheduleWidget::setShowIntensiveSchedule(
    bool showIntensive
    )
{
    m_showIntensive = showIntensive;

    saveBoolSetting(
        openDataService(m_services),
        SettingsKeys::ShowIntensive,
        m_showIntensive
        );

    updateButtons();
    loadSchedule();
}

void ScheduleWidget::setShowAllHours(
    bool showAllHours
    )
{
    m_showAllHours = showAllHours;

    saveBoolSetting(
        openDataService(m_services),
        SettingsKeys::ShowAllHours,
        m_showAllHours
        );

    updateButtons();
    loadSchedule();
}

void ScheduleWidget::setHideEmptyRows(
    bool hideEmptyRows
    )
{
    m_hideEmptyRows = hideEmptyRows;

    saveBoolSetting(
        openDataService(m_services),
        SettingsKeys::HideEmptyRows,
        m_hideEmptyRows
        );

    updateButtons();
    loadSchedule();
}

void ScheduleWidget::setShowWeekends(
    bool showWeekends
    )
{
    m_showWeekends = showWeekends;

    saveBoolSetting(
        openDataService(m_services),
        SettingsKeys::ShowWeekends,
        m_showWeekends
        );

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

        if (!widget->property("slot_toggling_enabled").toBool())
        {
            return;
        }

        const QString defaultState =
            widget->property("default_slot_state").toString();

        const QString currentState =
            widget->property("slot_state").toString();

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

void ScheduleWidget::printSchedule()
{
    SchedulePrintDialog dialog(
        SchedulePrintDialog::Action::Print,
        this
        );

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    SchedulePrintService::Request request;
    request.parent = this;
    request.model =
        buildScheduleModel();
    request.style =
        dialog.selectedStyle();
    request.pageOrientation =
        dialog.selectedOrientation();

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

    const SchedulePrintService::Result result =
        SchedulePrintService::printSchedule(
            request
            );

    if (result.status == SchedulePrintService::Status::Failed)
    {
        QMessageBox::warning(
            this,
            tr("Print Schedule"),
            result.message
            );
    }
}

void ScheduleWidget::exportSchedule()
{
    SchedulePrintDialog dialog(
        SchedulePrintDialog::Action::Export,
        this
        );

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    QFileDialog fileDialog(
        this,
        tr("Export Schedule"),
        QString(),
        tr("PDF Documents (*.pdf)")
        );
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    fileDialog.setFileMode(QFileDialog::AnyFile);
    fileDialog.setOption(QFileDialog::DontUseNativeDialog, true);
    fileDialog.setDefaultSuffix(QStringLiteral("pdf"));
    fileDialog.selectFile(QStringLiteral("Schedule.pdf"));

    if (fileDialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const QStringList selectedFiles =
        fileDialog.selectedFiles();

    if (selectedFiles.isEmpty())
    {
        return;
    }

    QString documentPath =
        selectedFiles.first();

    if (QFileInfo(documentPath).suffix().isEmpty())
    {
        documentPath += QStringLiteral(".pdf");
    }

    SchedulePrintService::Request request;
    request.parent = this;
    request.model = buildScheduleModel();
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

    const SchedulePrintService::Result result =
        SchedulePrintService::saveSchedulePdf(
            request,
            documentPath
            );

    if (result.status == SchedulePrintService::Status::Failed)
    {
        QMessageBox::warning(
            this,
            tr("Export Schedule"),
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
    m_table->verticalHeader()->setDefaultSectionSize(RowHeight);
    m_table->horizontalHeader()->setFont(
        FontManager::getUiFont(
            12,
            QFont::DemiBold
            )
        );
    m_table->horizontalHeader()->setFixedHeight(HeaderHeight);

    layout->addWidget(m_table);

    m_controlsWidget =
        new QWidget(this);
    m_controlsWidget->setObjectName(
        QStringLiteral("scheduleControls")
        );

    auto* controlsLayout =
        new QHBoxLayout(m_controlsWidget);

    controlsLayout->setContentsMargins(
        0,
        0,
        0,
        0
        );
    controlsLayout->setSpacing(8);

    m_use24HourTimeCheckBox =
        new QCheckBox(this);
    m_use24HourTimeCheckBox->setObjectName(
        QStringLiteral("scheduleUse24HourTimeCheckBox")
        );

    m_showKoreanTeacherEnglishNamesCheckBox =
        new QCheckBox(this);
    m_showKoreanTeacherEnglishNamesCheckBox->setObjectName(
        QStringLiteral("scheduleShowKoreanTeacherEnglishNamesCheckBox")
        );

    m_showWeekendsCheckBox =
        new QCheckBox(this);
    m_showWeekendsCheckBox->setObjectName(
        QStringLiteral("scheduleShowWeekendsCheckBox")
        );

    m_showAllHoursCheckBox =
        new QCheckBox(this);
    m_showAllHoursCheckBox->setObjectName(
        QStringLiteral("scheduleShowAllHoursCheckBox")
        );

    m_hideEmptyRowsCheckBox =
        new QCheckBox(this);
    m_hideEmptyRowsCheckBox->setObjectName(
        QStringLiteral("scheduleHideEmptyRowsCheckBox")
        );

    m_showIntensiveScheduleCheckBox =
        new QCheckBox(this);
    m_showIntensiveScheduleCheckBox->setObjectName(
        QStringLiteral("scheduleShowIntensiveCheckBox")
        );

    m_printButton =
        new TextFitPushButton(this);
    m_printButton->setObjectName(
        QStringLiteral("schedulePrintButton")
        );
    m_printButton->setMinimumWidth(120);

    m_exportButton =
        new TextFitPushButton(this);
    m_exportButton->setObjectName(
        QStringLiteral("scheduleExportButton")
        );
    m_exportButton->setMinimumWidth(
        m_printButton->minimumWidth()
        );

    auto* controlsColumnLayout =
        new QVBoxLayout;
    controlsColumnLayout->setContentsMargins(0, 0, 0, 0);
    controlsColumnLayout->setSpacing(4);
    controlsColumnLayout->addWidget(m_use24HourTimeCheckBox);
    controlsColumnLayout->addWidget(
        m_showKoreanTeacherEnglishNamesCheckBox
        );
    controlsColumnLayout->addWidget(m_showWeekendsCheckBox);
    controlsColumnLayout->addWidget(m_showIntensiveScheduleCheckBox);

    auto* intensiveOptionsLayout =
        new QVBoxLayout;
    intensiveOptionsLayout->setContentsMargins(20, 0, 0, 0);
    intensiveOptionsLayout->setSpacing(4);
    intensiveOptionsLayout->addWidget(m_showAllHoursCheckBox);
    intensiveOptionsLayout->addWidget(m_hideEmptyRowsCheckBox);
    controlsColumnLayout->addLayout(intensiveOptionsLayout);

    auto* actionsLayout =
        new QVBoxLayout;
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    actionsLayout->setSpacing(4);
    actionsLayout->addStretch();
    actionsLayout->addWidget(m_exportButton);
    actionsLayout->addWidget(m_printButton);

    controlsLayout->addLayout(controlsColumnLayout);
    controlsLayout->addStretch();
    controlsLayout->addLayout(actionsLayout);

    layout->addWidget(m_controlsWidget);

    m_controlsWidget->setHidden(
        m_mode == ScheduleMode::ReadOnly
        );

    connect(
        m_use24HourTimeCheckBox,
        &QCheckBox::toggled,
        this,
        &ScheduleWidget::setUse24HourTime
        );

    connect(
        m_showKoreanTeacherEnglishNamesCheckBox,
        &QCheckBox::toggled,
        this,
        &ScheduleWidget::setShowKoreanTeacherEnglishNames
        );

    connect(
        m_showWeekendsCheckBox,
        &QCheckBox::toggled,
        this,
        &ScheduleWidget::setShowWeekends
        );

    connect(
        m_showAllHoursCheckBox,
        &QCheckBox::toggled,
        this,
        &ScheduleWidget::setShowAllHours
        );

    connect(
        m_hideEmptyRowsCheckBox,
        &QCheckBox::toggled,
        this,
        &ScheduleWidget::setHideEmptyRows
        );

    connect(
        m_showIntensiveScheduleCheckBox,
        &QCheckBox::toggled,
        this,
        &ScheduleWidget::setShowIntensiveSchedule
        );

    connect(
        m_exportButton,
        &QPushButton::clicked,
        this,
        &ScheduleWidget::exportSchedule
        );

    connect(
        m_printButton,
        &QPushButton::clicked,
        this,
        &ScheduleWidget::printSchedule
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

    m_showIntensive =
        settingToBool(
            dataService->loadSetting(
                SettingsKeys::ShowIntensive,
                QStringLiteral("false")
                ),
            false
            );

    m_showAllHours =
        settingToBool(
            dataService->loadSetting(
                SettingsKeys::ShowAllHours,
                QStringLiteral("false")
                ),
            false
            );

    m_hideEmptyRows =
        settingToBool(
            dataService->loadSetting(
                SettingsKeys::HideEmptyRows,
                QStringLiteral("true")
                ),
            true
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
        timeItem->setTextAlignment(Qt::AlignCenter);
        timeItem->setFont(
            FontManager::getUiFont(
                11,
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
                RowHeight,
                28 + (maxEntryCount * 48)
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
            RowHeight
            );
    }

    updateTableMinimumHeight();
}

void ScheduleWidget::updateButtons()
{
    if (
        !m_use24HourTimeCheckBox
        || !m_showKoreanTeacherEnglishNamesCheckBox
        || !m_showWeekendsCheckBox
        || !m_showAllHoursCheckBox
        || !m_hideEmptyRowsCheckBox
        || !m_showIntensiveScheduleCheckBox
        || !m_exportButton
        || !m_printButton
        )
    {
        return;
    }

    const QSignalBlocker use24hBlocker(m_use24HourTimeCheckBox);
    const QSignalBlocker englishNamesBlocker(
        m_showKoreanTeacherEnglishNamesCheckBox
        );
    const QSignalBlocker weekendsBlocker(m_showWeekendsCheckBox);
    const QSignalBlocker allHoursBlocker(m_showAllHoursCheckBox);
    const QSignalBlocker hideEmptyBlocker(m_hideEmptyRowsCheckBox);
    const QSignalBlocker intensiveBlocker(m_showIntensiveScheduleCheckBox);

    m_use24HourTimeCheckBox->setText(tr("Use 24-hour time"));
    m_use24HourTimeCheckBox->setChecked(m_use24h);
    m_showKoreanTeacherEnglishNamesCheckBox->setText(
        tr("Show English names for Korean teachers")
        );
    m_showKoreanTeacherEnglishNamesCheckBox->setChecked(
        m_showKoreanTeacherEnglishNames
        );
    m_showWeekendsCheckBox->setText(tr("Show weekends"));
    m_showWeekendsCheckBox->setChecked(m_showWeekends);
    m_showIntensiveScheduleCheckBox->setText(
        tr("Show intensive schedule")
        );
    m_showIntensiveScheduleCheckBox->setChecked(m_showIntensive);
    m_showAllHoursCheckBox->setText(tr("Show all hours"));
    m_showAllHoursCheckBox->setChecked(m_showAllHours);
    m_showAllHoursCheckBox->setEnabled(m_showIntensive);
    m_hideEmptyRowsCheckBox->setText(tr("Hide empty rows"));
    m_hideEmptyRowsCheckBox->setChecked(m_hideEmptyRows);
    m_hideEmptyRowsCheckBox->setEnabled(m_showIntensive);

    m_exportButton->setText(
        tr("Export")
        );

    m_printButton->setText(
        tr("Print")
        );
}

void ScheduleWidget::configureColumns(
    const QStringList& days
    )
{
    QStringList headers{
        tr("Time")
    };

    headers.append(days);

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
        TimeColumnWidth
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

    int tableHeight =
        m_table->horizontalHeader()->height()
        + (m_table->frameWidth() * 2);

    for (int row = 0; row < m_table->rowCount(); ++row)
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

ScheduleViewRequest ScheduleWidget::buildScheduleViewRequest() const
{
    ScheduleViewRequest request;
    request.days =
        visibleScheduleDays(
            m_showWeekends
            );
    request.slotStateOverrides =
        m_intensiveSlotStates;
    request.use24h =
        m_use24h;
    request.useIntensive =
        m_showIntensive;
    request.regularWeekdaySlotTogglingEnabled =
        m_regularWeekdaySlotTogglingEnabled;
    request.rowFilter =
        m_showIntensive && m_hideEmptyRows
            ? ScheduleRowFilter::HideEmptyRows
            : ScheduleRowFilter::None;

    return request;
}

ScheduleViewModel ScheduleWidget::buildScheduleModel()
{
    reloadSlotStates();

    const ScheduleViewRequest request =
        buildScheduleViewRequest();

    ScheduleBuilder builder(
        openDataService(m_services)
        );

    const ScheduleBuildResult result =
        builder.build(
            request.useIntensive,
            request.days,
            request.useIntensive && m_showAllHours
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
            entry.fontColor
            )
        );

    const QString teacherLine =
        teacherRoomLine(
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
                FontManager::getKoreanFont().pointSize()
                )
            .arg(escaped(teacherLine))
            .arg(
                FontManager::adjustedPointSize(14)
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
                entries.first().fontColor
                )
            );
    }

    QString html;

    for (const ScheduleEntry& entry : entries)
    {
        const QString teacherLine =
            teacherRoomLine(
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
                "<div style=\"margin-bottom:8px; text-align:center; line-height:1.2;\">"
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
                    FontManager::getKoreanFont().pointSize()
                    )
                .arg(escaped(teacherLine))
                .arg(
                    FontManager::adjustedPointSize(14)
                    )
                .arg(escaped(englishLine));
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
    auto* label =
        new QLabel(this);

    label->setAlignment(Qt::AlignCenter);
    label->setProperty("role", UiRoles::ScheduleEmpty);
    label->setProperty("is_slot_cell", true);
    label->setProperty("day", cell.day);
    label->setProperty("time_label", cell.timeLabel);
    label->setProperty("default_slot_state", cell.defaultSlotState);
    label->setProperty("slot_state", cell.slotState);
    label->setProperty("slot_toggling_enabled", cell.slotTogglingEnabled);
    label->setAttribute(Qt::WA_TransparentForMouseEvents);

    if (cell.slotState == scheduleEssaySlotState())
    {
        label->setText(
            tr("Essay")
            );

        label->setFont(
            FontManager::getUiFont(
                16,
                QFont::Bold,
                true
                )
            );

        label->setStyleSheet(
            QStringLiteral(
                "QLabel {"
                "background:white;"
                "color:black;"
                "border-radius:6px;"
                "padding:6px;"
                "}"
                )
            );
    }
    else if (cell.slotState == scheduleLunchSlotState())
    {
        label->setText(
            tr("Lunch")
            );

        label->setFont(
            FontManager::getUiFont(
                16,
                QFont::Black,
                true
                )
            );

        label->setStyleSheet(
            QStringLiteral(
                "QLabel {"
                "background:#DCDCDC;"
                "color:black;"
                "border-radius:6px;"
                "padding:6px;"
                "}"
                )
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
