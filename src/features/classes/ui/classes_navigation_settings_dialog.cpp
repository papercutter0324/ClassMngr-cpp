#include "classes_navigation_settings_dialog.h"
#include "ui/shared/widgets/text_fit_dialog_button_box.h"


#include <QDialogButtonBox>
#include <QGroupBox>
#include <QRadioButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

ClassesNavigationSettingsDialog::ClassesNavigationSettingsDialog(
    const ClassesNavigationSettingsValues& values,
    QWidget* parent
    )
    : DialogShell(QStringLiteral("classesNavigationSettings"), parent)
    , m_initialValues(values)
{
    setObjectName(QStringLiteral("classesNavigationSettingsDialog"));
    setWindowTitle(tr("Classes Settings"));
    setModal(true);
    resize(480, 280);
    buildUi();
}

ClassesNavigationSettingsValues ClassesNavigationSettingsDialog::values() const
{
    return {
        m_allClassesRadio->isChecked()
            ? ClassTabNavigation::VisibilityScope::AllClasses
            : ClassTabNavigation::VisibilityScope::ActiveSchedule
    };
}

void ClassesNavigationSettingsDialog::buildUi()
{
    auto* layout = contentLayout();

    auto* tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("classesNavigationSettingsTabs"));
    tabs->addTab(buildDisplayTab(), tr("Display"));
    layout->addWidget(tabs, 1);

    auto* buttons = addButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel
        );
    buttons->button(QDialogButtonBox::Save)->setText(tr("Save"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
}

QWidget* ClassesNavigationSettingsDialog::buildDisplayTab()
{
    auto* page = new QWidget(this);
    page->setObjectName(QStringLiteral("classesNavigationDisplayTab"));
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(24);
    layout->setAlignment(Qt::AlignTop);

    auto* classesShownGroup = new QGroupBox(tr("Classes Shown"), page);
    auto* classesShownLayout = new QVBoxLayout(classesShownGroup);
    classesShownLayout->setSpacing(12);

    m_allClassesRadio = new QRadioButton(tr("All Classes"), classesShownGroup);
    m_allClassesRadio->setObjectName(QStringLiteral("classesNavigationAllClasses"));
    m_allClassesRadio->setChecked(
        m_initialValues.visibilityScope
        == ClassTabNavigation::VisibilityScope::AllClasses
        );

    m_activeScheduleRadio = new QRadioButton(
        tr("Active Schedule"),
        classesShownGroup
        );
    m_activeScheduleRadio->setObjectName(
        QStringLiteral("classesNavigationActiveSchedule")
        );
    m_activeScheduleRadio->setChecked(
        m_initialValues.visibilityScope
        == ClassTabNavigation::VisibilityScope::ActiveSchedule
        );

    classesShownLayout->addWidget(m_allClassesRadio);
    classesShownLayout->addWidget(m_activeScheduleRadio);
    layout->addWidget(classesShownGroup);
    return page;
}
