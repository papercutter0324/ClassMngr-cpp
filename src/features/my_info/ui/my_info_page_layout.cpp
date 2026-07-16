#include "my_info_page.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/widgets/sectioncards/class_info_section_card.h"
#include "ui/shared/widgets/sections/schedule_section_widget.h"

#include <QFrame>
#include <QLabel>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTextEdit>
#include <QVBoxLayout>

namespace
{
constexpr int UntitledCardTopMargin = 4;
constexpr int TextEditVerticalPadding = 24;

int textEditHeightForLines(
    const QTextEdit* edit,
    int lines
    )
{
    if (!edit)
    {
        return 0;
    }

    return edit->fontMetrics().lineSpacing() * lines
        + TextEditVerticalPadding;
}
}

void MyInfoPage::buildUi()
{
    contentLayout()->setContentsMargins(
        0,
        0,
        0,
        0
        );

    m_scrollArea =
        new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    m_scrollContent =
        new QWidget(m_scrollArea);
    m_scrollContentLayout =
        new QVBoxLayout(m_scrollContent);
    m_scrollContentLayout->setContentsMargins(
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin
        );
    m_scrollContentLayout->setSpacing(
        UiConstants::Pages::Spacing
        );
    m_scrollContentLayout->setAlignment(Qt::AlignTop);

    auto* headerLayout =
        new QVBoxLayout;
    headerLayout->setContentsMargins(
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin
        );
    headerLayout->setSpacing(
        UiConstants::Pages::HeaderSpacing
        );

    m_titleLabel =
        new QLabel(
            pageTitle(),
            m_scrollContent
            );
    m_titleLabel->setObjectName("pageTitle");
    m_titleLabel->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::TitleFontSize,
            QFont::Bold
            )
        );

    m_subtitleLabel =
        new QLabel(
            pageSubtitle(),
            m_scrollContent
            );
    m_subtitleLabel->setObjectName("pageSubtitle");
    m_subtitleLabel->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::SubtitleFontSize
            )
        );

    headerLayout->addWidget(m_titleLabel);
    headerLayout->addWidget(m_subtitleLabel);
    m_scrollContentLayout->addLayout(headerLayout);
    m_scrollContentLayout->addSpacing(
        UiConstants::Pages::HeaderContentSpacing
        );

    if (includesMyInformation())
    {
        buildMyInformationSection();
        buildSignatureSection();
    }

    if (includesClassSchedule())
    {
        buildClassScheduleSection();
    }

    if (includesClassInformation())
    {
        buildClassInformationSection();
    }

    if (includesMonthlyCalendar())
    {
        buildMonthlyCalendarSection();
    }

    m_scrollContentLayout->addStretch();

    m_scrollArea->setWidget(m_scrollContent);
    contentLayout()->addWidget(m_scrollArea);
}
QString MyInfoPage::pageTitle() const
{
    switch (m_mode)
    {
    case MyInfoPageMode::Information:
        return tr("My Details");

    case MyInfoPageMode::Calendar:
        return tr("Calendar");

    case MyInfoPageMode::Schedule:
        return tr("Schedule");

    case MyInfoPageMode::ClassInformation:
        return tr("Class Information");
    }

    return QString();
}
QString MyInfoPage::pageSubtitle() const
{
    switch (m_mode)
    {
    case MyInfoPageMode::Information:
        return tr("Manage your personal information and signature.");

    case MyInfoPageMode::Calendar:
        return tr("View and manage your monthly events and upcoming dates.");

    case MyInfoPageMode::Schedule:
        return tr("View and adjust your class schedule.");

    case MyInfoPageMode::ClassInformation:
        return tr("Review teacher, class, and roster details.");
    }

    return QString();
}
bool MyInfoPage::includesMyInformation() const
{
    return m_mode == MyInfoPageMode::Information;
}
bool MyInfoPage::includesClassSchedule() const
{
    return m_mode == MyInfoPageMode::Schedule;
}
bool MyInfoPage::includesClassInformation() const
{
    return m_mode == MyInfoPageMode::ClassInformation;
}
bool MyInfoPage::includesMonthlyCalendar() const
{
    return m_mode == MyInfoPageMode::Calendar;
}
void MyInfoPage::buildClassScheduleSection()
{
    if (m_mode != MyInfoPageMode::Schedule)
    {
        m_scrollContentLayout->addSpacing(
            UiConstants::Pages::MajorSectionSpacing
            );

        m_classScheduleHeading =
            createTopLevelHeading(
                tr("Schedule"),
                m_scrollContent
                );
        m_scrollContentLayout->addWidget(
            m_classScheduleHeading
            );
    }

    auto* card =
        new QFrame(m_scrollContent);
    card->setProperty(
        "role",
        UiRoles::Card
        );
    card->setObjectName(
        "sectionCard"
        );

    auto* cardLayout =
        new QVBoxLayout(card);
    cardLayout->setAlignment(Qt::AlignTop);
    cardLayout->setContentsMargins(
        UiConstants::ClassInfo::SectionCard::Margin,
        UntitledCardTopMargin,
        UiConstants::ClassInfo::SectionCard::Margin,
        UiConstants::ClassInfo::SectionCard::Margin
        );
    cardLayout->setSpacing(
        UiConstants::ClassInfo::SectionCard::Spacing
        );

    m_scheduleWidget =
        new ScheduleSectionWidget(
            m_services,
            card
            );

    connect(
        m_scheduleWidget,
        &ScheduleSectionWidget::classInfoSaved,
        this,
        &MyInfoPage::classInfoSaved
        );

    cardLayout->addWidget(
        m_scheduleWidget
        );

    m_scrollContentLayout->addWidget(
        card
        );
}
void MyInfoPage::buildClassInformationSection()
{
    if (m_mode != MyInfoPageMode::ClassInformation)
    {
        m_scrollContentLayout->addSpacing(
            UiConstants::Pages::MajorSectionSpacing
            );

        m_classInformationHeading =
            createTopLevelHeading(
                tr("Class Information"),
                m_scrollContent
                );
        m_scrollContentLayout->addWidget(
            m_classInformationHeading
            );
    }

    m_classInformationContent =
        new QWidget(m_scrollContent);
    m_classInformationLayout =
        new QVBoxLayout(m_classInformationContent);
    m_classInformationLayout->setContentsMargins(0, 0, 0, 0);
    m_classInformationLayout->setSpacing(
        UiConstants::ClassInfo::Page::ContentSpacing
        );
    m_classInformationLayout->setAlignment(Qt::AlignTop);
    m_scrollContentLayout->addWidget(
        m_classInformationContent
        );
}
QLabel* MyInfoPage::createTopLevelHeading(
    const QString& text,
    QWidget* parent
    ) const
{
    auto* label =
        new QLabel(
            text,
            parent
            );

    label->setObjectName("sectionTitle");
    label->setAlignment(Qt::AlignCenter);
    label->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::SectionTitleFontSize,
            QFont::DemiBold
            )
        );

    return label;
}
QTextEdit* MyInfoPage::createTextEdit(
    int minimumLines,
    bool readOnly,
    QWidget* parent
    ) const
{
    auto* edit =
        new QTextEdit(parent);

    edit->setReadOnly(
        readOnly
        );
    edit->setMinimumHeight(
        textEditHeightForLines(
            edit,
            minimumLines
            )
        );
    edit->setVerticalScrollBarPolicy(
        Qt::ScrollBarAsNeeded
        );
    edit->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAsNeeded
        );
    edit->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred
        );

    return edit;
}
QLabel* MyInfoPage::createFieldLabel(
    const QString& text,
    QWidget* parent
    ) const
{
    auto* label =
        new QLabel(
            text,
            parent
            );

    label->setContentsMargins(
        UiConstants::ClassInfo::Form::LabelIndent,
        0,
        0,
        0
        );

    return label;
}
