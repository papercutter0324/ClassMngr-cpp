#include "schedule_import_dialog.h"

#include "core/application_services.h"
#include "core/utils/colorutils.h"
#include "data/data_service.h"
#include "domain/models/classroom.h"
#include "domain/rules/schedule_import_rules.h"
#include "features/classes/config/class_info_config.h"
#include "features/schedule/import/schedule_workbook_parser.h"
#include "features/schedule/ui/schedule_view_model.h"
#include "features/schedule/ui/schedule_widget.h"
#include "features/teacher/import/teacher_import_name_utils.h"
#include "ui/shared/widgets/no_wheel_combobox.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHash>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScreen>
#include <QScrollBar>
#include <QSet>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStyle>
#include <QTableWidget>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>

namespace
{
constexpr int ActionRole = Qt::UserRole;
constexpr int TargetRole = Qt::UserRole + 1;
constexpr int SourcePage = 0;
constexpr int ReviewPage = 1;
constexpr int CompactDialogHeight = 520;
constexpr int ReviewDialogHeight = 820;
constexpr int MaximumReviewDialogWidth = 1000;
constexpr int SourceDialogWidthNumerator = 13;
constexpr int SourceDialogWidthDenominator = 10;

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

int weekdayIndex(
    const QString& day
    );

QString weekdayLabel(
    const QString& day
    );

QString meetingText(
    const QList<ClassTime>& times
    )
{
    QList<ClassTime> orderedTimes = times;
    std::stable_sort(
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

    QStringList meetings;
    for (const ClassTime& time : orderedTimes)
    {
        meetings.append(
            QStringLiteral("%1 %2–%3")
                .arg(
                    weekdayLabel(time.day),
                    timeDisplay(time.startTime),
                    timeDisplay(time.endTime)
                    )
            );
    }
    return meetings.join(QStringLiteral(", "));
}

int weekdayIndex(
    const QString& day
    )
{
    const int index =
        ClassInfoConfig::Days.indexOf(day.trimmed());
    return index >= 0
        ? index
        : std::numeric_limits<int>::max();
}

QString weekdayLabel(
    const QString& day
    )
{
    if (day == QStringLiteral("Monday"))
    {
        return QObject::tr("Mon.");
    }
    if (day == QStringLiteral("Tuesday"))
    {
        return QObject::tr("Tues.");
    }
    if (day == QStringLiteral("Wednesday"))
    {
        return QObject::tr("Wed.");
    }
    if (day == QStringLiteral("Thursday"))
    {
        return QObject::tr("Thurs.");
    }
    if (day == QStringLiteral("Friday"))
    {
        return QObject::tr("Fri.");
    }
    if (day == QStringLiteral("Saturday"))
    {
        return QObject::tr("Sat.");
    }
    if (day == QStringLiteral("Sunday"))
    {
        return QObject::tr("Sun.");
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
    const QString& classColor,
    const QColor& changesColor
    )
{
    const QString importedClass =
        QStringLiteral("<b>%1</b> %2")
            .arg(
                QObject::tr("Imported Class:").toHtmlEscaped(),
                meetingText(candidate.times).toHtmlEscaped()
                );

    if (!dataService || targetClassId <= 0)
    {
        return QStringLiteral("%1<br>%2")
            .arg(
                importedClass,
                QObject::tr(
                    "A new class will be created with color %1."
                    )
                    .arg(classColor)
                    .toHtmlEscaped()
                );
    }

    const auto differenceItem =
        [](const QString& label,
           const QString& existing,
           const QString& imported)
        {
            return QStringLiteral(
                "<li><b>%1:</b> %2 → %3</li>"
                )
                .arg(
                    label.toHtmlEscaped(),
                    existing.toHtmlEscaped(),
                    imported.toHtmlEscaped()
                    );
        };

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
            differenceItem(
                QObject::tr("Grade"),
                existing.classGrade,
                candidate.classGrade
                )
            );
    }
    if (existing.classLevel != candidate.classLevel)
    {
        differences.append(
            differenceItem(
                QObject::tr("Level"),
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
            differenceItem(
                QObject::tr("Teacher"),
                existingTeacher.teacherKr,
                candidate.teacherKr
                )
            );
    }
    if (meetingKeys(existingTimes) != meetingKeys(candidate.times))
    {
        differences.append(
            differenceItem(
                QObject::tr("Days"),
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
            differenceItem(
                QObject::tr("Color"),
                existing.classColor,
                classColor
                )
            );
    }

    if (differences.isEmpty())
    {
        return QStringLiteral("%1<br>%2")
            .arg(
                importedClass,
                QObject::tr(
                    "No grade, level, teacher, day, or color differences."
                    )
                    .toHtmlEscaped()
                );
    }

    return QStringLiteral(
        "%1<br>"
        "<span style=\"color:%2\"><b>%3</b>"
        "<ul style=\"margin-top:2px; margin-bottom:0px;\">%4</ul>"
        "</span>"
        )
        .arg(
            importedClass,
            changesColor.name(QColor::HexRgb),
            QObject::tr("Changes:").toHtmlEscaped(),
            differences.join(QString())
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

int configuredValueOrder(
    const QStringList& configuredValues,
    const QString& value
    )
{
    for (int index = 0; index < configuredValues.size(); ++index)
    {
        if (
            configuredValues[index].compare(
                value.trimmed(),
                Qt::CaseInsensitive
                ) == 0
            )
        {
            return index;
        }
    }
    return std::numeric_limits<int>::max();
}

bool importedClassLess(
    const ScheduleImportClassCandidate& left,
    const ScheduleImportClassCandidate& right
    )
{
    const int leftGrade =
        configuredValueOrder(
            ClassInfoConfig::Grades,
            left.classGrade
            );
    const int rightGrade =
        configuredValueOrder(
            ClassInfoConfig::Grades,
            right.classGrade
            );
    if (leftGrade != rightGrade)
    {
        return leftGrade < rightGrade;
    }
    if (
        leftGrade == std::numeric_limits<int>::max()
        && left.classGrade.compare(
            right.classGrade,
            Qt::CaseInsensitive
            ) != 0
        )
    {
        return left.classGrade.compare(
            right.classGrade,
            Qt::CaseInsensitive
            ) < 0;
    }

    const QString configuredGrade =
        leftGrade == std::numeric_limits<int>::max()
            ? left.classGrade.trimmed()
            : ClassInfoConfig::Grades[leftGrade];
    const QStringList levels =
        ClassInfoConfig::levelsForGrade(
            configuredGrade
            );
    const int leftLevel =
        configuredValueOrder(levels, left.classLevel);
    const int rightLevel =
        configuredValueOrder(levels, right.classLevel);
    if (leftLevel != rightLevel)
    {
        return leftLevel < rightLevel;
    }
    if (
        leftLevel == std::numeric_limits<int>::max()
        && left.classLevel.compare(
            right.classLevel,
            Qt::CaseInsensitive
            ) != 0
        )
    {
        return left.classLevel.compare(
            right.classLevel,
            Qt::CaseInsensitive
            ) < 0;
    }
    return false;
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
    buildUi();
    resizeForSourceStage();
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
    resetUserSelection();
    m_sourceStatus->setText(
        tr("Choose the schedule type, then continue.")
        );
    updateNavigation();
    resizeForSourceStage();
}

void ScheduleImportDialog::buildUi()
{
    auto* layout =
        new QVBoxLayout(this);
    m_pages =
        new QStackedWidget(this);
    m_pages->setSizePolicy(
        QSizePolicy::Ignored,
        QSizePolicy::Ignored
        );
    m_sourcePage = buildSourcePage();
    m_pages->addWidget(m_sourcePage);
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
    page->setSizePolicy(
        QSizePolicy::Ignored,
        QSizePolicy::Ignored
        );
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
        new NoWheelComboBox(page);
    m_sheetCombo->setObjectName(
        QStringLiteral("scheduleImportSheetCombo")
        );
    layout->addWidget(m_sheetLabel);
    layout->addWidget(m_sheetCombo);
    m_sheetLabel->setVisible(false);
    m_sheetCombo->setVisible(false);

    m_userSection =
        new QGroupBox(
            tr("Choose your schedule section"),
            page
            );
    auto* userLayout =
        new QVBoxLayout(m_userSection);
    m_userStatus =
        new QLabel(m_userSection);
    m_userStatus->setWordWrap(true);
    userLayout->addWidget(m_userStatus);

    m_userCombo =
        new NoWheelComboBox(m_userSection);
    m_userCombo->setObjectName(
        QStringLiteral("scheduleImportUserCombo")
        );
    userLayout->addWidget(m_userCombo);

    m_nameConfirmation =
        new QCheckBox(
            tr("Use the selected spreadsheet name even though it does not match My Information."),
            m_userSection
            );
    m_nameConfirmation->setObjectName(
        QStringLiteral("scheduleImportNameConfirmation")
        );
    userLayout->addWidget(m_nameConfirmation);
    m_userSection->setVisible(false);
    layout->addWidget(m_userSection);

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
            resetUserSelection();
            m_sourceStatus->setText(
                tr("Ready to read the workbook.")
                );
            updateNavigation();
            resizeForSourceStage();
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
        [this]()
        {
            updateSelectedSheet();
            updateNavigation();
            resizeForSourceStage();
        }
        );
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
    page->setSizePolicy(
        QSizePolicy::Ignored,
        QSizePolicy::Ignored
        );
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

    m_resolutionScrollArea =
        new QScrollArea(page);
    m_resolutionScrollArea->setObjectName(
        QStringLiteral("scheduleImportResolutionScrollArea")
        );
    m_resolutionScrollArea->setWidgetResizable(true);
    m_resolutionContent =
        new QWidget(m_resolutionScrollArea);
    m_resolutionLayout =
        new QVBoxLayout(m_resolutionContent);
    m_resolutionScrollArea->setWidget(m_resolutionContent);
    layout->addWidget(m_resolutionScrollArea, 1);

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
    const QSignalBlocker sheetComboBlocker(m_sheetCombo);
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
    updateSelectedSheet();
    updateNavigation();
    resizeForSourceStage();
    return true;
}

void ScheduleImportDialog::resetUserSelection()
{
    if (m_userCombo)
    {
        const QSignalBlocker blocker(m_userCombo);
        m_userCombo->clear();
    }
    if (m_nameConfirmation)
    {
        m_nameConfirmation->setChecked(false);
        m_nameConfirmation->setVisible(false);
    }
    if (m_userStatus)
    {
        m_userStatus->clear();
    }
    if (m_userSection)
    {
        m_userSection->setVisible(false);
    }
}

void ScheduleImportDialog::updateSelectedSheet()
{
    resetUserSelection();
    const ScheduleImportSheet* sheet =
        selectedSheet();
    if (!sheet)
    {
        return;
    }

    if (sheet->users.isEmpty())
    {
        if (!sheet->diagnostics.isEmpty())
        {
            QStringList diagnostics;
            for (const ScheduleImportDiagnostic& diagnostic :
                 sheet->diagnostics)
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

    m_sourceStatus->setText(
        tr("Workbook and worksheet are valid.")
        );
    prepareUserSelection();
}

void ScheduleImportDialog::prepareUserSelection()
{
    const ScheduleImportSheet* sheet =
        selectedSheet();
    const QSignalBlocker userComboBlocker(m_userCombo);
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

    m_userSection->setVisible(true);
    updateUserSelection();
    resizeForSourceStage();
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
            new NoWheelComboBox(teachersGroup);
        control.room =
            new NoWheelComboBox(teachersGroup);
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
    auto* classListLayout =
        new QVBoxLayout(classesGroup);
    classListLayout->setSpacing(8);
    const QList<Classroom> allClasses =
        dataService->getClasses();
    QList<ScheduleImportClassPreview> orderedClasses =
        m_preview.classes;
    std::stable_sort(
        orderedClasses.begin(),
        orderedClasses.end(),
        [this](
            const ScheduleImportClassPreview& left,
            const ScheduleImportClassPreview& right
            )
        {
            return importedClassLess(
                m_preview.user.classes[left.candidateIndex],
                m_preview.user.classes[right.candidateIndex]
                );
        }
        );

    for (const ScheduleImportClassPreview& preview :
         orderedClasses)
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
            new NoWheelComboBox(classesGroup);
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
            const ClassInfo info =
                dataService->loadClassInfo(classroom.id);
            if (
                !scheduleImportClassOptionIsEligible(
                    candidate,
                    info,
                    selectedKind()
                    )
                )
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

        auto* classRow =
            new QWidget(classesGroup);
        classRow->setObjectName(
            QStringLiteral("scheduleImportClassRow_%1")
                .arg(preview.candidateIndex)
            );
        auto* classRowLayout =
            new QGridLayout(classRow);
        classRowLayout->setContentsMargins(0, 0, 0, 0);
        classRowLayout->setVerticalSpacing(4);
        classRowLayout->setColumnStretch(1, 1);
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
        classRowLayout->addWidget(
            importedClassLabel,
            0,
            0,
            Qt::AlignVCenter
            );
        classRowLayout->addWidget(control.action, 0, 1);
        classRowLayout->addWidget(control.colorButton, 0, 2);
        classRowLayout->addWidget(control.details, 1, 1, 1, 2);
        classListLayout->addWidget(classRow);
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
                        control.color,
                        control.details->palette().color(
                            QPalette::Link
                            )
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
                        control.color,
                        control.details->palette().color(
                            QPalette::Link
                            )
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
                        "<br><span style=\"color:%1\">%2</span>"
                        )
                        .arg(
                            control.details->palette()
                                .color(QPalette::Link)
                                .name(QColor::HexRgb),
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

void ScheduleImportDialog::resizeForSourceStage()
{
    if (!m_sourcePage || !m_buttons || !layout())
    {
        return;
    }

    if (m_sourcePage->layout())
    {
        m_sourcePage->layout()->activate();
    }
    layout()->activate();

    const QMargins outerMargins =
        layout()->contentsMargins();
    const int contentWidth =
        std::max(
            m_sourcePage->sizeHint().width(),
            m_buttons->sizeHint().width()
            );
    int targetWidth =
        (
            contentWidth
            * SourceDialogWidthNumerator
            + SourceDialogWidthDenominator
            - 1
            )
        / SourceDialogWidthDenominator
        + outerMargins.left()
        + outerMargins.right();
    int targetHeight =
        CompactDialogHeight;

    if (QScreen* targetScreen = screen())
    {
        const QSize available =
            targetScreen->availableGeometry().size();
        targetWidth =
            std::min(targetWidth, available.width());
        targetHeight =
            std::min(targetHeight, available.height());
    }
    resize(
        std::max(1, targetWidth),
        std::max(1, targetHeight)
        );
    m_pages->updateGeometry();
    update();
}

void ScheduleImportDialog::resizeForReviewStage()
{
    if (
        !m_resolutionScrollArea
        || !m_resolutionContent
        || !m_previewWidget
        || !m_buttons
        || !layout()
        )
    {
        return;
    }

    if (m_resolutionLayout)
    {
        m_resolutionLayout->activate();
    }
    if (QWidget* reviewPage = m_pages->widget(ReviewPage))
    {
        if (reviewPage->layout())
        {
            reviewPage->layout()->activate();
        }
    }
    layout()->activate();

    const int scrollBarExtent =
        style()->pixelMetric(
            QStyle::PM_ScrollBarExtent,
            nullptr,
            m_resolutionScrollArea
            );
    const int resolutionWidth =
        std::max(
            m_resolutionContent->sizeHint().width(),
            m_resolutionContent->minimumSizeHint().width()
            )
        + (2 * m_resolutionScrollArea->frameWidth())
        + scrollBarExtent;

    int previewWidth =
        m_previewWidget->sizeHint().width();
    if (
        auto* table =
            m_previewWidget->findChild<QTableWidget*>(
                QStringLiteral("scheduleTable")
                )
        )
    {
        previewWidth =
            std::max(
                previewWidth,
                table->horizontalHeader()->length()
                    + (2 * table->frameWidth())
                );
    }

    int reviewContentWidth =
        std::max(resolutionWidth, previewWidth);
    if (QWidget* reviewPage = m_pages->widget(ReviewPage))
    {
        if (reviewPage->layout())
        {
            const QMargins pageMargins =
                reviewPage->layout()->contentsMargins();
            reviewContentWidth +=
                pageMargins.left()
                + pageMargins.right();
        }
    }

    const QMargins outerMargins =
        layout()->contentsMargins();
    int targetWidth =
        std::max(
            reviewContentWidth,
            m_buttons->sizeHint().width()
            )
        + outerMargins.left()
        + outerMargins.right();
    targetWidth =
        std::min(
            targetWidth,
            MaximumReviewDialogWidth
            );
    int targetHeight =
        ReviewDialogHeight;

    if (QScreen* targetScreen = screen())
    {
        const QSize available =
            targetScreen->availableGeometry().size();
        targetWidth =
            std::min(targetWidth, available.width());
        targetHeight =
            std::min(targetHeight, available.height());
    }
    resize(
        std::max(1, targetWidth),
        std::max(1, targetHeight)
        );
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
        const bool sourceReady =
            !m_fileEdit->text().trimmed().isEmpty()
            && (
                m_normalRadio->isChecked()
                || m_intensiveRadio->isChecked()
                );
        if (!m_workbookLoaded)
        {
            nextEnabled = sourceReady;
        }
        else
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
                sourceReady
                && selectedSheet()
                && user
                && (
                    !mismatch
                    || m_nameConfirmation->isChecked()
                    );
        }
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
    if (m_pages->currentIndex() == ReviewPage)
    {
        m_pages->setCurrentIndex(SourcePage);
        resizeForSourceStage();
    }
    updateNavigation();
}

void ScheduleImportDialog::goNext()
{
    const int page =
        m_pages->currentIndex();
    if (page == SourcePage)
    {
        const bool workbookWasLoaded =
            m_workbookLoaded
            && m_loadedFilePath == m_fileEdit->text()
            && m_loadedKind == selectedKind();
        if (!loadWorkbook())
        {
            return;
        }
        if (!workbookWasLoaded)
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
        if (!m_userSection->isVisible())
        {
            updateSelectedSheet();
            return;
        }
        if (!selectedUser() || !buildReview())
        {
            return;
        }
        m_pages->setCurrentIndex(ReviewPage);
        resizeForReviewStage();
        QTimer::singleShot(
            0,
            this,
            &ScheduleImportDialog::resizeForReviewStage
            );
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
