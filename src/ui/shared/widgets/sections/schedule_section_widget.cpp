#include "schedule_section_widget.h"

#include "ui/shared/widgets/text_fit_push_button.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/theme_service.h"
#include "data/data_service.h"
#include "features/schedule/ui/schedule_editor_dialog.h"
#include "features/schedule/ui/schedule_print_dialog.h"
#include "features/schedule/ui/schedule_print_service.h"
#include "ui/shared/styles/roles.h"

#include <algorithm>

#include <QAbstractItemView>
#include <QDialog>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVariant>
#include <QVBoxLayout>

namespace
{
constexpr int TimeColumnWidth = 90;
constexpr int HeaderHeight = 42;
constexpr int RowHeight = 60;

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
}

ScheduleSectionWidget::ScheduleSectionWidget(
    ApplicationServices* services,
    QWidget* parent
    )
    : QWidget(parent)
    , m_services(services)
{
    setProperty("role", UiRoles::Schedule);

    loadSettings();
    buildUi();
    loadSchedule();
}

void ScheduleSectionWidget::refreshSchedule()
{
    loadSchedule();
}

void ScheduleSectionWidget::retranslateUi()
{
    updateButtons();
    loadSchedule();
}

void ScheduleSectionWidget::toggleTimeFormat()
{
    m_use24h = !m_use24h;

    if (auto* dataService = openDataService(m_services))
    {
        dataService->saveSetting(
            QStringLiteral("schedule_use_24h"),
            m_use24h
                ? QStringLiteral("true")
                : QStringLiteral("false")
            );
    }

    updateButtons();
    loadSchedule();
}

void ScheduleSectionWidget::toggleScheduleMode()
{
    m_showIntensive = !m_showIntensive;

    if (!m_showIntensive)
    {
        m_showAllHours = false;
    }

    updateButtons();
    loadSchedule();
}

void ScheduleSectionWidget::toggleShowAllHours()
{
    m_showAllHours = !m_showAllHours;
    updateButtons();
    loadSchedule();
}

void ScheduleSectionWidget::toggleWeekends()
{
    m_showWeekends = !m_showWeekends;

    if (auto* dataService = openDataService(m_services))
    {
        dataService->saveSetting(
            QStringLiteral("schedule_show_weekends"),
            m_showWeekends
                ? QStringLiteral("true")
                : QStringLiteral("false")
            );
    }

    updateButtons();
    loadSchedule();
}

void ScheduleSectionWidget::onCellClicked(
    int row,
    int column
    )
{
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

void ScheduleSectionWidget::printSchedule()
{
    SchedulePrintDialog dialog(this);

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

void ScheduleSectionWidget::buildUi()
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

    auto* controlsLayout =
        new QHBoxLayout;

    controlsLayout->setContentsMargins(
        0,
        0,
        0,
        0
        );
    controlsLayout->setSpacing(8);

    m_timeFormatButton =
        new TextFitPushButton(this);
    m_timeFormatButton->setObjectName("primaryButton");
    m_timeFormatButton->setMinimumWidth(140);

    m_weekendButton =
        new TextFitPushButton(this);
    m_weekendButton->setObjectName("primaryButton");
    m_weekendButton->setMinimumWidth(150);

    m_showAllHoursButton =
        new TextFitPushButton(this);
    m_showAllHoursButton->setObjectName("primaryButton");
    m_showAllHoursButton->setCheckable(true);
    m_showAllHoursButton->setMinimumWidth(170);

    m_scheduleModeButton =
        new TextFitPushButton(this);
    m_scheduleModeButton->setObjectName("primaryButton");
    m_scheduleModeButton->setMinimumWidth(200);

    m_printButton =
        new TextFitPushButton(this);
    m_printButton->setObjectName("primaryButton");
    m_printButton->setMinimumWidth(120);

    controlsLayout->addWidget(m_timeFormatButton);
    controlsLayout->addWidget(m_weekendButton);
    controlsLayout->addStretch();
    controlsLayout->addWidget(m_showAllHoursButton);
    controlsLayout->addWidget(m_scheduleModeButton);
    controlsLayout->addWidget(m_printButton);

    layout->addLayout(controlsLayout);

    connect(
        m_timeFormatButton,
        &QPushButton::clicked,
        this,
        &ScheduleSectionWidget::toggleTimeFormat
        );

    connect(
        m_weekendButton,
        &QPushButton::clicked,
        this,
        &ScheduleSectionWidget::toggleWeekends
        );

    connect(
        m_showAllHoursButton,
        &QPushButton::clicked,
        this,
        &ScheduleSectionWidget::toggleShowAllHours
        );

    connect(
        m_scheduleModeButton,
        &QPushButton::clicked,
        this,
        &ScheduleSectionWidget::toggleScheduleMode
        );

    connect(
        m_printButton,
        &QPushButton::clicked,
        this,
        &ScheduleSectionWidget::printSchedule
        );

    connect(
        m_table,
        &QTableWidget::cellClicked,
        this,
        &ScheduleSectionWidget::onCellClicked
        );

    updateButtons();
}

void ScheduleSectionWidget::loadSettings()
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
                QStringLiteral("schedule_use_24h"),
                QStringLiteral("false")
                ),
            false
            );

    m_showWeekends =
        settingToBool(
            dataService->loadSetting(
                QStringLiteral("schedule_show_weekends"),
                QStringLiteral("false")
                ),
            false
            );
}

void ScheduleSectionWidget::loadSchedule()
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

void ScheduleSectionWidget::updateButtons()
{
    if (
        !m_timeFormatButton
        || !m_weekendButton
        || !m_showAllHoursButton
        || !m_scheduleModeButton
        || !m_printButton
        )
    {
        return;
    }

    m_timeFormatButton->setText(
        m_use24h
            ? tr("12-Hour Time")
            : tr("24-Hour Time")
        );

    m_weekendButton->setText(
        m_showWeekends
            ? tr("Hide Weekends")
            : tr("Show Weekends")
        );

    m_showAllHoursButton->setText(
        m_showAllHours
            ? tr("Hide All Hours")
            : tr("Show All Hours")
        );
    m_showAllHoursButton->setChecked(
        m_showAllHours
        );
    m_showAllHoursButton->setVisible(
        m_showIntensive
        );

    m_scheduleModeButton->setText(
        m_showIntensive
            ? tr("Show Regular Schedule")
            : tr("Show Intensive Schedule")
        );

    m_printButton->setText(
        tr("Print")
        );
}

void ScheduleSectionWidget::configureColumns(
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

void ScheduleSectionWidget::clearTableWidgets()
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

void ScheduleSectionWidget::updateTableMinimumHeight()
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

void ScheduleSectionWidget::reloadSlotStates()
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

ScheduleViewRequest ScheduleSectionWidget::buildScheduleViewRequest() const
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
        m_showIntensive && !m_showAllHours
            ? ScheduleRowFilter::TrimEmptyOuterRows
            : ScheduleRowFilter::None;

    return request;
}

ScheduleViewModel ScheduleSectionWidget::buildScheduleModel()
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
            request.useIntensive
            );

    return buildScheduleViewModel(
        result,
        request
        );
}

QWidget* ScheduleSectionWidget::createScheduleLabel(
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

    const QString koreanLine =
        QStringLiteral("%1 %2")
            .arg(entry.teacherKr.trimmed())
            .arg(entry.roomNumber.trimmed())
            .simplified();

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
            .arg(escaped(koreanLine))
            .arg(
                FontManager::adjustedPointSize(14)
                )
            .arg(escaped(englishLine))
        );

    return label;
}

QWidget* ScheduleSectionWidget::createMultiScheduleLabel(
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
        const QString koreanLine =
            QStringLiteral("%1 %2")
                .arg(entry.teacherKr.trimmed())
                .arg(entry.roomNumber.trimmed())
                .simplified();

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
                .arg(escaped(koreanLine))
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

QWidget* ScheduleSectionWidget::createSlotLabel(
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
