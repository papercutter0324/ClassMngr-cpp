#include "academic_calendar_dialog.h"

#include "academic_calendar_provider.h"
#include "core/fontmanager.h"
#include "features/my_info/calendar_event_import_service.h"

#include <QCheckBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

namespace
{
constexpr int DateFieldWidth = 138;
constexpr int WeekFieldWidth = 86;
constexpr int SchoolColumnSpacing = 32;
constexpr int MaximumTermWeeks = 53;
const QDate EmptyDate(1900, 1, 1);

SchoolLevel schoolLevel(int index)
{
    return index == 0
        ? SchoolLevel::Elementary
        : SchoolLevel::Middle;
}

AcademicTerm academicTerm(int index)
{
    return static_cast<AcademicTerm>(index);
}
}

AcademicCalendarDialog::AcademicCalendarDialog(
    AcademicCalendarProvider* provider,
    DataService* dataService,
    int termYear,
    QWidget* parent
    )
    : QDialog(parent)
    , m_provider(provider)
    , m_importService(
        new CalendarEventImportService(
            dataService,
            this
            )
        )
    , m_termYear(
        qMax(termYear, AcademicCalendarSchedule::FirstTermYear)
        )
{
    buildUi();
    loadSchedules();
}

void AcademicCalendarDialog::accept()
{
    if (!m_provider)
    {
        reject();
        return;
    }

    for (const AcademicYearSchedule& schedule : m_schedules)
    {
        if (!schedule.isValid())
        {
            QMessageBox::warning(
                this,
                tr("Invalid Academic Schedule"),
                tr("Every term must start on a Monday and last between 1 and 53 weeks.")
                );
            return;
        }
    }

    if (m_termYear > AcademicCalendarSchedule::FirstTermYear)
    {
        for (int school = 0; school < SchoolCount; ++school)
        {
            const AcademicYearSchedule previous =
                m_provider->schedule().yearSchedule(
                    schoolLevel(school),
                    m_termYear - 1
                    );
            const int days =
                previous
                    .termStart(AcademicTerm::Fall)
                    .daysTo(m_schedules[school].winterStart);

            if (
                days < 7
                || days % 7 != 0
                || days / 7 > MaximumTermWeeks
                )
            {
                QMessageBox::warning(
                    this,
                    tr("Invalid Winter Start"),
                    tr("Winter must begin 1 to 53 complete weeks after the preceding Fall term starts.")
                    );
                return;
            }
        }
    }

    if (m_provider->hasCustomYearAfter(m_termYear))
    {
        const auto answer =
            QMessageBox::warning(
                this,
                tr("Recalculate Later Years?"),
                tr("Saving term year %1 will replace later custom schedules with predictions based on the default term durations.")
                    .arg(m_termYear),
                QMessageBox::Save | QMessageBox::Cancel,
                QMessageBox::Cancel
                );

        if (answer != QMessageBox::Save)
        {
            return;
        }
    }

    m_provider->saveYearSchedules(
        m_termYear,
        m_schedules[0],
        m_schedules[1]
        );
    QDialog::accept();
}

void AcademicCalendarDialog::restoreDefaults()
{
    if (!m_provider)
    {
        return;
    }

    for (int school = 0; school < SchoolCount; ++school)
    {
        m_schedules[school] =
            m_provider->schedule().defaultYearSchedule(
                schoolLevel(school),
                m_termYear
                );
    }

    m_linkCheck->setChecked(true);
    synchronizeLinkedTerms();
    m_dirty = true;
    refreshFields();
}

void AcademicCalendarDialog::linkWinterSpring(bool linked)
{
    if (m_refreshing)
    {
        return;
    }

    if (linked)
    {
        synchronizeLinkedTerms();
        m_dirty = true;
        refreshFields();
    }

    updateLinkedFieldAvailability();
}

void AcademicCalendarDialog::importCalendarEvents()
{
    if (!m_importService || m_importService->isImporting())
    {
        return;
    }

    m_importButton->setEnabled(false);
    m_importStatusLabel->setText(
        tr("Importing events...")
        );
    m_importService->importFromDefaultSource();
}

void AcademicCalendarDialog::handleImportFinished(
    int importedCount,
    int skippedCount
    )
{
    if (m_importButton)
    {
        m_importButton->setEnabled(true);
    }

    if (m_importStatusLabel)
    {
        m_importStatusLabel->setText(
            tr("Imported %1 event(s). Skipped %2 existing or ignored item(s).")
                .arg(importedCount)
                .arg(skippedCount)
            );
    }

    if (importedCount > 0)
    {
        emit calendarEventsImported();
    }
}

void AcademicCalendarDialog::handleImportFailed(
    const QString& message
    )
{
    if (m_importButton)
    {
        m_importButton->setEnabled(true);
    }

    if (m_importStatusLabel)
    {
        m_importStatusLabel->setText(
            tr("Import failed: %1").arg(message)
            );
    }
}

void AcademicCalendarDialog::buildUi()
{
    setWindowTitle(tr("Academic Calendar Settings"));
    setMinimumWidth(820);

    auto* mainLayout =
        new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(14);

    auto* title =
        new QLabel(
            tr("Academic Term Schedule — %1").arg(m_termYear),
            this
            );
    title->setObjectName(QStringLiteral("pageTitle"));
    title->setAlignment(Qt::AlignCenter);
    title->setFont(
        FontManager::getUiFont(16, QFont::DemiBold)
        );
    mainLayout->addWidget(title);

    auto* fields =
        new QGridLayout;
    fields->setHorizontalSpacing(10);
    fields->setVerticalSpacing(8);

    auto* elementaryHeader =
        new QLabel(tr("Elementary"), this);
    elementaryHeader->setAlignment(Qt::AlignCenter);
    elementaryHeader->setFont(
        FontManager::getUiFont(11, QFont::DemiBold)
        );
    fields->addWidget(elementaryHeader, 0, 1, 1, 2);

    auto* middleHeader =
        new QLabel(tr("Middle School"), this);
    middleHeader->setAlignment(Qt::AlignCenter);
    middleHeader->setFont(
        FontManager::getUiFont(11, QFont::DemiBold)
        );
    fields->addWidget(middleHeader, 0, 4, 1, 2);

    auto addCenteredHeader =
        [this, fields](const QString& text, int column)
        {
            auto* label = new QLabel(text, this);
            label->setAlignment(Qt::AlignCenter);
            fields->addWidget(label, 1, column);
        };

    addCenteredHeader(tr("Term"), 0);
    addCenteredHeader(tr("Start"), 1);
    addCenteredHeader(tr("Weeks"), 2);
    addCenteredHeader(tr("Start"), 4);
    addCenteredHeader(tr("Weeks"), 5);
    fields->setColumnMinimumWidth(3, SchoolColumnSpacing);

    for (int term = 0; term < AcademicTermCount; ++term)
    {
        auto* termLabel =
            new QLabel(termName(term), this);
        termLabel->setAlignment(Qt::AlignCenter);
        fields->addWidget(termLabel, term + 2, 0);

        for (int school = 0; school < SchoolCount; ++school)
        {
            auto* dateEdit =
                new QDateEdit(this);
            dateEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
            dateEdit->setCalendarPopup(true);
            dateEdit->setMinimumDate(EmptyDate);
            dateEdit->setMaximumDate(QDate(2200, 12, 31));
            dateEdit->setSpecialValueText(QString());
            dateEdit->setFixedWidth(DateFieldWidth);
            m_dateEdits[school][term] = dateEdit;

            auto* weekEdit =
                new QSpinBox(this);
            weekEdit->setRange(0, MaximumTermWeeks);
            weekEdit->setSpecialValueText(QString());
            weekEdit->setFixedWidth(WeekFieldWidth);
            weekEdit->setAlignment(Qt::AlignCenter);
            m_weekEdits[school][term] = weekEdit;

            const int startColumn = school == 0 ? 1 : 4;
            fields->addWidget(dateEdit, term + 2, startColumn);
            fields->addWidget(weekEdit, term + 2, startColumn + 1);

            connect(
                dateEdit,
                &QDateEdit::editingFinished,
                this,
                [this, school, term]()
                {
                    commitDate(school, term);
                }
                );
            connect(
                weekEdit,
                &QSpinBox::editingFinished,
                this,
                [this, school, term]()
                {
                    commitWeeks(school, term);
                }
                );
        }
    }

    auto* centeredFields =
        new QHBoxLayout;
    centeredFields->addStretch(1);
    centeredFields->addLayout(fields);
    centeredFields->addStretch(1);
    mainLayout->addLayout(centeredFields);

    m_linkCheck =
        new QCheckBox(
            tr("Elementary and Middle School follow the same Winter and Spring term schedules."),
            this
            );
    mainLayout->addWidget(
        m_linkCheck,
        0,
        Qt::AlignHCenter
        );

    auto* separator =
        new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(separator);

    auto* importHeader =
        new QLabel(
            tr("Import Events"),
            this
            );
    importHeader->setAlignment(Qt::AlignCenter);
    importHeader->setFont(
        FontManager::getUiFont(12, QFont::DemiBold)
        );
    mainLayout->addWidget(importHeader);

    auto* importLayout =
        new QHBoxLayout;
    importLayout->setSpacing(8);

    m_importUrlEdit =
        new QLineEdit(
            CalendarEventImportService::defaultImportUrl(),
            this
            );
    m_importUrlEdit->setReadOnly(true);
    m_importUrlEdit->setMinimumWidth(420);

    m_importButton =
        new QPushButton(
            tr("Import Events"),
            this
            );

    importLayout->addWidget(m_importUrlEdit, 1);
    importLayout->addWidget(m_importButton);
    mainLayout->addLayout(importLayout);

    m_importStatusLabel =
        new QLabel(this);
    m_importStatusLabel->setObjectName(QStringLiteral("sectionSubtitle"));
    m_importStatusLabel->setWordWrap(true);
    mainLayout->addWidget(m_importStatusLabel);

    m_buttons =
        new QDialogButtonBox(
            QDialogButtonBox::Save | QDialogButtonBox::Cancel,
            this
            );
    m_restoreButton =
        m_buttons->addButton(
            tr("Restore Defaults"),
            QDialogButtonBox::ResetRole
            );

    connect(
        m_linkCheck,
        &QCheckBox::toggled,
        this,
        &AcademicCalendarDialog::linkWinterSpring
        );
    connect(
        m_importButton,
        &QPushButton::clicked,
        this,
        &AcademicCalendarDialog::importCalendarEvents
        );
    connect(
        m_importService,
        &CalendarEventImportService::importFinished,
        this,
        &AcademicCalendarDialog::handleImportFinished
        );
    connect(
        m_importService,
        &CalendarEventImportService::importFailed,
        this,
        &AcademicCalendarDialog::handleImportFailed
        );
    connect(
        m_restoreButton,
        &QPushButton::clicked,
        this,
        &AcademicCalendarDialog::restoreDefaults
        );
    connect(
        m_buttons,
        &QDialogButtonBox::accepted,
        this,
        &AcademicCalendarDialog::accept
        );
    connect(
        m_buttons,
        &QDialogButtonBox::rejected,
        this,
        &AcademicCalendarDialog::reject
        );

    mainLayout->addWidget(m_buttons);
}

void AcademicCalendarDialog::loadSchedules()
{
    if (!m_provider)
    {
        return;
    }

    m_schedules[0] =
        m_provider->schedule().yearSchedule(
            SchoolLevel::Elementary,
            m_termYear
            );
    m_schedules[1] =
        m_provider->schedule().yearSchedule(
            SchoolLevel::Middle,
            m_termYear
            );

    const bool matching =
        m_schedules[0].winterStart == m_schedules[1].winterStart
        && m_schedules[0].weeks[0] == m_schedules[1].weeks[0]
        && m_schedules[0].weeks[1] == m_schedules[1].weeks[1];

    m_refreshing = true;
    m_linkCheck->setChecked(matching);
    m_refreshing = false;
    m_dirty = false;
    refreshFields();
}

void AcademicCalendarDialog::refreshFields()
{
    m_refreshing = true;

    for (int school = 0; school < SchoolCount; ++school)
    {
        for (int term = 0; term < AcademicTermCount; ++term)
        {
            const QSignalBlocker dateBlocker(m_dateEdits[school][term]);
            const QSignalBlocker weekBlocker(m_weekEdits[school][term]);

            m_dateEdits[school][term]->setDate(
                m_schedules[school].termStart(academicTerm(term))
                );
            m_weekEdits[school][term]->setValue(
                m_schedules[school].weeks[term]
                );
        }
    }

    m_refreshing = false;
    updateLinkedFieldAvailability();
}

void AcademicCalendarDialog::commitDate(
    int schoolIndex,
    int termIndex
    )
{
    if (m_refreshing)
    {
        return;
    }

    const QDate edited =
        m_dateEdits[schoolIndex][termIndex]->date();

    if (edited == EmptyDate)
    {
        clearDate(schoolIndex, termIndex);
        return;
    }

    if (edited.dayOfWeek() != Qt::Monday)
    {
        QMessageBox::warning(
            this,
            tr("Monday Required"),
            tr("Academic terms must start on a Monday.")
            );
        refreshFields();
        return;
    }

    AcademicYearSchedule& schedule =
        m_schedules[schoolIndex];

    if (termIndex == 0)
    {
        schedule.winterStart = edited;
    }
    else
    {
        const QDate previousStart =
            schedule.termStart(academicTerm(termIndex - 1));
        const int days = previousStart.daysTo(edited);
        const int weeks = days / 7;

        if (
            days <= 0
            || days % 7 != 0
            || weeks > MaximumTermWeeks
            )
        {
            QMessageBox::warning(
                this,
                tr("Invalid Term Start"),
                tr("The start must be 1 to 53 complete weeks after the preceding term starts.")
                );
            refreshFields();
            return;
        }

        schedule.weeks[termIndex - 1] = weeks;
    }

    if (m_linkCheck->isChecked() && schoolIndex == 0 && termIndex <= 2)
    {
        synchronizeLinkedTerms();
    }

    m_dirty = true;
    refreshFields();
}

void AcademicCalendarDialog::clearDate(
    int schoolIndex,
    int termIndex
    )
{
    if (m_refreshing || !m_provider)
    {
        return;
    }

    AcademicYearSchedule& schedule =
        m_schedules[schoolIndex];

    if (termIndex == 0)
    {
        schedule.winterStart =
            m_provider->schedule()
                .defaultYearSchedule(
                    schoolLevel(schoolIndex),
                    m_termYear
                    )
                .winterStart;
    }

    if (m_linkCheck->isChecked() && schoolIndex == 0 && termIndex <= 2)
    {
        synchronizeLinkedTerms();
    }

    m_dirty = true;
    refreshFields();
}

void AcademicCalendarDialog::commitWeeks(
    int schoolIndex,
    int termIndex
    )
{
    if (m_refreshing)
    {
        return;
    }

    const int value =
        m_weekEdits[schoolIndex][termIndex]->value();

    if (value == 0)
    {
        refreshFields();
        return;
    }

    m_schedules[schoolIndex].weeks[termIndex] = value;

    if (
        m_linkCheck->isChecked()
        && schoolIndex == 0
        && termIndex <= 1
        )
    {
        synchronizeLinkedTerms();
    }

    m_dirty = true;
    refreshFields();
}

void AcademicCalendarDialog::synchronizeLinkedTerms()
{
    m_schedules[1].winterStart =
        m_schedules[0].winterStart;
    m_schedules[1].weeks[0] =
        m_schedules[0].weeks[0];
    m_schedules[1].weeks[1] =
        m_schedules[0].weeks[1];
}

void AcademicCalendarDialog::updateLinkedFieldAvailability()
{
    const bool linked =
        m_linkCheck && m_linkCheck->isChecked();

    for (int term = 0; term <= 1; ++term)
    {
        m_weekEdits[1][term]->setEnabled(!linked);
    }

    for (int term = 0; term <= 2; ++term)
    {
        m_dateEdits[1][term]->setEnabled(!linked);
    }
}

QString AcademicCalendarDialog::termName(int termIndex) const
{
    switch (academicTerm(termIndex))
    {
    case AcademicTerm::Winter:
        return tr("Winter");

    case AcademicTerm::Spring:
        return tr("Spring");

    case AcademicTerm::Summer:
        return tr("Summer");

    case AcademicTerm::Fall:
        return tr("Fall");
    }

    return {};
}
