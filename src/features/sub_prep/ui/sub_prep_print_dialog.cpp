#include "features/sub_prep/ui/sub_prep_print_dialog.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "features/sub_prep/services/sub_prep_package_service.h"
#include "ui/shared/widgets/text_fit_dialog_button_box.h"
#include "ui/shared/widgets/text_fit_push_button.h"
#include "ui/shared/dialogs/file_dialog_service.h"

#include <algorithm>
#include <utility>

#include <QCheckBox>
#include <QDir>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace
{
const QStringList Weekdays{
    QStringLiteral("Monday"),
    QStringLiteral("Tuesday"),
    QStringLiteral("Wednesday"),
    QStringLiteral("Thursday"),
    QStringLiteral("Friday")
};
constexpr int VacationLookaheadDays = 28;

QDate weekStartFor(
    const QDate& date
    )
{
    return date.isValid()
        ? date.addDays(Qt::Monday - date.dayOfWeek())
        : QDate();
}

QString checkBoxObjectName(
    const QString& day
    )
{
    return QStringLiteral("subPrepPrint%1CheckBox").arg(day);
}

QString displayDay(
    const QString& day
    )
{
    if (day == QStringLiteral("Monday"))
    {
        return SubPrepPrintDialog::tr("Monday");
    }
    if (day == QStringLiteral("Tuesday"))
    {
        return SubPrepPrintDialog::tr("Tuesday");
    }
    if (day == QStringLiteral("Wednesday"))
    {
        return SubPrepPrintDialog::tr("Wednesday");
    }
    if (day == QStringLiteral("Thursday"))
    {
        return SubPrepPrintDialog::tr("Thursday");
    }
    return SubPrepPrintDialog::tr("Friday");
}

bool isWeekday(
    const QDate& date
    )
{
    return date.dayOfWeek() >= Qt::Monday
        && date.dayOfWeek() <= Qt::Friday;
}

QSet<QDate> eventWeekdays(
    const QList<CalendarEvent>& calendarEvents,
    const QString& eventType
    )
{
    QSet<QDate> dates;

    for (const CalendarEvent& event : calendarEvents)
    {
        if (
            normalizedCalendarEventType(event.eventType) != eventType
            || !event.startDate.isValid()
            || !event.endDate.isValid()
            || event.endDate < event.startDate
            )
        {
            continue;
        }

        for (
            QDate date = event.startDate;
            date.isValid() && date <= event.endDate;
            date = date.addDays(1)
            )
        {
            if (isWeekday(date))
            {
                dates.insert(date);
            }

            if (date == event.endDate)
            {
                break;
            }
        }
    }

    return dates;
}

bool datesAreConnected(
    const QDate& previousVacationDate,
    const QDate& nextVacationDate,
    const QSet<QDate>& holidayDates
    )
{
    for (
        QDate date = previousVacationDate.addDays(1);
        date.isValid() && date < nextVacationDate;
        date = date.addDays(1)
        )
    {
        if (isWeekday(date) && !holidayDates.contains(date))
        {
            return false;
        }
    }

    return true;
}

QList<QDate> nextVacationDates(
    const QList<CalendarEvent>& calendarEvents,
    const QDate& referenceDate
    )
{
    if (!referenceDate.isValid())
    {
        return {};
    }

    QList<QDate> vacationDates =
        eventWeekdays(
            calendarEvents,
            QStringLiteral("Vacation")
            ).values();
    std::sort(vacationDates.begin(), vacationDates.end());

    if (vacationDates.isEmpty())
    {
        return {};
    }

    const QSet<QDate> holidayDates =
        eventWeekdays(
            calendarEvents,
            QStringLiteral("Holiday")
            );
    QList<QDate> block;

    for (const QDate& vacationDate : std::as_const(vacationDates))
    {
        if (
            !block.isEmpty()
            && !datesAreConnected(
                block.last(),
                vacationDate,
                holidayDates
                )
            )
        {
            if (block.last() >= referenceDate)
            {
                return block;
            }
            block.clear();
        }

        block.append(vacationDate);
    }

    return !block.isEmpty() && block.last() >= referenceDate
        ? block
        : QList<QDate>();
}

QList<QDate> nextVacationDatesWithinLookahead(
    const QList<CalendarEvent>& calendarEvents,
    const QDate& referenceDate
    )
{
    const QList<QDate> vacationDates =
        nextVacationDates(calendarEvents, referenceDate);

    if (vacationDates.isEmpty() || !referenceDate.isValid())
    {
        return {};
    }

    const QDate lookaheadEnd =
        referenceDate.addDays(VacationLookaheadDays);
    const bool fallsWithinLookahead = std::any_of(
        vacationDates.cbegin(),
        vacationDates.cend(),
        [&referenceDate, &lookaheadEnd](const QDate& date)
        {
            return date >= referenceDate && date <= lookaheadEnd;
        }
        );

    return fallsWithinLookahead
        ? vacationDates
        : QList<QDate>();
}

QStringList weekdayNamesForDates(
    const QList<QDate>& dates
    )
{
    QStringList days;

    for (int index = 0; index < Weekdays.size(); ++index)
    {
        const Qt::DayOfWeek dayOfWeek =
            static_cast<Qt::DayOfWeek>(Qt::Monday + index);
        const bool selected = std::any_of(
            dates.cbegin(),
            dates.cend(),
            [dayOfWeek](const QDate& date)
            {
                return date.dayOfWeek() == dayOfWeek;
            }
            );

        if (selected)
        {
            days.append(Weekdays.at(index));
        }
    }

    return days;
}

}

SubPrepPrintDialog::SubPrepPrintDialog(
    ApplicationServices* services,
    const ScheduleViewModel& schedule,
    const QList<CalendarEvent>& calendarEvents,
    const QDate& referenceDate,
    QWidget* parent
    )
    : DialogShell(QStringLiteral("subPrepPrint"), parent)
    , m_services(services)
    , m_schedule(schedule)
{
    SettingsService* settingsService =
        m_services
            ? m_services->settingsService()
            : nullptr;
    if (settingsService && settingsService->isAvailable())
    {
        m_storedUserName =
            settingsService->load(
                    QStringLiteral("myInfo/name"),
                    QString()
                    )
                .toString()
                .trimmed();
    }

    m_vacationDates =
        defaultSelectedDates(calendarEvents, referenceDate);
    m_weekStart = defaultWeekStart(calendarEvents, referenceDate);

    buildUi();
    initializeDays();
    updateFolderControls();
    updateOutputPreview();
    updateAcceptEnabled();
    setFixedSize(sizeHint());
}

SubPrepPrintDialog::SubPrepPrintDialog(
    const QList<CalendarEvent>& calendarEvents,
    const QDate& referenceDate,
    QWidget* parent
    )
    : SubPrepPrintDialog(
        nullptr,
        ScheduleViewModel(),
        calendarEvents,
        referenceDate,
        parent
        )
{
}

void SubPrepPrintDialog::buildUi()
{
    setModal(true);
    setWindowTitle(tr("Generate Sub Prep"));
    setMinimumWidth(560);

    auto* rootLayout = contentLayout();

    auto* daysGroup = new QGroupBox(tr("Days to Include"), this);
    daysGroup->setObjectName(QStringLiteral("subPrepDaysGroup"));
    auto* daysLayout = new QGridLayout(daysGroup);
    daysLayout->setObjectName(QStringLiteral("subPrepDaysLayout"));

    const bool hasNearbyVacation = !m_vacationDates.isEmpty();
    const int firstDayRow = hasNearbyVacation ? 1 : 0;

    if (hasNearbyVacation)
    {
        m_nextVacationCheck =
            new QCheckBox(tr("Next Vacation on the Calendar"), daysGroup);
        m_nextVacationCheck->setObjectName(
            QStringLiteral("subPrepNextVacationCheckBox")
            );
        daysLayout->addWidget(m_nextVacationCheck, 0, 0, 1, 3);
    }

    for (int index = 0; index < Weekdays.size(); ++index)
    {
        const QString& day = Weekdays.at(index);
        auto* checkBox = new QCheckBox(displayDay(day), daysGroup);
        checkBox->setObjectName(checkBoxObjectName(day));
        checkBox->setProperty("day", day);
        daysLayout->addWidget(
            checkBox,
            firstDayRow + index / 3,
            index % 3
            );
        m_dayChecks.append(checkBox);

        connect(
            checkBox,
            &QCheckBox::toggled,
            this,
            [this]
            {
                updateOutputPreview();
                updateAcceptEnabled();
            }
            );
    }
    for (int column = 0; column < 3; ++column)
    {
        daysLayout->setColumnStretch(column, 1);
    }

    rootLayout->addWidget(daysGroup);

    if (m_nextVacationCheck)
    {
        connect(
            m_nextVacationCheck,
            &QCheckBox::toggled,
            this,
            &SubPrepPrintDialog::updateVacationMode
            );
    }

    m_createFolderCheck =
        new QCheckBox(tr("Create Sub Prep Folder"), this);
    m_createFolderCheck->setObjectName(
        QStringLiteral("subPrepCreateFolderCheckBox")
        );
    m_createFolderCheck->setChecked(true);
    rootLayout->addWidget(m_createFolderCheck);

    m_folderOptions = new QWidget(this);
    m_folderOptions->setObjectName(QStringLiteral("subPrepFolderOptions"));
    auto* folderLayout = new QGridLayout(m_folderOptions);
    folderLayout->setContentsMargins(24, 0, 0, 0);
    const int sectionSpacing = std::max(
        0,
        folderLayout->verticalSpacing()
        );
    folderLayout->setVerticalSpacing(0);
    folderLayout->setRowMinimumHeight(1, sectionSpacing);
    folderLayout->setRowMinimumHeight(5, sectionSpacing);

    auto* targetFolderLabel =
        new QLabel(tr("Target Folder:"), m_folderOptions);
    targetFolderLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    folderLayout->addWidget(targetFolderLabel, 0, 0, Qt::AlignVCenter);
    m_targetRootEdit = new QLineEdit(m_folderOptions);
    m_targetRootEdit->setObjectName(QStringLiteral("subPrepTargetFolderEdit"));
    m_targetRootEdit->setText(SubPrepPackageService::defaultTargetRoot());
    folderLayout->addWidget(m_targetRootEdit, 0, 1, Qt::AlignVCenter);
    auto* selectFolderButton =
        new TextFitPushButton(tr("Select Folder"), m_folderOptions);
    selectFolderButton->setObjectName(
        QStringLiteral("subPrepSelectFolderButton")
        );
    folderLayout->addWidget(selectFolderButton, 0, 2, Qt::AlignVCenter);

    m_nameLabel = new QLabel(tr("Your Name:"), m_folderOptions);
    m_nameLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_nameEdit = new QLineEdit(m_folderOptions);
    m_nameEdit->setObjectName(QStringLiteral("subPrepUserNameEdit"));
    auto* nameHint =
        new QLabel(tr("Enter your name to continue."), m_folderOptions);
    nameHint->setObjectName(QStringLiteral("subPrepUserNameHintLabel"));
    nameHint->setWordWrap(true);
    m_nameEdit->setVisible(m_storedUserName.isEmpty());
    m_nameLabel->setVisible(m_storedUserName.isEmpty());
    nameHint->setVisible(m_storedUserName.isEmpty());
    folderLayout->addWidget(m_nameLabel, 2, 0, Qt::AlignVCenter);
    folderLayout->addWidget(m_nameEdit, 2, 1, 1, 2, Qt::AlignVCenter);
    folderLayout->addWidget(nameHint, 3, 1, 1, 2, Qt::AlignTop);

    auto* outputFolderLabel =
        new QLabel(tr("Output Folder:"), m_folderOptions);
    outputFolderLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    folderLayout->addWidget(
        outputFolderLabel,
        4,
        0,
        Qt::AlignVCenter
        );
    m_outputPreviewLabel = new QLabel(m_folderOptions);
    m_outputPreviewLabel->setObjectName(
        QStringLiteral("subPrepOutputFolderPreview")
        );
    m_outputPreviewLabel->setWordWrap(true);
    m_outputPreviewLabel->setTextInteractionFlags(
        Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard
        );
    m_outputPreviewLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_outputPreviewLabel->setFixedHeight(
        2 * m_outputPreviewLabel->fontMetrics().lineSpacing()
        );
    folderLayout->addWidget(
        m_outputPreviewLabel,
        4,
        1,
        1,
        2,
        Qt::AlignVCenter
        );

    const int labelColumnWidth = std::max(
        targetFolderLabel->sizeHint().width(),
        outputFolderLabel->sizeHint().width()
        );
    const int labelToValueSpacing = std::max(
        0,
        folderLayout->horizontalSpacing()
        );
    folderLayout->setColumnMinimumWidth(
        0,
        labelColumnWidth + labelToValueSpacing
        );

    m_openFolderCheck =
        new QCheckBox(tr("Open Folder After Generation"), m_folderOptions);
    m_openFolderCheck->setObjectName(
        QStringLiteral("subPrepOpenFolderCheckBox")
        );
    m_openFolderCheck->setChecked(true);
    folderLayout->addWidget(
        m_openFolderCheck,
        6,
        1,
        1,
        2,
        Qt::AlignTop
        );
    folderLayout->setRowStretch(7, 1);
    rootLayout->addWidget(m_folderOptions);

    m_printPaperCheck = new QCheckBox(tr("Print Paper Copies"), this);
    m_printPaperCheck->setObjectName(
        QStringLiteral("subPrepPrintPaperCopiesCheckBox")
        );
    rootLayout->addWidget(m_printPaperCheck, 0, Qt::AlignTop);

    m_validationLabel = new QLabel(this);
    m_validationLabel->setObjectName(
        QStringLiteral("subPrepGenerationValidationLabel")
        );
    m_validationLabel->setWordWrap(true);
    m_validationLabel->setFixedHeight(
        2 * m_validationLabel->fontMetrics().lineSpacing()
        );
    rootLayout->addWidget(m_validationLabel);

    auto* buttons = addButtonBox(QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Cancel)->setObjectName(
        QStringLiteral("subPrepPrintCancelButton")
        );
    m_okButton = buttons->addButton(
        tr("OK"),
        QDialogButtonBox::ActionRole
        );
    m_okButton->setObjectName(QStringLiteral("subPrepGenerateOkButton"));
    m_okButton->setDefault(true);

    connect(
        m_createFolderCheck,
        &QCheckBox::toggled,
        this,
        [this]
        {
            updateFolderControls();
            updateAcceptEnabled();
        }
        );
    connect(
        m_printPaperCheck,
        &QCheckBox::toggled,
        this,
        &SubPrepPrintDialog::updateAcceptEnabled
        );
    connect(
        m_targetRootEdit,
        &QLineEdit::textChanged,
        this,
        [this]
        {
            updateOutputPreview();
            updateAcceptEnabled();
        }
        );
    connect(
        m_nameEdit,
        &QLineEdit::textChanged,
        this,
        [this]
        {
            updateOutputPreview();
            updateAcceptEnabled();
        }
        );
    connect(
        selectFolderButton,
        &QPushButton::clicked,
        this,
        &SubPrepPrintDialog::chooseTargetRoot
        );
    connect(
        m_okButton,
        &QPushButton::clicked,
        this,
        &SubPrepPrintDialog::acceptGeneration
        );

    daysGroup->setSizePolicy(
        QSizePolicy::Preferred,
        QSizePolicy::Fixed
        );
    daysGroup->setFixedHeight(daysGroup->sizeHint().height());
    m_createFolderCheck->setFixedHeight(
        m_createFolderCheck->sizeHint().height()
        );
    m_folderOptions->setSizePolicy(
        QSizePolicy::Preferred,
        QSizePolicy::Fixed
        );
    m_folderOptions->setFixedHeight(
        m_folderOptions->sizeHint().height()
        );
    m_printPaperCheck->setFixedHeight(
        m_printPaperCheck->sizeHint().height()
        );
}

void SubPrepPrintDialog::initializeDays()
{
    const QStringList vacationDays =
        weekdayNamesForDates(m_vacationDates);

    for (int index = 0; index < m_dayChecks.size(); ++index)
    {
        QCheckBox* checkBox = m_dayChecks.at(index);
        const QString day = Weekdays.at(index);
        const QSignalBlocker blocker(checkBox);
        checkBox->setChecked(vacationDays.contains(day));
    }

    if (m_nextVacationCheck)
    {
        const QSignalBlocker blocker(m_nextVacationCheck);
        m_nextVacationCheck->setChecked(true);
    }
    updateVacationMode();
}

void SubPrepPrintDialog::updateFolderControls()
{
    if (m_folderOptions)
    {
        m_folderOptions->setEnabled(createFolder());
    }
    updateOutputPreview();
}

void SubPrepPrintDialog::updateVacationMode()
{
    const bool useNextVacation =
        m_nextVacationCheck
        && m_nextVacationCheck->isChecked();

    if (useNextVacation)
    {
        const QStringList vacationDays =
            weekdayNamesForDates(m_vacationDates);

        for (QCheckBox* checkBox : std::as_const(m_dayChecks))
        {
            const QSignalBlocker blocker(checkBox);
            checkBox->setChecked(
                vacationDays.contains(
                    checkBox->property("day").toString()
                    )
                );
        }
    }

    for (QCheckBox* checkBox : std::as_const(m_dayChecks))
    {
        checkBox->setEnabled(!useNextVacation);
    }

    updateOutputPreview();
    updateAcceptEnabled();
}

void SubPrepPrintDialog::updateOutputPreview()
{
    if (!m_outputPreviewLabel)
    {
        return;
    }

    const QString folderName =
        SubPrepPackageService::datedFolderName(
            userName(),
            selectedDates()
            );

    if (folderName.isEmpty() || userName().trimmed().isEmpty())
    {
        m_outputPreviewLabel->clear();
        return;
    }

    m_outputPreviewLabel->setText(
        tr(".../%1").arg(folderName)
        );
}

void SubPrepPrintDialog::updateAcceptEnabled()
{
    QString error;

    if (!createFolder() && !printPaperCopies())
    {
        error = tr(
            "Select Create Folder and/or Print Paper Copies to continue."
            );
    }
    else if (selectedDates().isEmpty())
    {
        if (createFolder() && userName().trimmed().isEmpty())
        {
            error = tr(
                "Select days and enter your name to preview the output folder."
                );
        }
        else if (createFolder())
        {
            error = tr("Select days to preview the output folder.");
        }
        else
        {
            error = tr("Select at least one day to include.");
        }
    }
    else if (selectedClassIds().isEmpty())
    {
        error = tr("No classes meet on the selected days.");
    }
    else if (createFolder() && targetRoot().trimmed().isEmpty())
    {
        error = tr("Choose a target folder.");
    }
    else if (createFolder() && userName().trimmed().isEmpty())
    {
        error = tr("Enter your name for the Sub Prep folder.");
    }
    else if (
        createFolder()
        && QFileInfo(targetRoot()).exists()
        && !QFileInfo(targetRoot()).isDir()
        )
    {
        error = tr("The target path is not a folder.");
    }

    m_validationLabel->setText(error);
    m_okButton->setEnabled(error.isEmpty());
}

void SubPrepPrintDialog::chooseTargetRoot()
{
    QString startPath = targetRoot();
    while (!startPath.isEmpty() && !QFileInfo(startPath).isDir())
    {
        const QString parentPath = QFileInfo(startPath).absolutePath();
        if (parentPath == startPath)
        {
            break;
        }
        startPath = parentPath;
    }

    const std::optional<QString> selection =
        DialogServices::fileDialogs().selectDirectory(
            DirectoryRequest{
                .parent = this,
                .title = tr("Select Folder"),
                .purpose = FileDialogPurpose::SubPrepPackage,
                .initialDirectory = startPath
            }
            );

    if (selection)
    {
        m_targetRootEdit->setText(QDir::cleanPath(*selection));
    }
}

void SubPrepPrintDialog::acceptGeneration()
{
    updateAcceptEnabled();
    if (!m_okButton->isEnabled())
    {
        return;
    }

    m_replaceExisting = false;
    const QString directory = outputDirectory();

    if (createFolder() && QFileInfo::exists(directory))
    {
        const PromptChoice answer =
            DialogServices::confirm(
                this,
                tr("Replace Existing Sub Prep Folder"),
                tr("Replace the existing folder and all of its contents?\n\n%1")
                    .arg(directory),
                tr("Replace"),
                tr("Cancel"),
                true
                );

        if (answer != PromptChoice::Destructive)
        {
            return;
        }

        m_replaceExisting = true;
    }

    if (
        createFolder()
        && m_storedUserName.isEmpty()
        && m_services
        )
    {
        SettingsService* settingsService =
            m_services->settingsService();
        if (settingsService && settingsService->isAvailable())
        {
            settingsService->save(
                QStringLiteral("myInfo/name"),
                m_nameEdit->text().trimmed()
                );
        }
    }

    accept();
}

QList<QDate> SubPrepPrintDialog::selectedDates() const
{
    if (
        m_nextVacationCheck
        && m_nextVacationCheck->isChecked()
        )
    {
        return m_vacationDates;
    }

    QList<QDate> dates;

    if (!m_weekStart.isValid())
    {
        return dates;
    }

    for (int index = 0; index < m_dayChecks.size(); ++index)
    {
        if (m_dayChecks.at(index)->isChecked())
        {
            dates.append(m_weekStart.addDays(index));
        }
    }

    return dates;
}

QStringList SubPrepPrintDialog::selectedDays() const
{
    QStringList days;

    for (const QCheckBox* checkBox : m_dayChecks)
    {
        if (checkBox->isChecked())
        {
            days.append(checkBox->property("day").toString());
        }
    }

    return days;
}

QList<int> SubPrepPrintDialog::selectedClassIds() const
{
    return SubPrepPackageService::classIdsForDays(
        m_schedule,
        selectedDays()
        );
}

bool SubPrepPrintDialog::createFolder() const
{
    return m_createFolderCheck && m_createFolderCheck->isChecked();
}

QString SubPrepPrintDialog::targetRoot() const
{
    return m_targetRootEdit ? m_targetRootEdit->text().trimmed() : QString();
}

QString SubPrepPrintDialog::userName() const
{
    return !m_storedUserName.isEmpty()
        ? m_storedUserName
        : m_nameEdit
            ? m_nameEdit->text().trimmed()
            : QString();
}

bool SubPrepPrintDialog::printPaperCopies() const
{
    return m_printPaperCheck && m_printPaperCheck->isChecked();
}

bool SubPrepPrintDialog::openFolderAfterGeneration() const
{
    return createFolder()
        && m_openFolderCheck
        && m_openFolderCheck->isChecked();
}

bool SubPrepPrintDialog::replaceExisting() const
{
    return m_replaceExisting;
}

QString SubPrepPrintDialog::outputDirectory() const
{
    return QDir(targetRoot()).filePath(
        SubPrepPackageService::datedFolderName(
            userName(),
            selectedDates()
            )
        );
}

QStringList SubPrepPrintDialog::defaultSelectedDays(
    const QList<CalendarEvent>& calendarEvents,
    const QDate& referenceDate
    )
{
    return weekdayNamesForDates(
        defaultSelectedDates(calendarEvents, referenceDate)
        );
}

QList<QDate> SubPrepPrintDialog::defaultSelectedDates(
    const QList<CalendarEvent>& calendarEvents,
    const QDate& referenceDate
    )
{
    return nextVacationDatesWithinLookahead(
        calendarEvents,
        referenceDate
        );
}

QDate SubPrepPrintDialog::defaultWeekStart(
    const QList<CalendarEvent>& calendarEvents,
    const QDate& referenceDate
    )
{
    const QList<QDate> vacationDates =
        defaultSelectedDates(calendarEvents, referenceDate);

    return !vacationDates.isEmpty()
        ? weekStartFor(vacationDates.first())
        : weekStartFor(referenceDate);
}
