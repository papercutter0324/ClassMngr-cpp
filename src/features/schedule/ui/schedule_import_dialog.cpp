#include "schedule_import_dialog.h"

#include "core/application_services.h"
#include "core/utils/colorutils.h"
#include "data/data_service.h"
#include "domain/models/classroom.h"
#include "domain/rules/schedule_import_rules.h"
#include "features/schedule/import/schedule_workbook_parser.h"
#include "features/schedule/ui/schedule_view_model.h"
#include "features/schedule/ui/schedule_widget.h"
#include "features/teacher/import/teacher_import_name_utils.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSet>
#include <QStackedWidget>
#include <QTime>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
constexpr int ActionRole = Qt::UserRole;
constexpr int TargetRole = Qt::UserRole + 1;
constexpr int SourcePage = 0;
constexpr int UserPage = 1;
constexpr int ReviewPage = 2;

DataService* openDataService(
    ApplicationServices* services
    )
{
    DataService* dataService =
        services
            ? services->dataService()
            : nullptr;
    return dataService && dataService->isOpen()
        ? dataService
        : nullptr;
}

int timeMinutes(
    const QString& value
    )
{
    const QTime time =
        QTime::fromString(
            value,
            QStringLiteral("h:mm AP")
            );
    return time.isValid()
        ? time.hour() * 60 + time.minute()
        : -1;
}

QString timeDisplay(
    const QString& value
    )
{
    const QTime time =
        QTime::fromString(
            value,
            QStringLiteral("h:mm AP")
            );
    return time.isValid()
        ? time.toString(QStringLiteral("h:mm AP"))
        : value;
}

QString meetingText(
    const QList<ClassTime>& times
    )
{
    QStringList meetings;
    for (const ClassTime& time : times)
    {
        meetings.append(
            QStringLiteral("%1 %2–%3")
                .arg(
                    scheduleImportWeekdayDisplayName(time.day),
                    timeDisplay(time.startTime),
                    timeDisplay(time.endTime)
                    )
            );
    }
    meetings.sort(Qt::CaseInsensitive);
    return meetings.join(QStringLiteral(", "));
}

int weekdayIndex(
    const QString& day
    )
{
    static const QStringList weekdays{
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday")
    };
    return weekdays.indexOf(day);
}

QString weekdayLabel(
    const QString& day
    )
{
    if (day == QStringLiteral("Monday"))
    {
        return QObject::tr("Mon");
    }
    if (day == QStringLiteral("Tuesday"))
    {
        return QObject::tr("Tues");
    }
    if (day == QStringLiteral("Wednesday"))
    {
        return QObject::tr("Wed");
    }
    if (day == QStringLiteral("Thursday"))
    {
        return QObject::tr("Thurs");
    }
    if (day == QStringLiteral("Friday"))
    {
        return QObject::tr("Fri");
    }
    if (day == QStringLiteral("Saturday"))
    {
        return QObject::tr("Sat");
    }
    if (day == QStringLiteral("Sunday"))
    {
        return QObject::tr("Sun");
    }
    return day.trimmed();
}

QString compactTimeDisplay(
    const QString& value
    )
{
    const QTime time =
        QTime::fromString(
            value,
            QStringLiteral("h:mm AP")
            );
    if (!time.isValid())
    {
        return value;
    }

    const QString minutePart =
        time.minute() == 0
            ? QString()
            : QStringLiteral(":%1")
                  .arg(time.minute(), 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1%2%3")
        .arg(
            time.hour() == 0 || time.hour() == 12
                ? 12
                : time.hour() % 12
            )
        .arg(minutePart)
        .arg(time.hour() < 12 ? QStringLiteral("am") : QStringLiteral("pm"));
}

QString compactMeetingText(
    const QList<ClassTime>& times
    )
{
    QList<ClassTime> orderedTimes = times;
    std::sort(
        orderedTimes.begin(),
        orderedTimes.end(),
        [](const ClassTime& left, const ClassTime& right)
        {
            const int leftDay = weekdayIndex(left.day);
            const int rightDay = weekdayIndex(right.day);
            if (leftDay != rightDay)
            {
                return leftDay < rightDay;
            }
            return timeMinutes(left.startTime)
                < timeMinutes(right.startTime);
        }
        );

    QStringList text;
    for (const ClassTime& time : orderedTimes)
    {
        text.append(
            QStringLiteral("%1 %2")
                .arg(
                    weekdayLabel(time.day),
                    compactTimeDisplay(time.startTime)
                    )
            );
    }
    return text.join(QStringLiteral(" / "));
}

QStringList meetingKeys(
    const QList<ClassTime>& times
    )
{
    QStringList keys;
    for (const ClassTime& time : times)
    {
        keys.append(
            QStringLiteral("%1\x1f%2\x1f%3")
                .arg(
                    time.day,
                    time.startTime,
                    time.endTime
                    )
            );
    }
    keys.sort(Qt::CaseInsensitive);
    return keys;
}

bool timesOverlap(
    const ClassTime& left,
    const ClassTime& right
    )
{
    if (left.day != right.day)
    {
        return false;
    }
    const int leftStart = timeMinutes(left.startTime);
    const int leftEnd = timeMinutes(left.endTime);
    const int rightStart = timeMinutes(right.startTime);
    const int rightEnd = timeMinutes(right.endTime);
    return leftStart >= 0
        && rightStart >= 0
        && leftEnd > leftStart
        && rightEnd > rightStart
        && leftStart < rightEnd
        && rightStart < leftEnd;
}

QString projectedScheduleConflict(
    const ScheduleImportUserBlock& user
    )
{
    struct Occurrence
    {
        QString label;
        ClassTime time;
    };
    QList<Occurrence> occurrences;

    for (const ScheduleImportClassCandidate& candidate : user.classes)
    {
        const QString label =
            QStringLiteral("%1 %2")
                .arg(
                    candidate.classGrade,
                    candidate.classLevel
                    );
        for (const ClassTime& time : candidate.times)
        {
            for (const Occurrence& existing : occurrences)
            {
                if (timesOverlap(time, existing.time))
                {
                    return QObject::tr(
                        "%1 overlaps %2 on %3."
                        )
                        .arg(
                            label,
                            existing.label,
                            scheduleImportWeekdayDisplayName(time.day)
                            );
                }
            }
            occurrences.append({label, time});
        }
    }

    return {};
}

QString classLabel(
    DataService* dataService,
    int classId,
    ScheduleImportKind kind
    )
{
    const Classroom classroom =
        dataService->getClassById(classId);
    const ClassInfo info =
        dataService->loadClassInfo(classId);
    const QString course =
        QStringLiteral("%1 %2")
            .arg(
                info.classGrade,
                info.classLevel
                )
            .simplified();

    const QString label =
        !course.isEmpty()
            ? course
            : !classroom.name.trimmed().isEmpty()
                ? classroom.name.trimmed()
                : QObject::tr("Class %1").arg(classId);
    const Teacher teacher =
        dataService->getTeacher(info.teacherId);
    const bool importingIntensive =
        kind == ScheduleImportKind::Intensive;
    const QList<ClassTime>& preferredTimes =
        importingIntensive
            ? info.intensiveTimes
            : info.classTimes;
    const QList<ClassTime>& fallbackTimes =
        importingIntensive
            ? info.classTimes
            : info.intensiveTimes;
    const bool usesPreferredTimes =
        !preferredTimes.isEmpty();
    const QList<ClassTime>& times =
        usesPreferredTimes
            ? preferredTimes
            : fallbackTimes;
    const QString schedule =
        compactMeetingText(times);
    QStringList detailParts;
    if (!teacher.teacherKr.trimmed().isEmpty())
    {
        detailParts.append(teacher.teacherKr.trimmed());
    }
    if (!schedule.isEmpty())
    {
        detailParts.append(schedule);
    }
    const QString details =
        detailParts.join(QLatin1Char(' '));
    const QString scheduleTag =
        schedule.isEmpty()
            ? QString()
            : usesPreferredTimes
                ? importingIntensive
                    ? QStringLiteral(" ") + QObject::tr("[Int]")
                    : QStringLiteral(" ") + QObject::tr("[Reg]")
                : importingIntensive
                    ? QStringLiteral(" ") + QObject::tr("[Reg]")
                    : QStringLiteral(" ") + QObject::tr("[Int]");
    return details.isEmpty()
        ? label
        : QStringLiteral("%1 (%2)%3")
              .arg(label, details, scheduleTag);
}

QString classDifferences(
    DataService* dataService,
    const ScheduleImportClassCandidate& candidate,
    int targetClassId,
    ScheduleImportKind kind,
    const QString& classColor
    )
{
    if (!dataService || targetClassId <= 0)
    {
        return QObject::tr(
            "Imported meetings: %1. A new class will be created with color %2."
            )
            .arg(meetingText(candidate.times))
            .arg(classColor);
    }

    const ClassInfo existing =
        dataService->loadClassInfo(targetClassId);
    const Teacher existingTeacher =
        dataService->getTeacher(existing.teacherId);
    const QList<ClassTime> existingTimes =
        kind == ScheduleImportKind::Intensive
            ? existing.intensiveTimes
            : existing.classTimes;
    QStringList differences;

    if (existing.classGrade != candidate.classGrade)
    {
        differences.append(
            QObject::tr("grade %1 → %2")
                .arg(
                    existing.classGrade,
                    candidate.classGrade
                    )
            );
    }
    if (existing.classLevel != candidate.classLevel)
    {
        differences.append(
            QObject::tr("level %1 → %2")
                .arg(
                    existing.classLevel,
                    candidate.classLevel
                    )
            );
    }
    if (
        TeacherImportNameUtils::hangulOnly(
            existingTeacher.teacherKr
            ) != candidate.teacherKey
        )
    {
        differences.append(
            QObject::tr("Korean teacher %1 → %2")
                .arg(
                    existingTeacher.teacherKr,
                    candidate.teacherKr
                    )
            );
    }
    if (meetingKeys(existingTimes) != meetingKeys(candidate.times))
    {
        differences.append(
            QObject::tr("schedule %1 → %2")
                .arg(
                    meetingText(existingTimes),
                    meetingText(candidate.times)
                    )
            );
    }
    if (
        existing.classColor.compare(
            classColor,
            Qt::CaseInsensitive
            ) != 0
        )
    {
        differences.append(
            QObject::tr("color %1 → %2")
                .arg(
                    existing.classColor,
                    classColor
                    )
            );
    }

    if (differences.isEmpty())
    {
        return QObject::tr(
            "Imported meetings: %1. No teacher, course, schedule, or color differences."
            )
            .arg(meetingText(candidate.times));
    }

    return QStringLiteral(
        "<b>%1</b> %2<br>"
        "<span style=\"color:#b45309\"><b>%3</b> %4</span>"
        )
        .arg(
            QObject::tr("Imported meetings:").toHtmlEscaped(),
            meetingText(candidate.times).toHtmlEscaped(),
            QObject::tr("Changes:").toHtmlEscaped(),
            differences.join(QStringLiteral("; ")).toHtmlEscaped()
            );
}

QString teacherLabel(
    const Teacher& teacher
    )
{
    return QObject::tr("%1 — Room %2")
        .arg(
            teacher.teacherKr.trimmed(),
            teacher.roomNumber.trimmed().isEmpty()
                ? QObject::tr("not set")
                : teacher.roomNumber.trimmed()
            );
}

void addResolutionItem(
    QComboBox* combo,
    const QString& text,
    int action,
    int target = -1
    )
{
    combo->addItem(text);
    const int index = combo->count() - 1;
    combo->setItemData(index, action, ActionRole);
    combo->setItemData(index, target, TargetRole);
}

int findActionIndex(
    QComboBox* combo,
    int action,
    int target = -2
    )
{
    for (int index = 0; index < combo->count(); ++index)
    {
        if (combo->itemData(index, ActionRole).toInt() != action)
        {
            continue;
        }
        if (
            target == -2
            || combo->itemData(index, TargetRole).toInt() == target
            )
        {
            return index;
        }
    }
    return -1;
}

ScheduleViewModel previewModel(
    const ScheduleImportUserBlock& user
    )
{
    const QStringList days{
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday")
    };

    QSet<int> starts;
    QHash<int, QString> endByStart;
    QHash<QString, QList<ScheduleEntry>> entries;

    for (const ScheduleImportClassCandidate& candidate : user.classes)
    {
        ScheduleEntry entry;
        entry.teacherKr = candidate.teacherKr;
        entry.roomNumber =
            candidate.rooms.isEmpty()
                ? QString()
                : candidate.rooms.first();
        entry.classGrade = candidate.classGrade;
        entry.classLevel = candidate.classLevel;
        entry.classColor =
            candidate.importedColors.isEmpty()
                ? QStringLiteral("#FFFFFF")
                : candidate.importedColors.first();
        entry.fontColor =
            ColorUtils::getContrastingFontColor(
                QColor(entry.classColor)
                );

        for (const ClassTime& time : candidate.times)
        {
            const int start =
                timeMinutes(time.startTime);
            if (start < 0)
            {
                continue;
            }
            starts.insert(start);
            endByStart.insert(start, time.endTime);
            entries[
                time.day
                + QLatin1Char('\x1f')
                + QString::number(start)
                ].append(entry);
        }
    }

    QList<int> sortedStarts =
        starts.values();
    std::sort(
        sortedStarts.begin(),
        sortedStarts.end()
        );

    ScheduleViewModel model;
    model.days = days;

    for (int start : sortedStarts)
    {
        ScheduleRowView row;
        const QTime startTime(
            start / 60,
            start % 60
            );
        row.timeLabel =
            startTime.toString(
                QStringLiteral("HH:mm")
                );
        row.timeRangeLabel =
            QStringLiteral("%1–%2")
                .arg(
                    startTime.toString(
                        QStringLiteral("h:mm AP")
                        ),
                    timeDisplay(
                        endByStart.value(start)
                        )
                    );

        for (const QString& day : days)
        {
            ScheduleCellView cell;
            cell.day = day;
            cell.timeLabel = row.timeLabel;
            cell.entries =
                entries.value(
                    day
                    + QLatin1Char('\x1f')
                    + QString::number(start)
                    );
            cell.defaultSlotState =
                scheduleEmptySlotState();
            cell.slotState =
                scheduleEmptySlotState();
            row.maxEntryCount =
                std::max(
                    row.maxEntryCount,
                    static_cast<int>(
                        cell.entries.size()
                        )
                    );
            row.cells.append(cell);
        }

        model.rows.append(row);
    }

    return model;
}
}

ScheduleImportDialog::ScheduleImportDialog(
    ApplicationServices* services,
    QWidget* parent
    )
    : QDialog(parent)
    , m_services(services)
{
    setWindowTitle(tr("Import Schedule"));
    setModal(true);
    resize(1100, 820);
    buildUi();
}

void ScheduleImportDialog::setFilePath(
    const QString& filePath
    )
{
    m_fileEdit->setText(filePath);
    m_workbookLoaded = false;
    m_sheetCombo->clear();
    m_sheetCombo->setVisible(false);
    m_sheetLabel->setVisible(false);
    m_sourceStatus->setText(
        tr("Choose the schedule type, then continue.")
        );
    updateNavigation();
}

void ScheduleImportDialog::buildUi()
{
    auto* layout =
        new QVBoxLayout(this);
    m_pages =
        new QStackedWidget(this);
    m_pages->addWidget(buildSourcePage());
    m_pages->addWidget(buildUserPage());
    m_pages->addWidget(buildReviewPage());
    layout->addWidget(m_pages, 1);

    m_buttons =
        new QDialogButtonBox(
            QDialogButtonBox::Cancel,
            this
            );
    m_backButton =
        m_buttons->addButton(
            tr("Back"),
            QDialogButtonBox::ActionRole
            );
    m_nextButton =
        m_buttons->addButton(
            tr("Next"),
            QDialogButtonBox::ActionRole
            );
    m_importButton =
        m_buttons->addButton(
            tr("Import"),
            QDialogButtonBox::AcceptRole
            );
    m_backButton->setObjectName(
        QStringLiteral("scheduleImportBackButton")
        );
    m_nextButton->setObjectName(
        QStringLiteral("scheduleImportNextButton")
        );
    m_importButton->setObjectName(
        QStringLiteral("scheduleImportAcceptButton")
        );
    layout->addWidget(m_buttons);

    connect(
        m_buttons,
        &QDialogButtonBox::rejected,
        this,
        &QDialog::reject
        );
    connect(
        m_backButton,
        &QPushButton::clicked,
        this,
        &ScheduleImportDialog::goBack
        );
    connect(
        m_nextButton,
        &QPushButton::clicked,
        this,
        &ScheduleImportDialog::goNext
        );
    connect(
        m_importButton,
        &QPushButton::clicked,
        this,
        &ScheduleImportDialog::applyImport
        );

    updateNavigation();
}

QWidget* ScheduleImportDialog::buildSourcePage()
{
    auto* page =
        new QWidget(this);
    auto* layout =
        new QVBoxLayout(page);
    auto* heading =
        new QLabel(
            tr("Choose a spreadsheet"),
            page
            );
    QFont headingFont = heading->font();
    headingFont.setBold(true);
    heading->setFont(headingFont);
    layout->addWidget(heading);

    auto* fileLayout =
        new QHBoxLayout;
    m_fileEdit =
        new QLineEdit(page);
    m_fileEdit->setObjectName(
        QStringLiteral("scheduleImportFilePath")
        );
    m_fileEdit->setReadOnly(true);
    m_fileEdit->setPlaceholderText(
        tr("Select an XLSX schedule...")
        );
    m_browseButton =
        new QPushButton(
            tr("Browse..."),
            page
            );
    m_browseButton->setObjectName(
        QStringLiteral("scheduleImportBrowseButton")
        );
    fileLayout->addWidget(m_fileEdit, 1);
    fileLayout->addWidget(m_browseButton);
    layout->addLayout(fileLayout);

    auto* typeGroup =
        new QGroupBox(
            tr("Schedule type"),
            page
            );
    auto* typeLayout =
        new QHBoxLayout(typeGroup);
    m_normalRadio =
        new QRadioButton(
            tr("Normal schedule"),
            typeGroup
            );
    m_intensiveRadio =
        new QRadioButton(
            tr("Intensive schedule"),
            typeGroup
            );
    m_normalRadio->setObjectName(
        QStringLiteral("scheduleImportNormalRadio")
        );
    m_intensiveRadio->setObjectName(
        QStringLiteral("scheduleImportIntensiveRadio")
        );
    auto* typeButtons =
        new QButtonGroup(typeGroup);
    typeButtons->addButton(m_normalRadio);
    typeButtons->addButton(m_intensiveRadio);
    typeLayout->addWidget(m_normalRadio);
    typeLayout->addWidget(m_intensiveRadio);
    typeLayout->addStretch();
    layout->addWidget(typeGroup);

    m_sheetLabel =
        new QLabel(
            tr("Worksheet:"),
            page
            );
    m_sheetCombo =
        new QComboBox(page);
    m_sheetCombo->setObjectName(
        QStringLiteral("scheduleImportSheetCombo")
        );
    layout->addWidget(m_sheetLabel);
    layout->addWidget(m_sheetCombo);
    m_sheetLabel->setVisible(false);
    m_sheetCombo->setVisible(false);

    m_sourceStatus =
        new QLabel(
            tr("Choose a file and schedule type."),
            page
            );
    m_sourceStatus->setObjectName(
        QStringLiteral("scheduleImportSourceStatus")
        );
    m_sourceStatus->setWordWrap(true);
    layout->addWidget(m_sourceStatus);
    layout->addStretch();

    connect(
        m_browseButton,
        &QPushButton::clicked,
        this,
        &ScheduleImportDialog::browseForFile
        );
    const auto invalidate =
        [this]()
        {
            m_workbookLoaded = false;
            m_sheetCombo->clear();
            m_sheetCombo->setVisible(false);
            m_sheetLabel->setVisible(false);
            m_sourceStatus->setText(
                tr("Ready to read the workbook.")
                );
            updateNavigation();
        };
    connect(
        m_normalRadio,
        &QRadioButton::toggled,
        this,
        invalidate
        );
    connect(
        m_intensiveRadio,
        &QRadioButton::toggled,
        this,
        invalidate
        );
    connect(
        m_sheetCombo,
        &QComboBox::currentIndexChanged,
        this,
        &ScheduleImportDialog::updateNavigation
        );

    return page;
}

QWidget* ScheduleImportDialog::buildUserPage()
{
    auto* page =
        new QWidget(this);
    auto* layout =
        new QVBoxLayout(page);
    auto* heading =
        new QLabel(
            tr("Choose your schedule section"),
            page
            );
    QFont headingFont = heading->font();
    headingFont.setBold(true);
    heading->setFont(headingFont);
    layout->addWidget(heading);

    m_userStatus =
        new QLabel(page);
    m_userStatus->setWordWrap(true);
    layout->addWidget(m_userStatus);

    m_userCombo =
        new QComboBox(page);
    m_userCombo->setObjectName(
        QStringLiteral("scheduleImportUserCombo")
        );
    layout->addWidget(m_userCombo);

    m_nameConfirmation =
        new QCheckBox(
            tr("Use the selected spreadsheet name even though it does not match My Information."),
            page
            );
    m_nameConfirmation->setObjectName(
        QStringLiteral("scheduleImportNameConfirmation")
        );
    layout->addWidget(m_nameConfirmation);
    layout->addStretch();

    connect(
        m_userCombo,
        &QComboBox::currentIndexChanged,
        this,
        &ScheduleImportDialog::updateUserSelection
        );
    connect(
        m_nameConfirmation,
        &QCheckBox::toggled,
        this,
        &ScheduleImportDialog::updateNavigation
        );

    return page;
}

QWidget* ScheduleImportDialog::buildReviewPage()
{
    auto* page =
        new QWidget(this);
    auto* layout =
        new QVBoxLayout(page);
    auto* heading =
        new QLabel(
            tr("Review and reconcile"),
            page
            );
    QFont headingFont = heading->font();
    headingFont.setBold(true);
    heading->setFont(headingFont);
    layout->addWidget(heading);

    m_previewWidget =
        new ScheduleWidget(
            m_services,
            page,
            ScheduleMode::ReadOnly
            );
    m_previewWidget->setObjectName(
        QStringLiteral("scheduleImportPreview")
        );
    layout->addWidget(m_previewWidget);

    auto* scrollArea =
        new QScrollArea(page);
    scrollArea->setWidgetResizable(true);
    m_resolutionContent =
        new QWidget(scrollArea);
    m_resolutionLayout =
        new QVBoxLayout(m_resolutionContent);
    scrollArea->setWidget(m_resolutionContent);
    layout->addWidget(scrollArea, 1);

    m_reviewStatus =
        new QLabel(page);
    m_reviewStatus->setObjectName(
        QStringLiteral("scheduleImportReviewStatus")
        );
    m_reviewStatus->setWordWrap(true);
    layout->addWidget(m_reviewStatus);

    m_reviewSummary =
        new QLabel(page);
    m_reviewSummary->setObjectName(
        QStringLiteral("scheduleImportReviewSummary")
        );
    m_reviewSummary->setWordWrap(true);
    layout->addWidget(m_reviewSummary);

    return page;
}

void ScheduleImportDialog::browseForFile()
{
    const QString selected =
        QFileDialog::getOpenFileName(
            this,
            tr("Select Schedule Import File"),
            QFileInfo(m_fileEdit->text()).absolutePath(),
            tr("Excel Workbooks (*.xlsx)")
            );
    if (!selected.isEmpty())
    {
        setFilePath(selected);
    }
}

bool ScheduleImportDialog::loadWorkbook()
{
    if (m_fileEdit->text().trimmed().isEmpty())
    {
        m_sourceStatus->setText(
            tr("Choose an XLSX file.")
            );
        return false;
    }
    if (
        QFileInfo(m_fileEdit->text()).suffix()
            .compare(
                QStringLiteral("xlsx"),
                Qt::CaseInsensitive
                ) != 0
        )
    {
        m_sourceStatus->setText(
            tr("Choose an Excel workbook with the .xlsx extension.")
            );
        return false;
    }
    if (
        !m_normalRadio->isChecked()
        && !m_intensiveRadio->isChecked()
        )
    {
        m_sourceStatus->setText(
            tr("Choose Normal schedule or Intensive schedule.")
            );
        return false;
    }

    const ScheduleImportKind kind =
        selectedKind();

    if (
        m_workbookLoaded
        && m_loadedFilePath == m_fileEdit->text()
        && m_loadedKind == kind
        )
    {
        return true;
    }

    QFile file(m_fileEdit->text());
    if (!file.open(QIODevice::ReadOnly))
    {
        m_sourceStatus->setText(
            tr("The selected workbook could not be opened.")
            );
        return false;
    }

    const auto parsed =
        parseScheduleImportWorkbook(
            file.readAll(),
            kind
            );
    if (!parsed)
    {
        m_sourceStatus->setText(
            tr("Invalid workbook: %1")
                .arg(parsed.error())
            );
        return false;
    }

    m_workbook = *parsed;
    m_loadedFilePath = m_fileEdit->text();
    m_loadedKind = kind;
    m_workbookLoaded = true;
    m_sheetCombo->clear();

    QList<int> visibleIndexes;
    for (int index = 0; index < m_workbook.sheets.size(); ++index)
    {
        if (m_workbook.sheets[index].visible)
        {
            visibleIndexes.append(index);
        }
    }

    if (visibleIndexes.size() > 1)
    {
        m_sheetCombo->addItem(
            tr("Select a worksheet...")
            );
        m_sheetCombo->setItemData(0, -1);
    }

    for (int sheetIndex : visibleIndexes)
    {
        m_sheetCombo->addItem(
            m_workbook.sheets[sheetIndex].name,
            sheetIndex
            );
    }

    if (visibleIndexes.size() == 1)
    {
        m_sheetCombo->setCurrentIndex(0);
    }

    const bool multiple =
        visibleIndexes.size() > 1;
    m_sheetLabel->setVisible(multiple);
    m_sheetCombo->setVisible(multiple);
    m_sourceStatus->setText(
        multiple
            ? tr("Workbook loaded. Choose the worksheet to import.")
            : tr("Workbook and worksheet are valid.")
        );
    updateNavigation();
    return true;
}

void ScheduleImportDialog::prepareUserSelection()
{
    const ScheduleImportSheet* sheet =
        selectedSheet();
    m_userCombo->clear();
    m_nameConfirmation->setChecked(false);

    DataService* dataService =
        openDataService(m_services);
    m_profileName =
        dataService
            ? dataService
                ->loadSetting(
                    QStringLiteral("myInfo/name"),
                    QString()
                    )
                .toString()
                .trimmed()
            : QString();

    if (!sheet)
    {
        return;
    }

    const QString normalizedProfile =
        normalizedScheduleImportUserName(
            m_profileName
            );
    int exactIndex = -1;
    int exactCount = 0;

    for (int index = 0; index < sheet->users.size(); ++index)
    {
        if (
            !normalizedProfile.isEmpty()
            && normalizedScheduleImportUserName(
                sheet->users[index].name
                ) == normalizedProfile
            )
        {
            exactIndex = index;
            ++exactCount;
        }
    }

    const bool requireExplicit =
        m_profileName.isEmpty()
        || exactCount != 1;

    if (
        requireExplicit
        && sheet->users.size() != 1
        )
    {
        m_userCombo->addItem(
            tr("Select a detected name..."),
            -1
            );
    }
    else if (
        requireExplicit
        && m_profileName.isEmpty()
        )
    {
        m_userCombo->addItem(
            tr("Select the detected name..."),
            -1
            );
    }

    for (int index = 0; index < sheet->users.size(); ++index)
    {
        m_userCombo->addItem(
            sheet->users[index].name,
            index
            );
    }

    if (exactCount == 1)
    {
        const int comboIndex =
            m_userCombo->findData(exactIndex);
        m_userCombo->setCurrentIndex(comboIndex);
    }
    else if (
        sheet->users.size() == 1
        && !m_profileName.isEmpty()
        )
    {
        m_userCombo->setCurrentIndex(
            m_userCombo->findData(0)
            );
    }

    updateUserSelection();
}

void ScheduleImportDialog::updateUserSelection()
{
    const ScheduleImportUserBlock* user =
        selectedUser();
    const bool profileBlank =
        m_profileName.trimmed().isEmpty();
    const bool mismatch =
        user
        && !profileBlank
        && normalizedScheduleImportUserName(user->name)
            != normalizedScheduleImportUserName(
                m_profileName
                );

    if (profileBlank)
    {
        m_userStatus->setText(
            tr("My Information has no name. The selected spreadsheet name will be saved after a successful import.")
            );
    }
    else if (mismatch)
    {
        m_userStatus->setText(
            tr("My Information says “%1”, but the selected spreadsheet section is “%2”.")
                .arg(
                    m_profileName,
                    user->name
                    )
            );
    }
    else
    {
        m_userStatus->setText(
            tr("My Information name: %1")
                .arg(m_profileName)
            );
    }

    m_nameConfirmation->setVisible(mismatch);
    if (!mismatch)
    {
        m_nameConfirmation->setChecked(false);
    }
    updateNavigation();
}

bool ScheduleImportDialog::buildReview()
{
    DataService* dataService =
        openDataService(m_services);
    const ScheduleImportUserBlock* user =
        selectedUser();

    if (!dataService || !user)
    {
        return false;
    }

    const auto preview =
        dataService->previewScheduleImport(
            *user,
            selectedKind()
            );
    if (!preview)
    {
        QMessageBox::warning(
            this,
            tr("Import Schedule"),
            preview.error()
            );
        return false;
    }

    m_preview = *preview;
    m_previewWidget->setPreviewModel(
        previewModel(m_preview.user)
        );
    rebuildResolutionControls();
    updateReviewState();
    return true;
}

void ScheduleImportDialog::rebuildResolutionControls()
{
    while (QLayoutItem* item =
           m_resolutionLayout->takeAt(0))
    {
        if (QWidget* widget = item->widget())
        {
            delete widget;
        }
        delete item;
    }

    m_teacherControls.clear();
    m_classControls.clear();

    DataService* dataService =
        openDataService(m_services);
    if (!dataService)
    {
        return;
    }

    if (!m_preview.user.diagnostics.isEmpty())
    {
        auto* warnings =
            new QGroupBox(
                tr("Unrecognized cells"),
                m_resolutionContent
                );
        auto* warningsLayout =
            new QVBoxLayout(warnings);
        QStringList lines;
        for (const ScheduleImportDiagnostic& diagnostic :
             m_preview.user.diagnostics)
        {
            lines.append(
                tr("%1: %2")
                    .arg(
                        diagnostic.cellReference,
                        diagnostic.value.trimmed()
                        )
                );
        }
        m_warningLabel =
            new QLabel(
                lines.join(QLatin1Char('\n')),
                warnings
                );
        m_warningLabel->setWordWrap(true);
        m_warningAcknowledgement =
            new QCheckBox(
                tr("I reviewed these cells and want to skip them."),
                warnings
                );
        m_warningAcknowledgement->setObjectName(
            QStringLiteral("scheduleImportWarningAcknowledgement")
            );
        warningsLayout->addWidget(m_warningLabel);
        warningsLayout->addWidget(
            m_warningAcknowledgement
            );
        m_resolutionLayout->addWidget(warnings);
        connect(
            m_warningAcknowledgement,
            &QCheckBox::toggled,
            this,
            &ScheduleImportDialog::updateReviewState
            );
    }
    else
    {
        m_warningLabel = nullptr;
        m_warningAcknowledgement = nullptr;
    }

    auto* teachersGroup =
        new QGroupBox(
            tr("Korean teachers and rooms"),
            m_resolutionContent
            );
    auto* teacherGrid =
        new QGridLayout(teachersGroup);
    teacherGrid->addWidget(
        new QLabel(tr("Spreadsheet"), teachersGroup),
        0,
        0
        );
    teacherGrid->addWidget(
        new QLabel(tr("Resolution"), teachersGroup),
        0,
        1
        );
    teacherGrid->addWidget(
        new QLabel(tr("Imported room"), teachersGroup),
        0,
        2
        );

    for (int row = 0;
         row < m_preview.teachers.size();
         ++row)
    {
        const ScheduleImportTeacherPreview& preview =
            m_preview.teachers[row];
        TeacherControl control;
        control.teacherKey = preview.teacherKey;
        control.action =
            new QComboBox(teachersGroup);
        control.room =
            new QComboBox(teachersGroup);
        control.action->setObjectName(
            QStringLiteral("scheduleImportTeacherAction_%1")
                .arg(row)
            );
        control.room->setObjectName(
            QStringLiteral("scheduleImportTeacherRoom_%1")
                .arg(row)
            );

        if (preview.importedRooms.size() > 1)
        {
            control.room->addItem(
                tr("Choose a room..."),
                QString()
                );
        }
        for (const QString& room : preview.importedRooms)
        {
            control.room->addItem(room, room);
        }

        if (preview.matchingTeacherIds.size() == 1)
        {
            const int teacherId =
                preview.matchingTeacherIds.first();
            const Teacher existing =
                dataService->getTeacher(teacherId);
            const bool exactRoom =
                preview.importedRooms.size() == 1
                && existing.roomNumber.trimmed()
                    == preview.importedRooms.first().trimmed();

            if (!exactRoom)
            {
                addResolutionItem(
                    control.action,
                    tr("Choose a resolution..."),
                    -1
                    );
            }
            addResolutionItem(
                control.action,
                tr("Keep existing: %1")
                    .arg(teacherLabel(existing)),
                static_cast<int>(
                    ScheduleImportTeacherAction::Reuse
                    ),
                teacherId
                );
            if (!exactRoom)
            {
                addResolutionItem(
                    control.action,
                    tr("Update room globally (%1 affected classes)")
                        .arg(preview.affectedClassCount),
                    static_cast<int>(
                        ScheduleImportTeacherAction::UpdateRoom
                        ),
                    teacherId
                    );
                addResolutionItem(
                    control.action,
                    tr("Skip affected classes"),
                    static_cast<int>(
                        ScheduleImportTeacherAction::Skip
                        )
                    );
            }
        }
        else
        {
            if (!preview.matchingTeacherIds.isEmpty())
            {
                addResolutionItem(
                    control.action,
                    tr("Choose a resolution..."),
                    -1
                    );
            }
            for (int teacherId : preview.matchingTeacherIds)
            {
                const Teacher existing =
                    dataService->getTeacher(teacherId);
                addResolutionItem(
                    control.action,
                    tr("Use existing: %1")
                        .arg(teacherLabel(existing)),
                    static_cast<int>(
                        ScheduleImportTeacherAction::Reuse
                        ),
                    teacherId
                    );
                addResolutionItem(
                    control.action,
                    tr("Update existing room: %1")
                        .arg(teacherLabel(existing)),
                    static_cast<int>(
                        ScheduleImportTeacherAction::UpdateRoom
                        ),
                    teacherId
                    );
            }
            addResolutionItem(
                control.action,
                tr("Create a new Korean teacher"),
                static_cast<int>(
                    ScheduleImportTeacherAction::Create
                    )
                );
            addResolutionItem(
                control.action,
                tr("Skip affected classes"),
                static_cast<int>(
                    ScheduleImportTeacherAction::Skip
                    )
                );
        }

        teacherGrid->addWidget(
            new QLabel(
                QStringLiteral("%1 (%2)")
                    .arg(
                        preview.teacherKr,
                        preview.importedRooms.join(
                            QStringLiteral(", ")
                            )
                        ),
                teachersGroup
                ),
            row + 1,
            0
            );
        teacherGrid->addWidget(
            control.action,
            row + 1,
            1
            );
        teacherGrid->addWidget(
            control.room,
            row + 1,
            2
            );
        connect(
            control.action,
            &QComboBox::currentIndexChanged,
            this,
            &ScheduleImportDialog::updateReviewState
            );
        connect(
            control.room,
            &QComboBox::currentIndexChanged,
            this,
            &ScheduleImportDialog::updateReviewState
            );
        m_teacherControls.append(control);
    }
    m_resolutionLayout->addWidget(teachersGroup);

    auto* classesGroup =
        new QGroupBox(
            tr("Classes"),
            m_resolutionContent
            );
    auto* classForm =
        new QFormLayout(classesGroup);
    const QList<Classroom> allClasses =
        dataService->getClasses();

    for (const ScheduleImportClassPreview& preview :
         m_preview.classes)
    {
        const ScheduleImportClassCandidate& candidate =
            m_preview.user.classes[
                preview.candidateIndex
                ];
        ClassControl control;
        control.candidateIndex =
            preview.candidateIndex;
        control.teacherKey =
            candidate.teacherKey;
        control.action =
            new QComboBox(classesGroup);
        control.colorButton =
            new QPushButton(classesGroup);
        control.color =
            candidate.importedColors.isEmpty()
                ? QStringLiteral("#FFFFFF")
                : candidate.importedColors.first().toUpper();
        control.details =
            new QLabel(classesGroup);
        control.action->setObjectName(
            QStringLiteral("scheduleImportClassAction_%1")
                .arg(preview.candidateIndex)
            );
        control.details->setObjectName(
            QStringLiteral("scheduleImportClassDifferences_%1")
                .arg(preview.candidateIndex)
            );
        control.colorButton->setObjectName(
            QStringLiteral("scheduleImportClassColor_%1")
                .arg(preview.candidateIndex)
            );
        control.details->setWordWrap(true);
        control.details->setTextFormat(Qt::RichText);
        updateClassColorButton(&control);

        if (
            !preview.exactMatch
            && !preview.matchingClassIds.isEmpty()
            )
        {
            addResolutionItem(
                control.action,
                tr("Choose Update, Create, or Skip..."),
                -1
                );
        }

        QSet<int> addedTargets;
        for (int classId : preview.matchingClassIds)
        {
            addResolutionItem(
                control.action,
                tr("Update suggested: %1")
                    .arg(
                        classLabel(
                            dataService,
                            classId,
                            selectedKind()
                            )
                        ),
                static_cast<int>(
                    ScheduleImportClassAction::UpdateExisting
                    ),
                classId
                );
            addedTargets.insert(classId);
        }
        for (const Classroom& classroom : allClasses)
        {
            if (addedTargets.contains(classroom.id))
            {
                continue;
            }
            addResolutionItem(
                control.action,
                tr("Update existing: %1")
                    .arg(
                        classLabel(
                            dataService,
                            classroom.id,
                            selectedKind()
                            )
                        ),
                static_cast<int>(
                    ScheduleImportClassAction::UpdateExisting
                    ),
                classroom.id
                );
        }
        addResolutionItem(
            control.action,
            tr("Create new class"),
            static_cast<int>(
                ScheduleImportClassAction::CreateNew
                )
            );
        addResolutionItem(
            control.action,
            tr("Skip imported class"),
            static_cast<int>(
                ScheduleImportClassAction::Skip
                ),
            preview.suggestedClassId
            );

        if (preview.exactMatch)
        {
            control.action->setCurrentIndex(
                findActionIndex(
                    control.action,
                    static_cast<int>(
                        ScheduleImportClassAction::UpdateExisting
                        ),
                    preview.suggestedClassId
                    )
                );
        }
        else if (preview.matchingClassIds.isEmpty())
        {
            control.action->setCurrentIndex(
                findActionIndex(
                    control.action,
                    static_cast<int>(
                        ScheduleImportClassAction::CreateNew
                        )
                    )
                );
        }

        auto* classResolution =
            new QWidget(classesGroup);
        auto* classResolutionLayout =
            new QVBoxLayout(classResolution);
        classResolutionLayout->setContentsMargins(0, 0, 0, 0);
        auto* actionAndColor =
            new QHBoxLayout;
        actionAndColor->setContentsMargins(0, 0, 0, 0);
        actionAndColor->addWidget(control.action, 1);
        actionAndColor->addWidget(control.colorButton);
        classResolutionLayout->addLayout(actionAndColor);
        classResolutionLayout->addWidget(control.details);
        const QString importedMeetings =
            compactMeetingText(candidate.times);
        auto* importedClassLabel =
            new QLabel(
                QStringLiteral("%1 %2 — %3 (%4)")
                    .arg(
                        candidate.classGrade,
                        candidate.classLevel,
                        candidate.teacherKr,
                        importedMeetings.isEmpty()
                            ? tr("time unavailable")
                            : importedMeetings
                        ),
                classesGroup
                );
        importedClassLabel->setObjectName(
            QStringLiteral("scheduleImportClassCandidate_%1")
                .arg(preview.candidateIndex)
            );
        importedClassLabel->setWordWrap(true);
        classForm->addRow(
            importedClassLabel,
            classResolution
            );
        connect(
            control.action,
            &QComboBox::currentIndexChanged,
            this,
            &ScheduleImportDialog::updateReviewState
            );
        connect(
            control.colorButton,
            &QPushButton::clicked,
            this,
            [this, candidateIndex = preview.candidateIndex]()
            {
                chooseClassColor(candidateIndex);
            }
            );
        m_classControls.append(control);
    }
    m_resolutionLayout->addWidget(classesGroup);
    m_resolutionLayout->addStretch();
}

void ScheduleImportDialog::chooseClassColor(
    int candidateIndex
    )
{
    for (ClassControl& control : m_classControls)
    {
        if (control.candidateIndex != candidateIndex)
        {
            continue;
        }

        const QColor selected =
            ColorUtils::getColor(
                QColor(control.color),
                this,
                tr("Select Imported Class Color"),
                openDataService(m_services)
                );
        if (!selected.isValid())
        {
            return;
        }

        control.color =
            selected.name(QColor::HexRgb)
                .toUpper();
        updateClassColorButton(&control);
        updateReviewState();
        return;
    }
}

void ScheduleImportDialog::updateClassColorButton(
    ClassControl* control
    )
{
    if (!control || !control->colorButton)
    {
        return;
    }

    QColor color(control->color);
    if (!color.isValid())
    {
        color = QColor(QStringLiteral("#FFFFFF"));
        control->color = QStringLiteral("#FFFFFF");
    }
    const QString fontColor =
        ColorUtils::getContrastingFontColor(color);
    control->colorButton->setText(
        tr("Color %1").arg(control->color)
        );
    control->colorButton->setStyleSheet(
        QStringLiteral(
            "QPushButton {"
            "background-color:%1;"
            "color:%2;"
            "border:1px solid #777;"
            "border-radius:4px;"
            "padding:5px 10px;"
            "}"
            )
            .arg(control->color, fontColor)
        );
}

void ScheduleImportDialog::updateReviewState()
{
    bool valid = true;
    QString message;
    QSet<QString> skippedTeachers;
    QHash<QString, int> teacherActions;
    QHash<QString, int> teacherTargets;
    QHash<QString, QString> teacherRooms;
    int teacherCreates = 0;
    int teacherUpdates = 0;
    int teacherSkips = 0;

    for (const TeacherControl& control : m_teacherControls)
    {
        const int action =
            control.action->currentData(
                ActionRole
                ).toInt();
        const QString room =
            control.room->currentData().toString();
        teacherActions.insert(control.teacherKey, action);
        teacherTargets.insert(
            control.teacherKey,
            control.action->currentData(TargetRole).toInt()
            );
        teacherRooms.insert(control.teacherKey, room);

        if (action < 0)
        {
            valid = false;
            if (message.isEmpty())
            {
                message =
                    tr("Choose a resolution for every Korean teacher.");
            }
            continue;
        }
        if (
            (
                action == static_cast<int>(
                    ScheduleImportTeacherAction::Create
                    )
                || action == static_cast<int>(
                    ScheduleImportTeacherAction::UpdateRoom
                    )
                || (
                    action == static_cast<int>(
                        ScheduleImportTeacherAction::Reuse
                        )
                    && control.room->findData(QString()) >= 0
                    )
                )
            && room.isEmpty()
            )
        {
            valid = false;
            if (message.isEmpty())
            {
                message =
                    tr("Choose one imported room for this Korean teacher resolution.");
            }
            continue;
        }
        if (
            action == static_cast<int>(
                ScheduleImportTeacherAction::Skip
                )
            )
        {
            skippedTeachers.insert(
                control.teacherKey
                );
            ++teacherSkips;
        }
        else if (
            action == static_cast<int>(
                ScheduleImportTeacherAction::Create
                )
            )
        {
            ++teacherCreates;
        }
        else if (
            action == static_cast<int>(
                ScheduleImportTeacherAction::UpdateRoom
                )
            )
        {
            ++teacherUpdates;
        }
    }

    QSet<int> targets;
    QHash<int, int> classActions;
    QHash<int, int> classTargets;
    int creates = 0;
    int updates = 0;
    int skips = 0;

    for (const ClassControl& control : m_classControls)
    {
        if (skippedTeachers.contains(control.teacherKey))
        {
            const int skipIndex =
                findActionIndex(
                    control.action,
                    static_cast<int>(
                        ScheduleImportClassAction::Skip
                        )
                    );
            if (skipIndex >= 0)
            {
                control.action->setCurrentIndex(skipIndex);
            }
        }

        const int action =
            control.action->currentData(
                ActionRole
                ).toInt();
        const int target =
            control.action->currentData(
                TargetRole
                ).toInt();
        classActions.insert(control.candidateIndex, action);
        classTargets.insert(control.candidateIndex, target);

        if (action < 0)
        {
            valid = false;
            if (message.isEmpty())
            {
                message =
                    tr("Choose an action for every class.");
            }
            if (control.details)
            {
                control.details->setText(
                    tr("Choose how this imported row should be reconciled.")
                    );
            }
            continue;
        }

        if (target > 0 && targets.contains(target))
        {
            valid = false;
            message =
                tr("Two imported classes cannot use the same existing class.");
        }

        if (
            action == static_cast<int>(
                ScheduleImportClassAction::UpdateExisting
                )
            )
        {
            if (target <= 0)
            {
                valid = false;
                message =
                    tr("Choose an existing class to update.");
            }
            ++updates;
            if (control.details)
            {
                control.details->setText(
                    classDifferences(
                        openDataService(m_services),
                        m_preview.user.classes[
                            control.candidateIndex
                        ],
                        target,
                        selectedKind(),
                        control.color
                        )
                    );
            }
        }
        else if (
            action == static_cast<int>(
                ScheduleImportClassAction::CreateNew
                )
            )
        {
            ++creates;
            if (control.details)
            {
                control.details->setText(
                    classDifferences(
                        openDataService(m_services),
                        m_preview.user.classes[
                            control.candidateIndex
                        ],
                        -1,
                        selectedKind(),
                        control.color
                        )
                    );
            }
        }
        else
        {
            ++skips;
            if (control.details)
            {
                control.details->setText(
                    target > 0
                        ? tr("The imported row will be skipped and its unique existing match will keep its current schedule.")
                        : tr("The imported row will be skipped.")
                );
            }
        }

        if (control.colorButton)
        {
            control.colorButton->setEnabled(
                action != static_cast<int>(
                    ScheduleImportClassAction::Skip
                    )
                );
        }

        if (
            action != static_cast<int>(
                ScheduleImportClassAction::Skip
                )
            )
        {
            const ScheduleImportClassCandidate& candidate =
                m_preview.user.classes[
                    control.candidateIndex
                    ];
            if (!candidate.meetingPatternError.isEmpty())
            {
                valid = false;
                if (message.isEmpty())
                {
                    message =
                        tr("%1 %2 has an invalid meeting pattern. Update the spreadsheet or skip this class.")
                            .arg(
                                candidate.classGrade,
                                candidate.classLevel
                                );
                }
                if (control.details)
                {
                    control.details->setText(
                        control.details->text()
                        + QStringLiteral(
                            "<br><span style=\"color:#b91c1c\"><b>%1</b> %2</span>"
                            )
                            .arg(
                                tr("Meeting pattern:").toHtmlEscaped(),
                                candidate.meetingPatternError
                                    .toHtmlEscaped()
                                )
                        );
                }
            }
            if (
                candidate.importedColors.size() > 1
                && control.details
                )
            {
                control.details->setText(
                    control.details->text()
                    + QStringLiteral(
                        "<br><span style=\"color:#b45309\">%1</span>"
                        )
                        .arg(
                            tr("The spreadsheet uses multiple colors for this class; confirm the selected color.")
                                .toHtmlEscaped()
                            )
                    );
            }
        }

        if (target > 0)
        {
            targets.insert(target);
        }
    }

    DataService* dataService =
        openDataService(m_services);
    ScheduleImportUserBlock projected;
    projected.name = m_preview.user.name;

    if (dataService)
    {
        for (const ClassControl& control : m_classControls)
        {
            const int action =
                classActions.value(
                    control.candidateIndex,
                    -1
                    );
            const int target =
                classTargets.value(
                    control.candidateIndex,
                    -1
                    );

            if (
                action == static_cast<int>(
                    ScheduleImportClassAction::Skip
                    )
                )
            {
                if (target <= 0)
                {
                    continue;
                }

                const ClassInfo info =
                    dataService->loadClassInfo(target);
                ScheduleImportClassCandidate preserved;
                preserved.teacherKr =
                    dataService->getTeacher(
                        info.teacherId
                        ).teacherKr;
                preserved.rooms = {
                    dataService->getTeacher(
                        info.teacherId
                        ).roomNumber
                };
                preserved.classGrade = info.classGrade;
                preserved.classLevel = info.classLevel;
                preserved.importedColors = {
                    info.classColor.isEmpty()
                        ? QStringLiteral("#FFFFFF")
                        : info.classColor
                };
                preserved.times =
                    selectedKind()
                            == ScheduleImportKind::Intensive
                        ? info.intensiveTimes
                        : info.classTimes;
                projected.classes.append(preserved);
                continue;
            }

            if (action < 0)
            {
                continue;
            }

            ScheduleImportClassCandidate candidate =
                m_preview.user.classes[
                    control.candidateIndex
                    ];
            candidate.importedColors = {
                control.color
            };
            const int teacherAction =
                teacherActions.value(
                    candidate.teacherKey,
                    -1
                    );
            QString room =
                teacherRooms.value(candidate.teacherKey);
            if (
                teacherAction == static_cast<int>(
                    ScheduleImportTeacherAction::Reuse
                    )
                )
            {
                room =
                    dataService->getTeacher(
                        teacherTargets.value(
                            candidate.teacherKey,
                            -1
                            )
                        ).roomNumber;
            }
            candidate.rooms =
                room.trimmed().isEmpty()
                    ? QStringList{}
                    : QStringList{room.trimmed()};
            projected.classes.append(candidate);
        }
    }
    else
    {
        projected = m_preview.user;
    }

    m_previewWidget->setPreviewModel(
        previewModel(projected)
        );
    const QString conflict =
        projectedScheduleConflict(projected);
    if (!conflict.isEmpty())
    {
        valid = false;
        if (message.isEmpty())
        {
            message =
                tr("The proposed schedule has a conflict: %1")
                    .arg(conflict);
        }
    }

    if (
        m_warningAcknowledgement
        && !m_warningAcknowledgement->isChecked()
        )
    {
        valid = false;
        if (message.isEmpty())
        {
            message =
                tr("Acknowledge the unrecognized cells before importing.");
            }
    }

    int cleared = 0;
    if (dataService)
    {
        for (const Classroom& classroom : dataService->getClasses())
        {
            const ClassInfo info =
                dataService->loadClassInfo(classroom.id);
            const bool hasSelectedTimes =
                selectedKind() == ScheduleImportKind::Intensive
                    ? !info.intensiveTimes.isEmpty()
                    : !info.classTimes.isEmpty();
            if (
                hasSelectedTimes
                && !targets.contains(classroom.id)
                )
            {
                ++cleared;
            }
        }
    }

    m_reviewStatus->setText(
        valid
            ? tr("All required resolutions are complete.")
            : message
        );
    m_reviewSummary->setText(
        tr("Proposed snapshot: %1 teacher(s) created, %2 room update(s), %3 teacher group(s) skipped; "
           "%4 class(es) created, %5 updated, %6 skipped; %7 existing schedule(s) cleared; "
           "%8 occupied cell(s) acknowledged and ignored.%9")
            .arg(teacherCreates)
            .arg(teacherUpdates)
            .arg(teacherSkips)
            .arg(creates)
            .arg(updates)
            .arg(skips)
            .arg(cleared)
            .arg(m_preview.user.diagnostics.size())
            .arg(
                m_profileName.trimmed().isEmpty()
                    ? tr(" My Information name will be set to “%1”.")
                        .arg(m_preview.user.name)
                    : QString()
                )
        );
    updateNavigation();
}

void ScheduleImportDialog::updateNavigation()
{
    if (!m_pages)
    {
        return;
    }

    const int page =
        m_pages->currentIndex();
    m_backButton->setVisible(page > SourcePage);
    m_nextButton->setVisible(page < ReviewPage);
    m_importButton->setVisible(page == ReviewPage);

    bool nextEnabled = false;
    if (page == SourcePage)
    {
        nextEnabled =
            !m_fileEdit->text().trimmed().isEmpty()
            && (
                m_normalRadio->isChecked()
                || m_intensiveRadio->isChecked()
                );
        if (
            m_workbookLoaded
            && m_sheetCombo->isVisible()
            )
        {
            nextEnabled =
                nextEnabled
                && m_sheetCombo->currentData().toInt() >= 0;
        }
    }
    else if (page == UserPage)
    {
        const ScheduleImportUserBlock* user =
            selectedUser();
        const bool mismatch =
            user
            && !m_profileName.isEmpty()
            && normalizedScheduleImportUserName(
                user->name
                )
                != normalizedScheduleImportUserName(
                    m_profileName
                    );
        nextEnabled =
            user
            && (
                !mismatch
                || m_nameConfirmation->isChecked()
                );
    }

    m_nextButton->setEnabled(nextEnabled);
    if (page != ReviewPage)
    {
        m_importButton->setEnabled(false);
    }
    else
    {
        m_importButton->setEnabled(
            m_reviewStatus->text()
                == tr("All required resolutions are complete.")
            );
    }
}

void ScheduleImportDialog::goBack()
{
    if (m_pages->currentIndex() > SourcePage)
    {
        m_pages->setCurrentIndex(
            m_pages->currentIndex() - 1
            );
    }
    updateNavigation();
}

void ScheduleImportDialog::goNext()
{
    const int page =
        m_pages->currentIndex();
    if (page == SourcePage)
    {
        if (!loadWorkbook())
        {
            return;
        }
        if (!selectedSheet())
        {
            m_sourceStatus->setText(
                tr("Choose a worksheet.")
                );
            updateNavigation();
            return;
        }
        if (selectedSheet()->users.isEmpty())
        {
            if (!selectedSheet()->diagnostics.isEmpty())
            {
                QStringList diagnostics;
                for (const ScheduleImportDiagnostic& diagnostic :
                     selectedSheet()->diagnostics)
                {
                    diagnostics.append(
                        tr("%1: %2")
                            .arg(
                                diagnostic.cellReference,
                                diagnostic.message
                                )
                        );
                }
                m_sourceStatus->setText(
                    diagnostics.join(QLatin1Char('\n'))
                    );
            }
            else
            {
                m_sourceStatus->setText(
                    tr("The selected worksheet contains no supported user schedules.")
                    );
            }
            return;
        }
        prepareUserSelection();
        m_pages->setCurrentIndex(UserPage);
    }
    else if (page == UserPage)
    {
        if (!selectedUser() || !buildReview())
        {
            return;
        }
        m_pages->setCurrentIndex(ReviewPage);
    }
    updateNavigation();
}

void ScheduleImportDialog::applyImport()
{
    updateReviewState();
    if (!m_importButton->isEnabled())
    {
        return;
    }

    const ScheduleImportPlan plan =
        importPlan();
    const QString confirmation =
        m_reviewSummary->text()
        + QStringLiteral("\n\n")
        + tr("Is this schedule valid and ready to import?");

    if (
        QMessageBox::question(
            this,
            tr("Confirm Schedule Import"),
            confirmation,
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
            ) != QMessageBox::Yes
        )
    {
        return;
    }

    DataService* dataService =
        openDataService(m_services);
    const auto summary =
        dataService
            ? dataService->importSchedule(plan)
            : Result<ScheduleImportSummary>(
                std::unexpected(
                    tr("No database is open.")
                    )
                );

    if (!summary)
    {
        QMessageBox::warning(
            this,
            tr("Import Schedule"),
            summary.error()
            );
        return;
    }

    QMessageBox::information(
        this,
        tr("Import Schedule"),
        tr("Schedule imported successfully.\n"
           "Korean teachers created: %1\n"
           "Korean teacher rooms updated: %2\n"
           "Classes created: %3\n"
           "Classes updated: %4\n"
           "Classes skipped: %5\n"
           "Schedules cleared: %6\n"
           "Ignored occupied cells: %7%8")
            .arg(summary->teachersCreated)
            .arg(summary->teachersUpdated)
            .arg(summary->classesCreated)
            .arg(summary->classesUpdated)
            .arg(summary->classesSkipped)
            .arg(summary->schedulesCleared)
            .arg(summary->ignoredCells)
            .arg(
                summary->profileNameUpdated
                    ? tr("\nMy Information name was updated.")
                    : QString()
                )
        );
    accept();
}

ScheduleImportKind ScheduleImportDialog::selectedKind() const
{
    return m_intensiveRadio->isChecked()
        ? ScheduleImportKind::Intensive
        : ScheduleImportKind::Normal;
}

const ScheduleImportSheet* ScheduleImportDialog::selectedSheet() const
{
    if (!m_workbookLoaded)
    {
        return nullptr;
    }
    const int sheetIndex =
        m_sheetCombo->currentData().toInt();
    return sheetIndex >= 0
        && sheetIndex < m_workbook.sheets.size()
        ? &m_workbook.sheets[sheetIndex]
        : nullptr;
}

const ScheduleImportUserBlock* ScheduleImportDialog::selectedUser() const
{
    const ScheduleImportSheet* sheet =
        selectedSheet();
    const int userIndex =
        m_userCombo->currentData().toInt();
    return sheet
        && userIndex >= 0
        && userIndex < sheet->users.size()
        ? &sheet->users[userIndex]
        : nullptr;
}

ScheduleImportPlan ScheduleImportDialog::importPlan() const
{
    ScheduleImportPlan plan;
    plan.kind = selectedKind();
    plan.selectedUserName =
        m_preview.user.name;
    plan.saveProfileNameIfBlank =
        m_profileName.trimmed().isEmpty();
    plan.unknownCellsAcknowledged =
        !m_warningAcknowledgement
        || m_warningAcknowledgement->isChecked();
    plan.candidates =
        m_preview.user.classes;
    plan.intensiveSlotStates =
        m_preview.user.intensiveSlotStates;
    plan.diagnostics =
        m_preview.user.diagnostics;

    for (const TeacherControl& control : m_teacherControls)
    {
        ScheduleImportTeacherResolution resolution;
        resolution.teacherKey =
            control.teacherKey;
        resolution.action =
            static_cast<ScheduleImportTeacherAction>(
                control.action->currentData(
                    ActionRole
                    ).toInt()
                );
        resolution.targetTeacherId =
            control.action->currentData(
                TargetRole
                ).toInt();
        resolution.selectedRoom =
            control.room->currentData().toString();
        plan.teachers.append(resolution);
    }

    for (const ClassControl& control : m_classControls)
    {
        ScheduleImportClassResolution resolution;
        resolution.candidateIndex =
            control.candidateIndex;
        resolution.action =
            static_cast<ScheduleImportClassAction>(
                control.action->currentData(
                    ActionRole
                    ).toInt()
                );
        resolution.targetClassId =
            control.action->currentData(
                TargetRole
                ).toInt();
        resolution.classColor =
            control.color;
        resolution.fontColor =
            ColorUtils::getContrastingFontColor(
                QColor(control.color)
                );
        plan.classes.append(resolution);
    }

    return plan;
}
