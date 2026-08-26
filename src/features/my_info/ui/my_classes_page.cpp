#include "my_classes_page.h"

#include "ui/shared/styles/roles.h"

#include <QLabel>

MyClassesPage::MyClassesPage(
    ApplicationServices* services,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
{
    setProperty("role", UiRoles::MyInfo);
    buildUi();
}

void MyClassesPage::refresh()
{
    BasePage::refresh();

    refreshGeneratedContent();
}

void MyClassesPage::clearDatabaseState()
{
    clearClassInformation();
    m_classInformationTabs = nullptr;
    m_selectedClassId = -1;
}

void MyClassesPage::retranslateUi()
{
    m_titleLabel->setText(tr("Class Information"));
    m_subtitleLabel->setText(
        tr("Review teacher, class, and roster details.")
        );
    refreshGeneratedContent();
}
