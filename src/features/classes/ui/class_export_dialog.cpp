#include "class_export_dialog.h"

#include "core/utils/sidebar_node_naming.h"
#include "data/data_service.h"
#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/teacher.h"

#include <QDialogButtonBox>
#include <QCollator>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace
{
QString classDisplayName(
    DataService* dataService,
    const Classroom& classroom
    )
{
    const ClassInfo info = dataService->loadClassInfo(classroom.id);
    Teacher teacher;

    if (info.teacherId > 0)
    {
        teacher = dataService->getTeacher(info.teacherId);
    }

    const QString formatted =
        SidebarNodeNaming::formatClassDisplayName(info, teacher).trimmed();

    if (!formatted.isEmpty())
    {
        return formatted;
    }

    if (!classroom.name.trimmed().isEmpty())
    {
        return classroom.name.trimmed();
    }

    return QObject::tr("Class %1").arg(classroom.id);
}
}

ClassExportDialog::ClassExportDialog(
    DataService* dataService,
    QWidget* parent
    )
    : QDialog(parent)
{
    setWindowTitle(tr("Export Classes"));
    setModal(true);
    resize(620, 480);

    auto* layout = new QVBoxLayout(this);
    auto* description = new QLabel(
        tr("Select the classes to include in the package."), this);
    description->setWordWrap(true);
    layout->addWidget(description);

    m_classList = new QListWidget(this);
    m_classList->setObjectName(QStringLiteral("classExportList"));
    layout->addWidget(m_classList, 1);

    if (dataService)
    {
        QList<QPair<QString, int>> classes;

        for (const Classroom& classroom : dataService->getClasses())
        {
            classes.append({
                classDisplayName(dataService, classroom),
                classroom.id
            });
        }

        QCollator collator;
        collator.setCaseSensitivity(Qt::CaseInsensitive);
        collator.setNumericMode(true);

        std::sort(
            classes.begin(),
            classes.end(),
            [&collator](const auto& left, const auto& right)
            {
                const int comparison = collator.compare(
                    left.first, right.first);

                return comparison == 0
                    ? left.second < right.second
                    : comparison < 0;
            });

        for (const auto& [displayName, classId] : std::as_const(classes))
        {
            auto* item = new QListWidgetItem(displayName, m_classList);
            item->setData(Qt::UserRole, classId);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
        }
    }

    auto* selectionLayout = new QHBoxLayout;
    auto* selectAllButton = new QPushButton(tr("Select All"), this);
    auto* clearButton = new QPushButton(tr("Clear"), this);
    selectionLayout->addWidget(selectAllButton);
    selectionLayout->addWidget(clearButton);
    selectionLayout->addStretch(1);
    layout->addLayout(selectionLayout);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_exportButton = buttons->button(QDialogButtonBox::Ok);
    m_exportButton->setObjectName(QStringLiteral("exportClassesButton"));
    m_exportButton->setText(tr("Export"));
    layout->addWidget(buttons);

    connect(selectAllButton, &QPushButton::clicked, this, [this]()
    {
        setAllChecked(true);
    });
    connect(clearButton, &QPushButton::clicked, this, [this]()
    {
        setAllChecked(false);
    });
    connect(m_classList, &QListWidget::itemChanged,
            this, &ClassExportDialog::updateExportEnabled);
    connect(buttons, &QDialogButtonBox::accepted,
            this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);

    updateExportEnabled();
}

QList<int> ClassExportDialog::selectedClassIds() const
{
    QList<int> result;

    for (int index = 0; index < m_classList->count(); ++index)
    {
        const QListWidgetItem* item = m_classList->item(index);

        if (item->checkState() == Qt::Checked)
        {
            result.append(item->data(Qt::UserRole).toInt());
        }
    }

    return result;
}

void ClassExportDialog::setAllChecked(
    bool checked
    )
{
    for (int index = 0; index < m_classList->count(); ++index)
    {
        m_classList->item(index)->setCheckState(
            checked ? Qt::Checked : Qt::Unchecked);
    }

    updateExportEnabled();
}

void ClassExportDialog::updateExportEnabled()
{
    if (m_exportButton)
    {
        m_exportButton->setEnabled(!selectedClassIds().isEmpty());
    }
}
