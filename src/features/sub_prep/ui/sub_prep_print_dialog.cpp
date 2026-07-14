#include "features/sub_prep/ui/sub_prep_print_dialog.h"

#include <algorithm>

#include <QCheckBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
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

QString checkBoxObjectName(
    const QString& day
    )
{
    return QStringLiteral("subPrepPrint%1CheckBox")
        .arg(day);
}

QString displayDay(
    const QString& day,
    const SubPrepPrintDialog* dialog
    )
{
    if (day == QStringLiteral("Monday"))
    {
        return dialog->tr("Monday");
    }
    if (day == QStringLiteral("Tuesday"))
    {
        return dialog->tr("Tuesday");
    }
    if (day == QStringLiteral("Wednesday"))
    {
        return dialog->tr("Wednesday");
    }
    if (day == QStringLiteral("Thursday"))
    {
        return dialog->tr("Thursday");
    }

    return dialog->tr("Friday");
}

QDate weekStartFor(
    const QDate& date
    )
{
    return date.addDays(
        Qt::Monday - date.dayOfWeek()
        );
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
    const QList<CalendarEvent>& calendarEvents,
    const QDate& referenceDate,
    QWidget* parent
    )
    : QDialog(parent)
{
    setModal(true);
    setWindowTitle(tr("Print Sub Prep"));

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    auto* daysGroup =
        new QGroupBox(
            tr("Days to Include"),
            this
            );
    auto* daysLayout = new QVBoxLayout(daysGroup);
    const QStringList defaultDays =
        defaultSelectedDays(
            calendarEvents,
            referenceDate
            );

    for (const QString& day : Weekdays)
    {
        auto* checkBox =
            new QCheckBox(
                displayDay(day, this),
                daysGroup
                );
        checkBox->setObjectName(checkBoxObjectName(day));
        checkBox->setProperty("day", day);
        checkBox->setChecked(defaultDays.contains(day));
        daysLayout->addWidget(checkBox);
        m_dayChecks.append(checkBox);
    }

    auto* buttonLayout = new QHBoxLayout;
    auto* cancelButton =
        new QPushButton(
            tr("Cancel"),
            this
            );
    cancelButton->setObjectName(QStringLiteral("subPrepPrintCancelButton"));
    auto* saveAsButton =
        new QPushButton(
            tr("Save As"),
            this
            );
    saveAsButton->setObjectName(QStringLiteral("subPrepPrintSaveAsButton"));
    auto* printButton =
        new QPushButton(
            tr("Print"),
            this
            );
    printButton->setObjectName(QStringLiteral("subPrepPrintButton"));
    printButton->setDefault(true);

    buttonLayout->addWidget(cancelButton);
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(saveAsButton);
    buttonLayout->addWidget(printButton);

    layout->addWidget(daysGroup);
    layout->addLayout(buttonLayout);

    connect(
        cancelButton,
        &QPushButton::clicked,
        this,
        &QDialog::reject
        );
    connect(
        saveAsButton,
        &QPushButton::clicked,
        this,
        &SubPrepPrintDialog::chooseSavePath
        );
    connect(
        printButton,
        &QPushButton::clicked,
        this,
        &SubPrepPrintDialog::acceptPrint
        );
}

SubPrepPrintDialog::Action SubPrepPrintDialog::selectedAction() const
{
    return m_selectedAction;
}

QString SubPrepPrintDialog::selectedSavePath() const
{
    return m_selectedSavePath;
}

QStringList SubPrepPrintDialog::selectedDays() const
{
    QStringList days;

    for (const QCheckBox* checkBox : m_dayChecks)
    {
        if (checkBox && checkBox->isChecked())
        {
            days.append(checkBox->property("day").toString());
        }
    }

    return days;
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

    const QDate currentWeekStart =
        weekStartFor(referenceDate);
    const QStringList currentWeekDays =
        vacationDaysInWeek(
            calendarEvents,
            currentWeekStart
            );

    if (!currentWeekDays.isEmpty())
    {
        return currentWeekDays;
    }

    return vacationDaysInWeek(
        calendarEvents,
        currentWeekStart.addDays(7)
        );
}

void SubPrepPrintDialog::acceptPrint()
{
    m_selectedAction = Action::Print;
    m_selectedSavePath.clear();
    accept();
}

void SubPrepPrintDialog::chooseSavePath()
{
    QFileDialog dialog(
        this,
        tr("Save Sub Prep As"),
        QString(),
        tr("PDF Documents (*.pdf)")
        );
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setDefaultSuffix(QStringLiteral("pdf"));
    dialog.selectFile(QStringLiteral("Sub Prep.pdf"));

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const QStringList selectedFiles = dialog.selectedFiles();

    if (selectedFiles.isEmpty())
    {
        return;
    }

    QString savePath = selectedFiles.first();

    if (QFileInfo(savePath).suffix().isEmpty())
    {
        savePath += QStringLiteral(".pdf");
    }

    m_selectedAction = Action::SaveAs;
    m_selectedSavePath = savePath;
    accept();
}
