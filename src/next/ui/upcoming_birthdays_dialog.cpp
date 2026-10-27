#include "next/ui/upcoming_birthdays_dialog.h"

#include "ui/shared/widgets/text_fit_dialog_button_box.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDate>
#include <QDialogButtonBox>
#include <QEvent>
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

namespace ClassMngr::Next::Ui
{
namespace
{

constexpr int DialogWidth = 520;
constexpr int DialogHeight = 560;

QString translate(const char* source)
{
    return QCoreApplication::translate("NextUpcomingBirthdaysDialog", source);
}

QString fromUtf16(const std::u16string& value)
{
    return QString::fromUtf16(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

QDate asQDate(const Domain::GregorianDate& value)
{
    return QDate(value.year(), value.month(), value.day());
}

} // namespace

UpcomingBirthdaysDialog::UpcomingBirthdaysDialog(
    const Application::UpcomingBirthdaySchedule& schedule,
    QWidget* parent
    )
    : QDialog(parent)
    , m_schedule(schedule)
{
    setObjectName(QStringLiteral("nextUpcomingBirthdaysDialog"));
    setModal(true);
    setSizeGripEnabled(true);
    setMinimumSize(DialogWidth, DialogHeight);
    resize(DialogWidth, DialogHeight);

    m_dialogLayout = new QVBoxLayout(this);
    m_dialogLayout->setContentsMargins(18, 18, 18, 18);
    m_dialogLayout->setSpacing(10);

    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("nextUpcomingBirthdaysHeader"));
    auto* headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 4);
    headerLayout->setSpacing(4);
    m_headerTitle = new QLabel(header);
    m_headerTitle->setObjectName(QStringLiteral("nextUpcomingBirthdaysHeaderTitle"));
    QFont headerFont = m_headerTitle->font();
    headerFont.setBold(true);
    m_headerTitle->setFont(headerFont);
    m_headerTitle->setWordWrap(true);
    m_headerSubtitle = new QLabel(header);
    m_headerSubtitle->setObjectName(QStringLiteral("nextUpcomingBirthdaysHeaderSubtitle"));
    m_headerSubtitle->setWordWrap(true);
    headerLayout->addWidget(m_headerTitle);
    headerLayout->addWidget(m_headerSubtitle);
    m_dialogLayout->addWidget(header);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setObjectName(QStringLiteral("nextUpcomingBirthdaysScrollArea"));
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* scrollContent = new QWidget(scrollArea);
    scrollContent->setObjectName(QStringLiteral("nextUpcomingBirthdaysContent"));
    auto* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(4, 4, 4, 4);
    scrollLayout->setSpacing(14);

    m_todaySection = createSection(
        QStringLiteral("nextUpcomingBirthdaysToday"),
        m_schedule.today,
        true
        );
    m_todaySection->setVisible(!m_schedule.today.empty());
    scrollLayout->addWidget(m_todaySection);
    scrollLayout->addWidget(createSection(
        QStringLiteral("nextUpcomingBirthdaysThisWeek"),
        m_schedule.thisWeek,
        false
        ));
    scrollLayout->addWidget(createSection(
        QStringLiteral("nextUpcomingBirthdaysNextWeek"),
        m_schedule.nextWeek,
        false
        ));
    scrollLayout->addStretch();
    scrollArea->setWidget(scrollContent);
    m_dialogLayout->addWidget(scrollArea, 1);

    m_dismissForToday = new QCheckBox(this);
    m_dismissForToday->setObjectName(
        QStringLiteral("nextUpcomingBirthdaysDismissForTodayCheck")
        );
    m_dialogLayout->addWidget(m_dismissForToday);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->setObjectName(QStringLiteral("nextUpcomingBirthdaysButtonBox"));
    buttons->setAccessibleName(translate("Dialog actions"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    m_dialogLayout->addWidget(buttons);
    if (auto* closeButton = buttons->button(QDialogButtonBox::Close))
    {
        closeButton->setObjectName(
            QStringLiteral("nextUpcomingBirthdaysCloseButton")
            );
    }

    updateText();
}

bool UpcomingBirthdaysDialog::dismissForToday() const
{
    return m_dismissForToday && m_dismissForToday->isChecked();
}

void UpcomingBirthdaysDialog::changeEvent(QEvent* event)
{
    QDialog::changeEvent(event);
    if (event && event->type() == QEvent::LanguageChange)
    {
        updateText();
    }
}

QWidget* UpcomingBirthdaysDialog::createSection(
    const QString& objectPrefix,
    const std::vector<Application::UpcomingBirthdayOccurrence>& birthdays,
    const bool centered
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

    if (objectPrefix == QStringLiteral("nextUpcomingBirthdaysToday"))
    {
        m_todayTitle = title;
    }
    else if (objectPrefix == QStringLiteral("nextUpcomingBirthdaysThisWeek"))
    {
        m_thisWeekTitle = title;
    }
    else
    {
        m_nextWeekTitle = title;
    }

    if (birthdays.empty())
    {
        auto* empty = new QLabel(section);
        empty->setObjectName(objectPrefix + QStringLiteral("Empty"));
        empty->setWordWrap(true);
        empty->setAlignment(centered ? Qt::AlignCenter : Qt::AlignLeft);
        layout->addWidget(empty);

        if (objectPrefix == QStringLiteral("nextUpcomingBirthdaysThisWeek"))
        {
            m_thisWeekEmpty = empty;
        }
        else if (objectPrefix == QStringLiteral("nextUpcomingBirthdaysNextWeek"))
        {
            m_nextWeekEmpty = empty;
        }
        return section;
    }

    for (std::size_t index = 0; index < birthdays.size(); ++index)
    {
        layout->addWidget(createEntry(
            objectPrefix,
            static_cast<int>(index),
            birthdays[index],
            centered
            ));
    }
    return section;
}

QWidget* UpcomingBirthdaysDialog::createEntry(
    const QString& objectPrefix,
    const int index,
    const Application::UpcomingBirthdayOccurrence& birthday,
    const bool centered
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
    const Application::UpcomingBirthdayOccurrence& birthday
    ) const
{
    QStringList details{
        QLocale().toString(asQDate(birthday.date), QLocale::LongFormat),
        groupText(birthday.group)
    };
    if (!birthday.position.empty())
    {
        details.append(fromUtf16(birthday.position));
    }
    return details.join(QStringLiteral(" • "));
}

QString UpcomingBirthdaysDialog::groupText(
    const Application::UpcomingBirthdayStaffGroup group
    ) const
{
    switch (group)
    {
    case Application::UpcomingBirthdayStaffGroup::KoreanTeacher:
        return translate("Korean Teacher");
    case Application::UpcomingBirthdayStaffGroup::NativeEnglishTeacher:
        return translate("Native English Teacher");
    case Application::UpcomingBirthdayStaffGroup::GsTeam:
        return translate("GS Team");
    }
    return {};
}

void UpcomingBirthdaysDialog::updateText()
{
    const QString title = translate("Upcoming Birthdays");
    setWindowTitle(title);
    if (m_headerTitle)
    {
        m_headerTitle->setText(title);
        m_headerTitle->setAccessibleName(title);
    }
    if (m_headerSubtitle)
    {
        const QString subtitle = translate(
            "Birthdays today and over the next two weeks.");
        m_headerSubtitle->setText(subtitle);
        m_headerSubtitle->setAccessibleName(subtitle);
        m_headerSubtitle->setVisible(!subtitle.isEmpty());
    }
    if (m_todayTitle)
    {
        m_todayTitle->setText(translate("Today's Birthdays"));
    }
    if (m_thisWeekTitle)
    {
        m_thisWeekTitle->setText(translate("This Week"));
    }
    if (m_nextWeekTitle)
    {
        m_nextWeekTitle->setText(translate("Next Week"));
    }
    if (m_thisWeekEmpty)
    {
        m_thisWeekEmpty->setText(translate("No birthdays this week."));
    }
    if (m_nextWeekEmpty)
    {
        m_nextWeekEmpty->setText(translate("No birthdays next week."));
    }
    if (m_dismissForToday)
    {
        m_dismissForToday->setText(translate("Don't show again today"));
    }
    for (const EntryPresentation& entry : m_entries)
    {
        if (entry.name)
        {
            const QString name = fromUtf16(entry.birthday.displayName);
            entry.name->setText(name);
            entry.name->setAccessibleName(name);
        }
        if (entry.detail)
        {
            const QString detail = detailText(entry.birthday);
            entry.detail->setText(detail);
            entry.detail->setAccessibleName(detail);
        }
    }
    for (QDialogButtonBox* buttons : findChildren<QDialogButtonBox*>())
    {
        if (auto* closeButton = buttons->button(QDialogButtonBox::Close))
        {
            closeButton->setText(translate("Close"));
        }
    }
}

} // namespace ClassMngr::Next::Ui
