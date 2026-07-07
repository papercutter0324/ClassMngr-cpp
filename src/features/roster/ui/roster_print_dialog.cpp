#include "features/roster/ui/roster_print_dialog.h"

#include "core/application_services.h"
#include "data/data_service.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

namespace
{
constexpr int AllClassesId = 0;
constexpr int CurrentClassId = 1;
constexpr int SelectedClassesId = 2;

int scopeId(
    RosterTemplatePrintService::Scope scope
    )
{
    switch (scope)
    {
    case RosterTemplatePrintService::Scope::CurrentClass:
        return CurrentClassId;

    case RosterTemplatePrintService::Scope::SelectedClasses:
        return SelectedClassesId;

    case RosterTemplatePrintService::Scope::AllClasses:
    default:
        return AllClassesId;
    }
}
}

RosterPrintDialog::RosterPrintDialog(
    ApplicationServices* services,
    int currentClassId,
    RosterTemplatePrintService::Scope defaultScope,
    QWidget* parent
    )
    : QDialog(parent)
    , m_services(services)
    , m_currentClassId(currentClassId)
    , m_defaultScope(defaultScope)
{
    buildUi();
    loadClasses();
    retranslateUi();
    updateClassListEnabled();
}

QString RosterPrintDialog::templatePath() const
{
    return m_templatePathEdit
        ? m_templatePathEdit->text().trimmed()
        : QString();
}

RosterTemplatePrintService::Scope RosterPrintDialog::selectedScope() const
{
    if (!m_scopeGroup)
    {
        return m_defaultScope;
    }

    switch (m_scopeGroup->checkedId())
    {
    case CurrentClassId:
        return RosterTemplatePrintService::Scope::CurrentClass;

    case SelectedClassesId:
        return RosterTemplatePrintService::Scope::SelectedClasses;

    case AllClassesId:
    default:
        return RosterTemplatePrintService::Scope::AllClasses;
    }
}

QList<int> RosterPrintDialog::selectedClassIds() const
{
    QList<int> ids;

    if (!m_classList)
    {
        return ids;
    }

    for (int index = 0; index < m_classList->count(); ++index)
    {
        const QListWidgetItem* item =
            m_classList->item(index);

        if (item && item->checkState() == Qt::Checked)
        {
            ids.append(item->data(Qt::UserRole).toInt());
        }
    }

    return ids;
}

void RosterPrintDialog::browseTemplate()
{
    const QString path =
        QFileDialog::getOpenFileName(
            this,
            tr("Choose Roster Template"),
            QString(),
            tr("Spreadsheet Templates (*.xlsx *.ods)")
            );

    if (!path.isEmpty() && m_templatePathEdit)
    {
        m_templatePathEdit->setText(path);
    }
}

void RosterPrintDialog::updateClassListEnabled()
{
    if (!m_classList)
    {
        return;
    }

    m_classList->setEnabled(
        selectedScope() == RosterTemplatePrintService::Scope::SelectedClasses
        );
}

void RosterPrintDialog::buildUi()
{
    setModal(true);
    resize(520, 520);

    auto* rootLayout =
        new QVBoxLayout(this);
    rootLayout->setSpacing(12);

    auto* templateGroup =
        new QGroupBox(this);
    auto* templateLayout =
        new QVBoxLayout(templateGroup);

    m_templateHintLabel =
        new QLabel(templateGroup);
    m_templateHintLabel->setWordWrap(true);

    auto* templatePathLayout =
        new QHBoxLayout();

    m_templatePathEdit =
        new QLineEdit(templateGroup);
    m_templatePathEdit->setClearButtonEnabled(true);

    m_browseButton =
        new QPushButton(templateGroup);

    templatePathLayout->addWidget(m_templatePathEdit, 1);
    templatePathLayout->addWidget(m_browseButton);

    templateLayout->addWidget(m_templateHintLabel);
    templateLayout->addLayout(templatePathLayout);

    auto* scopeGroupBox =
        new QGroupBox(this);
    auto* scopeLayout =
        new QVBoxLayout(scopeGroupBox);

    m_scopeGroup =
        new QButtonGroup(this);

    auto* allClassesRadio =
        new QRadioButton(scopeGroupBox);
    auto* currentClassRadio =
        new QRadioButton(scopeGroupBox);
    auto* selectedClassesRadio =
        new QRadioButton(scopeGroupBox);

    allClassesRadio->setObjectName(QStringLiteral("allClassesRadio"));
    currentClassRadio->setObjectName(QStringLiteral("currentClassRadio"));
    selectedClassesRadio->setObjectName(QStringLiteral("selectedClassesRadio"));

    m_scopeGroup->addButton(allClassesRadio, AllClassesId);
    m_scopeGroup->addButton(currentClassRadio, CurrentClassId);
    m_scopeGroup->addButton(selectedClassesRadio, SelectedClassesId);

    if (auto* button = m_scopeGroup->button(scopeId(m_defaultScope)))
    {
        button->setChecked(true);
    }

    scopeLayout->addWidget(allClassesRadio);
    scopeLayout->addWidget(currentClassRadio);
    scopeLayout->addWidget(selectedClassesRadio);

    m_classList =
        new QListWidget(scopeGroupBox);
    m_classList->setSelectionMode(QAbstractItemView::NoSelection);
    scopeLayout->addWidget(m_classList, 1);

    auto* buttons =
        new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            this
            );

    rootLayout->addWidget(templateGroup);
    rootLayout->addWidget(scopeGroupBox, 1);
    rootLayout->addWidget(buttons);

    connect(
        m_browseButton,
        &QPushButton::clicked,
        this,
        &RosterPrintDialog::browseTemplate
        );

    connect(
        m_scopeGroup,
        &QButtonGroup::idClicked,
        this,
        &RosterPrintDialog::updateClassListEnabled
        );

    connect(
        buttons,
        &QDialogButtonBox::accepted,
        this,
        &QDialog::accept
        );

    connect(
        buttons,
        &QDialogButtonBox::rejected,
        this,
        &QDialog::reject
        );
}

void RosterPrintDialog::loadClasses()
{
    if (!m_classList || !m_services || !m_services->dataService())
    {
        return;
    }

    const QList<Classroom> classes =
        m_services->dataService()->getClasses();

    for (const Classroom& classroom : classes)
    {
        auto* item =
            new QListWidgetItem(
                classroom.name.trimmed().isEmpty()
                    ? tr("Class %1").arg(classroom.id)
                    : classroom.name.trimmed(),
                m_classList
                );

        item->setFlags(
            item->flags()
            | Qt::ItemIsUserCheckable
            );
        item->setCheckState(
            classroom.id == m_currentClassId
                ? Qt::Checked
                : Qt::Unchecked
            );
        item->setData(Qt::UserRole, classroom.id);
    }
}

void RosterPrintDialog::retranslateUi()
{
    setWindowTitle(tr("Print Rosters"));

    if (auto* group = qobject_cast<QGroupBox*>(m_templateHintLabel->parentWidget()))
    {
        group->setTitle(tr("Template"));
    }

    if (m_templateHintLabel)
    {
        m_templateHintLabel->setText(
            tr("Leave this blank to use the bundled roster template for this platform.")
            );
    }

    if (m_templatePathEdit)
    {
        m_templatePathEdit->setPlaceholderText(
            tr("Bundled roster template")
            );
    }

    if (m_browseButton)
    {
        m_browseButton->setText(tr("Browse..."));
    }

    if (m_scopeGroup)
    {
        if (auto* button = m_scopeGroup->button(AllClassesId))
        {
            button->setText(tr("All classes"));
        }

        if (auto* button = m_scopeGroup->button(CurrentClassId))
        {
            button->setText(tr("Current class"));
        }

        if (auto* button = m_scopeGroup->button(SelectedClassesId))
        {
            button->setText(tr("Selected classes"));
        }
    }

    if (m_classList)
    {
        if (auto* group = qobject_cast<QGroupBox*>(m_classList->parentWidget()))
        {
            group->setTitle(tr("Classes"));
        }
    }
}
