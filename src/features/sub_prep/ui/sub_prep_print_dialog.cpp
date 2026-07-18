#include "features/sub_prep/ui/sub_prep_print_dialog.h"

#include "core/application_services.h"
#include "data/data_service.h"
#include "features/sub_prep/services/sub_prep_package_service.h"

#include <algorithm>

#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
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

QStringList vacationDaysInWeek(
    const QList<CalendarEvent>& calendarEvents,
    const QDate& weekStart
    )
{
    QStringList days;

    for (int offset = 0; offset < Weekdays.size(); ++offset)
    {
        const QDate date = weekStart.addDays(offset);
        const bool vacation = std::any_of(
            calendarEvents.cbegin(),
            calendarEvents.cend(),
            [&date](const CalendarEvent& event)
            {
                return normalizedCalendarEventType(event.eventType)
                    == QStringLiteral("Vacation")
                    && event.startDate.isValid()
                    && event.endDate.isValid()
                    && event.startDate <= date
                    && event.endDate >= date;
            }
            );

        if (vacation)
        {
            days.append(Weekdays.at(offset));
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
    : QDialog(parent)
    , m_services(services)
    , m_schedule(schedule)
    , m_initialCalendarEvents(calendarEvents)
{
    if (m_services && m_services->dataService())
    {
        m_storedUserName =
            m_services->dataService()
                ->loadSetting(
                    QStringLiteral("myInfo/name"),
                    QString()
                    )
                .toString()
                .trimmed();
    }

    buildUi();
    initializeDays(calendarEvents, referenceDate);
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

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setSpacing(12);

    auto* daysGroup = new QGroupBox(tr("Days to Include"), this);
    auto* daysLayout = new QGridLayout(daysGroup);
    daysLayout->setObjectName(QStringLiteral("subPrepDaysLayout"));

    for (int index = 0; index < Weekdays.size(); ++index)
    {
        const QString& day = Weekdays.at(index);
        auto* checkBox = new QCheckBox(displayDay(day), daysGroup);
        checkBox->setObjectName(checkBoxObjectName(day));
        checkBox->setProperty("day", day);
        daysLayout->addWidget(checkBox, index / 3, index % 3);
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

    auto* targetFolderLabel =
        new QLabel(tr("Target Folder:"), m_folderOptions);
    targetFolderLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    folderLayout->addWidget(targetFolderLabel, 0, 0, Qt::AlignVCenter);
    m_targetRootEdit = new QLineEdit(m_folderOptions);
    m_targetRootEdit->setObjectName(QStringLiteral("subPrepTargetFolderEdit"));
    m_targetRootEdit->setText(SubPrepPackageService::defaultTargetRoot());
    folderLayout->addWidget(m_targetRootEdit, 0, 1, Qt::AlignVCenter);
    auto* selectFolderButton =
        new QPushButton(tr("Select Folder"), m_folderOptions);
    selectFolderButton->setObjectName(
        QStringLiteral("subPrepSelectFolderButton")
        );
    folderLayout->addWidget(selectFolderButton, 0, 2, Qt::AlignVCenter);

    m_nameLabel = new QLabel(tr("Your Name"), m_folderOptions);
    m_nameEdit = new QLineEdit(m_folderOptions);
    m_nameEdit->setObjectName(QStringLiteral("subPrepUserNameEdit"));
    m_nameEdit->setVisible(m_storedUserName.isEmpty());
    m_nameLabel->setVisible(m_storedUserName.isEmpty());
    folderLayout->addWidget(m_nameLabel, 1, 0, Qt::AlignTop);
    folderLayout->addWidget(m_nameEdit, 1, 1, 1, 2, Qt::AlignTop);

    folderLayout->addWidget(
        new QLabel(tr("Output Folder:"), m_folderOptions),
        2,
        0,
        Qt::AlignTop
        );
    m_outputPreviewLabel = new QLabel(m_folderOptions);
    m_outputPreviewLabel->setObjectName(
        QStringLiteral("subPrepOutputFolderPreview")
        );
    m_outputPreviewLabel->setWordWrap(true);
    m_outputPreviewLabel->setTextInteractionFlags(
        Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard
        );
    folderLayout->addWidget(
        m_outputPreviewLabel,
        2,
        1,
        1,
        2,
        Qt::AlignTop
        );

    m_openFolderCheck =
        new QCheckBox(tr("Open Folder After Generation"), m_folderOptions);
    m_openFolderCheck->setObjectName(
        QStringLiteral("subPrepOpenFolderCheckBox")
        );
    m_openFolderCheck->setChecked(true);
    folderLayout->addWidget(
        m_openFolderCheck,
        3,
        1,
        1,
        2,
        Qt::AlignTop
        );
    folderLayout->setRowStretch(4, 1);
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

    auto* buttonLayout = new QHBoxLayout;
    auto* cancelButton = new QPushButton(tr("Cancel"), this);
    cancelButton->setObjectName(QStringLiteral("subPrepPrintCancelButton"));
    m_okButton = new QPushButton(tr("OK"), this);
    m_okButton->setObjectName(QStringLiteral("subPrepGenerateOkButton"));
    m_okButton->setDefault(true);
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(m_okButton);
    rootLayout->addLayout(buttonLayout);

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
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(
        m_okButton,
        &QPushButton::clicked,
        this,
        &SubPrepPrintDialog::acceptGeneration
        );
}

void SubPrepPrintDialog::initializeDays(
    const QList<CalendarEvent>& calendarEvents,
    const QDate& referenceDate
    )
{
    m_weekStart = defaultWeekStart(calendarEvents, referenceDate);
    const QStringList vacationDays = vacationDaysForWeek(m_weekStart);

    for (int index = 0; index < m_dayChecks.size(); ++index)
    {
        QCheckBox* checkBox = m_dayChecks.at(index);
        const QString day = Weekdays.at(index);
        const QSignalBlocker blocker(checkBox);
        checkBox->setChecked(vacationDays.contains(day));
    }
}

QStringList SubPrepPrintDialog::vacationDaysForWeek(
    const QDate& weekStart
    ) const
{
    const QStringList initialDays =
        vacationDaysInWeek(m_initialCalendarEvents, weekStart);

    if (!initialDays.isEmpty())
    {
        return initialDays;
    }

    if (
        m_services
        && m_services->dataService()
        && m_services->dataService()->isOpen()
        )
    {
        return vacationDaysInWeek(
            m_services->dataService()->loadCalendarEventsInRange(
                weekStart,
                weekStart.addDays(4)
                ),
            weekStart
            );
    }

    return initialDays;
}

void SubPrepPrintDialog::updateFolderControls()
{
    if (m_folderOptions)
    {
        m_folderOptions->setEnabled(createFolder());
    }
    updateOutputPreview();
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
        m_outputPreviewLabel->setText(
            tr("Select days and enter your name to preview the output folder.")
            );
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
        error = tr("Choose Create Sub Prep Folder, Print Paper Copies, or both.");
    }
    else if (selectedDates().isEmpty())
    {
        error = tr("Select at least one day to include.");
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

    const QString selected =
        QFileDialog::getExistingDirectory(
            this,
            tr("Select Folder"),
            startPath
            );

    if (!selected.isEmpty())
    {
        m_targetRootEdit->setText(QDir::cleanPath(selected));
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
        const QMessageBox::StandardButton answer =
            QMessageBox::question(
                this,
                tr("Replace Existing Sub Prep Folder"),
                tr("Replace the existing folder and all of its contents?\n\n%1")
                    .arg(directory),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No
                );

        if (answer != QMessageBox::Yes)
        {
            return;
        }

        m_replaceExisting = true;
    }

    if (
        createFolder()
        && m_storedUserName.isEmpty()
        && m_services
        && m_services->dataService()
        )
    {
        m_services->dataService()->saveSetting(
            QStringLiteral("myInfo/name"),
            m_nameEdit->text().trimmed()
            );
    }

    accept();
}

QList<QDate> SubPrepPrintDialog::selectedDates() const
{
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
    if (!referenceDate.isValid())
    {
        return {};
    }

    const QDate currentWeek = weekStartFor(referenceDate);
    const QStringList currentDays =
        vacationDaysInWeek(calendarEvents, currentWeek);

    if (!currentDays.isEmpty())
    {
        return currentDays;
    }

    return vacationDaysInWeek(calendarEvents, currentWeek.addDays(7));
}

QDate SubPrepPrintDialog::defaultWeekStart(
    const QList<CalendarEvent>& calendarEvents,
    const QDate& referenceDate
    )
{
    if (!referenceDate.isValid())
    {
        return {};
    }

    const QDate currentWeek = weekStartFor(referenceDate);
    if (!vacationDaysInWeek(calendarEvents, currentWeek).isEmpty())
    {
        return currentWeek;
    }
    if (!vacationDaysInWeek(calendarEvents, currentWeek.addDays(7)).isEmpty())
    {
        return currentWeek.addDays(7);
    }
    return currentWeek;
}
