#include "upcoming_birthdays_dialog.h"

#include "ui/shared/widgets/text_fit_dialog_button_box.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QFrame>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QScrollArea>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
constexpr int DialogWidth = 520;
constexpr int DialogHeight = 560;
}

UpcomingBirthdaysDialog::UpcomingBirthdaysDialog(
    const UpcomingBirthdaySchedule& schedule,
    QWidget* parent
    )
    : DialogShell(QStringLiteral("upcomingBirthdays"), parent)
    , m_schedule(schedule)
{
    setMinimumWidth(DialogWidth);
    resize(DialogWidth, DialogHeight);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setObjectName(QStringLiteral("upcomingBirthdaysScrollArea"));
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* scrollContent = new QWidget(scrollArea);
    scrollContent->setObjectName(QStringLiteral("upcomingBirthdaysContent"));
    auto* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(4, 4, 4, 4);
    scrollLayout->setSpacing(14);

    m_todaySection = createSection(
        QStringLiteral("upcomingBirthdaysToday"),
        m_schedule.today,
        true
        );
    m_todaySection->setVisible(!m_schedule.today.isEmpty());
    scrollLayout->addWidget(m_todaySection);

    scrollLayout->addWidget(createSection(
        QStringLiteral("upcomingBirthdaysThisWeek"),
        m_schedule.thisWeek,
        false
        ));
    scrollLayout->addWidget(createSection(
        QStringLiteral("upcomingBirthdaysNextWeek"),
        m_schedule.nextWeek,
        false
        ));
    scrollLayout->addStretch();

    scrollArea->setWidget(scrollContent);
    contentLayout()->addWidget(scrollArea, 1);

    m_dismissForToday = new QCheckBox(this);
    m_dismissForToday->setObjectName(
        QStringLiteral("upcomingBirthdaysDismissForTodayCheck")
        );
    contentLayout()->addWidget(m_dismissForToday);

    auto* buttons = addButtonBox(QDialogButtonBox::Close);
    if (auto* closeButton = buttons->button(QDialogButtonBox::Close))
    {
        closeButton->setObjectName(QStringLiteral("upcomingBirthdaysCloseButton"));
    }

    updateText();
}

bool UpcomingBirthdaysDialog::dismissForToday() const
{
    return m_dismissForToday && m_dismissForToday->isChecked();
}

void UpcomingBirthdaysDialog::retranslateDialog()
{
    updateText();
}

QWidget* UpcomingBirthdaysDialog::createSection(
    const QString& objectPrefix,
    const QList<UpcomingBirthday>& birthdays,
    bool centered
    )
{
    auto* section = new QWidget(this);
    section->setObjectName(objectPrefix + QStringLiteral("Section"));

    auto* layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* title = new QLabel(section);
    title->setObjectName(objectPrefix + QStringLiteral("Title"));
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setAlignment(centered ? Qt::AlignCenter : Qt::AlignLeft);
    layout->addWidget(title);

    if (objectPrefix == QStringLiteral("upcomingBirthdaysToday"))
    {
        m_todayTitle = title;
    }
    else if (objectPrefix == QStringLiteral("upcomingBirthdaysThisWeek"))
    {
        m_thisWeekTitle = title;
    }
    else
    {
        m_nextWeekTitle = title;
    }

    if (birthdays.isEmpty())
    {
        auto* empty = new QLabel(section);
        empty->setObjectName(objectPrefix + QStringLiteral("Empty"));
        empty->setWordWrap(true);
        empty->setAlignment(centered ? Qt::AlignCenter : Qt::AlignLeft);
        layout->addWidget(empty);

        if (objectPrefix == QStringLiteral("upcomingBirthdaysThisWeek"))
        {
            m_thisWeekEmpty = empty;
        }
        else if (objectPrefix == QStringLiteral("upcomingBirthdaysNextWeek"))
        {
            m_nextWeekEmpty = empty;
        }

        return section;
    }

    for (int index = 0; index < birthdays.size(); ++index)
    {
        layout->addWidget(createEntry(
            objectPrefix,
            index,
            birthdays.at(index),
            centered
            ));
    }

    return section;
}

QWidget* UpcomingBirthdaysDialog::createEntry(
    const QString& objectPrefix,
    int index,
    const UpcomingBirthday& birthday,
    bool centered
    )
{
    auto* entry = new QWidget(this);
    entry->setObjectName(
        QStringLiteral("%1Entry%2").arg(objectPrefix).arg(index)
        );

    auto* layout = new QVBoxLayout(entry);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(2);

    auto* name = new QLabel(entry);
    name->setObjectName(entry->objectName() + QStringLiteral("Name"));
    QFont nameFont = name->font();
    nameFont.setBold(true);
    if (centered)
    {
        nameFont.setPointSize(nameFont.pointSize() + 6);
    }
    name->setFont(nameFont);
    name->setAlignment(centered ? Qt::AlignCenter : Qt::AlignLeft);
    name->setWordWrap(true);
    layout->addWidget(name);

    auto* detail = new QLabel(entry);
    detail->setObjectName(entry->objectName() + QStringLiteral("Detail"));
    detail->setAlignment(centered ? Qt::AlignCenter : Qt::AlignLeft);
    detail->setWordWrap(true);
    layout->addWidget(detail);

    m_entries.append({birthday, name, detail});
    return entry;
}

QString UpcomingBirthdaysDialog::detailText(
    const UpcomingBirthday& birthday
    ) const
{
    QStringList details{
        QLocale().toString(birthday.date, QLocale::LongFormat),
        groupText(birthday.group)
    };

    if (!birthday.position.isEmpty())
    {
        details.append(birthday.position);
    }

    return details.join(QStringLiteral(" • "));
}

QString UpcomingBirthdaysDialog::groupText(
    UpcomingBirthdayGroup group
    ) const
{
    switch (group)
    {
    case UpcomingBirthdayGroup::KoreanTeacher:
        return tr("Korean Teacher");
    case UpcomingBirthdayGroup::NativeEnglishTeacher:
        return tr("Native English Teacher");
    case UpcomingBirthdayGroup::GsTeam:
        return tr("GS Team");
    }

    return {};
}

void UpcomingBirthdaysDialog::updateText()
{
    setWindowTitle(tr("Upcoming Birthdays"));
    setHeader(
        tr("Upcoming Birthdays"),
        tr("Birthdays today and over the next two weeks.")
        );

    if (m_todayTitle)
    {
        m_todayTitle->setText(tr("Today's Birthdays"));
    }
    if (m_thisWeekTitle)
    {
        m_thisWeekTitle->setText(tr("This Week"));
    }
    if (m_nextWeekTitle)
    {
        m_nextWeekTitle->setText(tr("Next Week"));
    }
    if (m_thisWeekEmpty)
    {
        m_thisWeekEmpty->setText(tr("No birthdays this week."));
    }
    if (m_nextWeekEmpty)
    {
        m_nextWeekEmpty->setText(tr("No birthdays next week."));
    }
    if (m_dismissForToday)
    {
        m_dismissForToday->setText(tr("Don't show again today"));
    }

    for (const EntryPresentation& entry : m_entries)
    {
        if (entry.name)
        {
            entry.name->setText(entry.birthday.displayName);
            entry.name->setAccessibleName(entry.birthday.displayName);
        }
        if (entry.detail)
        {
            const QString detail = detailText(entry.birthday);
            entry.detail->setText(detail);
            entry.detail->setAccessibleName(detail);
        }
    }

    const auto buttons = findChildren<QDialogButtonBox*>();
    for (QDialogButtonBox* buttons : buttons)
    {
        if (auto* closeButton = buttons->button(QDialogButtonBox::Close))
        {
            closeButton->setText(tr("Close"));
        }
    }
}
