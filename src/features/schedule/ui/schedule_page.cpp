#include "schedule_page.h"

#include "core/fontmanager.h"
#include "features/schedule/ui/schedule_widget.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"

#include <QFont>
#include <QLabel>
#include <QShowEvent>
#include <QVBoxLayout>

SchedulePage::SchedulePage(
    ApplicationServices* services,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
{
    setProperty("role", UiRoles::Schedule);
    buildUi();
}

void SchedulePage::refresh()
{
    BasePage::refresh();

    if (isVisible() && m_scheduleWidget)
    {
        m_scheduleWidget->refreshSchedule();
    }
}

void SchedulePage::clearDatabaseState()
{
    if (m_scheduleWidget)
    {
        m_scheduleWidget->clearDatabaseState();
    }
}

void SchedulePage::retranslateUi()
{
    if (m_titleLabel)
    {
        m_titleLabel->setText(
            tr("Weekly Class Schedule")
            );
    }

    if (m_subtitleLabel)
    {
        m_subtitleLabel->setText(
            tr("Generated from registered classes and their meeting times.")
            );
    }

    if (m_scheduleWidget)
    {
        m_scheduleWidget->retranslateUi();
    }
}

void SchedulePage::showEvent(
    QShowEvent* event
    )
{
    BasePage::showEvent(event);

    if (m_scheduleWidget)
    {
        m_scheduleWidget->refreshSchedule();
    }
}

void SchedulePage::buildUi()
{
    contentLayout()->setContentsMargins(
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin
        );
    contentLayout()->setSpacing(
        UiConstants::Pages::Spacing
        );

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
            tr("Weekly Class Schedule"),
            this
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
            tr("Generated from registered classes and their meeting times."),
            this
            );
    m_subtitleLabel->setObjectName("pageSubtitle");
    m_subtitleLabel->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::SubtitleFontSize
            )
        );

    headerLayout->addWidget(m_titleLabel);
    headerLayout->addWidget(m_subtitleLabel);
    contentLayout()->addLayout(headerLayout);
    contentLayout()->addSpacing(
        UiConstants::Pages::HeaderContentSpacing
        );

    m_scheduleWidget =
        new ScheduleWidget(
            m_services,
            this
            );
    contentLayout()->addWidget(
        m_scheduleWidget,
        1
        );

    connect(
        m_scheduleWidget,
        &ScheduleWidget::classInfoSaved,
        this,
        &SchedulePage::classInfoSaved
        );
}
