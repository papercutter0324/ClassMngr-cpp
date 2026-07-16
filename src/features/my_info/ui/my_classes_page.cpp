#include "my_classes_page.h"

#include "ui/shared/styles/roles.h"

#include <QLabel>
#include <QShowEvent>

MyClassesPage::MyClassesPage(
    ApplicationServices* services,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
{
    setProperty("role", UiRoles::MyInfo);
    buildUi();
    refreshGeneratedContent();
}

void MyClassesPage::refresh()
{
    BasePage::refresh();

    if (isVisible())
    {
        refreshGeneratedContent();
    }
}

void MyClassesPage::retranslateUi()
{
    m_titleLabel->setText(tr("Class Information"));
    m_subtitleLabel->setText(
        tr("Review teacher, class, and roster details.")
        );
    refreshGeneratedContent();
}

void MyClassesPage::showEvent(QShowEvent* event)
{
    BasePage::showEvent(event);
    refreshGeneratedContent();
}
