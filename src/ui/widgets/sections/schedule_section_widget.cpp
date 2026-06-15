#include "schedule_section_widget.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "services/dataservice.h"
#include "ui/pages/schedule/schedule_editor_dialog.h"
#include "ui/styles/roles.h"

#include <algorithm>

#include <QAbstractItemView>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTime>
#include <QVariant>
#include <QVBoxLayout>

namespace
{
constexpr int TimeColumnWidth = 90;
constexpr int HeaderHeight = 42;
constexpr int RowHeight = 60;

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

void ScheduleSectionWidget::toggleTimeFormat()
{
    m_use24h = !m_use24h;

    if (m_services && m_services->dataService())
    {
        m_services
            ->dataService()
            ->saveSetting(
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
    updateButtons();
    loadSchedule();
}

void ScheduleSectionWidget::toggleWeekends()
{
    m_showWeekends = !m_showWeekends;

    if (m_services && m_services->dataService())
    {
        m_services
            ->dataService()
            ->saveSetting(
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

    if (
        m_showIntensive
        && widget->property("is_slot_cell").toBool()
        )
    {
        const QString day =
            widget->property("day").toString();

        const QString timeLabel =
            widget->property("time_label").toString();

        const QString currentState =
            slotState(
                day,
                timeLabel
                );

        QString newState;

        if (currentState == QStringLiteral("essay"))
        {
            newState = QStringLiteral("lunch");
        }
        else if (currentState == QStringLiteral("lunch"))
        {
            newState = QStringLiteral("empty");
        }
        else
        {
            newState = QStringLiteral("essay");
        }

        if (m_services && m_services->dataService())
        {
            m_services
                ->dataService()
                ->saveIntensiveSlotState(
                    day,
                    timeLabel,
                    newState
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
        new QPushButton(this);
    m_timeFormatButton->setObjectName("primaryButton");
    m_timeFormatButton->setMinimumWidth(140);

    m_weekendButton =
        new QPushButton(this);
    m_weekendButton->setObjectName("primaryButton");
    m_weekendButton->setMinimumWidth(150);

    m_scheduleModeButton =
        new QPushButton(this);
    m_scheduleModeButton->setObjectName("primaryButton");
    m_scheduleModeButton->setMinimumWidth(200);

    controlsLayout->addWidget(m_timeFormatButton);
    controlsLayout->addStretch();
    controlsLayout->addWidget(m_weekendButton);
    controlsLayout->addWidget(m_scheduleModeButton);

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
        m_scheduleModeButton,
        &QPushButton::clicked,
        this,
        &ScheduleSectionWidget::toggleScheduleMode
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
    if (!m_services || !m_services->dataService())
    {
        return;
    }

    m_use24h =
        settingToBool(
            m_services
                ->dataService()
                ->loadSetting(
                    QStringLiteral("schedule_use_24h"),
                    QStringLiteral("false")
                    ),
            false
            );

    m_showWeekends =
        settingToBool(
            m_services
                ->dataService()
                ->loadSetting(
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

    reloadSlotStates();

    const QStringList days =
        visibleDays();

    configureColumns(days);

    ScheduleBuilder builder(
        m_services
            ? m_services->dataService()
            : nullptr
        );

    const ScheduleBuildResult result =
        builder.build(
            m_showIntensive,
            days
            );

    const QList<ScheduleRow> rows =
        result.rows;

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

    updateTableMinimumHeight();
}

void ScheduleSectionWidget::updateButtons()
{
    if (
        !m_timeFormatButton
        || !m_weekendButton
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

    m_scheduleModeButton->setText(
        m_showIntensive
            ? tr("Show Regular Schedule")
            : tr("Show Intensive Schedule")
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

QStringList ScheduleSectionWidget::visibleDays() const
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

void ScheduleSectionWidget::reloadSlotStates()
{
    m_intensiveSlotStates.clear();

    if (!m_services || !m_services->dataService())
    {
        return;
    }

    const QList<IntensiveSlotState> states =
        m_services
            ->dataService()
            ->loadIntensiveSlotStates();

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

QString ScheduleSectionWidget::slotKey(
    const QString& day,
    const QString& timeLabel
    ) const
{
    return day + QLatin1Char('\x1f') + timeLabel;
}

QString ScheduleSectionWidget::slotState(
    const QString& day,
    const QString& timeLabel
    ) const
{
    return m_intensiveSlotStates.value(
        slotKey(
            day,
            timeLabel
            ),
        QStringLiteral("essay")
        );
}

QString ScheduleSectionWidget::formatDisplayTime(
    const QString& timeLabel
    ) const
{
    const QTime time =
        QTime::fromString(
            timeLabel,
            QStringLiteral("HH:mm")
            );

    if (!time.isValid())
    {
        return timeLabel;
    }

    if (m_use24h)
    {
        return time.toString(
            QStringLiteral("HH:mm")
            );
    }

    return time.toString(
        QStringLiteral("h:mm AP")
        );
}

QString ScheduleSectionWidget::buildTimeRangeLabel(
    const QString& startLabel,
    bool uses55Endings
    ) const
{
    const QTime start =
        QTime::fromString(
            startLabel,
            QStringLiteral("HH:mm")
            );

    if (!start.isValid())
    {
        return startLabel;
    }

    const QTime end =
        start.addSecs(
            (uses55Endings ? 55 : 50) * 60
            );

    const QString startDisplay =
        formatDisplayTime(
            start.toString(QStringLiteral("HH:mm"))
            );

    const QString endDisplay =
        formatDisplayTime(
            end.toString(QStringLiteral("HH:mm"))
            );

    if (m_use24h)
    {
        return QStringLiteral("%1 - %2")
            .arg(startDisplay)
            .arg(endDisplay);
    }

    const QString startAmpm =
        startDisplay.endsWith(QStringLiteral("AM"))
            ? QStringLiteral("AM")
            : QStringLiteral("PM");

    const QString endAmpm =
        endDisplay.endsWith(QStringLiteral("AM"))
            ? QStringLiteral("AM")
            : QStringLiteral("PM");

    QString startClean =
        startDisplay;

    startClean.remove(QStringLiteral(" AM"));
    startClean.remove(QStringLiteral(" PM"));

    QString endClean =
        endDisplay;

    endClean.remove(QStringLiteral(" AM"));
    endClean.remove(QStringLiteral(" PM"));

    if (startAmpm == endAmpm)
    {
        return QStringLiteral("%1 -\n%2 %3")
            .arg(startClean)
            .arg(endClean)
            .arg(endAmpm);
    }

    return QStringLiteral("%1\n- %2")
        .arg(startDisplay)
        .arg(endDisplay);
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

    label->setText(
        QStringLiteral(
            "<div style=\"text-align:center; line-height:1.25;\">"
            "<div style=\"color:%1; font-size:12pt; font-weight:600;\">%2</div>"
            "<div style=\"color:%1; font-size:14pt; font-weight:400;\">%3</div>"
            "</div>"
            )
            .arg(
                entry.fontColor.isEmpty()
                    ? QStringLiteral("#000000")
                    : entry.fontColor
                )
            .arg(escaped(koreanLine))
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
                "<div style=\"color:%1; font-size:12pt; font-weight:600;\">%2</div>"
                "<div style=\"color:%1; font-size:14pt; font-weight:400;\">%3</div>"
                "</div>"
                )
                .arg(fontColor)
                .arg(escaped(koreanLine))
                .arg(escaped(englishLine));
    }

    label->setText(
        html
        );

    return label;
}

QWidget* ScheduleSectionWidget::createSlotLabel(
    const QString& day,
    const QString& timeLabel
    )
{
    QString state =
        m_showIntensive
            ? slotState(day, timeLabel)
            : QStringLiteral("essay");

    auto* label =
        new QLabel(this);

    label->setAlignment(Qt::AlignCenter);
    label->setProperty("role", UiRoles::ScheduleEmpty);
    label->setProperty("is_slot_cell", true);
    label->setProperty("day", day);
    label->setProperty("time_label", timeLabel);
    label->setProperty("slot_state", state);
    label->setAttribute(Qt::WA_TransparentForMouseEvents);

    if (state == QStringLiteral("essay"))
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
    else if (state == QStringLiteral("lunch"))
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
