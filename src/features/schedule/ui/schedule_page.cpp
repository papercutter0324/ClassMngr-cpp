#include "schedule_page.h"

#include "ui/shared/widgets/text_fit_push_button.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "data/data_service.h"
#include "features/schedule/ui/schedule_editor_dialog.h"
#include "features/schedule/ui/schedule_time_formatter.h"
#include "ui/shared/styles/roles.h"

#include <algorithm>
#include <QFrame>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTime>
#include <QVBoxLayout>

namespace
{
constexpr int TimeColumnWidth = 90;
constexpr int HeaderHeight = 42;
constexpr int RowHeight = 60;
constexpr int RegularEarlyEmptyFinalHour = 15;

QString emptySlotState()
{
    return QStringLiteral("empty");
}

QString essaySlotState()
{
    return QStringLiteral("essay");
}

QString lunchSlotState()
{
    return QStringLiteral("lunch");
}

bool isWeekendDay(
    const QString& day
    )
{
    return day == QStringLiteral("Saturday")
        || day == QStringLiteral("Sunday");
}

bool isRegularEarlyEmptySlot(
    const QString& timeLabel
    )
{
    const QTime time =
        QTime::fromString(
            timeLabel,
            QStringLiteral("HH:mm")
            );

    return time.isValid()
        && time.hour() <= RegularEarlyEmptyFinalHour;
}

QString nextSlotState(
    const QString& currentState
    )
{
    if (currentState == essaySlotState())
    {
        return lunchSlotState();
    }

    if (currentState == lunchSlotState())
    {
        return emptySlotState();
    }

    return essaySlotState();
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

SchedulePage::SchedulePage(
    ApplicationServices* services,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
{
    setProperty("role", UiRoles::Schedule);

    loadSettings();
    buildUi();
    loadSchedule();
}

void SchedulePage::refresh()
{
    if (!isVisible())
    {
        return;
    }

    loadSchedule();
}

void SchedulePage::retranslateUi()
{
    if (m_titleLabel)
    {
        m_titleLabel->setText(
            tr("Weekly Class Schedule")
            );
    }

    if (m_subtitleLabel)
    {
        m_subtitleLabel->setText(
            tr("Generated from registered classes and their meeting times.")
            );
    }

    updateButtons();
    loadSchedule();
}

void SchedulePage::showEvent(
    QShowEvent* event
    )
{
    BasePage::showEvent(event);
    loadSchedule();
}

void SchedulePage::toggleTimeFormat()
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

void SchedulePage::toggleHideEmpty()
{
    m_hideEmptyBlocks = !m_hideEmptyBlocks;
    updateButtons();
    loadSchedule();
}

void SchedulePage::toggleScheduleMode()
{
    m_showIntensive = !m_showIntensive;
    updateButtons();
    loadSchedule();
}

void SchedulePage::toggleWeekends()
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

void SchedulePage::onCellClicked(
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

        if (!slotTogglingEnabled(day))
        {
            return;
        }

        const QString defaultState =
            defaultSlotState(
                day,
                timeLabel
                );

        const QString currentState =
            slotState(
                day,
                timeLabel,
                defaultState
                );

        const QString newState =
            nextSlotState(
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

void SchedulePage::buildUi()
{
    contentLayout()->setContentsMargins(
        24,
        18,
        24,
        0
        );

    contentLayout()->setSpacing(8);

    m_titleLabel =
        new QLabel(
            tr("Weekly Class Schedule"),
            this
            );

    m_titleLabel->setObjectName("pageTitle");
    m_titleLabel->setFont(
        FontManager::getUiFont(
            20,
            QFont::DemiBold
            )
        );

    m_subtitleLabel =
        new QLabel(
            tr("Generated from registered classes and their meeting times."),
            this
            );

    m_subtitleLabel->setObjectName("pageSubtitle");
    m_subtitleLabel->setFont(
        FontManager::getUiFont(11)
        );

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

    contentLayout()->addWidget(m_titleLabel);
    contentLayout()->addWidget(m_subtitleLabel);
    contentLayout()->addWidget(m_table);

    m_timeFormatButton =
        new TextFitPushButton(this);

    m_timeFormatButton->setObjectName("primaryButton");
    m_timeFormatButton->setMinimumWidth(140);

    m_weekendButton =
        new TextFitPushButton(this);

    m_weekendButton->setObjectName("primaryButton");
    m_weekendButton->setMinimumWidth(150);

    m_hideEmptyButton =
        new TextFitPushButton(this);

    m_hideEmptyButton->setObjectName("primaryButton");
    m_hideEmptyButton->setMinimumWidth(200);

    m_scheduleModeButton =
        new TextFitPushButton(this);

    m_scheduleModeButton->setObjectName("primaryButton");
    m_scheduleModeButton->setMinimumWidth(200);

    bottomLayout()->addWidget(m_timeFormatButton);
    bottomLayout()->addStretch();
    bottomLayout()->addWidget(m_weekendButton);
    bottomLayout()->addWidget(m_hideEmptyButton);
    bottomLayout()->addWidget(m_scheduleModeButton);

    connect(
        m_timeFormatButton,
        &QPushButton::clicked,
        this,
        &SchedulePage::toggleTimeFormat
        );

    connect(
        m_weekendButton,
        &QPushButton::clicked,
        this,
        &SchedulePage::toggleWeekends
        );

    connect(
        m_hideEmptyButton,
        &QPushButton::clicked,
        this,
        &SchedulePage::toggleHideEmpty
        );

    connect(
        m_scheduleModeButton,
        &QPushButton::clicked,
        this,
        &SchedulePage::toggleScheduleMode
        );

    connect(
        m_table,
        &QTableWidget::cellClicked,
        this,
        &SchedulePage::onCellClicked
        );

    updateButtons();
}

void SchedulePage::loadSettings()
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

void SchedulePage::loadSchedule()
{
    if (!m_table)
    {
        return;
    }

    reloadSlotStates();

    const QStringList days =
        visibleDays();

    configureColumns(days);

    ScheduleBuilder builder(
        openDataService(m_services)
        );

    ScheduleBuildResult result =
        builder.build(
            m_showIntensive,
            days
            );

    QList<ScheduleRow> rows =
        result.rows;

    if (m_showIntensive && m_hideEmptyBlocks)
    {
        QList<ScheduleRow> filteredRows;

        for (const ScheduleRow& scheduleRow : rows)
        {
            bool hasRealClass = false;

            for (const QString& day : days)
            {
                const QList<ScheduleEntry> entries =
                    result.schedule
                        .value(day)
                        .value(scheduleRow.label);

                if (!entries.isEmpty())
                {
                    hasRealClass = true;
                    break;
                }

                if (
                    slotState(
                        day,
                        scheduleRow.label,
                        defaultSlotState(
                            day,
                            scheduleRow.label
                            )
                        )
                    != emptySlotState()
                    )
                {
                    hasRealClass = true;
                    break;
                }
            }

            if (hasRealClass)
            {
                filteredRows.append(scheduleRow);
            }
        }

        rows = filteredRows;
    }

    clearTableWidgets();
    m_table->clearContents();
    m_table->clearSpans();
    m_table->setRowCount(rows.size());

    for (int rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
    {
        const ScheduleRow& scheduleRow =
            rows[rowIndex];

        auto* timeItem =
            new QTableWidgetItem(
                buildTimeRangeLabel(
                    scheduleRow.label,
                    result.uses55Endings
                    )
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

        for (int dayIndex = 0; dayIndex < days.size(); ++dayIndex)
        {
            const QString& day =
                days[dayIndex];

            const QList<ScheduleEntry> entries =
                result.schedule
                    .value(day)
                    .value(scheduleRow.label);

            const int column =
                dayIndex + 1;

            if (entries.isEmpty())
            {
                m_table->setCellWidget(
                    rowIndex,
                    column,
                    createSlotLabel(
                        day,
                        scheduleRow.label
                        )
                    );

                continue;
            }

            maxEntryCount =
                std::max(
                    maxEntryCount,
                    static_cast<int>(entries.size())
                    );

            if (entries.size() == 1)
            {
                m_table->setCellWidget(
                    rowIndex,
                    column,
                    createScheduleLabel(
                        entries.first()
                        )
                    );
            }
            else
            {
                m_table->setCellWidget(
                    rowIndex,
                    column,
                    createMultiScheduleLabel(entries)
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

    if (rows.isEmpty())
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
}

void SchedulePage::updateButtons()
{
    if (
        !m_timeFormatButton
        || !m_weekendButton
        || !m_hideEmptyButton
        || !m_scheduleModeButton
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

    m_hideEmptyButton->setText(
        m_hideEmptyBlocks
            ? tr("Show Empty Blocks/Rows")
            : tr("Hide Empty Blocks/Rows")
        );

    m_hideEmptyButton->setVisible(
        m_showIntensive
        );

    m_scheduleModeButton->setText(
        m_showIntensive
            ? tr("Show Regular Schedule")
            : tr("Show Intensive Schedule")
        );
}

void SchedulePage::configureColumns(
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

void SchedulePage::clearTableWidgets()
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

QStringList SchedulePage::visibleDays() const
{
    QStringList days{
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday")
    };

    if (m_showWeekends)
    {
        days.append(
            QStringLiteral("Saturday")
            );

        days.append(
            QStringLiteral("Sunday")
            );
    }

    return days;
}

void SchedulePage::reloadSlotStates()
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
            slotKey(
                state.day,
                state.startTime
                ),
            state.state
            );
    }
}

QString SchedulePage::slotKey(
    const QString& day,
    const QString& timeLabel
    ) const
{
    return day + QLatin1Char('\x1f') + timeLabel;
}

QString SchedulePage::slotState(
    const QString& day,
    const QString& timeLabel,
    const QString& defaultState
    ) const
{
    return m_intensiveSlotStates.value(
        slotKey(
            day,
            timeLabel
            ),
        defaultState
        );
}

QString SchedulePage::defaultSlotState(
    const QString& day,
    const QString& timeLabel
    ) const
{
    if (isWeekendDay(day))
    {
        return emptySlotState();
    }

    if (m_showIntensive)
    {
        return essaySlotState();
    }

    if (isRegularEarlyEmptySlot(timeLabel))
    {
        return emptySlotState();
    }

    return essaySlotState();
}

bool SchedulePage::regularSlotTogglingEnabled(
    const QString& day
    ) const
{
    if (isWeekendDay(day))
    {
        return true;
    }

    return m_regularWeekdaySlotTogglingEnabled;
}

bool SchedulePage::slotTogglingEnabled(
    const QString& day
    ) const
{
    return m_showIntensive
        || regularSlotTogglingEnabled(day);
}

QString SchedulePage::formatDisplayTime(
    const QString& timeLabel
    ) const
{
    return ScheduleTimeFormatter::displayTime(
        timeLabel,
        m_use24h
        );
}

QString SchedulePage::buildTimeRangeLabel(
    const QString& startLabel,
    bool uses55Endings
    ) const
{
    return ScheduleTimeFormatter::rangeLabel(
        startLabel,
        uses55Endings,
        m_use24h
        );
}

QWidget* SchedulePage::createScheduleLabel(
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

QWidget* SchedulePage::createMultiScheduleLabel(
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

QWidget* SchedulePage::createSlotLabel(
    const QString& day,
    const QString& timeLabel
    )
{
    const QString defaultState =
        defaultSlotState(
            day,
            timeLabel
            );

    const QString state =
        slotTogglingEnabled(day)
            ? slotState(
                day,
                timeLabel,
                defaultState
                )
            : defaultState;

    auto* label =
        new QLabel(this);

    label->setAlignment(Qt::AlignCenter);
    label->setProperty("role", UiRoles::ScheduleEmpty);
    label->setProperty("is_slot_cell", true);
    label->setProperty("day", day);
    label->setProperty("time_label", timeLabel);
    label->setProperty("slot_state", state);
    label->setAttribute(Qt::WA_TransparentForMouseEvents);

    if (state == essaySlotState())
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
    else if (state == lunchSlotState())
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
